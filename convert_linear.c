/*
 * FILE:    convert_linear.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-9 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "converter_types.h"
#include "convert_linear.h"
#include "convert_util.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

/* Linear Interpolation Conversion *******************************************/

struct s_li_state;
typedef void (*inter_cf)(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len, struct s_li_state *s);

typedef struct s_li_state {
        int      steps;
        int      scale;
        sample  *tmp_buf;
        int      tmp_len;
        sample  *last;
        int      last_len;
        inter_cf convert_f;
} li_state_t;

static void 
linear_upsample(int offset, int channels, 
                sample *src, int src_len, 
                sample *dst, int dst_len, 
                li_state_t *l)
{
        register int r, loop;
        register short *sp, *dp;
        short *last = l->last + offset;

        sp = src + offset;
        dp = dst + offset;
        
        loop = min(src_len/channels, dst_len/(channels*l->scale));

        switch (l->scale) {
        case 6:
                while(loop--) {
                        register int il, ic;
                        il = *last; ic = *sp;
                        r = 5 * il + 1 * ic; r /= 6; *dp = (sample)r; dp += channels;
                        r = 4 * il + 2 * ic; r /= 6; *dp = (sample)r; dp += channels;
                        r = 3 * il + 3 * ic; r /= 6; *dp = (sample)r; dp += channels;
                        r = 2 * il + 4 * ic; r /= 6; *dp = (sample)r; dp += channels;
                        r = 1 * il + 5 * ic; r /= 6; *dp = (sample)r; dp += channels;
                        *dp = (sample)ic; dp += channels;
                        last = sp;
                        sp += channels;
                }
                break;
        case 5:
                while(loop--) {
                        register int il, ic;
                        il = *last; ic = *sp;
                        r = 4 * il + 1 * ic; r /= 5; *dp = (sample)r; dp += channels;
                        r = 3 * il + 2 * ic; r /= 5; *dp = (sample)r; dp += channels;
                        r = 2 * il + 3 * ic; r /= 5; *dp = (sample)r; dp += channels;
                        r = 1 * il + 4 * ic; r /= 5; *dp = (sample)r; dp += channels;
                        *dp = (sample)ic; dp += channels;
                        last = sp;
                        sp  += channels;
                }
                break;
        case 4:
                while(loop--) {
                        register int il, ic;
                        il = *last; ic = *sp;
                        r = 3 * il + 1 * ic; r /= 4; *dp = (sample)r; dp += channels;
                        r = 2 * il + 2 * ic; r /= 4; *dp = (sample)r; dp += channels;
                        r = 1 * il + 3 * ic; r /= 4; *dp = (sample)r; dp += channels;
                        *dp = (sample)ic; dp += channels;
                        last = sp;
                        sp  += channels;
                }
                break;
        case 3:
                while(loop--) {
                        register int il, ic;
                        il = *last; ic = *sp;
                        r = 2 * il + 1 * ic; r /= 3; *dp = (sample)r; dp += channels;
                        r = 1 * il + 2 * ic; r /= 3; *dp = (sample)r; dp += channels;
                        *dp = (sample)ic; dp += channels;
                        last = sp;
                        sp  += channels;
                }
                break;
        case 2:
                while(loop--) {
                        register int il, ic;
                        il = *last; ic = *sp;
                        r = il + ic; r /= 2; *dp = (sample)r; dp += channels;
                        *dp = (sample)ic; dp += channels;
                        last = sp;
                        sp  += channels;
                }
                break;
        default:
                assert(0); /* Should never get here */
        }
        l->last[offset] = src[src_len - channels + offset];
}

static void
linear_downsample(int offset, int channels, 
                  sample *src, int src_len, 
                  sample *dst, int dst_len, 
                  li_state_t *l)
{
        register int loop, r, c, lim;
        register short *sp, *dp;

        loop = min(src_len / (channels * l->scale), dst_len / channels);
        sp = src + offset;
        dp = dst + offset;
        lim = l->scale - 1;
        while(loop--) {
                r  = (int)*sp; sp+=channels;
                c  = lim;
                while(c--) {
                        r += (int)*sp; sp+=channels;
                } 
                r /= l->scale;
                *dp = (short)r;
                dp+= channels;
        }
}

