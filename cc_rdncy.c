/*
 * FILE:      cc_rdncy.c
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
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

#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_rdncy.h"

#include "memory.h"
#include "util.h"
#include "debug.h"

#include "timers.h"

/* This is pap.  Have run out of time.  Apologies if you ever end
 * up here... [oh] 
 */

#define RED_MAX_LAYERS 3
#define RED_MAX_OFFSET 16383u
#define RED_MAX_LEN    1023u

#define RED_PRIMARY 1
#define RED_EXTRA   2



#define RED_HDR32_INIT(x)      (x)  = 0x80000000
#define RED_HDR32_SET_PT(x,y)  (x) |= ((y)<<24)
#define RED_HDR32_SET_OFF(x,y) (x) |= ((y)<<10)
#define RED_HDR32_SET_LEN(x,y) (x) |= (y)
#define RED_HDR32_GET_PT(z)    (((z) >> 24) & 0x7f)
#define RED_HDR32_GET_OFF(z)   (((z) >> 10) & 0x3fff)
#define RED_HDR32_GET_LEN(z)   ((z) & 0x3ff)

#define RED_HDR8_INIT(x)       (x) = 0
#define RED_HDR8_SET_PT(x,y)   (x) = (y)
#define RED_HDR8_GET_PT(z)     (z)

typedef struct s_red_layer {
        codec_id_t          cid;
        u_int32             units_off;
} red_layer;

typedef struct {
        red_layer            layer[RED_MAX_LAYERS];
        u_int32              n_layers;
        struct s_pb          *media_buffer;
        struct s_pb_iterator *media_pos;
        u_int32               units_ready;
} red_enc_state;

int
redundancy_encoder_create(u_char **state, u_int32 *len)
{
        red_enc_state *re = (red_enc_state*)xmalloc(sizeof(red_enc_state));

        if (re == NULL) {
                goto fail_alloc;
        }

        *state = (u_char*)re;
        *len   = sizeof(red_enc_state);

        if (pb_create(&re->media_buffer,
                      (playoutfreeproc)media_data_destroy) == FALSE) {
                goto fail_pb;
        }

        if (pb_iterator_create(re->media_buffer, &re->media_pos) == FALSE) {
                goto fail_pb_iterator;
        }

        re->n_layers    = 0;        
        re->units_ready = 0;

        return TRUE;

fail_pb_iterator:
        pb_destroy(&re->media_buffer);

fail_pb:
        xfree(re);

fail_alloc:
        *state = NULL;
        return FALSE;
}

void
redundancy_encoder_destroy(u_char **state, u_int32 len)
{
        red_enc_state *re = *((red_enc_state**)state);
        
        assert(len == sizeof(red_enc_state));

        pb_iterator_destroy(re->media_buffer,
                            &re->media_pos);

        pb_destroy(&re->media_buffer);

        xfree(*state);
        *state = NULL;
}

int
redundancy_encoder_reset(u_char *state)
{
        red_enc_state *re = (red_enc_state*)state;
        
        pb_flush(re->media_buffer);
        re->units_ready = 0;

        return TRUE;
}

/* Adds header to next free slot in channel_data */
static void
add_hdr(channel_data *cd, int hdr_type, codec_id_t cid, u_int32 uo, u_int32 len)
{
        channel_unit *chu;
        u_int32 so;             /* sample time offset */
        u_char  pt;

        pt = codec_get_payload(cid);
        assert(payload_is_valid(cid));

        so = codec_get_samples_per_frame(cid) * uo;

        assert(so <= RED_MAX_OFFSET);
        assert(len <= RED_MAX_LEN );

        chu = (channel_unit*)block_alloc(sizeof(channel_unit));
        cd->elem[cd->nelem++] = chu;
        
        if (hdr_type == RED_EXTRA) {
                u_int32 *h;
                h = (u_int32*)block_alloc(4);
                RED_HDR32_INIT(*h);
                RED_HDR32_SET_PT(*h, (u_int32)pt);
                RED_HDR32_SET_OFF(*h, so);
                RED_HDR32_SET_LEN(*h, len);
                chu->data     = (u_char*)h;
                chu->data_len = 4;
        } else {
                u_char *h;
                assert(hdr_type == RED_PRIMARY);
                h = (u_char*)block_alloc(1);
                RED_HDR8_INIT(*h);
                RED_HDR8_SET_PT(*h, pt);
                chu->data     = h;
                chu->data_len = 1;
        }
}

