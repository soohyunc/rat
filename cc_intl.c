/*
 * FILE: cc_intl.c
 * PROGRAM: RAT / interleaver
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
#include <string.h>
#include "session.h"
#include "codec.h"
#include "channel.h"
#include "receive.h"
#include "util.h"
#include "cc_intl.h"

/*- code pertaining to interleaver only
 * ------------------------------------- 
 * It is not optimum as this imposes constraints on the relationship between 
 * n1 and n2 and the benefit in timing delay is small compared to the delay
 * incurred by interleaving.
 */

typedef struct s_il {
        unsigned char n1;    /* number of rows */
        unsigned char n2;    /* number of columns */
        int sz;              /* number of elements in each matrix */
        char **m[2];         /* pointer to array of linear matices 
                              * with elements pointing to data */
        int in;              /* index of matrix for writing to */
        int idx;             /* index of next element to be read/written */
        int nlelem;          /* number of live elements */
} il_t;

static struct s_il*
create_il(int n1, int n2) {
        il_t *il;
        int i;
    
        il = (il_t*) calloc(1,sizeof(il_t));
        il->n1  = n1;
        il->n2  = n2;
        il->sz = n1*n2;
        for(i=0;i<2;i++) 
                il->m[i] = (char**)calloc(il->sz,sizeof(char*));
        il->in  = 0;
        il->idx   = 0;
        il->nlelem = 0;
        return il;
}

static char *
il_exchange(struct s_il *s, char *in)
{
        char *out;
        int out_idx;

        if (in) s->nlelem++;

        assert(*(s->m[s->in] + s->idx)==NULL);
        *(s->m[s->in] + s->idx) = in;

        out_idx = (s->idx%s->n1)*s->n2 + (s->idx/s->n1);
        assert(out_idx<s->sz);
        out = *(s->m[!s->in] + out_idx);
        *(s->m[!s->in] + out_idx) = (char*)NULL;

        s->idx++;
        /* check for matrix full */
        if (s->idx == s->sz) {
                /* rotate if so */
                s->idx = 0;
                s->in = !s->in;
        }

        if (out) s->nlelem--;

        return out;
}

static int 
il_empty(struct s_il *s)
{
        return (s->nlelem ? 0 : 1);
}

static void
free_il(struct s_il *s)
{
        int i;
        assert(il_empty(s));
        for(i=0;i<2;i++)
                free(s->m[i]);
        s->n1 = s->n2 = 0;
        free(s);
}

/*- Interface to rat --------------------------------------------------------*/

/* For time being (this is experimental) header format is: 
 *     0                                   32
 *     +-+-------+---+---+------+---------+
 *     |R|  PT   | N1| N2| MASK | PHASE   |
 *     +-+-------+---+---+------+---------+
 *      1    7     3   3    8       10
 *
 * R     = reserved (maybe for denoting multiple payloads but redundancy
 *         format might suit this better)
 * PT    = payload
 * N1,N2 = parameters of interleaver (N1 = n1-1, N2 = n2-1).
 *         n1 being the number of units per packet and n2 the separation
 *         in units of adjacent units in packet.  Since N1,N2 are each 
 *         represented by 3 bits and mask is 8 bits we make n = N - 1 
 *         as n = 0 is not sensible.  Neither is n = 1, but we only have 8
 *         bits for the mask.
 * MASK  = denotes which of the interleaved elements are contained, 
 *         necessary because not all may have relevent data
 * PHASE = needed to find bearings by decoder - this has to be bigger
 *         than longest expected consecutive loss of packets.
 *
 * The potential limitation with this scheme of things is that we only
 * support the interleaving of single units (typically 20ms slices).
 * However, this is not really a problem as this is the right order of
 * magnitude for the losses we can conceal from the ear.  It is not clear
 * interleaving of longer blocks serves any useful purpose.
 */

