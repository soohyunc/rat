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

#include <stdlib.h>
#include <string.h>
#include "assert.h"
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
 * incurred by interleaving techniques
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
il_create(int n1, int n2) {
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
il_free(struct s_il *s)
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
 *
 * 31                                     0
 *  +-------+--------+---+---+---+--------+
 *  |  PT   |   UPL  |N1'|N2'|PHS|  MASK  |
 *  +-------+--------+---+---+---+--------+
 *      7         8    3   3   3      8
 *
 *  where PT   = payload of interleaved media
 *        UPL  = number of audio units per leaf 
 *        N1'  = dimension of interleaver (N1' = N1 - 1)
 *        N2'  = dimension of interleaver (N2' = N2 - 1)
 *        PHS  = phase of interleaver (column being output) (1..n2)
 *        MASK = bitmap of which units are present.
 *  A version field would be nice here.
 */

#define PUT_PT(x)    ((x)   << 25)
#define PUT_UPL(x)   ((x)   << 17)
#define PUT_N1(x)    ((x-1) << 14)
#define PUT_N2(x)    ((x-1) << 11)
#define PUT_PHASE(x) ((x)   <<  8)
#define PUT_MASK(x)  ((x))

#define GET_PT(x)    (((x)>>25)  & 0x7f)
#define GET_UPL(x)   (((x)>>17)  & 0xff)
#define GET_N1(x)    ((((x)>>14) & 0x07)+1)
#define GET_N2(x)    ((((x)>>11) & 0x07)+1)
#define GET_PHASE(x) (((x)>>8)   & 0x07)  
#define GET_MASK(x)  ((x)        & 0xff)

typedef struct s_intl_coder {
        u_char  cnt;
        u_char  src_pt;
        u_int16 mask;
        u_int32 intl_hdr;
        il_t *il;  
        cc_unit last;     /* out going cc_unit    */
        u_int32 last_ts;  /* used only by decoder */
        u_int32 upl;      /* ditto */
} intl_coder_t;

intl_coder_t *
new_intl_coder(session_struct *sp)
{
        intl_coder_t *t;
        UNUSED(sp);

        t = (intl_coder_t*)xmalloc(sizeof(intl_coder_t));
        memset(t,0,sizeof(intl_coder_t));
        t->il = il_create(4,4);
        return t;
}

void
free_intl_coder(intl_coder_t *t)
{
        intl_reset(t);
        if (t->il) {
                il_free(t->il);
        }
        xfree(t);
}

void
intl_reset(intl_coder_t *s)
{
        cc_unit *u;

        while(!il_empty(s->il)) {
                u = (cc_unit*)il_exchange(s->il,NULL);
                if (u) {
                        clear_cc_unit(u,0);
                        block_free(u,sizeof(cc_unit));
                }
        }
        clear_cc_unit(&s->last,0);
        s->il->idx   = 0; 
		s->cnt       = 0;
		s->mask      = 0;
		s->last_ts   = 0;
		s->last.iovc = 0;
		
        debug_msg("intl_reset\n");
}

/* expects string of form "codec/n1/n2" */
int 
intl_config(struct session_tag *sp,
            intl_coder_t       *s,
            char               *cmd)
{
        int n1, n2;

        UNUSED(sp);

        n1   = atoi(strtok(cmd,  "/"));
        n2   = atoi(strtok(NULL, "/"));

        if ((n1 < 0 || n1 > 8) || (n2 < 0 || n2 > 8)) 
                return 0;

        intl_reset(s);
        il_free(s->il);
        s->il = il_create(n1,n2);

        return 1;
}

