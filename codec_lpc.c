/*
 * FILE:    codec_lpc.c
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
#include "codec_lpc.h"
#include "cx_lpc.h"

static codec_format_t cs[] = {
        {"LPC", "LPC-8K-MONO", 
         "Pitch excited linear prediction codec. Contributed by R. Frederick and implemented by R. Zuckerman.",
         7, 0, LPCTXSIZE,
         {DEV_S16, 8000, 16, 1, 160 * BYTES_PER_SAMPLE}}
};

#define LPC_NUM_FORMATS (sizeof(cs)/sizeof(codec_format_t))

u_int16
lpc_get_formats_count()
{
        return LPC_NUM_FORMATS;
}

const codec_format_t*
lpc_get_format(u_int16 idx)
{
        assert(idx < LPC_NUM_FORMATS);
        return &cs[idx];
}

void
lpc_setup(void)
{
        lpc_init();
}

int
lpc_encoder_state_create(u_int16 idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        *state = (u_char*) xmalloc(sizeof(lpc_encstate_t));
        lpc_enc_init((lpc_encstate_t*) *state);
        return sizeof(lpc_encstate_t);
}

void
lpc_encoder_state_destroy(u_int16 idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        
        xfree(*state);
        *state = (u_char*)NULL;
}

int
lpc_decoder_state_create(u_int16 idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        *state = (u_char*) xmalloc(sizeof(lpc_intstate_t));
        lpc_dec_init((lpc_intstate_t*) *state);
        return sizeof(lpc_intstate_t);
}

void
lpc_decoder_state_destroy(u_int16 idx, u_char **state)
{
        assert(idx < LPC_NUM_FORMATS);
        UNUSED(idx);
        
        xfree(*state);
        *state = (u_char*)NULL;
}

int
lpc_encoder  (u_int16 idx, u_char *state, sample *in, coded_unit *out)
{
        assert(idx < LPC_NUM_FORMATS);
        assert(in);
        assert(out);
        UNUSED(idx);
        UNUSED(state);

        out->state     = NULL;
        out->state_len = 0;
        out->data      = (u_char*)block_alloc(LPCTXSIZE);
        out->data_len  = LPCTXSIZE;

        lpc_analyze((const short*)in, 
                    (lpc_encstate_t*)state, 
                    (lpc_txstate_t*)out->data);
        return out->data_len;
}

int
lpc_decoder (u_int16 idx, u_char *state, coded_unit *in, sample *out)
{
        assert(idx < LPC_NUM_FORMATS);
        assert(state);
        assert(in && in->data);
        assert(out);

        UNUSED(idx);
        lpc_synthesize((short*)out,  
                       (lpc_txstate_t*)in->data, 
                       (lpc_intstate_t*)state);
        return cs[idx].format.bytes_per_block / BYTES_PER_SAMPLE;
}

int  
lpc_repair (u_int16 idx, u_char *state, u_int16 consec_lost,
            coded_unit *prev, coded_unit *missing, coded_unit *next)
{
        lpc_txstate_t *lps;

        assert(prev);
        assert(missing);

        if (missing->data) {
                debug_msg("lpc_repair: missing unit had data!\n");
                block_free(missing->data, missing->data_len);
        }
        
        missing->data     = (u_char*)block_alloc(LPCTXSIZE);
        missing->data_len = LPCTXSIZE;
        
        assert(prev->data);
        assert(prev->data_len == LPCTXSIZE);
        memcpy(missing->data, prev->data, LPCTXSIZE);       
        
        lps = (lpc_txstate_t*)missing->data;
        lps->gain = (u_char)((float)lps->gain * 0.8f);

        UNUSED(next);
        UNUSED(consec_lost);
        UNUSED(state);
        UNUSED(idx);

        return TRUE;
}