#define PUT_PT(x) ((x)<<24)
#define PUT_N1(x) ((x-1)<<21)
#define PUT_N2(x) ((x-1)<<18)
#define PUT_MASK(x) ((x)<<10)
#define PUT_PHASE(x) (x)

#define GET_PT(x) (((x)>>24)&0x7f)
#define GET_N1(x) ((((x)>>21)&0x07)+1)
#define GET_N2(x) ((((x)>>18)&0x07)+1)
#define GET_MASK(x) (((x)>>10)&0xff)
#define GET_PHASE(x) ((x)&0x3ff)

typedef struct s_intl_coder {
        u_int32 src_pt;
        int cnt; 
        u_int16 mask;
        u_int32 intl_hdr;
        il_t *il;  
        cc_unit buf;     /* out going cc_unit    */
        u_int32 last_ts; /* used only by decoder */
} intl_coder_t;

intl_coder_t *
new_intl_coder(session_struct *sp)
{
        intl_coder_t *t;
        t = (intl_coder_t*)xmalloc(sizeof(intl_coder_t));
        memset(t,0,sizeof(intl_coder_t));
        t->il = create_il(4,4);
        if (sp) { /* true if transmitter - this needs tidying up*/
                t->src_pt = sp->encodings[0];
        }
        return t;
}

void
free_intl_coder(intl_coder_t *t)
{
        if (t->il) {
                free_il(t->il);
        }
        xfree(t);
}

void
intl_reset(intl_coder_t *s)
{
        coded_unit *u;
        while(!il_empty(s->il)) {
                u = (coded_unit*)il_exchange(s->il,NULL);
                if (u) {
                        clear_coded_unit(u);
                        block_free(u,sizeof(coded_unit));
                }
        }
        clear_cc_unit(&s->buf,0);
        s->il->idx = s->cnt = s->mask = s->last_ts = s->buf.iovc = 0;
}

/* expects string of form "codec/n1/n2" */
int 
intl_config(struct session_tag *sp,
            intl_coder_t       *s,
            char               *cmd)
{
        int n1, n2;
        char *name;
        codec_t *cp;

        name = strtok(cmd,"/");
        n1   = atoi(strtok(NULL,"/"));
        n2   = atoi(strtok(NULL,"/"));

        cp = get_codec_byname(name,sp);

        if ((n1 < 0 || n1 > 15) || (n2 < 0 || n2 > 15) || !cp) 
                return 0;

        intl_reset(s);
        free_il(s->il);
        s->il = create_il(n1,n2);
        s->src_pt = sp->encodings[0] = cp->pt;
        set_units_per_packet(sp,n1);
        return 1;
}

void 
intl_qconfig(session_struct    *sp, 
             intl_coder_t      *s,
             char              *buf, 
             unsigned int       blen) 
{
        codec_t *cp;

        UNUSED(sp);

        cp = get_codec(s->src_pt);
        sprintf(buf,"%s/%d/%d",cp->name,s->il->n1,s->il->n2);
        assert(strlen(buf)<blen);
}

int
intl_bps(session_struct        *sp,
         intl_coder_t          *s)
{
        int ups, upp, us;
        codec_t *cp;

        UNUSED(sp);
    
        upp = s->il->n1;
        cp  = get_codec(s->src_pt);
        ups = cp->freq / cp->unit_len;
        us  = cp->sent_state_sz + cp->max_unit_sz;
        return (8 * (upp*us + 4 + 12) * ups/upp);
}

/* This (c|sh)ould be much more efficient 
 * ... this is just a first pass [oth]
 */

