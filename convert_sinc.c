/*
 * FILE:    convert_sinc.c
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
#include "convert_sinc.h"
#include "convert_util.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

#include <math.h>

/* Fixed Point Sinc Interpolation Conversion ****************************************/

/* Using integer maths to reduce cost of type conversion.  SINC_SCALE is scaling factor
 * used in filter coefficients == (1 << SINC_ROLL)
 */

#define SINC_SCALE 4096
#define SINC_ROLL    12

/* Integer changes between 2 and 6 times 8-16-24-32-40-48 */

#define SINC_MAX_CHANGE 7
#define SINC_MIN_CHANGE 2

/* Theoretically we want an infinite width filter, instead go for
 * limited number of cycles 
*/

#define SINC_CYCLES     4

static int32 *upfilter[SINC_MAX_CHANGE], *downfilter[SINC_MAX_CHANGE];

int 
sinc_startup (void)
{
        double dv, ham;
        int m, k, c, w;

        /* Setup filters, because we are truncating sinc fn use
         * Hamming window to smooth artefacts.  
         */
        for (m = SINC_MIN_CHANGE; m < SINC_MAX_CHANGE; m++) {
                w = 2 * m * SINC_CYCLES + 1;
                c = w/2;
                upfilter[m]   = (int32*)xmalloc(sizeof(int32) * w);
                downfilter[m]   = (int32*)xmalloc(sizeof(int32) * w);
                for (k = -c; k <= +c; k++) {
                        if (k != 0) {
                                dv = sin(M_PI * k / m) / (M_PI * k / (double)m);
                        } else {
                                dv = 1.0;
                        }
                        ham = 0.54 + 0.46 * cos(2.0*k*M_PI/ (double)w);
                        upfilter[m][k + c]   =  (int32)(SINC_SCALE * dv *ham);
                        downfilter[m][k + c] =  (int32)(2.0 * ham * SINC_SCALE * dv / (double)m);
                }
        }
        return TRUE;
}

void
sinc_shutdown (void)
{
        int i;

        xmemchk();
        for (i = SINC_MIN_CHANGE; i < SINC_MAX_CHANGE; i++) {
                xfree(upfilter[i]);
                xfree(downfilter[i]);
        }
}

struct s_filter_state;

static void sinc_upsample_mono   (struct s_filter_state *s, 
                                  sample *src, int src_len, 
                                  sample *dst, int dst_len);
static void sinc_upsample_stereo (struct s_filter_state *s, 
                                  sample *src, int src_len, 
                                  sample *dst, int dst_len);

static void sinc_downsample_mono   (struct s_filter_state *s, 
                                    sample *src, int src_len, 
                                    sample *dst, int dst_len);
static void sinc_downsample_stereo (struct s_filter_state *s, 
                                    sample *src, int src_len, 
                                    sample *dst, int dst_len);

typedef void (*sinc_cf)(struct s_filter_state *s, sample *src, int src_len, sample *dst, int dst_len);

typedef struct s_filter_state {
        int32  *filter;
        u_int16 taps;
        sample *hold_buf;     /* used to hold over samples from previous round. */
        u_int16 hold_bytes;
        sinc_cf fn;           /* function to be used */
        u_int16 scale;        /* ratio of sampling rates */
} filter_state_t;

typedef struct {
        int steps;            /* Number of conversion steps = 1 or 2 */
        filter_state_t fs[2]; /* Filter states used for each step    */
} sinc_state_t;

static void
sinc_init_filter(filter_state_t *fs, int channels, int src_rate, int dst_rate)
{
        if (src_rate < dst_rate) {
                assert(dst_rate / src_rate < SINC_MAX_CHANGE);
                fs->scale  = dst_rate/src_rate;
                fs->filter = upfilter[fs->scale];
                fs->taps   = 2 * SINC_CYCLES * (dst_rate/src_rate) + 1;
                fs->hold_bytes = fs->taps * channels * sizeof(sample);
                fs->hold_buf   = (sample*)block_alloc(fs->hold_bytes);
                switch(channels) {
                case 1:   fs->fn = sinc_upsample_mono;   break;
                case 2:   fs->fn = sinc_upsample_stereo; break;
                default:  abort();
                }                        
        } else if (src_rate > dst_rate) {
                assert(src_rate / dst_rate < SINC_MAX_CHANGE);
                fs->scale  = src_rate/dst_rate;
                fs->filter = downfilter[fs->scale];
                fs->taps   = 2 * SINC_CYCLES * (src_rate/dst_rate) + 1;
                fs->hold_bytes = fs->taps * channels * sizeof(sample);
                fs->hold_buf = (sample*)block_alloc(fs->hold_bytes);
                switch(channels) {
                case 1:   fs->fn = sinc_downsample_mono;   break;
                case 2:   fs->fn = sinc_downsample_stereo; break;
                default:  abort();
                }                        
        }
        memset(fs->hold_buf, 0, fs->hold_bytes);
}

