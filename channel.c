/*
 * FILE:    channel.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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
#include <sys/types.h>
#endif /* WIN32 */

#include <stdlib.h>
#include <ctype.h>
#include "config.h"
#include "session.h"
#include "receive.h"
#include "codec.h"
#include "channel.h"
#include "util.h"
#include "cc_red.h"
#include "cc_intl.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"

typedef struct s_cc_state {
        struct s_cc_state *next;
        int    pt;
        char  *s;
} cc_state_t;

typedef struct {
        cc_unit *last;
} vanilla_state;

static void 
vanilla_init (session_struct *sp, cc_state_t *cs)
{
        UNUSED(sp);
        cs->s = (char*)xmalloc(sizeof(vanilla_state));
        memset(cs->s,0,sizeof(vanilla_state));
}

static void
vanilla_free (cc_state_t *cs)
{
        vanilla_state *v = (vanilla_state*)cs->s;

        if (v->last != NULL) {
                clear_cc_unit(v->last, 0);
                block_free(v->last, sizeof(cc_unit));
        }

        xfree(cs->s);
}

static int
vanilla_encode (session_struct *sp, 
                cc_unit       **in,
                int             no_in,
                cc_unit       **out, 
                cc_state_t     *ccs)
{
        vanilla_state *v;
        int new_ts;

        UNUSED(sp);

        assert(2 > no_in);

        v = (vanilla_state*)ccs->s;
        if (v->last) {
                clear_cc_unit(v->last,0);
                block_free(v->last, sizeof(cc_unit));
                new_ts = 0;
        } else {
                new_ts = CC_NEW_TS;
        }

        *out = v->last = *in;

        if (*out) {
                return CC_READY | new_ts;
        } else {
                return CC_NOT_READY;
        }
}

static int 
vanilla_bitrate(session_struct *sp, cc_state_t *cs)
{
        int pps, upp;
        codec_t *cp;

        UNUSED(cs);

        cp = get_codec(sp->encodings[0]);
        pps = cp->freq / cp->unit_len;
        upp = collator_get_units(sp->collator);
        return (8*pps*(cp->max_unit_sz * upp + cp->sent_state_sz + 12)/upp);
}

static int
vanilla_valsplit(char *blk, unsigned int blen, cc_unit *u, int *trailing, int *inter_pkt_gap) 
{
        codec_t *cp = get_codec(u->pt);
        int n = 0;

        UNUSED(blk);

        assert(u->iovc == 0);
        if (cp) {
                n = fragment_sizes(cp, blen, u->iov, &u->iovc, CC_UNITS);
        }

        if (n < 0) n = 0;

        (*trailing) = n;
        (*inter_pkt_gap) = n * cp->unit_len;
        return (n);
}

#define VANILLA_NO_PT 0xff;
typedef struct {
        char pt;
        session_struct *sp;
} v_dec_st;
 
static void 
vanilla_dec_init(session_struct *sp, cc_state_t *cs)
{
        v_dec_st *v;

        UNUSED (sp);

        cs->s = (char*)xmalloc(sizeof(v_dec_st));
        v     = (v_dec_st*) cs->s;
        v->pt = VANILLA_NO_PT;
}

static void 
vanilla_dec_free(cc_state_t *cs)
{
        xfree(cs->s);
}

static void 
vanilla_decode(session_struct *sp, rx_queue_element_struct *u, cc_state_t *ccs)
{
        int i,len,iovc;
        struct iovec *iov;
        codec_t *cp;
        v_dec_st *v;
        
        if (!u->ccu_cnt) return;

        cp = get_codec(u->ccu[0]->pt);
        assert(cp);
        iov  = u->ccu[0]->iov;
        iovc = u->ccu[0]->iovc;

        len = 0;
        for(i = 0; i < iovc; i++)
                len += iov[i].iov_len;

        fragment_spread(cp, len, iov, iovc, u);
        v = (v_dec_st*)ccs->s;
        if (cp->pt != v->pt) {
                v->pt = cp->pt;
                rtcp_set_encoder_format(sp, u->dbe_source[0], cp->name);
        }
}