int
intl_encode(session_struct *sp,
            sample         *raw,
            cc_unit        *cu,
            intl_coder_t   *s)
{
        codec_t *cp;
        coded_unit *u;

        if (s->src_pt != (u_int32)sp->encodings[0]) {
                /* we don't have any of this data. for the time being we
                 * flush interleaver, but we could wait for start of new
                 * talkspurt before effecting change to new codec. 
                 */
                intl_reset(s);
                s->src_pt = sp->encodings[0];
        }
    
        cp = get_codec(s->src_pt);
        if (!cp) {
#ifdef DEBUG
                fprintf(stderr,"%s:%d codec not recognized.\n",__FILE__,__LINE__);
#endif
                return 0;
        }

        if (!(s->cnt % s->il->n1)) {
                clear_cc_unit(&s->buf,0); 
                s->buf.iovc = 0;          
        }

        u = NULL;
        if (raw) {
                u = (coded_unit*)block_alloc(sizeof(coded_unit));
                encoder(sp,raw,s->src_pt,u);
        }

        u = (coded_unit*)il_exchange(s->il,(char*)u);
        s->mask <<= 1;
        if (u) {
                s->mask     |=  1;
                s->buf.iovc += coded_unit_to_iov(u, 
                                                 s->buf.iov+s->buf.iovc+1, 
                                                 INCLUDE_STATE);
                /* nb iov[0] is used for header hence +1 */
                block_free(u,sizeof(coded_unit));
        }

        s->cnt++;
        if (!(s->cnt % s->il->n1) && s->mask) {
                u_int32 *hdr;
                s->buf.iov[0].iov_base = (caddr_t)block_alloc(sizeof(u_int32));
                s->buf.iov[0].iov_len  = sizeof(u_int32);
                s->buf.iovc+=1;
                hdr     = (u_int32*)s->buf.iov[0].iov_base;
                (*hdr)  = PUT_PT(s->src_pt);
                (*hdr) |= PUT_N1(s->il->n1);
                (*hdr) |= PUT_N2(s->il->n2);
                (*hdr) |= PUT_MASK(s->mask);
                (*hdr) |= PUT_PHASE(((s->cnt-s->il->sz)/s->il->n1-1)%(8*s->il->n2));
                (*hdr)  = htonl((*hdr));
                memcpy(cu->iov+1,s->buf.iov,s->buf.iovc*sizeof(struct iovec));
                cu->iovc = 1 + s->buf.iovc;
                s->mask = 0;
                return TRUE;
        }
        return FALSE;
}

static void
intl_check_hdr(u_int32        hdr, 
               intl_coder_t  *s)
{
        if ((GET_PT(hdr) != s->src_pt) ||
            (GET_N1(hdr) != s->il->n2) ||
            (GET_N2(hdr) != s->il->n1)) {
                intl_reset(s);
                free_il(s->il);
                /* unscrambler of an (n1,n2) interleaver is an (n2,n1) interleaver */
                s->il     = create_il(GET_N2(hdr),GET_N1(hdr));
                s->src_pt = GET_PT(hdr);
        }
}

