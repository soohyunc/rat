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

#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
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

#define MAX_UNITS_PER_PACKET 8

typedef struct s_cc_state {
    struct s_cc_state *next;
    int    pt;
    char  *s;
} cc_state_t;

void 
set_units_per_packet(session_struct *sp, int n)
{
    if (n>0 && n<MAX_UNITS_PER_PACKET)
        sp->units_per_pckt = n;
    else
        fprintf(stderr,"%s:%d %d is not acceptable number of units per packet.\n",__FILE__,__LINE__,n);
}

int
get_units_per_packet(session_struct *sp)
{
    return sp->units_per_pckt;
}

/*****************************************************************************/
/* Vanilla channel coding - just gathers/separates media units               */

typedef struct {
    int clk;
    int cnt;
    coded_unit cx[MAX_UNITS_PER_PACKET];
} vanilla_state;

static void 
vanilla_init(session_struct *sp, cc_state_t *cs) {
    cs->s = (char*)xmalloc(sizeof(vanilla_state));
    memset(cs->s,0,sizeof(vanilla_state));
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

static int
vanilla_encode(session_struct *sp, 
               sample *raw, 
               cc_unit *cu, 
               cc_state_t *ccs)
{
    vanilla_state *v = (vanilla_state*)ccs->s;
    int i;

    if (v->clk % sp->units_per_pckt==0) {
        for(i=0;i<v->cnt;i++) 
            clear_coded_unit(v->cx+i);
        v->cnt = 0;
        cu->iovc = 1;
    }

    if (raw) {
        assert((v->cx+v->cnt)->data_len == 0);
        assert((v->cx+v->cnt)->state_len == 0);
        encoder(sp, raw, sp->encodings[0], v->cx+v->cnt);
        ++v->cnt;
    } 
    
    v->clk++;
    if (v->clk % sp->units_per_pckt == 0 && v->cnt) {
        assert(cu->iovc == 1);
        for(i=0;i<v->cnt;i++) {
            cu->iovc += coded_unit_to_iov(v->cx   + i,
                                          cu->iov + cu->iovc,
                                          (!i) ? INCLUDE_STATE : NO_INCLUDE_STATE);
        }
        return TRUE;
    }

    return FALSE;
}

static void 
vanilla_encoder_reset(cc_state_t *ccs) 
{
    vanilla_state *v = (vanilla_state*)ccs->s;
    int i;
    for(i=0;i<v->cnt;i++)
        clear_coded_unit(v->cx + i);
    v->clk = v->cnt = 0;
}

static int 
vanilla_bitrate(session_struct *sp, cc_state_t *cs)
{
    int pps, upp;
    codec_t *cp;
    cp = get_codec(sp->encodings[0]);
    pps = cp->freq / cp->unit_len;
    upp = get_units_per_packet(sp);
    return (8*pps*(cp->max_unit_sz * upp + cp->sent_state_sz + 12)/upp);
}

static int
vanilla_valsplit(char *blk, int blen, cc_unit *u, int *trailing) 
{
    codec_t *cp = get_codec(u->src_pt);
    int n = 0;

    assert(!u->iovc);
    if (cp) {
        if (cp->sent_state_sz) {
            u->iov[u->iovc++].iov_len = cp->sent_state_sz;
            blen -= cp->sent_state_sz;
        }
        while(blen>0 && cp->max_unit_sz) {
            n++;
            u->iov[u->iovc++].iov_len = cp->max_unit_sz;
            blen -= cp->max_unit_sz;
        }
    }

    if (blen) u->iovc = 0;
    
    (*trailing) = n;

    return (blen?0:n);
}

static void 
vanilla_decode(rx_queue_element_struct *u, cc_state_t *ccs)
{
    int i,n,pt;
    struct iovec *iov;
    codec_t *cp;

    if (!u->ccu_cnt) return;

    pt = u->ccu[0]->src_pt;
    cp = get_codec(pt);
    assert(cp);

    iov = u->ccu[0]->iov;
    n  = u->ccu[0]->iovc;

    for(i=0; i<n; i++, u=get_rx_unit(1,PT_VANILLA,u)) {
        if (i==0 && cp->sent_state_sz) {
            add_comp_data(u,pt,iov,2);
            i++;
        } else {
            add_comp_data(u,pt,iov+i,1);
        }
    }
}

static void 
red_init(session_struct *sp, cc_state_t *cs) 
{
    cs->s = (char*)new_red_coder();
} 

static int
red_configure(session_struct *sp, cc_state_t *cs, char *cmd)
{
    return red_config(sp,(struct s_red_coder*)cs->s,cmd);
}

static void 
red_query(session_struct    *sp, 
          struct s_cc_state *cs,
          char *buf, int blen) 
{
    return red_qconfig(sp,(struct s_red_coder*)cs->s,buf,blen);
}

static int
red_encoder(session_struct *sp, 
            sample *raw, 
            cc_unit *cu, 
            cc_state_t *ccs)
{
    return red_encode(sp,raw,cu,(struct s_red_coder*)ccs->s);
}

static int
red_bitrate(session_struct *sp,
            cc_state_t     *cs)
{
    return red_bps(sp, (struct s_red_coder*)cs->s);
}

static void
red_decoder(rx_queue_element_struct *u,
            cc_state_t              *cs)
{
    if (!u->ccu_cnt) return;
    red_decode(u);
}

static void 
intl_init(session_struct *sp, cc_state_t *cs) 
{
    cs->s = (char*)new_intl_coder(sp);
} 

static void 
intl_dec_init(cc_state_t *cs) 
{
    cs->s = (char*)new_intl_coder();
} 

static int
intl_configure(session_struct *sp, cc_state_t *cs, char *cmd)
{
    return intl_config(sp,(struct s_intl_coder*)cs->s,cmd);
}

static void 
intl_query(session_struct    *sp, 
           struct s_cc_state *cs,
           char *buf, int blen) 
{
    intl_qconfig(sp,(struct s_intl_coder*)cs->s,buf,blen);
}

static int
intl_encoder(session_struct *sp, 
             sample *raw, 
             cc_unit *cu, 
             cc_state_t *ccs)
{
    return intl_encode(sp,raw,cu,(struct s_intl_coder*)ccs->s);
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
intl_decoder(rx_queue_element_struct *u,
             cc_state_t              *cs)
{
    intl_decode(u,(struct s_intl_coder*)cs->s);
}

#define N_CC_CODERS 3

static cc_coder_t cc_list[] = {
    {"VANILLA", 
     PT_VANILLA, 
     vanilla_init, 
     NULL, 
     NULL,
     vanilla_bitrate,
     vanilla_encode,
     vanilla_encoder_reset,
     1,
     vanilla_valsplit,
     NULL,
     NULL,
     vanilla_decode},
    {"REDUNDANCY", 
     PT_REDUNDANCY,
     red_init,
     red_configure,
     red_query,
     red_bitrate,
     red_encoder,
     NULL,
     1,
     red_valsplit,
     red_wrapped_pt,
     NULL,
     red_decoder},
    {"INTERLEAVER",
     PT_INTERLEAVED,
     intl_init,
     intl_configure,
     intl_query,
     intl_bitrate,
     intl_encoder,
     intl_encoder_reset,
     1,
     intl_valsplit,
     intl_wrapped_pt,
     intl_dec_init,
     intl_decoder}
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

    for (stp = *lp; stp; stp = stp->next)
        if (stp->pt == pt)
            break;
    
    if (stp == 0) {
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
                cp->dec_init(stp);
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

void
reset_channel_encoder(session_struct *sp,
                      int cc_pt)
{
    cc_state_t *stp;
    cc_coder_t *cc;
    cc  = get_channel_coder(cc_pt);
    stp = get_cc_state(sp,&sp->cc_state_list,cc->pt,ENCODE);
    if (cc->enc_reset)
        cc->enc_reset(stp);
}

int
channel_code(session_struct *sp,
             cc_unit        *u,
             int             pt,
             sample         *raw)
{
    cc_state_t *stp;
    cc_coder_t *cc;
    cc = get_channel_coder(pt);
    u->cc = cc;
    stp = get_cc_state(sp, &sp->cc_state_list, cc->pt, ENCODE);
    return cc->encode(sp, raw, u, stp);
}

void 
channel_decode(rx_queue_element_struct *u)
{
    cc_state_t *stp;
    cc_coder_t *cc;
    cc  = get_channel_coder(u->cc_pt);
    stp = get_cc_state(NULL, 
                       &u->dbe_source[0]->cc_state_list, 
                       u->cc_pt,
                       DECODE);
    cc->decode(u, stp);
}

int
validate_and_split(int pt, char *blk, int blen, cc_unit *u, int *trailing)
{
    /* The valsplit function serves 4 purposes:
     * 1) it validates the data.
     * 2) it works out the sizes of the discrete blocks that the pkt data 
     *    should be split into. 
     * 3) it sets units = the units per packet
     * 4) it sets the number of trailing units that the channel coder 
     *    gets proded with.
     */
    cc_coder_t *cc;
    if (!(cc = get_channel_coder(pt)))
        return FALSE;
    u->cc = cc;
    u->src_pt = pt;
    return cc->valsplit(blk, blen, u, trailing);
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
query_channel_coder(session_struct *sp, int pt, char *buf, int blen)
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
    while(n>0 && 
          u->next_ptr && 
          (u->next_ptr->playoutpt - u->playoutpt) == u->unit_size &&
          u->next_ptr->unit_size == u->unit_size &&
          u->next_ptr->talk_spurt_start == FALSE) {
        if (u->ccu_cnt && u->ccu[0]->cc->pt != cc_pt) break;
        u = u->next_ptr;
        n--;
    }
    return n ? NULL : u;
}

int
add_comp_data(rx_queue_element_struct *u, int pt, struct iovec *iov, int iovc)
{
    int i,j;
    codec_t * cp = get_codec(pt);
    assert(u->comp_count < MAX_ENCODINGS-1);

    /* Sort adds compressed data to rx element. */

    /* We keep lower quality data in case of loss that
     * can be covered using channel coding generates lower
     * quality data and we need earlier state.
     */

    i = 0;
    while(i<u->comp_count && u->comp_data[i].cp->value > cp->value) 
        i++;

    j = u->comp_count;
    while(j>i)
        memcpy(&u->comp_data[j], &u->comp_data[--j], sizeof(coded_unit));

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

/* Channel coders operate over iovec and source coders use coded_units */
/* so the following facilitate interchange                             */

int
coded_unit_to_iov(coded_unit   *cu,
                  struct iovec *iov, 
                  int inc_state)
{
    int i=0;
    if (cu->cp->sent_state_sz) {
        if (inc_state == INCLUDE_STATE) {
            iov[i].iov_base  = cu->state;
            iov[i++].iov_len = cu->state_len;
        }
    }
    iov[i].iov_base  = cu->data;
    iov[i++].iov_len = cu->data_len;
    return i;
}

int
iov_to_coded_unit(struct iovec *iov,
                  coded_unit   *cu,
                  int           pt)
{
    int i = 0;
    cu->cp = get_codec(pt);
    assert(cu->cp);
    if (cu->cp->sent_state_sz) {
        cu->state     = iov->iov_base;
        cu->state_len = iov->iov_len;
        memset(iov,0,sizeof(struct iovec));
        iov++;
        i++;
    } else {
            cu->state     = NULL;
            cu->state_len = 0;
    }
    cu->data     = iov->iov_base;
    cu->data_len = iov->iov_len;
    memset(iov,0,sizeof(struct iovec));
    return ++i;
}