static void 
red_enc_init(session_struct *sp, cc_state_t *cs) 
{
        UNUSED(sp);

        cs->s = (char*)red_enc_create();
} 

static void
red_enc_free(cc_state_t *cs)
{
        red_enc_destroy((struct s_red_coder*) cs->s);
}

static void 
red_dec_init(session_struct *sp, cc_state_t *cs)
{
        UNUSED(sp);
        cs->s = (char*)red_dec_create();
}

static void
red_dec_free(cc_state_t *cs)
{
        red_dec_destroy((struct s_red_dec_state*)cs->s);
}

static void
red_reset(cc_state_t *cs)
{
       red_flush((struct s_red_coder*) cs->s);
}

static int
red_configure(session_struct *sp, cc_state_t *cs, char *cmd)
{
        return red_config(sp,(struct s_red_coder*)cs->s,cmd);
}

static void 
red_query(session_struct    *sp, 
          struct s_cc_state *cs,
          char *buf, 
          unsigned int blen) 
{
        red_qconfig(sp,(struct s_red_coder*)cs->s,buf,blen);
}

static int
red_encoder(session_struct *sp, 
            cc_unit       **in,
            int             no_in,
            cc_unit       **out, 
            cc_state_t     *ccs)
{
        return red_encode(sp, in, no_in, out, (struct s_red_coder*)ccs->s);
}

static int
red_bitrate(session_struct *sp,
            cc_state_t     *cs)
{
        return red_bps(sp, (struct s_red_coder*)cs->s);
}

static void
red_decoder(session_struct *sp,
            rx_queue_element_struct *u,
            cc_state_t              *cs)
{
        if (!u->ccu_cnt) return;
        red_decode(sp, u, (struct s_red_dec_state*)cs->s);
}

static void 
intl_init(session_struct *sp, cc_state_t *cs) 
{
        cs->s = (char*)new_intl_coder(sp);
} 

static void
intl_free(cc_state_t *cs)
{
        free_intl_coder((struct s_intl_coder*) cs->s);
}

static int
intl_configure(session_struct *sp, cc_state_t *cs, char *cmd)
{
        return intl_config(sp,(struct s_intl_coder*)cs->s,cmd);
}

static void 
intl_query(session_struct    *sp, 
           struct s_cc_state *cs,
           char *buf, 
           unsigned int blen) 
{
        intl_qconfig(sp,(struct s_intl_coder*)cs->s,buf,blen);
}

static int
intl_encoder(session_struct *sp, 
             cc_unit       **in,
             int             num_coded,
             cc_unit       **out, 
             cc_state_t     *ccs)
{
        assert(2 > num_coded);
        if (num_coded) {      
                return intl_encode(sp, *in,  out, (struct s_intl_coder*)ccs->s);
        } else {
                return intl_encode(sp, NULL, out, (struct s_intl_coder*)ccs->s);
        }
}

static void
intl_encoder_reset(cc_state_t *ccs)
{
        intl_reset((struct s_intl_coder*)ccs->s);
}

static int
intl_bitrate(session_struct *sp,
             cc_state_t     *cs)
{
        return intl_bps(sp, (struct s_intl_coder*)cs->s);
}

static void
intl_decoder(session_struct *sp,
             rx_queue_element_struct *u,
             cc_state_t              *cs)
{
        intl_decode(sp, u, (struct s_intl_coder*)cs->s);
}

#define N_CC_CODERS 3
#define CC_ID_VANILLA     0
#define CC_ID_REDUNDANCY  1
#define CC_ID_INTERLEAVER 2

