/*
 * FILE:    repair.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "util.h"
#include "debug.h"

#include "audio_util.h"

#include <stdlib.h>

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

static int  codec_specific_repair_allowed = TRUE;

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
repetition(sample             **audio_buf, 
           const audio_format **audio_fmt, 
           int                  nbufs,
           int                  missing,
           int                  consec_lost)
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

/* Noise subsitution is about the worst way to repair loss.
 * It's here as a reference point, let's hope nobody uses it!
 */

static int
noise_substitution(sample             **audio_buf, 
                   const audio_format **audio_fmt, 
                   int                  nbufs,
                   int                  missing,
                   int                  consec_lost)
{
        double e[2];
        int    i, j, nsamples, step;

        if ((audio_buf[missing - 1] == NULL) || 
            (nbufs < 2)) {
                debug_msg("no prior audio\n");
                return FALSE;
        }

        step     = audio_fmt[missing - 1]->channels;
        nsamples = audio_fmt[missing - 1]->bytes_per_block / sizeof(sample);

        /* Calculate energy of each channel for previous frame */
        for(i = 0; i < step; i++) {
                e[i] = 0.0;
                for(j = i; j < nsamples; j += step) {
                        e[i] += (double)(audio_buf[missing - 1][j] * audio_buf[missing - 1][j]);
                }
                e[i] *= (double)step / (double)nsamples;
                e[i] = sqrt(e[i]);
        }        

        nsamples = audio_fmt[missing]->bytes_per_block / sizeof(sample);

        /* Fill in the noise */
        for (i = 0; i < step; i++) {
                for(j = i; j < nsamples; j += step) {
                        audio_buf[missing][j] = (sample)(e[i] * 2.0 * (drand48() - 0.5));
                }
        }

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
        {"Pattern-Match", single_side_pattern_match},
        {"Repeat",        repetition},
        {"Noise",         noise_substitution},
        {"None",          NULL}  /* Special place for scheme none - move at own peril */
};

/* General interface */

#define REPAIR_NUM_SCHEMES sizeof(schemes)/sizeof(repair_scheme)
#define REPAIR_NONE        ((REPAIR_NUM_SCHEMES) - 1)

#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "repair.h"

const char *
repair_get_name(u_int16 id)
{
        if (id < REPAIR_NUM_SCHEMES) return schemes[id].name;
        return schemes[REPAIR_NONE].name;
}

u_int16
repair_get_by_name(const char *name)
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


int
repair(int                         repair, 
       int                         consec_lost,
       struct s_codec_state_store *states,
       media_data                 *prev, 
       coded_unit                 *missing)
{
        int          src, success;
        assert((unsigned)repair < REPAIR_NUM_SCHEMES);

        if (schemes[repair].action == NULL) {
                /* Nothing to do - this must be repair scheme "none" */
                return FALSE;
        } else if (prev->nrep == 0) {
                debug_msg("No data to repair from\n");
                return FALSE;
        }

        assert(missing->state == 0);
        assert(missing->data  == 0);

        /* check if first encoding has repair routine, test does
         * not make sense with a native (raw) encoding so check
         * that first 
         */
        if (codec_specific_repair_allowed &&
            codec_is_native_coding(prev->rep[0]->id) == FALSE && 
            codec_decoder_can_repair(prev->rep[0]->id) &&
            prev->rep[0]->id == missing->id) {
                codec_state *st;

                st = codec_state_store_get(states, prev->rep[0]->id);
                success = codec_decoder_repair(prev->rep[0]->id, 
                                               st,
                                               (u_int16)consec_lost,
                                               prev->rep[0],
                                               missing,
                                               NULL);
                return success;
        }

        /* No codec specific repair exists look for waveform to repair.
         * Start at the back since further codings are appended to the
         * list.
         */
        for(src = 0; src < prev->nrep; src++) {
                if (codec_is_native_coding(prev->rep[src]->id)) {
                        const audio_format *pfmts[2];
                        audio_format        fmt;
                        coded_unit  *p;
                        u_int16      rate, channels;
                        sample      *bufs[2];

                        p = prev->rep[src];

                        /* set up missing block */
                        missing->id       = p->id;
                        missing->data     = (u_char*)block_alloc(p->data_len);
                        missing->data_len = p->data_len;

                        /* set up format */
                        codec_get_native_info(p->id, &rate, &channels);
                        fmt.encoding        = DEV_S16;
                        fmt.sample_rate     = (int)rate;
                        fmt.bits_per_sample = 16;
                        fmt.channels        = channels;
                        fmt.bytes_per_block = p->data_len;

                        /* Pointer tweaking to get data in format repair function
                         * expects.
                         */
                        pfmts[0] = &fmt;
                        pfmts[1] = &fmt;

                        bufs[0] = (sample*) p->data;
                        bufs[1] = (sample*) missing->data;

                        schemes[repair].action(bufs, pfmts, 2, 1, consec_lost);
                        return TRUE;
                }
        }
        return FALSE;
}

void
repair_set_codec_specific_allowed(int allowed)
{
        codec_specific_repair_allowed = allowed;
}

int
repair_get_codec_specific_allowed(void)
{
        return codec_specific_repair_allowed;
}
