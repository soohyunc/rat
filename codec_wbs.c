/*
 * FILE:    codec_wbs.c
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
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

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "assert.h"
#include "codec_wbs.h"
#include "cx_wbs.h"

static codec_format_t cs[] = {
        {"WBS", "WBS-16K-MONO",  
         "Wide band speech coder. (c) Markus Iken, University College London, 1997-98.", 
         /* NB payload 109 for backward compatibility */
         109, WBS_STATE_SIZE, WBS_UNIT_SIZE, 
         {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}
        }
};

#define WBS_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

u_int16
wbs_get_formats_count()
{
        return (u_int16)WBS_NUM_FORMATS;
}

const codec_format_t *
wbs_get_format(u_int16 idx)
{
        assert(idx < WBS_NUM_FORMATS);
        return &cs[idx];
}

typedef struct s_wbs_state {
        wbs_state_struct        state;
        double                  qmf_lo[16];
        double                  qmf_hi[16];
        short                   ns;             /* Noise shaping state */
} wbs_t;

int 
wbs_state_create(u_int16 idx, u_char **s)
{
        wbs_t *st;
        int    sz;

        if (idx < WBS_NUM_FORMATS) {
                sz = sizeof(wbs_t);
                st = (wbs_t*)xmalloc(sz);
                if (st) {
                        memset(st, 0, sz);
                        wbs_state_init(&st->state, 
                                       st->qmf_lo, 
                                       st->qmf_hi, 
                                       &st->ns);
                        *s = (u_char*)st;
                        return sz;
                }
        }
        *s = NULL;
        return 0;
}

void
wbs_state_destroy(u_int16 idx, u_char **s)
{
        UNUSED(idx);
        assert(idx < WBS_NUM_FORMATS);
        xfree(*s);
        *s = (u_char*)NULL;
}

int
wbs_encoder(u_int16 idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        subband_struct SubBandData;
        wbs_t *wsp;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < WBS_NUM_FORMATS);
        UNUSED(idx);
        
        /* Transfer state and fix ordering */
        c->state     = (u_char*)block_alloc(WBS_STATE_SIZE);
        c->state_len = WBS_STATE_SIZE;
        c->data      = (u_char*)block_alloc(WBS_UNIT_SIZE);
        c->data_len  = WBS_UNIT_SIZE;

        wsp = (wbs_t*)encoder_state;
        memcpy(c->state, &wsp->state, WBS_STATE_SIZE);
        QMF(inbuf, &SubBandData, wsp->qmf_lo, wsp->qmf_hi);
        LowEnc(SubBandData.Low, c->data, wsp->state.low, &wsp->ns);
        HighEnc(SubBandData.High, c->data, wsp->state.hi);

        return c->data_len;
}

int
wbs_decoder(u_int16 idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        subband_struct SubBandData;
        wbs_t   *wsp = (wbs_t *)decoder_state;

        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < WBS_NUM_FORMATS);

        if (c->state_len > 0) {
                assert(c->state_len == WBS_STATE_SIZE);
                memcpy(&wsp->state, c->state, WBS_STATE_SIZE);
        }

        LowDec(c->data, SubBandData.Low, wsp->state.low, &wsp->ns);
        HighDec(c->data, SubBandData.High, wsp->state.hi);
        deQMF(&SubBandData, data, wsp->qmf_lo, wsp->qmf_hi);
        return 160; /* Only does this size */
}




