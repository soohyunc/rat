/*
 * FILE:    codec_l16.c
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
#include "codec_l16.h"

/* This is where your sanity gives in and love begins...
 * never lose your grip don't fall don't fall you'll lose it all
 * [The Cardigans, Paralyszed - like my phd by this code ;-) ]
 */ 

/* Note payload numbers are dynamic and selected so:
 * (a) we always have one codec that can be used at each sample rate and freq
 * (b) to backwards match earlier releases.
 */

static codec_format_t cs[] = {
        {"Linear-16", "L16-8K-Mono",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         122, 0, 320, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"Linear-16", "L16-8K-Stereo",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         111, 0, 640, {DEV_S16,  8000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"Linear-16", "L16-16K-Mono",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         112, 0, 320, {DEV_S16,  16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10 ms */
        {"Linear-16", "L16-16K-Stereo",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         113, 0, 640, {DEV_S16,  16000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 10 ms */
        {"Linear-16", "L16-32K-Mono",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         114, 0, 320, {DEV_S16,  32000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 5 ms */
        {"Linear-16", "L16-32K-Stereo",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         115, 0, 640, {DEV_S16,  32000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}}, /* 5 ms */
        {"Linear-16", "L16-48K-Mono",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         116, 0, 320, {DEV_S16,  48000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 3.3_ ms */
        {"Linear-16", "L16-8K-Stereo",  
         "Linear 16 uncompressed audio, please do not use wide area.", 
         117, 0, 640, {DEV_S16,  48000, 16, 2, 2 * 160 * BYTES_PER_SAMPLE}} /* 3.3_ ms */
};

#define L16_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

u_int16
l16_get_formats_count()
{
        return (u_int16)L16_NUM_FORMATS;
}

const codec_format_t *
l16_get_format(u_int16 idx)
{
        assert(idx < L16_NUM_FORMATS);
        return &cs[idx];
}

int
l16_encode(u_int16 idx, u_char *state, sample *in, coded_unit *out)
{
        int samples;
        sample *d, *de;

        assert(idx < L16_NUM_FORMATS);
        UNUSED(state);

        out->state     = NULL;
        out->state_len = 0;
        out->data      = (u_char*)block_alloc(cs[idx].mean_coded_frame_size);
        out->data_len  = cs[idx].mean_coded_frame_size;

        samples = out->data_len / 2;
        d = (sample*)out->data;
        de = d + samples;
        
        while (d != de) {
                *d = htons(*in);
                d++; in++;
        }
        return samples;
}

int
l16_decode(u_int16 idx, u_char *state, coded_unit *in, sample *out)
{
        int samples;
        sample *s = NULL, *se = NULL;
        
        assert(idx < L16_NUM_FORMATS);
        UNUSED(state);

        samples = in->data_len / BYTES_PER_SAMPLE;
        s = (sample*)in->data;
        se = s + samples;
        while(s != se) {
                *out = ntohs(*s);
                out++; s++;
        }
        return samples;
}