static u_int32
make_pdu(struct s_pb          *pb, 
         struct s_pb_iterator *pbi, 
         codec_id_t            cid, 
         channel_data         *cd, 
         u_int32               upp)
{
        struct s_pb_iterator *p;
        u_int32        units, got, md_len, r;
        channel_unit  *chu;
        media_data    *md;
        coded_unit    *cdu_target[MAX_UNITS_PER_PACKET];
        u_char        *dp;
        ts_t           playout;

        pb_iterator_dup(&p, pbi);

        chu = (channel_unit*)block_alloc(sizeof(channel_unit));
        chu->data_start = 0;
        chu->data_len   = 0;

        chu->pt = codec_get_payload(cid);
        assert(payload_is_valid(chu->pt));

        /* First work out how much space this is going to need and
         * to save time cache units wanted in cdu_target.
         */

        for(units = 0; units < upp; units++) {        
                pb_iterator_get_at(p, (u_char**)&md, &md_len, &playout);
                if (md == NULL) break;
                assert(md_len == sizeof(media_data));
                for(r = 0; r < md->nrep; r++) {
                        if (md->rep[r]->id == cid) {
                                cdu_target[r] = md->rep[r];
                                chu->data_len += md->rep[r]->data_len;
                                if (units == 0) {
                                        chu->data_len += md->rep[r]->state_len;
                                }
                        }
                }
                pb_iterator_advance(p);
        }

        if (chu->data_len == 0) {
                /* Nothing to do, chu not needed */
                block_free(chu, sizeof(channel_unit));
                return 0;
        }

        chu->data = (u_char*)block_alloc(chu->data_len);
        dp = chu->data;
        for(r = 0; r < units; r++) {
                if (r == 0 && cdu_target[r]->state_len) {
                        memcpy(dp, cdu_target[r]->state, cdu_target[r]->state_len);
                        dp += cdu_target[r]->state_len;
                }
                memcpy(dp, cdu_target[r]->data, cdu_target[r]->data_len);
                dp += cdu_target[r]->data_len;
        }

        assert((u_int32)(dp - chu->data) == chu->data_len);
        assert(cd->nelem < MAX_CHANNEL_UNITS);

        cd->elem[cd->nelem] = chu;
        cd->nelem++;

        pb_iterator_destroy(pb, &p);
        return got;
}


/* redundancy_check_layers - checks codings specified for redundant
 * encoder are present in incomding media_data unit, m.  And only
 * those are present.
 */

__inline static void
redundancy_check_layers(red_enc_state *re, media_data *m)
{
        u_int8     i;
        u_int32    lidx;
        int        found, used[MAX_MEDIA_UNITS];
        assert(re);
        assert(m);

        for(i = 0; i < m->nrep; i++) {
                used[i] = 0;
        }
        
        for(lidx = 0; lidx < re->n_layers ; lidx++) {
                found = FALSE;
                for(i = 0; i < m->nrep; i++) {
                        assert(m->rep[i] != NULL);
                        if (m->rep[i]->id == re->layer[lidx].cid) {
                                found = TRUE;
                                used[i]++;
                                break;
                        }
                }
                assert(found == TRUE); /* Coding for layer not found */
        }

        for(i = 0; i < m->nrep; i++) {
                assert(used[i]); /* i'th rep in media_data not used */
        }
}