static void
sinc_free_filter(filter_state_t *fs)
{
        block_free(fs->hold_buf, fs->hold_bytes);
}

int 
sinc_create (const converter_fmt_t *cfmt, u_char **state, u_int32 *state_len)
{
        sinc_state_t *s;
        int denom, steps;
        
        if ((cfmt->from_freq % cfmt->to_freq) != 0 &&
            (cfmt->to_freq % cfmt->from_freq) != 0) {
                debug_msg("Integer rate conversion supported only\n");
                return FALSE;
        }

        steps    = conversion_steps(cfmt->from_freq, cfmt->to_freq);        
        s        = (sinc_state_t*) xmalloc(sizeof(sinc_state_t));
        memset(s, 0, sizeof(sinc_state_t));
        s->steps = steps;

        switch(s->steps) {
        case 1:
                sinc_init_filter(s->fs, cfmt->from_channels, cfmt->from_freq, cfmt->to_freq);
                break;
        case 2:
                denom = gcd(cfmt->from_freq, cfmt->to_freq);
                sinc_init_filter(s->fs,     cfmt->from_channels, cfmt->from_freq, denom);
                sinc_init_filter(s->fs + 1, cfmt->from_channels, denom,           cfmt->to_freq);                
                break;
        }
        *state     = (u_char*)s;
        *state_len = sizeof(sinc_state_t);
        return TRUE;
}

void 
sinc_destroy (u_char **state, u_int32 *state_len)
{
        int i;

        sinc_state_t *s = (sinc_state_t*)*state;

        assert(*state_len == sizeof(sinc_state_t));
        
        for(i = 0; i < s->steps; i++) {
                sinc_free_filter(&s->fs[i]);
        }
        xfree(s);
        *state     = NULL;
        *state_len = 0;
}