static cc_coder_t cc_list[] = {
        {"VANILLA", 
         PT_VANILLA, 
         CC_ID_VANILLA,
         vanilla_init, 
         NULL, 
         NULL,
         vanilla_bitrate,
         vanilla_encode,
         NULL,
         vanilla_free,
         1,
         vanilla_valsplit,
         NULL,
         vanilla_dec_init,
         vanilla_decode,
         vanilla_dec_free },
        {"REDUNDANCY", 
         PT_REDUNDANCY,
         CC_ID_REDUNDANCY,
         red_enc_init,
         red_configure,
         red_query,
         red_bitrate,
         red_encoder,
         red_reset,
         red_enc_free,
         1,
         red_valsplit,
         red_wrapped_pt,
         red_dec_init,
         red_decoder,
         red_dec_free },
        {"INTERLEAVER",
         PT_INTERLEAVED,
         CC_ID_INTERLEAVER,
         intl_init,
         intl_configure,
         intl_query,
         intl_bitrate,
         intl_encoder,
         intl_encoder_reset,
         intl_free,
         1,
         intl_valsplit,
         intl_wrapped_pt,
         intl_init,
         intl_decoder, 
         intl_free}
}; 


cc_coder_t *
get_channel_coder(int pt)
{
        codec_t *c;
        int i=0;

        while(i<N_CC_CODERS) {
                if (cc_list[i].pt == pt)
                        return &cc_list[i];
                i++;
        }
        if ((c=get_codec(pt))) /* hack for vanilla */
                return &cc_list[0];
    
        return NULL;
}

enum cc_e {
        ENCODE,
        DECODE
};

static cc_state_t *
get_cc_state(session_struct *sp, cc_state_t **lp, int pt, enum cc_e ed)
{
        cc_state_t *stp;
        cc_coder_t *cp;

        xmemchk();
        for (stp = *lp; stp; stp = stp->next)
                if (stp->pt == pt)
                        break;
    
        if (stp == 0) {
                debug_msg("creating list %d\n", pt);
                stp = (cc_state_t *)xmalloc(sizeof(cc_state_t));
                memset(stp, 0, sizeof(cc_state_t));
                cp = get_channel_coder(pt);
                stp->pt = pt;
        
                switch(ed) {
                case ENCODE:
                        if (cp->enc_init)
                                cp->enc_init(sp,stp);
                        break;
                case DECODE:
                        if (cp->dec_init)
                                cp->dec_init(sp,stp);
                        break;
                default:
                        fprintf(stderr, "get_cc_state: unknown op\n");
                        exit(1);
                }
        
                stp->next = *lp;
                *lp = stp;
        }
        return (stp);
}

static void
clear_cc_state_list(cc_state_t **list, enum cc_e ed)
{
        cc_state_t *stp;
        cc_coder_t *cp;

        while(*list) {
                stp = *list;
                *list = (*list)->next;
                cp = get_channel_coder(stp->pt);

                switch(ed) {
                case ENCODE:
                        if (cp->enc_free) cp->enc_free(stp);
                        break;
                case DECODE:
                        if (cp->dec_free) cp->dec_free(stp);
                        break;
                }
                xfree(stp);
                stp = NULL;
        }
        *list = NULL;
}

void
clear_cc_unit(cc_unit *cu, int start)
{
        int i; 
        for(i=start;i<cu->iovc;i++) {
                if (cu->iov[i].iov_len || cu->iov[i].iov_base) {
                        block_free(cu->iov[i].iov_base, cu->iov[i].iov_len);
                        cu->iov[i].iov_base = NULL;
                        cu->iov[i].iov_len  = 0;
                }
        }
}

void
clear_cc_encoder_states(cc_state_t **list)
{
        clear_cc_state_list(list, ENCODE);
}

void
clear_cc_decoder_states(cc_state_t **list)
{
        clear_cc_state_list(list, DECODE);
}

void
channel_encoder_reset(session_struct *sp,
                      int cc_pt)
{
        cc_state_t *stp;
        cc_coder_t *cc;

        cc  = get_channel_coder(cc_pt);
        stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
        if (cc->enc_reset)
                cc->enc_reset(stp);
}