void
intl_decode(rx_queue_element_struct *u,
            intl_coder_t            *s)
{
        rx_queue_element_struct *su;
        u_int32     hdr;
        codec_t    *cp;
        coded_unit *cu;
        int i,j,iovc,m,p,ep;
        u_int32 units;
        struct iovec iov[2];

        /* we got a packet */
        units = s->il->n2;
        if (u->ccu_cnt) {
                hdr = ntohl(*(u_int32*)u->ccu[0]->iov[0].iov_base);
                intl_check_hdr(hdr,s);
                m = GET_MASK(hdr) << (32-units);
                p = GET_PHASE(hdr);
                /* free interleaver header */
                block_free(u->ccu[0]->iov[0].iov_base,
                           u->ccu[0]->iov[0].iov_len);
                memset(u->ccu[0]->iov,0,sizeof(struct iovec));
        } else {
                m   =  0;
                p   = -1;
        }

        cp = get_codec(s->src_pt);
    
        if (!cp) {
#ifdef DEBUG
                fprintf(stderr,"%s:%d Codec not recognized.\n",__FILE__,__LINE__);
#endif
                return;
        }

        if (!u->ccu_cnt) {
                if (il_empty(s->il)&&s->cnt) {
                        intl_reset(s); /* we should delete this rx unit and all trailing
                                        * ones here to stop burning CPU on repair cause
                                        * this is almost definitely the end of the talkspurt
                                        * [oth]
                                        */
                        return;
                } else if ((u->playoutpt - s->last_ts)/cp->unit_len != units) {
                        return;
                }
        }

        s->last_ts = u->playoutpt;

        ep = s->cnt/units;
        while (p!=-1 && (ep % units) != (p % units)) {
                /* we are out of phase so realign */
                cu = (coded_unit*)il_exchange(s->il,NULL);
                if (cu) {
                        clear_coded_unit(cu);
                        block_free(cu,sizeof(coded_unit));
                }
                ep = ++s->cnt/units;
        }

        su = u;
        for(i=0, j=1; i < (int)units; i++) {
                cu = NULL;
                if (m & 0x80000000) {  
                        cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                        j += iov_to_coded_unit(u->ccu[0]->iov+j,
                                               cu,
                                               cp->pt);
                }
                cu = (coded_unit*)il_exchange(s->il,(char*)cu);
                if (cu) {
                        if (su) {
                                iovc = coded_unit_to_iov(cu,iov,INCLUDE_STATE);
                                add_comp_data(su,
                                              s->src_pt,
                                              iov,
                                              iovc);
                                su = get_rx_unit(1,u->cc_pt,su);
                        } else { 
                                /* we had nowhere to write this block -
                                 * probably because the playout offset got
                                 * changed mid talkspurt
                                 */
                                clear_coded_unit(cu);
                        }
                        block_free(cu,sizeof(coded_unit));
                } 
                m<<=1;
        }
    
        for(i=0;i<CC_UNITS&&u->ccu_cnt;i++) {
                assert(u->ccu[0]->iov[i].iov_len==0);
                assert(u->ccu[0]->iov[i].iov_base==0);
        }

        s->cnt += units;
}

/* 
 * intl_valsplit:
 * returns number of units per packet,
 * fills in how memory should be allocated into cu,
 * checks format, and sets trailing to number of
 * units that should follow to flush interleaver
 */

int  
intl_valsplit(char                *blk,
              unsigned int         blen,
              cc_unit             *cu,
              int                 *trailing)
{
        u_int32 hdr;
        int i, m, units, todo;
        codec_t *cp;

        todo = blen;
        hdr  = ntohl(*((u_int32*)blk));

        assert(cu->iovc == 0);
        cu->iov[0].iov_base = (caddr_t)blk;
        cu->iov[0].iov_len  = 4;
        todo               -= 4;

        cp = get_codec(GET_PT(hdr));
        if (!cp) {
#ifdef DEBUG
                fprintf(stderr, "%s:%d Codec not recognized.\n",__FILE__,__LINE__);
#endif
                return 0;
        }

        cu->iovc = 1;
        units = GET_N1(hdr);
        m = GET_MASK(hdr) << (32-units);
        for(i=units;i>0;i--) {
                if (m&0x80000000) {
                        if (cp->sent_state_sz) { 
                                cu->iov[cu->iovc++].iov_len = cp->sent_state_sz;
                                todo                       -= cp->sent_state_sz;
                        }
                        cu->iov[cu->iovc++].iov_len = cp->max_unit_sz;
                        todo                       -= cp->max_unit_sz;
                }
                m<<=1;
        }

        if (todo != 0) {
#ifdef DEBUG
                fprintf(stderr,"%s:%d Incorrect block length.\n", __FILE__, __LINE__);
#endif
                return 0;
        }

        (*trailing) = 2 * units * GET_N2(hdr);
        return units;
}

int
intl_wrapped_pt(char          *blk,
                unsigned int   blen)
{
        UNUSED(blen);

        return (GET_PT(ntohl(*((u_int32*)blk))));
}




