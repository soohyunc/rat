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

#ifndef   WIN32
#include <sys/param.h>
#endif /* WIN32 */

#include <stdlib.h>
#include <sys/types.h>

#include "assert.h"
#include "config.h"
#include "session.h"
#include "codec.h"
#include "channel.h"
#include "receive.h"
#include "util.h"
#include "cc_red.h"

#define MAX_RED_LAYERS  4
/* 3 is excessive, and the ui lets folks set 2, 4 is some margin for decoder */
#define MAX_RED_OFFSET  40

#define RED_F(x)  ((x)&0x80000000)
#define RED_PT(x) (((x)>>24)&0x7f)
#define RED_OFF(x) (((x)>>10)&0x3fff)
#define RED_LEN(x) ((x)&0x3ff)

typedef struct s_red_coder {
        int         offset[MAX_RED_LAYERS];            /* Relative to primary    */
        int         coding[MAX_RED_LAYERS];            /* Coding of each layer   */
        int         nlayers;
        cc_unit    *c[MAX_RED_OFFSET][MAX_RED_LAYERS]; /* Circular buffer        */
        int         n[MAX_RED_OFFSET];                 /* Number of encs present */
        int         head;
        int         tail;
        int         len;
        cc_unit     last;
        int         last_hdr_idx;
} red_coder_t;

__inline static void 
red_clear_row(red_coder_t *r, int row)
{
        int i;
        for(i=0;i<r->n[row];i++) {
                clear_cc_unit(r->c[row][i], 0);
                block_free(r->c[row][i], sizeof(cc_unit));
        }
        r->n[row] = 0;
}

__inline static void
red_clear_last_hdrs(red_coder_t *r)
{
        while(r->last_hdr_idx > 0) {
                r->last_hdr_idx--;
                if (r->last.iov[r->last_hdr_idx].iov_base != NULL) {
                        block_free(r->last.iov[r->last_hdr_idx].iov_base, 
                                   r->last.iov[r->last_hdr_idx].iov_len);
                        r->last.iov[r->last_hdr_idx].iov_base = NULL; 
                        r->last.iov[r->last_hdr_idx].iov_len  = 0;
                }
        }

        r->last.iovc = 0;
}

void
red_flush(red_coder_t *r)
{
        int i;
        red_clear_last_hdrs(r);
        for(i=0;i<MAX_RED_OFFSET;i++) {
                red_clear_row(r, i);
        }
        r->head = r->tail = r->len = 0;
        debug_msg("flushed\n");
}

red_coder_t *
red_create(void)
{
        red_coder_t *r = (red_coder_t*)xmalloc(sizeof(red_coder_t));
        memset(r,0,sizeof(red_coder_t));
        return r;
}

void
red_destroy(red_coder_t *r)
{
        red_flush(r);
        xfree(r);
}

/* expects string with "codec1/offset1/codec2/offset2..." 
 * nb we don't save samples so if coding changes we take our chances.
 * No attempt is made to recode to new format as this takes an 
 * unspecified amount of cpu.  */

int
red_config(session_struct *sp, red_coder_t *r, char *cmd)
{
        char *s;
        codec_t *cp;
        int   i;

        UNUSED(sp);
        red_flush(r);

        r->offset[0] = 0;
        r->nlayers = 0;

        s = strtok(cmd, "/");
        do {
                cp = get_codec_byname(s,sp);
                if (!cp) {
                        fprintf(stderr, "%s:%d codec not recognized.\n", __FILE__, __LINE__);
                        assert(0);
                }
                r->coding[r->nlayers] = cp->pt;
                s = strtok(NULL,"/");
                r->offset[r->nlayers] = atoi(s);

                if (r->offset[r->nlayers]<0 || r->offset[r->nlayers]>MAX_RED_OFFSET) {
                        fprintf(stderr, "%s:%d offset of < 0 or > MAX_RED_OFFSET.\n", __FILE__, __LINE__);
                        assert(0);
                }

                for(i=0;i<r->nlayers;i++) {
                        if (r->offset[i]>=r->offset[r->nlayers]) {
                                fprintf(stderr, "%s:%d successing redundant encodings must have bigger offsets %d <= %d\n", __FILE__, __LINE__, r->offset[i], r->offset[r->nlayers]);
                                assert(0);
                        }
                }

                if (r->nlayers>0) {
                        codec_t *cp0, *cp1;
                        cp0 = get_codec(r->coding[0]);
                        cp1 = get_codec(r->coding[r->nlayers]);
                        assert(cp0 != NULL); assert(cp1 != NULL);
                        assert(codec_compatible(cp0,cp1));
                }
                r->nlayers++;
        } while((s=strtok(NULL,"/")) && r->nlayers<MAX_RED_LAYERS);

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
                        debug_msg("Buffer too small!");
                        break;
                }
        }
        /* zap trailing slash */
        if (len>0) {
                buf[len-1] = '\0';
        }
}

