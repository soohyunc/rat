/*
 * FILE: cc_red.c
 * PROGRAM: RAT / redundancy
 * AUTHOR: Orion Hodson
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-97 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include "config.h"
#include "session.h"
#include "codec.h"
#include "channel.h"
#include "receive.h"
#include "util.h"
#include "cc_red.h"

#define MAX_RED_LAYERS  3
#define MAX_RED_OFFSET  20

#define RED_F(x)  ((x)&0x80000000)
#define RED_PT(x) (((x)>>24)&0x7f)
#define RED_OFF(x) (((x)>>10)&0x3fff)
#define RED_LEN(x) ((x)&0x3ff)

typedef struct s_red_coder {
        int         cnt;
        int         offset[MAX_RED_LAYERS];            /* Relative to primary    */
        int         coding[MAX_RED_LAYERS];            /* Coding of each layer   */
        int         nlayers;
        coded_unit  c[MAX_RED_OFFSET][MAX_RED_LAYERS]; /* Circular buffer        */
        int         n[MAX_RED_OFFSET];                 /* Number of encs present */
        cc_unit     buf;                               /* outgoing cc_unit       */
        int         head;
        int         tail;
        int         len;
} red_coder_t;

red_coder_t *
new_red_coder(void)
{
        red_coder_t *r = (red_coder_t*)xmalloc(sizeof(red_coder_t));
        memset(r,0,sizeof(red_coder_t));
        return r;
}

void
free_red_coder(red_coder_t *r)
{
        xfree(r);
}

static void
clear_red_headers(red_coder_t *r) 
{
        int hdr_idx = 0;
        while (r->buf.iov[hdr_idx].iov_len == 4) {
                block_free(r->buf.iov[hdr_idx].iov_base, r->buf.iov[hdr_idx].iov_len);
                hdr_idx++;
        }
        assert(r->buf.iov[hdr_idx].iov_len == 1);
        block_free(r->buf.iov[hdr_idx].iov_base, r->buf.iov[hdr_idx].iov_len);
}

static void
flush_red(red_coder_t *r)
{
        int i,j;
        i = r->head;
        while(i!=r->tail) {
                for(j=0;j<r->n[i];j++) {
                        clear_coded_unit(&r->c[i][j]);
                }
                r->n[i] = 0;
                i = (i+1)%MAX_RED_OFFSET;
        }
        r->head = r->tail = r->len = 0;
        if (r->buf.iovc) clear_red_headers(r);
}

/* expects string with "codec1/offset1/codec2/offset2..." 
 * nb we don't save samples so if coding changes we take our chances.
 * No attempt is made to recode to new format as this takes an 
 * unspecified amount of cpu.
 */

int
red_config(session_struct *sp, red_coder_t *r, char *cmd)
{
        char *s;
        codec_t *cp;
        int   i;

        UNUSED(sp);
        
        flush_red(r);

        r->offset[0] = 0;
        r->nlayers = 0;

        s = strtok(cmd, "/");
        do {
                cp = get_codec_byname(s,sp);
                if (!cp) {
                        fprintf(stderr, "%s:%d codec not recognized.\n", __FILE__, __LINE__);
                        exit(-1);
                }
                r->coding[r->nlayers] = cp->pt;
                s = strtok(NULL,"/");
                r->offset[r->nlayers] = atoi(s);

                if (r->offset[r->nlayers]<0 || r->offset[r->nlayers]>MAX_RED_OFFSET) {
                        fprintf(stderr, "%s:%d offset of < 0 or > MAX_RED_OFFSET.\n", __FILE__, __LINE__);
                        exit(-1);
                }

                for(i=0;i<r->nlayers;i++) {
                        if (r->offset[i]==r->offset[r->nlayers]) {
                                fprintf(stderr, "%s:%d multiple encodings at offset %d", __FILE__, __LINE__, r->offset[i]);
                                exit(-1);
                        }
                }
                r->nlayers++;
        } while((s=strtok(NULL,"/")) && r->nlayers<MAX_RED_LAYERS);

        sp->encodings[0] = r->coding[0];
        r->offset[0]     = 0;

        return TRUE;
}

#define RED_FRAG_SZ 32