static channel_data *
redundancy_encoder_output(red_enc_state *re, u_int32 upp)
{
        struct s_pb_iterator *pbm;
        channel_data *cd_part, *cd_out;
        u_int32       i, units, lidx;

        pbm = re->media_pos;
        pb_iterator_ffwd(pbm);

        /* Rewind iterator to start of first pdu */ 
        for(i = 1; i < upp; i++) {
                pb_iterator_retreat(pbm);
        }

        channel_data_create(&cd_part, 0);

        for(lidx = 0; lidx < re->n_layers; lidx++) {
                for(i = 0; i < re->layer[lidx].units_off; i++) {
                        pb_iterator_retreat(pbm);
                }
                units = make_pdu(re->media_buffer, pbm, re->layer[lidx].cid, cd_part, upp);
                if (units == 0) break;
                assert(units == upp);
        }

        assert(lidx != 0);

        /* cd_part contains pdu's for layer 0, layer 1, layer 2... in
         * ascending order Now fill cd_out with pdu's with headers in
         * protocol order layer n, n-1, .. 1.  Note we must transfer
         * pointers from cd_part to cd_out and make pointers in
         * cd_part null because channel_data_destroy will free the
         * channel units referred to by cd_part. 
         */
        
        channel_data_create(&cd_out, 0);

        if (lidx != re->n_layers) {
                /* Put maximum offset info if not present in the packet */
                add_hdr(cd_out, RED_EXTRA, 
                        re->layer[re->n_layers - 1].cid, 
                        re->layer[re->n_layers - 1].units_off,
                        0);
        }

        /* Slot in redundant payloads and their headers */
        while(lidx != 0) {
                add_hdr(cd_out, RED_EXTRA,
                        re->layer[lidx].cid,
                        re->layer[lidx].units_off,
                        cd_part->elem[lidx]->data_len);
                cd_out->elem[cd_out->nelem] = cd_part->elem[lidx];
                cd_out->nelem ++;
                cd_part->elem[lidx] = NULL;
        }

        /* Now the primary and it's header */
        add_hdr(cd_out, RED_PRIMARY,
                re->layer[lidx].cid,
                re->layer[lidx].units_off,
                cd_part->elem[lidx]->data_len);
        cd_out->elem[cd_out->nelem] = cd_part->elem[lidx];
        cd_out->nelem++;
        cd_part->elem[lidx] = NULL;

#ifndef NDEBUG
        for(i = 0; i < cd_part->nelem; i++) {
                assert(cd_part->elem[i] == NULL);
        }
#endif
        cd_part->nelem = 0;
        channel_data_destroy(&cd_part, sizeof(channel_data));

        return cd_out;
}

int
redundancy_encoder_encode (u_char      *state,
                           struct s_pb *in,
                           struct s_pb *out,
                           u_int32      upp)
{
        u_int32        m_len;
        ts_t           playout;
        struct s_pb_iterator *pi;
        media_data     *m;
        red_enc_state  *re = (red_enc_state*)state;

        assert(upp != 0 && upp <= MAX_UNITS_PER_PACKET);

        pb_iterator_create(in, &pi);
        assert(pi != NULL);

        while(pb_iterator_advance(pi)) {
                /* Remove element from playout buffer - it belongs to
                 * the redundancy encoder now.  */
                pb_iterator_detach_at(pi, (u_char**)&m, &m_len, &playout);
                assert(m != NULL);

#ifndef NDEBUG
                redundancy_check_layers(re, m);
#endif /* NDEBUG */

                pb_add(re->media_buffer, 
                       (u_char*)m,
                       m_len,
                       playout);
                re->units_ready++;

                if ((re->units_ready % upp) == 0) {
                        channel_data *cd;
                        int s;
                        cd = redundancy_encoder_output(re, upp);
                        assert(cd != NULL);
                        s  = pb_add(out, (u_char*)cd, sizeof(channel_data), playout);
                        assert(s);
                }
        }
        pb_iterator_destroy(in, &pi);

        return TRUE;
}