__inline static void
red_pack_hdr(char *h, char more, char pt, short offset, short len)
{
        assert(((~0 <<  7) & pt)     == 0);
        assert(((~0 << 14) & offset) == 0);
        assert(((~0 << 10) & len)    == 0);
        assert(get_codec(pt));
        if (more) {
                u_int32 *hdr = (u_int32*)h;
                (*hdr)       = 0;
                (*hdr)       = (0x80|pt)<<24;
                (*hdr)      |= offset<<10;
                (*hdr)      |= len;
                (*hdr)       = htonl((*hdr));
        } else {
                (*h) = pt;
        }
}

__inline static cc_unit*
red_get_unit(red_coder_t *r, int unit_off, int pt)
{
        int i,pos;

        assert (pt < 128);

        if (unit_off > r->len) return NULL;

        pos = r->head - unit_off;

        if (pos < 0) pos += MAX_RED_OFFSET;

        if (r->head > r->tail) {
                if (pos > r->head || pos < r->tail) return NULL;
        } else if (r->head < r->tail) {
                if (pos > r->head && pos < r->tail) return NULL;
        }

        for(i = 0; i<r->n[pos]; i++) {
                 if (r->c[pos][i]->pt == pt) return r->c[pos][i];
        }

        return NULL;
}

__inline static int
red_available(red_coder_t *r) {
        int i;
        cc_unit *u;

        i = 0;
        while (((u = red_get_unit(r, r->offset[i], r->coding[i])) != NULL) && 
               i < r->nlayers)
                i++;

        return i;
}


int 
red_encode(session_struct *sp, cc_unit **coded, int num_coded, cc_unit **out, red_coder_t *r) 
{
        int i, avail, new_ts;
        cc_unit *u;
        codec_t *cp;

        r->head = (r->head + 1) % MAX_RED_OFFSET;
        r->len++;
        
        if (r->head == r->tail) {
                r->tail = (r->tail + 1) % MAX_RED_OFFSET;
                r->len--;
        }

        /* check if new talkspurt */
        if (r->last_hdr_idx != 0) {
                new_ts = 0;
        } else {
                new_ts = CC_NEW_TS;
        }

        /* get rid of old data */
        red_clear_row(r, r->head);
        red_clear_last_hdrs(r);

        if (num_coded == 0) {
                /* NB above steps are necessary to keep 
                 * stored data in correct temportal order
                 * when no data incoming.
                 */
                *out = NULL;
                return CC_NOT_READY;
        }

        /* transfer new data into head */

        for(i = 0; i < num_coded; i++) {
                r->c[r->head][i] = coded[i];
                assert(r->c[r->head][i]->pt < 128);
                r->n[r->head]++;
        }

        avail = red_available(r);
        assert(avail <= r->nlayers);

        cp = get_codec(r->coding[0]);

        if (avail != r->nlayers) {
                /* Not enough redundancy available, so advertise max offset 
                 * to help receivers*/
                r->last.iov[0].iov_base = (caddr_t)block_alloc(sizeof(u_int32));
                r->last.iov[0].iov_len  = sizeof(u_int32);
                red_pack_hdr((char*)r->last.iov[0].iov_base, 
                             1, 
                             (char)r->coding[r->nlayers-1], 
                             (short)(r->offset[r->nlayers-1] * collator_get_units(sp->collator) * cp->unit_len),
                             0);
                r->last_hdr_idx = 1;
                r->last.iovc    = 1;
        }

        /* pack rest of headers */
        i = avail;
        while(i-- > 0) {
                cc_unit *tmp;
                int sz = (i != 0) ? sizeof(u_int32) : sizeof(u_char);

                tmp = red_get_unit(r, r->offset[i], r->coding[i]);
                assert(tmp != NULL);

                r->last.iov[r->last.iovc].iov_base = (caddr_t)block_alloc(sz);
                r->last.iov[r->last.iovc].iov_len  = sz;
                red_pack_hdr((char*)r->last.iov[r->last.iovc].iov_base,
                             (char)i,
                             (char)r->coding[i],
                             (short)(r->offset[r->nlayers-1] * collator_get_units(sp->collator) * cp->unit_len),
                             (short)get_bytes(tmp)
                             );
                r->last.iovc++;
                r->last_hdr_idx++;
        }
        assert(r->last_hdr_idx <= r->nlayers);

        /* pack data */
        while(avail-- > 0) {
                u = red_get_unit(r, r->offset[avail], r->coding[avail]);
                memmove(r->last.iov + r->last.iovc, u->iov, sizeof(struct iovec) * u->iovc);
                r->last.iovc += u->iovc;
        }

        *out = &r->last;
        return CC_READY | new_ts;
}

__inline static int
red_max_offset(cc_unit *ccu)
{
        int i,off=0,max_off=0;
        for(i=0;i<ccu->iovc && ccu->iov[i].iov_len==4;i++) {
                off = RED_OFF(ntohl(*((u_int32*)ccu->iov[i].iov_base)));
                max_off = max(max_off, off);
        }
        if (i > MAX_RED_OFFSET || i == ccu->iovc) return -1;
        return max_off;
}