void 
red_qconfig(session_struct    *sp, 
            red_coder_t *r,
            char *buf, int blen) 
{
        codec_t *cp;
        int i,fraglen,len;
        char fragbuf[RED_FRAG_SZ];

        UNUSED(sp);

        len = 0;
        for(i=0;i<r->nlayers;i++) {
                cp = get_codec(r->coding[i]);
                sprintf(fragbuf,"%s/%d/", cp->name, r->offset[i]);
                fraglen = strlen(fragbuf);
                
                if ((fraglen + len) < blen) {
                        sprintf(buf + len, "%s", fragbuf);
                        len += fraglen;
                } else {
                        dprintf("Buffer too small!");
                        break;
                }
        }
        /* zap trailing slash */
        if (len>0) {
                buf[len-1] = '\0';
        }
}

static coded_unit *
red_coded_unit(red_coder_t *r, int pt, int off)
{
        int i,pos;
        /* find position in circular buffer */
        if (off >= MAX_RED_OFFSET) {
#ifdef DEBUG
                fprintf(stderr,"%s:%d Attempt to get block outside of buffer.\n", __FILE__,__LINE__);
#endif
                return NULL;
        }

        if (off > r->len)
                return NULL;

        pos = r->tail - off;
        pos += (pos<0)?MAX_RED_OFFSET:0;
    
        i = 0;
        while(i<r->n[pos]) {
                if (r->c[pos][i].cp->pt == pt) 
                        return &r->c[pos][i];
                i++;
        }
        return NULL;
}

__inline static int
red_offset_time(int coding, int units)
{
        codec_t *cp = get_codec(coding);
        return cp->unit_len*units;
}

int
red_encode(session_struct *sp, 
           sample *raw, 
           cc_unit *cu, 
           red_coder_t *r)
{
        int i, j, units, hdr_idx, data_idx, send;

        r->coding[0] = sp->encodings[0];
        units = get_units_per_packet(sp);

        r->cnt++;

        if (raw) {
                for(i=0;i<r->nlayers;i++) {
                        j = 0;
                        while(j < r->n[r->tail] && r->c[r->tail][j].cp 
                              && r->c[r->tail][j].cp->pt != r->coding[i])
                                j++;
                        if (j == r->n[r->tail]) {
                                encoder(sp, raw, r->coding[i], &r->c[r->tail][j]);
                                r->n[r->tail]++;
                        }
                }
                send = !(r->cnt % units);
        } else {
                /* check there is some primary to send */
                i = send = 0;
                while(++i<units) {
                        send += r->n[(r->head + r->len - i)%MAX_RED_OFFSET] ? 1 : 0;
                }
                if (!send) r->cnt = 0;
        }

        if (send) {
                int len, offset;
                coded_unit *c;
                hdr_idx = 0;

                /* free old headers */
                if (r->buf.iovc>1) clear_red_headers(r);

                hdr_idx  = 0;
                data_idx = MAX_RED_LAYERS;

                for(i=r->nlayers-1;i>=0;i--) {
                        offset = len = 0;
                        for(j=units-1;j>=0;j--) {
                                c = red_coded_unit(r,r->coding[i],j+r->offset[i]);
                                if (c) {
                                        if (len == 0) {
                                                r->buf.iov[hdr_idx].iov_len  = (i==0)?1:4;
                                                r->buf.iov[hdr_idx].iov_base = 
                                                        (caddr_t)block_alloc(r->buf.iov[hdr_idx].iov_len);
                                                offset  = r->offset[i] + j + 1 - units;
                                                if (c->state_len) {
                                                        r->buf.iov[data_idx].iov_base = c->state;
                                                        r->buf.iov[data_idx++].iov_len = c->state_len;
                                                        len += c->state_len;
                                                }
                                        }
                                        r->buf.iov[data_idx].iov_base  = c->data;
                                        r->buf.iov[data_idx++].iov_len = c->data_len;
                                        len += c->data_len;
                                }
                        }
            
                        assert(len < 1<<10);    
                        if (len != 0) {
                                if (i==0) {
                                        char *h = (char*)r->buf.iov[hdr_idx].iov_base;
                                        (*h) = r->coding[0];
                                        printf("offset(0)\n");
                                } else {
                                        unsigned int *h = (unsigned int*)r->buf.iov[hdr_idx].iov_base;
                                        (*h)  = (0x80|r->coding[i])<<24;
                                        (*h) |=(red_offset_time(r->coding[0],offset)<<10);
                                        printf("offset = %d head = %d tail = %d\n",offset,r->head,r->tail);
                                        (*h) |= len;
                                        (*h) = htonl((*h));
                                }
                                hdr_idx++;
                        }
                }
                /* fix gap between headers and data */
                r->buf.iovc = hdr_idx + data_idx - MAX_RED_LAYERS;
                memcpy(r->buf.iov+hdr_idx, 
                       r->buf.iov+MAX_RED_LAYERS, 
                       sizeof(struct iovec)*(data_idx - MAX_RED_LAYERS ));
                /* transfer to cc unit */
                memcpy(cu->iov+1,
                       r->buf.iov,
                       sizeof(struct iovec)*r->buf.iovc);
                cu->iovc = 1+r->buf.iovc;
        }
    
        r->tail = (r->tail+1) % MAX_RED_OFFSET;
        r->len++;

        /* Clear old state out */
        if (r->tail == r->head) {
                for(i=0;i<r->n[r->head];i++) {
                        if (r->c[r->head][i].data_len) clear_coded_unit(&r->c[r->head][i]);
                }
                r->n[r->head] = 0;
                r->head = (r->head+1) % MAX_RED_OFFSET;
                r->len--;
        }
    
        return send;
}


