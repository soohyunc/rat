/*
 * FILE:    codec_vdvi.c
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
#include "codec_vdvi.h"
#include "cx_dvi.h"
#include "cx_vdvi.h"

static codec_format_t cs[] = {
        {"VDVI", "VDVI-8K-MONO",  
         "Variable Rate IMA ADPCM codec.", 
         CODEC_PAYLOAD_DYNAMIC, 4, 80, 
         {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"VDVI", "VDVI-16K-MONO",  
         "Variable Rate IMA ADPCM codec.", 
         CODEC_PAYLOAD_DYNAMIC, 4, 80, 
         {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"VDVI", "VDVI-32K-MONO",  
         "Variable Rate IMA ADPCM codec.", 
         CODEC_PAYLOAD_DYNAMIC, 4, 80, 
         {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 5   ms */
        {"VDVI", "VDVI-48K-MONO",  
         "Variable Rate IMA ADPCM codec.", 
         CODEC_PAYLOAD_DYNAMIC, 4, 80, 
         {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}}  /* 3.3 ms */
};

#define VDVI_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

u_int16
vdvi_get_formats_count()
{
        return (u_int16)VDVI_NUM_FORMATS;
}

const codec_format_t *
vdvi_get_format(u_int16 idx)
{
        assert(idx < VDVI_NUM_FORMATS);
        return &cs[idx];
}

int 
vdvi_state_create(u_int16 idx, u_char **s)
{
        struct adpcm_state *as;
        int                 sz;

        if (idx < VDVI_NUM_FORMATS) {
                sz = sizeof(struct adpcm_state);
                as = (struct adpcm_state*)xmalloc(sz);
                if (as) {
                        memset(as, 0, sz);
                        *s = (u_char*)as;
                        return sz;
                }
        }
        return 0;
}

void
vdvi_state_destroy(u_int16 idx, u_char **s)
{
        UNUSED(idx);
        assert(idx < VDVI_NUM_FORMATS);
        xfree(*s);
        *s = (u_char*)NULL;
}

/* Buffer of maximum length of vdvi coded data - never know how big
 * it needs to be
 */

int
vdvi_encoder(u_int16 idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        int samples, len;

        u_char dvi_buf[80];
        u_char vdvi_buf[160];

        assert(encoder_state);
        assert(inbuf);
        assert(idx < VDVI_NUM_FORMATS);
        UNUSED(idx);
        
        /* Transfer state and fix ordering */
        c->state     = (u_char*)block_alloc(sizeof(struct adpcm_state));
        c->state_len = sizeof(struct adpcm_state);
        memcpy(c->state, encoder_state, sizeof(struct adpcm_state));

        /* Fix coded state for byte ordering */
	((struct adpcm_state*)c->state)->valprev = htons(((struct adpcm_state*)c->state)->valprev);
        
        samples = cs[idx].format.bytes_per_block * 8 / cs[idx].format.bits_per_sample;
        
        assert(samples == 160);

        adpcm_coder(inbuf, dvi_buf, samples, (struct adpcm_state*)encoder_state);

        len = vdvi_encode(dvi_buf, 160, vdvi_buf, 160);

        c->data     = (u_char*)block_alloc(len); 
        c->data_len = len;
        memcpy(c->data, vdvi_buf, len);

        return len;
}

int
vdvi_decoder(u_int16 idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        int samples, len; 
        u_char dvi_buf[80];

        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < VDVI_NUM_FORMATS);

	if (c->state_len > 0) {
		assert(c->state_len == sizeof(struct adpcm_state));
		memcpy(decoder_state, c->state, sizeof(struct adpcm_state));
		((struct adpcm_state*)decoder_state)->valprev = ntohs(((struct adpcm_state*)decoder_state)->valprev);
	}

        len = vdvi_decode(c->data, c->data_len, dvi_buf, 160);

        samples = cs[idx].format.bytes_per_block / sizeof(sample);
	adpcm_decoder(dvi_buf, data, samples, (struct adpcm_state*)decoder_state);

        return samples;
}

int
vdvi_peek_frame_size(u_int16 idx, u_char *data, int data_len)
{
        u_char dvi_buf[80];
        int len;

        UNUSED(idx);

        len = vdvi_decode(data, data_len, dvi_buf, 160);

        assert(len < data_len);
        return len;
}