void
sinc_convert (const converter_fmt_t *cfmt, 
              u_char *state, 
              sample* src_buf, int src_len, 
              sample *dst_buf, int dst_len)
{
        sinc_state_t *s;
        int channels;
        sample *tmp_buf;
        int     tmp_len;

        channels = cfmt->from_channels;

        s = (sinc_state_t*)state;

        if (cfmt->from_channels == 2 && cfmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                if (s->steps) {
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
        
        switch(s->steps) {
        case 1:
                assert(s->fs[0].fn);
                        s->fs[0].fn(&s->fs[0], src_buf, src_len, dst_buf, dst_len);
                break;
        case 2:
                /* first step is downsampling */
                tmp_len  = src_len / s->fs[0].scale;
                tmp_buf = (sample*)block_alloc(sizeof(sample) * tmp_len);
                assert(s->fs[0].fn);
                assert(s->fs[1].fn);

                s->fs[0].fn(&s->fs[0], src_buf, src_len, tmp_buf, tmp_len);
                s->fs[1].fn(&s->fs[1], tmp_buf, tmp_len, dst_buf, dst_len);
                block_free(tmp_buf, tmp_len * sizeof(sample));
        }
        
        if (cfmt->from_channels == 1 && cfmt->to_channels == 2) {
                /* sample rate change before mono-> stereo */
                if (s->steps) {
                        /* in place needed */
                        converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
                } else {
                        /* this is our only conversion here */
                        converter_change_channels(src_buf, src_len, 1, dst_buf, dst_len * 2, 2);
                }
        }
}


/* Here begin the conversion functions... A quick word on the
 * principle of operation.  We use fixed point arithmetic to save time
 * converting to floating point and back again.  All of the
 * conversions take place using a sample buffer called work_buf.  It's
 * allocated at the start of each conversion cycle, but it is done
 * with a memory re-cycler - block_alloc, rather than malloc.  We copy
 * the incoming samples into workbuf together with samples held over
 * from previous frame.  This allows for variable block sizes to be
 * converted and makes the maths homegenous.  In an earlier attempt we
 * did not use work_buf and broke operation up across boundary between
 * incoming samples and held-over samples, but this was fiddly coding
 * and was hard to debug.
 */

static void 
sinc_upsample_mono (struct s_filter_state *fs, 
                    sample *src, int src_len, 
                    sample *dst, int dst_len)
{
        sample *work_buf, *ss, *sc, *se, *d, *de;
        int     work_len;
        int32   t;
        int32  *h, *hc, *he, half_width;

        work_len = src_len + fs->taps - 1;
        work_buf = (sample*)block_alloc(sizeof(sample)*work_len);
        
        assert(fs->hold_bytes == fs->taps * sizeof (sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
        memcpy(work_buf + fs->hold_bytes/sizeof(sample), src, src_len * sizeof(sample));
        
        /* Save last samples in src into hold_buf for next time */
        memcpy(fs->hold_buf, src + src_len - fs->hold_bytes / sizeof(sample), fs->hold_bytes);

        d  = dst;
        de = dst + dst_len;
        ss = work_buf;
        se = work_buf + work_len;
        he = fs->filter + fs->taps;
        h = fs->filter;
        half_width = fs->taps / 2; 

        while (d < de) {
                hc = h;
                t  = 0;
                sc = ss;
                while (hc < he) {
                        t += (*hc) * (*sc);
                        hc += fs->scale;
                        sc++;
                }

                t = t >> SINC_ROLL;
                if (t > 32767) {
                        *d = 32767;
                } else if (t < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample)t;
                }

                d++;
                ss++;

                h  = fs->filter + fs->scale - 1;
                while (h != fs->filter) {
                        hc = h;
                        t = 0;
                        sc = ss;
                        while (hc < he) {
                                t  += (*hc) * (*sc);
                                hc += fs->scale;
                                sc++;
                        }
                        t = t >> SINC_ROLL;
                        if (t > 32767) {
                                *d = 32767;
                        } else if (t < -32768) {
                                *d = -32768;
                        } else {
                                *d = (sample)t;
                        }
                        d++;
                        h--;
                }
        }
        assert(d == de);
/*        assert(sc == se);  */
        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}

#ifdef BRUTE_FORCE
static void
sinc_upsample_mono(struct s_filter_state *fs,
                   sample *src, int src_len,
                   sample *dst, int dst_len)
{
        sample *work_buf;
        int32 i,j, t, work_len;

        work_len = (src_len + fs->taps - 1) * fs->scale;
        work_buf = (sample*)block_alloc(work_len * sizeof(sample));
        memset(work_buf, 0, sizeof(sample) * work_len);

        /* Transfer samples into workspace */
        for (i = 0; i < (int32)(fs->hold_bytes/sizeof(sample)); i++) {
                work_buf[i * fs->scale] = fs->hold_buf[i];
        }
        j = i;
        for (i = 0; i < src_len; i++) {
                work_buf[(i+j) * fs->scale] = src[i];
        }

        /* Copy hold over */
        memcpy(fs->hold_buf, src + src_len - fs->hold_bytes/sizeof(sample), fs->hold_bytes);
        
        for (i = 0; i < dst_len; i++) {
                t = 0;
                for (j = 0; j < fs->taps; j++) {
                        t += work_buf[i + j] * fs->filter[j];
                }
                t >>= SINC_ROLL;
                dst[i] = t;
        }
}
#endif /* BRUTE_FORCE */

static void 
sinc_upsample_stereo (struct s_filter_state *fs, 
                      sample *src, int src_len, 
                      sample *dst, int dst_len)
{
        sample *work_buf, *ss, *sc, *se, *d, *de;
        int     work_len;
        int32   t0, t1;
        int32  *h, *hc, *he, half_width;

        work_len = src_len + fs->taps;
        work_buf = (sample*)block_alloc(sizeof(sample)*work_len);

        assert(fs->hold_bytes == 2 * fs->taps * sizeof(sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
        memcpy(work_buf + fs->hold_bytes / sizeof(sample), src, src_len * sizeof(sample));
        
        /* Save last samples in src into hold_buf for next time */
        memcpy(fs->hold_buf, src + src_len - fs->hold_bytes / sizeof(sample), fs->hold_bytes);

        d  = dst;
        de = dst + dst_len;
        ss = work_buf;
        se = work_buf + work_len;
        he = fs->filter + fs->taps;

        half_width = fs->taps / 2; 

        while (d < de) {
                *d = (sample)(*(ss+half_width));
                ss++;
                d++;
                *d = (sample)(*(ss+half_width));
                ss++;
                d++;
                h  = fs->filter + fs->scale - 1;
                while (h != fs->filter) {
                        hc = h;
                        t0 = t1 = 0;
                        sc = ss;
                        while (hc < he) {
                                t0 += (*hc) * (*sc);
                                sc++;
                                t1 += (*hc) * (*sc);
                                sc++;
                                hc++;
                        }

                        t0 = t0 >> SINC_ROLL;
                        if (t0 > 32767) {
                                *d = 32767;
                        } else if (t0 < -32768) {
                                *d = -32768;
                        } else {
                                *d = (sample)t0;
                        }
                        d++;
                        
                        t1 = t1 >> SINC_ROLL;
                        if (t1 > 32767) {
                                *d = 32767;
                        } else if (t1 < -32768) {
                                *d = -32768;
                        } else {
                                *d = (sample)t1;
                        }
                        d++;
                        h--;
                }
        }
        assert(d == de);
        assert(sc == se);
        block_free(work_buf, work_len);
        xmemchk();
}

static void
sinc_downsample_mono(struct s_filter_state *fs,
                      sample *src, int src_len,
                      sample *dst, int dst_len)
{
        int32 *hc, *he, t, work_len;

        sample *work_buf, *ss, *sc, *de, *d;

        work_len = src_len + fs->taps;
        work_buf = (sample*)block_alloc(work_len * sizeof(sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
        memcpy(work_buf + fs->hold_bytes / sizeof(sample), src, src_len * sizeof(sample));
        /* Save last samples in src into hold_buf for next time */
        memcpy(fs->hold_buf, src + src_len - fs->hold_bytes / sizeof(sample), fs->hold_bytes);

        d  = dst;
        de = dst + dst_len;
        sc = ss = work_buf;
        he      = fs->filter + fs->taps;

        while (d != de) {
                t = 0;
                hc = fs->filter;
                while(hc < he) {
                        t += (*sc) * (*hc);
                        sc++;
                        hc++;
                }
                t = t >> SINC_ROLL;
                if (t > 32767) {
                        *d = 32767;
                } else if (t < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample) t;
                }
                d++;
                ss += fs->scale;
                sc = ss;

        }

        assert(d  == dst + dst_len);
        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}

static void
sinc_downsample_stereo(struct s_filter_state *fs,
                       sample *src, int src_len,
                       sample *dst, int dst_len)
{
        int32 *hc, *he, t0, t1, work_len;

        sample *work_buf, *ss, *sc, *se, *d;

        work_len = src_len + fs->taps - fs->scale;
        work_buf = (sample*)block_alloc(work_len * sizeof(sample));

        /* Get samples into work_buf */
        memcpy(work_buf, fs->hold_buf, fs->hold_bytes);
        memcpy(work_buf + fs->hold_bytes / sizeof(sample), src, src_len * sizeof(sample));
        
        /* Save last samples in src into hold_buf for next time */
        memcpy(fs->hold_buf, src + src_len - fs->hold_bytes / sizeof(sample), fs->hold_bytes);

        d  = dst;
        sc = ss = work_buf;
        se      = work_buf + work_len;
        he      = fs->filter + fs->taps;

        while (sc < se) {
                t0 = t1 = 0;
                hc = fs->filter;
                sc = ss;
                while(hc < he) {
                        t0 += (*sc) * (*hc);
                        sc++;
                        t1 += (*sc) * (*hc);
                        sc++;
                        hc++;
                }

                t0 = t0 >> SINC_ROLL;
                if (t0 > 32767) {
                        *d = 32767;
                } else if (t0 < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample) t0;
                }
                d++;

                t1 = t1 >> SINC_ROLL;
                if (t1 > 32767) {
                        *d = 32767;
                } else if (t1 < -32768) {
                        *d = -32768;
                } else {
                        *d = (sample) t1;
                }
                d++;

                ss += fs->scale;
        }

        assert(sc == se);
        assert(d  == dst + dst_len);
        block_free(work_buf, work_len * sizeof(sample));
        xmemchk();
}