static int
red_max_offset(cc_unit *ccu)
{
        int i,off=0,max_off=-1;
        for(i=0;i<ccu->iovc && ccu->iov[i].iov_len==4;i++) {
                off = RED_OFF(ntohl(*((u_int32*)ccu->iov[i].iov_base)));
                if (off>max_off) max_off = off;
        }
        return max_off;
}

/*
 * In the previous scheme all of the codec data was split across
 * rx elements as soon as it was received and the the playout point
 * was shifted with a guess of the correct offset.
 *
 * Now we should have enough information in this unit (if not the first
 * of a group) or in a further unit (if the first of the group).  If we don't
 * then we ignore the data.
 *
 */

void
red_decode(rx_queue_element_struct *u)
{
        rx_queue_element_struct *su;
        u_int32 red_hdr;
        int i, max_off, hdr_idx, data_idx;
        int off, len;
        codec_t *cp;

        cc_unit *cu = u->ccu[0];

        if (cu->iov[0].iov_len == 1) {
                /* This is the start of the talkspurt       
                 * and we don't know the appropriate offset 
                 * for this data, but we can find it from
                 * one of the headers from a later red pckt. 
                 */
                max_off = -1;
                i = 0;
                su = u;
                while(++i<MAX_RED_OFFSET && 
                      ((su = get_rx_unit(1, cu->cc->pt, su))) &&
                        max_off == -1) {
                        if (su->ccu_cnt) {
                                max_off = red_max_offset(su->ccu[0]);
                        }
                }
                if (max_off == -1) {
#ifdef DEBUG
                        fprintf(stderr,"%s:%d maximum offset not found\n",
                                __FILE__,__LINE__);
#endif
                        return; /* couldn't find offset info - giving up */
                }
        } else {
                max_off = red_max_offset(cu);
        }

        data_idx = hdr_idx = 0;
        while(cu->iov[data_idx].iov_len == 4)
                data_idx++;
        assert(cu->iov[data_idx].iov_len == 1);
        data_idx++; 
        /* decode redundant data */
        while(cu->iov[hdr_idx].iov_len == 4) {
                red_hdr = ntohl((*((u_int32*)cu->iov[hdr_idx].iov_base)));
                cp      = get_codec(RED_PT(red_hdr));
                if (!cp) {
#ifdef DEBUG
                        fprintf(stderr,"%s:%d codec not found.\n",__FILE__,__LINE__);
#endif            
                        return;
                }
                len     = RED_LEN(RED_LEN(red_hdr));
                off     = max_off - RED_OFF(red_hdr);
                su      = get_rx_unit(off/cp->unit_len, cu->cc->pt, u);
                i = 0;
                while(len>0 && su) {
                        if (i==0 && cp->sent_state_sz) {
                                len -= cp->sent_state_sz;
                                add_comp_data(su,cp->pt,(cu->iov+data_idx),2);
                                data_idx += 2;
                        } else {
                                add_comp_data(su,cp->pt,(cu->iov+data_idx),1);
                                data_idx += 1;
                        }
                        len -= cp->max_unit_sz;
                        su = get_rx_unit(1, cu->cc->pt, su);
                        i++;
                }
                hdr_idx++;
        }
        assert(data_idx<cu->iovc);
        /* decode primary */
        cp = get_codec(*((char*)cu->iov[hdr_idx].iov_base)&0x7f);
        if (!cp) {
#ifdef DEBUG
                fprintf(stderr,"%s:%d primary codec not found.\n",__FILE__,__LINE__);
#endif
                return;
        }
        su = get_rx_unit(max_off/cp->unit_len, cu->cc->pt, u);
        i = 0;
        while(data_idx < cu->iovc && su) {
                if (i==0 && cp->sent_state_sz) {
                        add_comp_data(su,cp->pt,(cu->iov+data_idx),2);
                        data_idx += 2;
                } else {
                        add_comp_data(su,cp->pt,(cu->iov+data_idx),1);
                        data_idx += 1;
                }
                su = get_rx_unit(1,cu->cc->pt,su);
                i++;
        }
}    