int
channel_encode(session_struct *sp,
               int             pt,
               cc_unit       **coded,
               int             num_coded,
               cc_unit       **out)
{
        int r;
        cc_state_t *stp;
        cc_coder_t *cc;

        for(r=0;r<num_coded;r++) assert(coded[r]->pt < 128);

        cc  = get_channel_coder(pt);
        stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
        r   = cc->encode(sp, coded, num_coded, out, stp);
        
        if (*out) {
                (*out)->cc = cc;
                (*out)->pt = pt;
        }

        return r;
}

void 
channel_decode(session_struct *sp, rx_queue_element_struct *u)
{
        cc_state_t *stp;
        cc_coder_t *cc;
        cc  = get_channel_coder(u->cc_pt);
        stp = get_cc_state(NULL, 
                           &u->dbe_source[0]->cc_state_list, 
                           u->cc_pt,
                           DECODE);
        assert(u);
        cc->decode(sp, u, stp);
}

int
validate_and_split(int pt, char *blk, unsigned int blen, cc_unit *u, int *trailing, int *inter_pkt_gap)
{
        /* The valsplit function serves 5 purposes (probably 4 more than it should):
         * 1) it validates the data.
         * 2) it works out the sizes of the discrete blocks that the pkt data 
         *    should be split into. 
         * 3) it sets units = the units per packet
         * 4) it sets the number of trailing units that the channel coder 
         *    gets proded with.
         * 5) it fills in the expected gap between packets.
         */
        cc_coder_t *cc;
        if (!(cc = get_channel_coder(pt)))
                return FALSE;
        u->cc = cc;
        u->pt = pt;
        return cc->valsplit(blk, blen, u, trailing, inter_pkt_gap);
}

int
get_wrapped_payload(int pt, char *data, int data_len)
{
        cc_coder_t *cc;
        if (!(cc = get_channel_coder(pt)) || cc->pt == PT_VANILLA)
                return -1; /* won't be recognized as a payload format as wrong range */
        if (cc->get_wrapped_pt)
                return (cc->get_wrapped_pt(data,data_len));
        return -1;
}

void 
config_channel_coder(session_struct *sp, int pt, char *cmd)
{
        cc_state_t *stp;
        cc_coder_t *cc;
        cc = get_channel_coder(pt);
        stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
        if (cc->config) cc->config(sp, stp, cmd);
}

void
query_channel_coder(session_struct *sp, int pt, char *buf, unsigned int blen)
{
        cc_state_t *stp;
        cc_coder_t *cc;
        cc = get_channel_coder(pt);
        stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
        if (cc->query) cc->query(sp, stp, buf, blen);
}

int
get_bps(session_struct *sp, int pt)
{
        cc_state_t *stp;
        cc_coder_t *cc;
        cc = get_channel_coder(pt);
        stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
        if (cc->bitrate) 
                return cc->bitrate(sp, stp);
        else
                return -1;
}

int
set_cc_pt(char *name, int pt)
{
        int i=0;
    
        while(i<N_CC_CODERS && !strcasecmp(name,cc_list[i].name))
                i++;
        if (i<N_CC_CODERS && cc_list[i].pt !=-1 && pt>96 && pt<127) {
                cc_list[i].pt = pt;
                return TRUE;
        } else {
                return FALSE;
        }
}

int 
get_cc_pt(session_struct *sp, char *name)
{
        int i=0,pt=-1;

        while(i<N_CC_CODERS && strcasecmp(name,cc_list[i].name))
                i++;
        if (i<N_CC_CODERS) {
                if (cc_list[i].pt == -1) 
                        pt = sp->encodings[0];
                else
                        pt = cc_list[i].pt;
        } else {
                pt = -1;
        }
        return pt;
}

int 
get_bytes(cc_unit *cu)
{
        int i,b=0;
        for(i=0;i<cu->iovc;i++)
                b += cu->iov[i].iov_len;
        return b;
}