void
red_decode(rx_queue_element_struct *u)
{
        /* Second system 1 Old system 0 */
        rx_queue_element_struct *su;
        int i, max_off, hdr_idx, data_idx, off, len;
        u_int32 red_hdr;
        codec_t *cp;
        cc_unit *cu = u->ccu[0];

        hdr_idx  = cu->hdr_idx;
        data_idx = cu->data_idx;

        if ((cu->iov[hdr_idx].iov_len != 4) ||
            (max_off = red_max_offset(cu)) == -1) {
                return;
        }
        
        do {
                red_hdr = ntohl(*((u_int32*)cu->iov[hdr_idx].iov_base));
                cp      = get_codec(RED_PT(red_hdr));
                len     = RED_LEN(red_hdr);
                if (cp != NULL && len != 0) {
                        off = RED_OFF(red_hdr);
                        su = get_rx_unit((max_off - off) / cp->unit_len, u->ccu[0]->pt, u);
                        data_idx += fragment_spread(cp, len, &cu->iov[data_idx], cu->iovc - data_idx, su);
                }
                hdr_idx++;
        } while (cu->iov[hdr_idx].iov_len != 1);

        cp = get_codec(*((char*)cu->iov[hdr_idx].iov_base)&0x7f);
        assert(cp);
        len = 0;
        i = data_idx;
        while (i < cu->iovc) 
                len += cu->iov[i++].iov_len;
        su = get_rx_unit(max_off / cp->unit_len, u->cc_pt, u);
        data_idx += fragment_spread(cp, len, &cu->iov[data_idx], cu->iovc - data_idx, su);
        assert(data_idx == cu->iovc);
}    

int
red_bps(session_struct *sp, red_coder_t *r)
{
        int i, b, ups, upp;
        codec_t *cp;
        b = 0;
        upp = collator_get_units(sp->collator);
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
red_valsplit(char *blk, unsigned int blen, cc_unit *cu, int *trailing, int *inter_pkt_gap) {
        int tlen, n, todo;
        int hdr_idx; 
        u_int32 red_hdr, max_off;
        codec_t *cp;

        assert(!cu->iovc);
        max_off  = 0;
        hdr_idx  = 0;
        cu->iovc = MAX_RED_LAYERS;
        todo     = blen;

        while(RED_F((red_hdr=ntohl(*((u_int32*)blk)))) && todo >0) {
                cu->iov[hdr_idx++].iov_len = 4;
                todo -= 4;
                blk  += 4;
                cp      = get_codec(RED_PT(red_hdr));
                max_off = max(max_off, RED_OFF(red_hdr));
                tlen    = RED_LEN(red_hdr);
                /* we do not discard packet if we cannot decode redundant data */
                if (cp && fragment_sizes(cp, tlen, cu->iov, &cu->iovc, CC_UNITS) < 0) {
                        debug_msg("frg sz");
                        goto fail;
                }
                todo -= tlen;
#ifdef DEBUG
                assert(hdr_idx <= 2);
#endif
        }
        
        if (hdr_idx >= MAX_RED_LAYERS || todo <= 0) {
                debug_msg("hdr ovr\n");
                goto fail;
        }
        cp = get_codec((*blk)&0x7f);
                /* we do discard data if cannot do primary */
        if (!cp) {
                debug_msg("primary?");
                goto fail;
        }

        cu->iov[hdr_idx++].iov_len = 1;
        todo -= 1;

        if (hdr_idx >= MAX_RED_LAYERS || todo <= 0) {
                debug_msg("hdr ovr\n");
                goto fail;
        }
        
        if ((n = fragment_sizes(cp, todo, cu->iov, &cu->iovc, CC_UNITS)) < 0) goto fail;

        /* label hdr and data starts */
        cu->hdr_idx  = 0;
        cu->data_idx = hdr_idx; 
        /* push headers and data against each other */
        cu->iovc    -= MAX_RED_LAYERS - hdr_idx;
        memmove(cu->iov+hdr_idx, 
                cu->iov+MAX_RED_LAYERS, 
                sizeof(struct iovec)*(cu->iovc - hdr_idx));

        (*trailing)      = max_off/cp->unit_len + n;
        (*inter_pkt_gap) = cp->unit_len * n; 

        return (n);

fail:
        for(hdr_idx=0; hdr_idx<cu->iovc; hdr_idx++) {
                debug_msg("%d -> %03d bytes\n", 
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
        while (RED_F(ntohl(*hdr)) && todo>0) {
                hdr++;
                todo -= 4;
        }

        return (todo ? RED_PT(ntohl(*hdr)) : -1);
}

void
red_fix_encodings(session_struct *sp, red_coder_t *r)
{
        int i,j;
        
        sp->encodings[0]  = r->coding[0];
        sp->num_encodings = 1;
        debug_msg("added %d\n", r->coding[0]);
        for(i = 1; i < r->nlayers; i++) {
                for(j = 0; j<sp->num_encodings && sp->encodings[j] != r->coding[i]; j++);
                if (j == sp->num_encodings) {
                        sp->encodings[sp->num_encodings] = r->coding[i];
                        debug_msg("added %d\n", r->coding[i]);
                        sp->num_encodings++;
                }
                assert(r->coding[i]<127);
        }
}