int
red_bps(session_struct *sp, red_coder_t *r)
{
        int i, b, ups, upp;
        codec_t *cp;
        b = 0;
        upp = get_units_per_packet(sp);
        cp  = get_codec(r->coding[0]);
        ups = cp->freq / cp->unit_len;
        for(i=0;i<r->nlayers;i++) {
                cp = get_codec(r->coding[i]);
                b += cp->max_unit_sz * upp + cp->sent_state_sz + 4;
        }
        b += 1 + (r->nlayers-1)*4 + 12; /* headers */
        return (8*b*ups/upp);
}


/* this ultra paranoid function works out the correct allocation
 * of memory blocks for a redundantly encoded block, sets the number 
 * of trailing units needed to prod the channel coder and returns 
 * the number units per packet of the primary encoding.
 */

int
red_valsplit(char *blk, unsigned int blen, cc_unit *cu, int *trailing) {
        int tlen, n, todo;
        int hdr_idx, data_idx; 
        u_int32 red_hdr, max_off;
        codec_t *cp;
    
        assert(!cu->iovc);
        max_off = 0;
        hdr_idx = 0;
        data_idx = MAX_RED_LAYERS;
        todo     = blen;
        while(RED_F((red_hdr=ntohl(*((u_int32*)blk)))) && todo >0) {
                cu->iov[hdr_idx++].iov_len = 4;
                todo -= 4;
                blk  += 4;

                cp = get_codec(RED_PT(red_hdr));
                if (!cp) goto fail;

                if (RED_OFF(red_hdr)>max_off) 
                        max_off = RED_OFF(red_hdr);
                tlen = RED_LEN(red_hdr);
                if (cp->sent_state_sz) {
                        cu->iov[data_idx++].iov_len = cp->sent_state_sz;
                        todo -= cp->sent_state_sz;
                        tlen -= cp->sent_state_sz;
                }
                while(todo>0 && tlen>0) {
                        cu->iov[data_idx++].iov_len = cp->max_unit_sz;
                        todo -= cp->max_unit_sz;
                        tlen -= cp->max_unit_sz;
                }
        }

        if (todo == 0) goto fail;
        cp = get_codec((*blk)&0x7f);
        if (!cp) goto fail;

        cu->iov[hdr_idx++].iov_len = 1;
        todo -= 1;

        if (cp->sent_state_sz) {
                cu->iov[data_idx++].iov_len = cp->sent_state_sz;
                todo -= cp->sent_state_sz;
        }

        n =  0;
        while(todo>0) {
                cu->iov[data_idx++].iov_len = cp->max_unit_sz;
                todo -= cp->max_unit_sz;
                n++;
        }

        if (todo||hdr_idx>MAX_RED_LAYERS) goto fail; 

        /* push headers and data against each other */
        cu->iovc = hdr_idx + data_idx - MAX_RED_LAYERS;
        memcpy(cu->iov+hdr_idx, 
               cu->iov+MAX_RED_LAYERS, 
               sizeof(struct iovec)*(data_idx-MAX_RED_LAYERS));

        (*trailing) = max_off/cp->unit_len + n;
        return (n);
fail:
        for(hdr_idx=0; hdr_idx<cu->iovc; hdr_idx++) {
                fprintf(stderr,
                        "%d -> %03d bytes", 
                        hdr_idx,
                        cu->iov[hdr_idx].iov_len);
        }
        cu->iovc = 0;
        return 0;
}

int
red_wrapped_pt(char *blk, unsigned int blen)
{
        u_int32 *hdr;
        int todo;

        hdr  = (u_int32*)blk;
        todo = blen;

        while (ntohl(*hdr)&0x80 && todo>0) {
                hdr++;
                todo -= 4;
        }
        return (todo ? RED_PT(ntohl(*hdr)) : -1);
}








