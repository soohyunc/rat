/*
 * FILE:    convert_extra.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "converter_types.h"
#include "convert_extra.h"
#include "convert_util.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

/* Extrusion *************************************************************
 * This is the cheap and nasty, for upsampling we drag out samples and 
 * downsamping we just subsample and suffer aliasing effects (v. dumb).
 */

typedef void (*extra_cf)(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len);

typedef struct {
        short scale;
        int   steps;
        sample *tmp_buf;
        short   tmp_len;
        extra_cf convert_f;
} extra_state_t;

static void
extra_upsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len)
{
        register short *sp, *dp;
        register int dstep, loop;

        sp = src + offset;
        dp = dst + offset;
        dstep = channels * dst_len / src_len;

        loop = min(dst_len / dstep, src_len / channels);
#ifdef DEBUG_CONVERT
        debug_msg("loop %d choice (%d, %d)\n", loop, dst_len/dstep, src_len/channels);
#endif
        dstep /= channels;
        while(loop--) {
                switch(dstep) {                   /* Duff's Device */
                case 6: *dp = *sp; dp+=channels;
                case 5: *dp = *sp; dp+=channels;
                case 4: *dp = *sp; dp+=channels;
                case 3: *dp = *sp; dp+=channels;
                case 2: *dp = *sp; dp+=channels;
                case 1: *dp = *sp; dp+=channels;
                }
                sp += channels;
        }
}

static void
extra_downsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len)
{
        register short *sp, *dp;
        register int src_step, loop;

        src_step = channels * src_len / dst_len;
        sp = src + offset;
        dp = dst + offset;

        loop = min(src_len / src_step, dst_len / channels);

        while(loop--) {
                *dp = *sp;
                dp += channels;
                sp += src_step;
        }
}

static void 
extra_init_state(extra_state_t *e, int src_freq, int dst_freq)
{
        if (src_freq > dst_freq) {
                e->convert_f = extra_downsample;
                e->scale     = src_freq / dst_freq;
        } else if (src_freq < dst_freq) {
                e->convert_f = extra_upsample;
                e->scale     = dst_freq / src_freq; 
        }
        e->tmp_buf = NULL;
        e->tmp_len = 0;
}

int 
extra_create (const converter_fmt_t *cfmt, u_char **state, u_int32 *state_len)
{
        extra_state_t *e;
        int denom, steps;

        if ((cfmt->from_freq % cfmt->to_freq) != 0 &&
            (cfmt->to_freq % cfmt->from_freq) != 0) {
                debug_msg("Integer rate conversion supported only\n");
                return FALSE;
        }
        
        steps    = conversion_steps(cfmt->from_freq, cfmt->to_freq);
        e        = (extra_state_t*)xmalloc(steps * sizeof(extra_state_t));
        e->steps = steps;

        switch(e->steps) {
        case 1:
                extra_init_state(e, cfmt->from_freq, cfmt->to_freq);
                break;
        case 2:
                denom = gcd(cfmt->from_freq, cfmt->to_freq);
                extra_init_state(e, cfmt->from_freq, denom);
                extra_init_state(e + 1, denom, cfmt->to_freq);                
                break;
        }

        *state     = (u_char*)e;
        *state_len = steps * sizeof(extra_state_t);
         
        return TRUE;
}

void
extra_convert (const converter_fmt_t  *cfmt, u_char *state, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        extra_state_t *e;
        int i, channels;

        channels = cfmt->from_channels;
        e = (extra_state_t*)state;

        if (cfmt->from_channels == 2 && cfmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                if (e->steps) {
                        /* inplace conversion needed */
                        converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1); 
                        src_len /= 2;
                } else {
                        /* this is only conversion */
                        converter_change_channels(src_buf, src_len, 2, dst_buf, dst_len, 1);
                }
                channels = 1;
        } else if (cfmt->from_channels == 1 && cfmt->to_channels == 2) {
                dst_len /= 2;
        }
        
        switch(e->steps) {
        case 1:
                assert(e[0].convert_f);
                for(i = 0; i < channels; i++) {
                        e[0].convert_f(i, channels, src_buf, src_len, dst_buf, dst_len);
                }
                break;
        case 2:
                /* first step is always downsampling for moment */
                if (e->tmp_buf == NULL) {
                        e->tmp_len  = src_len / e->scale;
                        e->tmp_buf = (sample*)xmalloc(sizeof(sample) * e->tmp_len);
                }
                assert(e[0].convert_f);
                assert(e[1].convert_f);

                for(i = 0; i < channels; i++)
                        e[0].convert_f(i, channels, src_buf, src_len, e->tmp_buf, e->tmp_len);
                for(i = 0; i < channels; i++)
                        e[1].convert_f(i, channels, e->tmp_buf, e->tmp_len, dst_buf, dst_len);
                break;
        }
        
        if (cfmt->from_channels == 1 && cfmt->to_channels == 2) {
                /* sample rate change before mono-> stereo */
                if (e->steps) {
                        /* in place needed */
                        converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
                } else {
                        /* this is our only conversion here */
                        converter_change_channels(src_buf, src_len, 1, dst_buf, dst_len * 2, 2);
                }
        }
}

static void
extra_state_free(extra_state_t *e)
{
        if (e->tmp_buf) {
                xfree(e->tmp_buf);
                e->tmp_buf = NULL;
                e->tmp_len = 0;
        }
}

void
extra_destroy (u_char **state, u_int32 *state_len)
{
        int i;

        extra_state_t *e = (extra_state_t*)*state;

        assert(*state_len == e->steps * sizeof(extra_state_t));

        for(i = 0; i < e->steps; i++) {
                extra_state_free(e + i);
        }
        xfree(e);
        *state     = NULL;
        *state_len = 0;
}
