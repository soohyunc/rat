/*
 * FILE:    repair.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
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
#include "audio_types.h"
#include "util.h"
#include "debug.h"

/* This is prototype for repair functions - */
/* Repair scheme is passed a set of pointers to sample buffers, their formats,
 * how many of them there are, the index of the buffer to be repaired and how
 * many losses have happened consecutively
 */

typedef int (*repair_f)(sample **audio_buf, 
                        const audio_format **buffer_formats, 
                        int nbufs, 
                        int missing_buf, 
                        int consec_lost);

typedef struct {
        const char *name;
        repair_f    action;
} repair_scheme;

/* Lots of room for optimization here on in - went for cleanliness + 
 * min of coding time */

/* fade in ms */
#define FADE_DURATION   320.0f

static void
fade_buffer(sample *buffer, const audio_format *fmt, int consec_lost)
{
        sample *src, *srce;
        float fade_per_sample, scale_factor;
        int samples;
        
        samples = fmt->bytes_per_block * 8 / 
                (fmt->bits_per_sample * fmt->channels);
        
        src  = buffer;
        srce = src + samples * fmt->channels;
        
        fade_per_sample = 1000.0f / 
                (FADE_DURATION * fmt->sample_rate);
        
        scale_factor = 1.0f - consec_lost * samples * fade_per_sample;
        assert(scale_factor < 1.0);
        if (scale_factor <= 0.0) {
                scale_factor = fade_per_sample = 0.0;
        }

        switch(fmt->channels) {
        case 1:
                while (src != srce) {
                        *src = (sample)(scale_factor * (float)(*src));
                        src++;
                        scale_factor -= fade_per_sample;
                }
                break;
        case 2:
                while (src != srce) {
                        *src = (sample)(scale_factor * (float)(*src));
                        src++;
                        *src = (sample)(scale_factor * (float)(*src));
                        src++;
                        scale_factor -= fade_per_sample;
                }
                break;
        }
}

static int
repetition(sample **audio_buf, const audio_format **audio_fmt, int nbufs,
           int missing, int consec_lost)
{
        if ((audio_buf[missing - 1] == NULL) || 
            (nbufs < 2)) {
                debug_msg("no prior audio\n");
                return FALSE;
        }

        memcpy(audio_buf[missing], 
               audio_buf[missing - 1], 
               audio_fmt[missing]->bytes_per_block);
        
        if (consec_lost > 0) {
                fade_buffer(audio_buf[missing], audio_fmt[missing], consec_lost);
        }

        return TRUE;
}

/* Based on 'Waveform Subsititution Techniques for Recovering Missing Speech
 *           Segments in Packet Voice Communications', Goodman, D.J., et al,
 *           IEEE Trans Acoustics, Speech and Signal Processing, Vol ASSP-34,
 *           Number 6, pages 1440-47, December 1986. (see also follow up by 
 *           Waseem 1988)
 * Equation (7) as this is cheapest.  Should stretch across multiple 
 * previous units  so that it has a chance to work at higher frequencies. 
 * Should also have option for 2 sided repair with interpolation.[oth]
 */

#define MATCH_WINDOW        20    
#define MAX_MATCH_WINDOW    64
#define MIN_PITCH            5

static u_int16 
get_match_length(sample *buf, const audio_format *fmt, u_int16 window_length)
{
        /* Only do first channel */
        float fwin[MAX_MATCH_WINDOW], fbuf[MAX_MATCH_WINDOW], fnorm, score, target = 1e6;
        sample *win_s, *win_e, *win;
        int samples = fmt->bytes_per_block * 8 / (fmt->bits_per_sample * fmt->channels);
        int norm, i, j, k, cmp_end;
        int best_match = 0; 

        assert(window_length < MAX_MATCH_WINDOW);

        /* Calculate normalization of match window */
        win_s = buf + (samples - window_length) * fmt->channels;
        win_e = buf + samples *fmt->channels;

        norm  = 1;
        win   = win_s;
        while(win != win_e) {
                norm += abs(*win);
                win += fmt->channels;
        }

        /* Normalize and store match window */
        fnorm = (float)norm;

        win = win_s;
        i = 0;
        while(win != win_e) {
                fwin[i] = ((float)(*win)) / fnorm;
                win += fmt->channels;
                i++;
        }

        /* Find best match */
        cmp_end = samples - (window_length + MIN_PITCH * (fmt->sample_rate / 1000)) * fmt->channels;

        if (cmp_end <= 0) return samples;

        for(i = 0; i < cmp_end; i += fmt->channels) {
                norm = 1;
                for(j = i, k = 0;  k < window_length; j+= fmt->channels, k++) {
                        fbuf[k] = (float)buf[j];
                        norm += abs(buf[j]);
                }
                fnorm = (float)norm;
                score = 0.0;
                for(k = 0;  k < window_length; k++) {
                        score += (float)fabs(fbuf[k]/fnorm - fwin[k]);
                }
                if (score < target) {
                        target = score;
                        best_match = i / fmt->channels;
                } 
        }
        return (u_int16)(samples - best_match - window_length);
}