static void
linear_init_state(li_state_t *l, int channels, int src_freq, int dst_freq)
{
        if (src_freq > dst_freq) {
                l->scale = src_freq / dst_freq;
                l->last_len = 0;
                l->convert_f = linear_downsample;
        } else if (src_freq < dst_freq) {
                l->scale = dst_freq / src_freq;
                l->last_len = channels;
                l->convert_f = linear_upsample;
                l->last = (sample*)xmalloc(sizeof(sample) * l->last_len);
                memset(l->last,0,sizeof(sample)*channels);
        }
}

int 
linear_create (const converter_fmt_t *cfmt, u_char **state, u_int32 *state_len)
{
        li_state_t *l;
        int denom, steps;
        
        if ((cfmt->from_freq % cfmt->to_freq) != 0 &&
            (cfmt->to_freq % cfmt->from_freq) != 0) {
                debug_msg("Integer rate conversion supported only\n");
                return FALSE;
        }
        steps    = conversion_steps(cfmt->from_freq, cfmt->to_freq);        
        l        = (li_state_t*) xmalloc(steps * sizeof(li_state_t));
        memset(l, 0 , steps * sizeof(li_state_t));
        l->steps = steps;

        switch(l->steps) {
        case 1:
                linear_init_state(l, cfmt->from_channels, cfmt->from_freq, cfmt->to_freq);
                break;
        case 2:
                denom = gcd(cfmt->from_freq, cfmt->to_freq);
                linear_init_state(l, cfmt->from_channels,     cfmt->from_freq, denom);
                linear_init_state(l + 1, cfmt->from_channels, denom, cfmt->to_freq);                
                break;
        }
        *state     = (u_char*)l;
        *state_len = steps * sizeof(li_state_t);
        return TRUE;
}

void
linear_convert (const converter_fmt_t *cfmt, 
                u_char *state, 
                sample* src_buf, int src_len, 
                sample *dst_buf, int dst_len)
{
        li_state_t *l;
        int         channels, i;

        channels = cfmt->from_channels;

        l = (li_state_t*)state;

        if (cfmt->from_channels == 2 && cfmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                if (l->steps) {
                        /* inplace conversion needed */
                        converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1); 
                        src_len /= 2;
                } else {
                        /* this is only conversion */
                        converter_change_channels(src_buf, src_len, 2, dst_buf, dst_len, 1);
                        return;
                }
                channels = 1;
        } else if (cfmt->from_channels == 1 && cfmt->to_channels == 2) {
                dst_len /= 2;
        }
        
        switch(l->steps) {
        case 1:
                assert(l[0].convert_f);
                for(i = 0; i < channels; i++) {
                        l[0].convert_f(i, channels, src_buf, src_len, dst_buf, dst_len, l);
                }
                break;
        case 2:
                /* first step is always downsampling for moment */
                if (l->tmp_buf == NULL) {
                        l->tmp_len  = src_len / l->scale;
                        l->tmp_buf = (sample*)xmalloc(sizeof(sample) * l->tmp_len);
                }
                assert(l[0].convert_f);
                assert(l[1].convert_f);

                for(i = 0; i < channels; i++)
                        l[0].convert_f(i, channels, src_buf, src_len, l->tmp_buf, l->tmp_len, l);
                for(i = 0; i < channels; i++)
                        l[1].convert_f(i, channels, l->tmp_buf, l->tmp_len, dst_buf, dst_len, l + 1);
                break;
        }
        
        if (cfmt->from_channels == 1 && cfmt->to_channels == 2) {
                /* sample rate change before mono-> stereo */
                if (l->steps) {
                        /* in place needed */
                        converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
                } else {
                        /* this is our only conversion here */
                        converter_change_channels(src_buf, src_len, 1, dst_buf, dst_len * 2, 2);
                }
        }
}

void 
linear_destroy (u_char **state, u_int32 *state_len)
{
        int i;

        li_state_t *l = (li_state_t*)*state;

        assert(*state_len == l->steps * sizeof(li_state_t));
        
        for(i = 0; i < l->steps; i++) {
                if (l[i].last)    xfree(l[i].last);
                if (l[i].tmp_buf) xfree(l[i].tmp_buf);
        }
        xfree(l);
        *state     = NULL;
        *state_len = 0;
}