void 
intl_qconfig(session_struct    *sp, 
             intl_coder_t      *s,
             char              *buf, 
             unsigned int       blen) 
{
        UNUSED(sp);

        sprintf(buf,"%d/%d", s->il->n1, s->il->n2);
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

__inline static void
intl_pack_hdr(intl_coder_t *s, u_int32* hdr, int units_per_leaf)
{
        int st_phase;
        (*hdr)  = PUT_PT    (s->src_pt);
        (*hdr) |= PUT_UPL   (units_per_leaf);
        (*hdr) |= PUT_N1    (s->il->n1);
        (*hdr) |= PUT_N2    (s->il->n2);
        /* want to pack phase at time we first started packing */
        st_phase = (((s->il->idx / s->il->n1) + s->il->n2 - 1) % s->il->n2);
        (*hdr) |= PUT_PHASE (st_phase);
        (*hdr) |= PUT_MASK  (s->mask & 0xff);
        (*hdr)  = htonl     ((*hdr));
}

/* This (c|sh)ould be much more efficient 
 * ... this is just a first pass [oth]
 */

int
intl_encode(session_struct *sp,
            cc_unit        *coded,
            cc_unit       **out,
            intl_coder_t   *s)
{
        cc_unit *u;
        int new_ts;

        UNUSED(sp);

        if (s->cnt == s->il->n1) {
                clear_cc_unit(&s->last,0);
                s->cnt = 0;
        }

        if (s->cnt == 0 && coded) {
                s->src_pt = coded->pt;
        } else if (coded && coded->pt != s->src_pt) {
                intl_reset(s);
        }

        if (s->cnt == 0) {
                s->last.iovc = 1; /* reserve space for hdr */
        }

        if (il_empty(s->il)) {
                new_ts = CC_NEW_TS;
        } else {
                new_ts = 0;
        }

        u = (cc_unit*)il_exchange(s->il,(char*)coded);
        s->mask <<= 1;

        if (u) {
                s->mask     |=  1;
                memcpy(s->last.iov + s->last.iovc, u->iov, u->iovc * sizeof(struct iovec));
                s->last.iovc += u->iovc;
                block_free(u,sizeof(cc_unit));
        }

        s->cnt++;
        if ((s->cnt == s->il->n1) && s->mask) {
                s->last.iov[0].iov_base = (caddr_t)block_alloc(sizeof(u_int32));
                s->last.iov[0].iov_len  = sizeof(u_int32);
                intl_pack_hdr(s, 
                              (u_int32*)s->last.iov[0].iov_base, 
                              collator_get_units(sp->collator));
                (*out)  = &s->last;
                s->mask = 0;
                return CC_READY | new_ts;
        }

        (*out) = NULL;
        return CC_NOT_READY;
}

static void
intl_compat_chk(u_int32        hdr, 
                intl_coder_t  *s)
{
        if (GET_N1(hdr) != s->il->n2 ||
            GET_N2(hdr) != s->il->n1 ||
            GET_PT(hdr) != s->src_pt || 
            GET_UPL(hdr) != s->upl) {
                debug_msg("Header incompatible - adjusting parameters.\n");
                intl_reset(s);
                il_free(s->il);
                /* unscrambler of an (n1,n2) interleaver is an (n2,n1) interleaver */
                assert(GET_N1(hdr));
                assert(GET_N2(hdr));
                s->il     = il_create(GET_N2(hdr),GET_N1(hdr));
                s->src_pt = (u_char)GET_PT(hdr);
                s->upl    = GET_UPL(hdr);
        }
}

void
intl_decode(rx_queue_element_struct *u,
            intl_coder_t            *s)
{
        rx_queue_element_struct *su;
        u_int32     hdr;
        codec_t    *cp;
        cc_unit    *ccu;
        int32 i, j, len, mask, idx, iovc;

        hdr = 0; /* gcc needs this when optimizing ? */

        /* REMEMBER s->il->n1 is what is s->il->n2 in encoder 
         * and vice versa.
         */

        if (u->ccu_cnt) {
                block_trash_chk();
                hdr = ntohl(*(u_int32*)u->ccu[0]->iov[0].iov_base);
                intl_compat_chk(hdr,s);
                /* free interleaver header */
                block_free(u->ccu[0]->iov[0].iov_base,
                           u->ccu[0]->iov[0].iov_len);
                memset(u->ccu[0]->iov,0,sizeof(struct iovec));
        }
        
        if ((cp = get_codec(s->src_pt)) == FALSE) return;

        if (u->ccu_cnt) {
                s->last_ts = u->src_ts;
                /* check phase */
                if (s->il->idx / s->il->n2 != (int32)GET_PHASE(hdr)) {
                        debug_msg("Out of phase %d %d\n",s->il->idx / s->il->n2,(int32)GET_PHASE(hdr));
                        while(s->il->idx / s->il->n2 != (int32)GET_PHASE(hdr)) {
                                ccu = (cc_unit*)il_exchange(s->il, (char*)NULL);
                                if (ccu) {
                                        debug_msg("Freeing good data\n");
                                        clear_cc_unit(ccu,0);
                                        block_free(ccu, sizeof(cc_unit));
                                }
                        }
                }

                mask = GET_MASK(hdr) << (32 - GET_N1(hdr));
                for(i = 0, idx = 1; i < s->il->n2; i++, mask<<=1) {
                        ccu = NULL;
                        iovc    = (cp->sent_state_sz ? 1 : 0) + s->upl;
                        if (mask & 0x80000000) {
                                ccu     = (cc_unit*)block_alloc(sizeof(cc_unit));
                                ccu->pt = s->src_pt;
                                memcpy(ccu->iov, u->ccu[0]->iov+idx, iovc * sizeof (struct iovec)); 
                                memset(u->ccu[0]->iov+idx, 0, iovc * sizeof(struct iovec));
                                ccu->iovc = iovc;
                                idx      += iovc;
                        }
                        /* i want a trade-in... */
                        ccu = (cc_unit*) il_exchange(s->il, (char*)ccu);
                        if (ccu) {
                                codec_t *cp;
                                debug_msg("splitting %d blocks\n", ccu->iovc);
                                for(j = 0, len = 0; j < ccu->iovc; j++) len += ccu->iov[j].iov_len;
                                su = get_rx_unit(i * s->upl, u->cc_pt, u);
                                cp = get_codec(s->src_pt);
                                fragment_spread(cp, len, ccu->iov, ccu->iovc, su);
#ifdef DEBUG
                                for(j = 0; j < ccu->iovc;j++) assert(ccu->iov[j].iov_base == NULL && ccu->iov[j].iov_len == 0);
#endif
                                block_free(ccu, sizeof(cc_unit));
                        } else {
                                debug_msg("nothing out\n");
                        }
                }
                assert(mask == 0);
                assert(i == s->il->n2);
                block_free(u->ccu[0],sizeof(cc_unit));
                u->ccu[0] = u->ccu[1];
                u->ccu_cnt--;
                assert(u->ccu_cnt >= 0);
        } else {
                if (il_empty(s->il) && s->cnt) {
                        intl_reset(s); 
                        return;
                } else if ((u->src_ts - s->last_ts)/cp->unit_len != (s->il->n2 * s->upl)) {
                        debug_msg("Nothing doing %d %d \n",(u->src_ts - s->last_ts)/cp->unit_len,s->il->n2 * s->upl);
                        return;
                }
                /* ... You can't always get what you want, x2
                 * but if you try sometimes you just might find
                 * you get what you neeed ...
                 *
                 * (c) Jagger/Richards 196x
                 */ 
                debug_msg("adding dummies %d\n", s->il->n2);
                for(i=0;i<s->il->n2;i++) {
                        ccu = (cc_unit*) il_exchange(s->il, (char*)NULL);
                        if (ccu) {
                                codec_t *cp;
                                for(j = 0, len = 0; j < ccu->iovc; j++) len += ccu->iov[j].iov_len;
                                su = get_rx_unit(i * s->upl, u->cc_pt, u);
                                cp = get_codec(s->src_pt);
                                fragment_spread(cp, len, ccu->iov, ccu->iovc, su);
#ifdef DEBUG
                                for(j = 0; j < ccu->iovc;j++) assert(ccu->iov[j].iov_base == NULL && ccu->iov[j].iov_len == 0);
#endif
                                block_free(ccu, sizeof(cc_unit));
                        }
                }
        }
}

/* 
 * intl_valsplit:
 * returns number of units per packet,
 * fills in how memory should be allocated into cu,
 * checks format, and sets trailing to number of
 * units that should follow to flush interleaver
 */

int  
intl_valsplit(char         *blk,
              unsigned int  blen,
              cc_unit      *cu,
              int          *trailing,
              int          *inter_pkt_gap)
{
        u_int32 hdr, len;
        int i, upl, mask;
        codec_t *cp;

        hdr  = ntohl(*((u_int32*)blk));

        assert(cu->iovc == 0);
        cu->iov[0].iov_base = (caddr_t)blk;
        cu->iov[0].iov_len  = 4;
        cu->iovc            = 1;

        cp = get_codec(GET_PT(hdr));
        if (!cp) {
                debug_msg("Codec (pt = %d) not recognized.\n", GET_PT(hdr));
                (*trailing) = 0;
                return 0;
        }

        upl = GET_UPL(hdr);
        len = cp->max_unit_sz * upl + cp->sent_state_sz;

        mask = GET_MASK(hdr) << (32 - GET_N1(hdr));
        while(mask) {
                if (mask & 0x80000000) fragment_sizes(cp, len, cu->iov, &cu->iovc, CC_UNITS);
                mask <<= 1;
        }
        
        for(i = 0, len = 0; i < cu->iovc; i++) len += cu->iov[i].iov_len;

        if (len != blen) {
                debug_msg("sizes don't tally %d %d\n", len, blen);
                (*trailing) = 0;
                return 0;
        }
        
        (*trailing)      = (GET_N2(hdr) - GET_PHASE(hdr)) * GET_N1(hdr) * upl; 
        (*inter_pkt_gap) = upl * GET_N1(hdr) * cp->unit_len;

        return GET_N1(hdr) * upl;
}

int
intl_wrapped_pt(char          *blk,
                unsigned int   blen)
{
        codec_t *cp;
        int pt;
        UNUSED(blen);
        pt = GET_PT(ntohl(*((u_int32*)blk)));
        cp = get_codec(pt);
        assert(cp);
        return pt;
}




