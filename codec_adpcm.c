/*
 * FILE:    codec_adpcm.c
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
#include "codec_adpcm.h"
#include "cx_dvi.h"

static codec_format_t cs[] = {
        {"DVI", "DVI-8K-Mono",  
         "IMA ADPCM codec. (c) 1992 Stichting Mathematisch Centrum, Amsterdam, Netherlands.", 
         5,                     4, 80, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"DVI", "DVI-16K-Mono", 
         "IMA ADPCM codec - Overclocked. (c) 1992 Stichting Mathematisch Centrum, Amsterdam, Netherlands.",
         6,                     4, 80, {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"DVI", "DVI-32K-Mono", 
         "IMA ADPCM codec - Overclocked. (c) 1992 Stichting Mathematisch Centrum, Amsterdam, Netherlands.",
         CODEC_PAYLOAD_DYNAMIC, 4, 80, {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 5   ms */
        {"DVI", "DVI-32K-Mono", 
         "IMA ADPCM codec - Overclocked. (c) 1992 Stichting Mathematisch Centrum, Amsterdam, Netherlands.",
         CODEC_PAYLOAD_DYNAMIC, 4, 80, {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}}  /* 3.3 ms */
};

#define DVI_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

u_int16
dvi_get_formats_count()
{
        return (u_int16)DVI_NUM_FORMATS;
}

const codec_format_t *
dvi_get_format(u_int16 idx)
{
        assert(idx < DVI_NUM_FORMATS);
        return &cs[idx];
}

int 
dvi_state_create(u_int16 idx, u_char **s)
{
        struct adpcm_state *as;
        int                 sz;

        if (idx < DVI_NUM_FORMATS) {
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
dvi_state_destroy(u_int16 idx, u_char **s)
{
        UNUSED(idx);
        assert(idx < DVI_NUM_FORMATS);
        xfree(*s);
        *s = (u_char*)NULL;
}

int
dvi_encode(u_int16 idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        int samples;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < DVI_NUM_FORMATS);
        UNUSED(idx);
        
        /* Transfer state and fix ordering */
        c->state     = (u_char*)block_alloc(sizeof(struct adpcm_state));
        c->state_len = sizeof(struct adpcm_state);
        memcpy(c->state, encoder_state, sizeof(struct adpcm_state));

        /* Fix coded state for byte ordering */
	((struct adpcm_state*)c->state)->valprev = htons(((struct adpcm_state*)c->state)->valprev);
        
        samples = cs[idx].format.bytes_per_block * 8 / cs[idx].format.bits_per_sample;
        c->data     = (u_char*)block_alloc(samples / sizeof(sample)); /* 4 bits per sample */
        c->data_len = samples / sizeof(sample);
        adpcm_coder(inbuf, c->data, samples, (struct adpcm_state*)encoder_state);
        return samples / 2;
}


int
dvi_decode(u_int16 idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        int samples; 
        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < DVI_NUM_FORMATS);

	if (c->state_len > 0) {
		assert(c->state_len == sizeof(struct adpcm_state));
		memcpy(decoder_state, c->state, sizeof(struct adpcm_state));
		((struct adpcm_state*)decoder_state)->valprev = ntohs(((struct adpcm_state*)decoder_state)->valprev);
	}

        samples = cs[idx].format.bytes_per_block / sizeof(sample);
	adpcm_decoder(c->data, data, samples, (struct adpcm_state*)decoder_state);

        return samples;
}