rx_queue_element_struct *
get_rx_unit(int n, int cc_pt, rx_queue_element_struct *u)
{
        /* returns unit n into the future from the same talkspurt and 
         * under the same channel coder control.
         */
        if (n > 0) {
                while(n>0 && 
                      u->next_ptr && 
                      (u->next_ptr->src_ts - u->src_ts) == u->unit_size &&
                      u->next_ptr->unit_size == u->unit_size &&
                      u->next_ptr->talk_spurt_start == FALSE) {
                        if (u->ccu_cnt && u->ccu[0]->cc->pt != cc_pt) break;
                        u = u->next_ptr;
                        n--;
                }
#ifdef DEBUG
                if (n>0) {
                        if (u->next_ptr == NULL)                                debug_msg("Nothing follows\n");
                        else if (u->next_ptr->src_ts-u->src_ts != u->unit_size) debug_msg("ts jump\n");
                        else if (u->next_ptr->unit_size != u->unit_size)        debug_msg("fmt change\n");
                        else if (u->next_ptr->talk_spurt_start)                 debug_msg("New Talkspurt\n");
                }
#endif
        } else if (n < 0) {
                while(n<0 && 
                      u->prev_ptr && 
                      (u->prev_ptr->src_ts - u->src_ts) == u->unit_size &&
                      u->prev_ptr->unit_size == u->unit_size &&
                      u->prev_ptr->talk_spurt_start == FALSE) {
                        if (u->ccu_cnt && u->ccu[0]->cc->pt != cc_pt) break;
                        u = u->prev_ptr;
                        n++;
                }
        }
        if (n) debug_msg("unit %d not found\n", n);
        return n ? NULL : u;
}

int
add_comp_data(rx_queue_element_struct *u, int pt, struct iovec *iov, int iovc)
{
        int i,j;
        codec_t * cp = get_codec(pt);

        assert(u != NULL);
        assert(u->comp_count < MAX_ENCODINGS-1);

        /* Look for appropriate place to add this data. */
        i = 0;
        while(i<u->comp_count && u->comp_data[i].cp->value > cp->value) 
                i++;

        /* we already have this type of data */
        if (i < u->comp_count && u->comp_data[i].cp->pt == cp->pt) {
                for(i=0;i<iovc;i++) 
                        block_free(iov[i].iov_base, iov[i].iov_len);
                memset(iov, 0, sizeof(struct iovec) * iovc);
                return 0;
        }

        /* We keep lower quality data in case of loss that
         * can be covered using channel coding generates lower
         * quality data and we need earlier state.
         */

        j = u->comp_count;
        while(j>i) {
                memcpy(u->comp_data + j, 
                       u->comp_data + (--j), 
                       sizeof(coded_unit));
        }
        
        j = 0;
        if (iovc>1) {
                u->comp_data[i].state     = iov[j].iov_base;
                u->comp_data[i].state_len = iov[j++].iov_len;
                assert(u->comp_data[i].state_len == cp->sent_state_sz);
        } else {
                u->comp_data[i].state     = NULL;
                u->comp_data[i].state_len = 0;
        }
        u->comp_data[i].data     = iov[j].iov_base;
        u->comp_data[i].data_len = iov[j++].iov_len;
        u->comp_data[i].cp       = cp;

        assert(u->comp_data[i].data_len == cp->max_unit_sz);
        memset(iov,0,j*sizeof(struct iovec));

        return (u->comp_count++);
}

void
channel_set_coder(session_struct *sp, int pt)
{
        cc_coder_t *cc;

        sp->cc_encoding = pt;
        cc = get_channel_coder(pt);

        if (0 == strcmp(cc->name, "REDUNDANCY")) {
                cc_state_t *stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
                red_fix_encodings(sp, (struct s_red_coder*)stp->s);
        } else {
                sp->num_encodings = 1;
        }
        debug_msg("%s\n", cc->name);
}

typedef struct s_collator {
        u_char   units_in_group;
        u_char   units_done[MAX_ENCODINGS];
        cc_unit  *cur[MAX_ENCODINGS];
} collator_t;

collator_t *
collator_create()
{
        collator_t *c = (collator_t*) xmalloc (sizeof(collator_t));
        c->units_in_group = 2;
        memset(c->units_done,0,MAX_ENCODINGS*sizeof(u_char));
        return c;
}