static int
single_side_pattern_match(sample **audio_buf, const audio_format **audio_fmt, int nbufs,
                          int missing, int consec_lost)
{
        u_int16 match_length;
        sample *pat, *dst;
        int remain;

        if ((audio_buf[missing - 1] == NULL) || 
            (nbufs < 2)) {
                debug_msg("no prior audio\n");
                return FALSE;
        }

        match_length = get_match_length(audio_buf[missing-1], audio_fmt[missing-1], MATCH_WINDOW);

        remain = audio_fmt[missing]->bytes_per_block * 8 / (audio_fmt[missing]->bits_per_sample * audio_fmt[missing]->channels);
        pat = audio_buf[missing-1] + (remain - match_length) * audio_fmt[missing]->channels;
        dst = audio_buf[missing];
        while(remain > 0) {
                memcpy(dst, pat, match_length * audio_fmt[missing]->channels * audio_fmt[missing]->bits_per_sample / 8);
                remain -= match_length;
                dst    += match_length;
                match_length = min(remain, match_length);
        }

        if (consec_lost > 0) {
                fade_buffer(audio_buf[missing], audio_fmt[missing], consec_lost);
        }

        return TRUE;
}

static repair_scheme schemes[] = {
        {"None",          NULL},  /* Special place for scheme none - move at own peril */
        {"Repeat",        repetition},
        {"Pattern-Match", single_side_pattern_match}
};

/* General interface */

#define REPAIR_NUM_SCHEMES sizeof(schemes)/sizeof(repair_scheme)
#define REPAIR_NONE        0

#include "repair.h"

const char *
repair_get_name(u_int16 id)
{
        if (id < REPAIR_NUM_SCHEMES) return schemes[id].name;
        return schemes[REPAIR_NONE].name;
}

u_int16
repair_get_by_name(char *name)
{
        u_int16 i;
        for(i = 0; i < REPAIR_NUM_SCHEMES; i++) {
                if (!strcasecmp(name, schemes[i].name)) return i;
        }
        return REPAIR_NONE;
}

u_int16
repair_get_count()
{
        return REPAIR_NUM_SCHEMES;
}

/* RAT droppings - everything below here depends on RAT code base */

#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "session.h"
#include "channel.h"
#include "receive.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"

/* Consecutive dummies behind */
static int
count_dummies_back(rx_queue_element_struct *up)
{
        int n = 0;
	while (up->prev_ptr && up->dummy==1) {
		up = up->prev_ptr;
		n++;
	}
        return n;
}

void
repair(int repair, rx_queue_element_struct *ip)
{
	rx_queue_element_struct *pp, *np;
        const codec_format_t    *cf;
        const audio_format      *fmt[2];
        sample                  *bufs[2];
	int consec_lost;

	pp = ip->prev_ptr;
        np = ip->next_ptr;

	if (!pp) {
		debug_msg("repair - no previous unit\n");
		return;
	}
	ip->dummy = TRUE;
        
	if (!pp->native_count) {
                /* XXX should never happen */
                debug_msg("repair - previous block not yet decoded - error!\n");
                decode_unit(pp); 
        }

	if (pp->comp_data[0].id) {
		ip->comp_data[0].id = pp->comp_data[0].id;
	} else {
		fprintf(stderr, "Could not repair as no codec pointer for previous interval\n");
		return;
	}
        
        consec_lost = count_dummies_back(pp);

	if (codec_decoder_can_repair(pp->comp_data[0].id)) {
                /* Codec can do own repair */
                int success      = FALSE;
                codec_state *st  = codec_state_store_get(
                        ip->dbe_source[0]->state_store, 
                        ip->comp_data[0].id);
                assert(st);
                if (np) {
                        success = codec_decoder_repair(pp->comp_data[0].id, 
                                                       st,
                                                       (u_int16)consec_lost,
                                                       &pp->comp_data[0], 
                                                       &ip->comp_data[0], 
                                                       &np->comp_data[0]);
                } else {
                        success = codec_decoder_repair(pp->comp_data[0].id, 
                                                       st,
                                                       (u_int16)consec_lost,
                                                       &pp->comp_data[0], 
                                                       &ip->comp_data[0], 
                                                       NULL);
                }
                if (success) {
                        ip->comp_count++;
                        decode_unit(ip);
                        return;
                }
        } 
	
	assert(!ip->native_count);

        cf = codec_get_format(pp->comp_data[0].id);
        fmt[0] = fmt[1] = &cf->format;

        ip->native_size[0] = fmt[0]->bytes_per_block;
	ip->native_data[0] = (sample*)block_alloc(fmt[0]->bytes_per_block);
	ip->native_count   = 1;
        bufs[0] = pp->native_data[0];
        bufs[1] = ip->native_data[0];

        assert((unsigned)repair < REPAIR_NUM_SCHEMES);
        schemes[repair].action(bufs, fmt, 2, 1, consec_lost);
}