void
collator_destroy(collator_t *c)
{
        int i;
        for(i = 0; i < MAX_ENCODINGS; i++) {
                assert(c->units_done[i] == 0);                
        }
        xfree(c);
}

#define MAX_UNITS_PER_PACKET 16

void 
collator_set_units(collator_t *c, int n)
{
        if (n>0 && n<MAX_UNITS_PER_PACKET)
                c->units_in_group = n;
        else
                fprintf(stderr,"%s:%d %d is not acceptable number of units per packet.\n",__FILE__,__LINE__,n);
}

int
collator_get_units(collator_t *c)
{
        return c->units_in_group;
}


cc_unit* 
collate_coded_units(collator_t *c, coded_unit *cu, int enc_no)
{
        cc_unit   *out;

        if (c->units_done[enc_no] == 0) {
                out = c->cur[enc_no] = (cc_unit*) block_alloc (sizeof(cc_unit));
                out->pt               = cu->cp->pt;
                out->iovc             = 0; 
                if (cu->state_len != 0) {
                        out->iov[out->iovc].iov_base = cu->state;
                        out->iov[out->iovc].iov_len  = cu->state_len;
                        out->iovc++;
                        cu->state     = NULL;
                        cu->state_len = 0;
                } 
        } else {
                out = c->cur[enc_no];
                assert(cu->cp->pt == out->pt);
                if (cu->state_len != 0) {
                        block_free(cu->state, cu->state_len);
                        cu->state     = NULL;
                        cu->state_len = 0;
                }
        }

        assert(cu->data != NULL);
        out->iov[out->iovc].iov_base = cu->data;
        out->iov[out->iovc].iov_len  = cu->data_len;
        out->iovc++;
        assert(out->iovc < CC_UNITS);
        c->units_done[enc_no]++;

        if (c->units_done[enc_no] == c->units_in_group) {
                c->units_done[enc_no] = 0;
                return out;
        }

        return NULL;
}

int 
fragment_sizes(codec_t *cp, int blk_len, struct iovec *store, int *iovc, int store_len)
{
        int n = 0;

        /* When the time comes for variable bitrate codecs to go in 
         * codec specific sniffer functions need to be written and hooks
         * go here to determine blk sizes.  Same should go in fragment_spread
         */

        if (blk_len > 0 && cp->sent_state_sz != 0 && (*iovc) < store_len) {
                store[(*iovc)++].iov_len = cp->sent_state_sz;
                blk_len                 -= cp->sent_state_sz;
        }
        
        while(blk_len > 0 && (*iovc) < store_len) {
                store[(*iovc)++].iov_len = cp->max_unit_sz;
                blk_len                 -= cp->max_unit_sz;
                n++;
        }

#ifdef    DEBUG
        if (blk_len != 0) debug_msg("Fragmentation failed.\n");
#endif /* DEBUG */
        
        return (blk_len == 0 ? n : -1);
}

int 
fragment_spread(codec_t *cp, int len, struct iovec *iov, int iovc, rx_queue_element_struct *u)
{
        int done = 0, cc_pt;
        assert(cp);

        while(len > 0 && done < iovc) {
                if (u) {
                        cc_pt = u->cc_pt;
                        if (done != 0 || (done == 0 && cp->sent_state_sz == 0)) {
                                len -= iov[done].iov_len;
                                add_comp_data(u, cp->pt, iov + done, 1);
                                done += 1;
                        } else {
                                len -= iov[done].iov_len + iov[done+1].iov_len;
                                add_comp_data(u, cp->pt, iov + done, 2);
                                done += 2;
                        }
                        if (len) u = get_rx_unit(1, cc_pt, u);
                } else {
                        debug_msg("Unit missing\n");
                        if (done != 0 || (done == 0 && cp->sent_state_sz == 0)) {
                                len -= iov[done].iov_len;
                                done += 1;
                        } else {
                                len -= iov[done].iov_len + iov[done+1].iov_len;
                                done += 2;
                        }
                }
        }
        assert(len == 0);
        assert(done <= iovc);
        return done;
}
