/*
 * FILE:    convert.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996 University College London
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

#include <math.h>
#include <stdio.h>

#include "assert.h"
#include "rat_types.h"
#include "convert.h"
#include "util.h"

#ifdef WIN32
#include <mmreg.h>
#include <msacm.h>
#endif

typedef struct s_converter_fmt{
        u_int16 from_channels;
        u_int16 from_freq;
        u_int16 to_channels;
        u_int16 to_freq;
} converter_fmt_t;

/* Mono-Stereo Conversion ****************************************************/ 
/* Note src_len is length block in number of samples i.e
 *                                              nChannels * nSamplingIntervals 
 */

static void
converter_change_channels (sample *src, 
                           int src_len, 
                           int src_channels, 
                           sample *dst, 
                           int dst_len, 
                           int dst_channels)
{
        int i;
        sample *s, *d;
        int t;
        assert(src_channels == 1 || src_channels == 2);
        assert(dst_channels == 1 || dst_channels == 2);
        assert(dst_channels != src_channels);
        assert(src_len/src_channels == dst_len/dst_channels);
        
        /* nb we run backwards through bufs so inplace conversion
         * works if necessary.
         */

        switch(src_channels) {
        case 1:
                s = src + src_len - 1; 
                d = dst + dst_len - 1;
                for(i = 0; i < src_len; i++) {
                        *d-- = *s;
                        *d-- = *s--;
                }
                break;
        case 2:
                s = src + src_len - 1;
                d = dst + dst_len - 1;
                for(i = 0; i < src_len; i++) {
                        t    = *s--;
                        t   += *s--;
                        t   /= 2;
                        *d-- = t;
                }
                break;
        }
}

static int
gcd (int a, int b)
{
        if (b) return gcd(b, a%b);
        return a;
}

#ifdef WIN32

/* WINDOWS ACM CONVERSION CODE **********************************************/

static HACMDRIVER hDrv;

static BOOL CALLBACK 
getPCMConverter (HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
        if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CONVERTER) {
                ACMDRIVERDETAILS add;
                add.cbStruct = sizeof(ACMDRIVERDETAILS);
                if (acmDriverDetails(hadid, &add, 0)
                    || strcmp(add.szShortName,"MS-PCM")
                    || acmDriverOpen(&hDrv, hadid, 0)) return TRUE;
                return FALSE;
        }
        return TRUE;
}

static int 
acm_conv_load (void)
{
     acmDriverEnum(getPCMConverter, 0L, 0L);
     if (hDrv) return TRUE;
     return FALSE;                /* Failed initialization, entry disabled in table */
}

static void 
acm_conv_unload (void)
{
        if (hDrv) acmDriverClose(hDrv, 0);
        hDrv = 0;
}

void
acm_conv_init_fmt (WAVEFORMATEX *pwfx, u_int16 nChannels, u_int16 nSamplesPerSec)
{
       pwfx->wFormatTag      = WAVE_FORMAT_PCM;
       pwfx->nChannels       = nChannels;
       pwfx->nSamplesPerSec  = nSamplesPerSec;
       pwfx->nAvgBytesPerSec = nSamplesPerSec * nChannels * sizeof(sample);
       pwfx->nBlockAlign     = nChannels * sizeof(sample);
       pwfx->wBitsPerSample  = 8 * sizeof(sample);
}

static int
acm_conv_init (converter_t *c)
{
        LPHACMSTREAM lpa;
        WAVEFORMATEX wfxSrc, wfxDst;

        lpa = (LPHACMSTREAM)c->data;

        acm_conv_init_fmt(&wfxSrc, c->conv_fmt->from_channels, c->conv_fmt->from_freq);
        acm_conv_init_fmt(&wfxDst, c->conv_fmt->to_channels,   c->conv_fmt->to_freq);

        if (acmStreamOpen(lpa, hDrv, &wfxSrc, &wfxDst, NULL, 0L, 0L, 0L)) return FALSE;
 
        return TRUE;
}

static void
acm_convert (converter_t *c, sample *src_buf, int src_len, sample *dst_buf, int dst_len)
{
        ACMSTREAMHEADER ash;
        LPHACMSTREAM    lphs;

        memset(&ash, 0, sizeof(ash));
        ash.cbStruct        = sizeof(ash);
        ash.pbSrc           = (LPBYTE)src_buf;
        ash.cbSrcLength     = src_len * sizeof(sample);
        ash.pbDst           = (LPBYTE)dst_buf;
        ash.cbDstLength     = dst_len * sizeof(sample);

        lphs = (LPHACMSTREAM)c->data;

        if (acmStreamPrepareHeader(*lphs, &ash, 0) || 
            acmStreamConvert(*lphs, &ash, ACM_STREAMCONVERTF_BLOCKALIGN)) {
                memset(dst_buf, 0, dst_len * sizeof(sample));
        }
        return;
}

static void
acm_conv_free(converter_t *c)
{
        if (c->data) {
                xfree(c->data);
                c->data     = NULL;
                c->data_len = 0;
        }
}

#endif /* WIN32 */

/* FILTERED SAMPLE RATE CONVERSION CODE *************************************/

#define SRF_FILTW_SCALE  6 
#define SRF_NTABLES      5
#define SRF_SCALE        256

/* Tables are for M = 2, 3, 4, 5(not used), 6 */
int **srf_tbl_up;
int **srf_tbl_dn;

/* If you want better quality make tbl sizes bigger (costs more),
 * stick to multiples of these sizes otherwise converter becomes
 * a noise generator.
 */

int tbl_sz_up[] = {8, 12, 16, 20, 24};
int tbl_sz_dn[] = {8, 12, 16, 20, 24};

#define SF2IDX(x) ((x)-2)
#define IDX2SF(x) ((x)+2)

struct s_srf_filter_state;

typedef void (*srf_cf)(int     offset, 
                       int     channels, 
                       sample* src_buf, 
                       int     src_len, 
                       sample* dst_buf, 
                       int     dst_len, 
                       struct s_srf_filter_state *sf);


typedef struct s_srf_filter_state {
        short   scale;
        short   tbl_idx;
        int*    tbl;
        srf_cf  convert_f;
        sample *last;
        short   last_sz;
        short   phase;
        short   dst_freq;
} srf_filter_state_t;

typedef struct s_srf_state {
        srf_filter_state_t *fs;
        int                 steps;      /* number of steps conversion takes */
        sample             *tmp_buf;
        int                 tmp_sz;
} srf_state_t;

static int 
srf_tbl_init(void)
{
        int i,j;
        double f;
        srf_tbl_up = (int**)xmalloc(sizeof(int*)*SRF_NTABLES);
        srf_tbl_dn = (int**)xmalloc(sizeof(int*)*SRF_NTABLES);

        for(i = 0; i < SRF_NTABLES; i++) {
                srf_tbl_up[i] = (int*)xmalloc(sizeof(int)*tbl_sz_up[i]);
                srf_tbl_dn[i] = (int*)xmalloc(sizeof(int)*tbl_sz_dn[i]);
                srf_tbl_up[i][0] = SRF_SCALE;
                for(j = 1; j < tbl_sz_up[i]; j++) {
                        f = M_PI * (double)j / (double)IDX2SF(i);
                        srf_tbl_up[i][j] = (int)((double)SRF_SCALE * sin(f)/ f);
                }
                srf_tbl_dn[i][0] = (int)((double)SRF_SCALE * (1.0 / (double)(i+2))); 
                for(j = 1; j < tbl_sz_dn[i]; j++) {
                        f = M_PI * (double)j / (double)IDX2SF(i);
                        srf_tbl_up[i][j] = (int)((double)SRF_SCALE * sin(f)/ f);
                        srf_tbl_dn[i][j] = IDX2SF(i) * ((int)((double)SRF_SCALE * sin(f)/ f));
                }
        }

        return TRUE;
}

static void
srf_tbl_free (void)
{
        int i;
        for(i = 0; i < SRF_NTABLES; i++) {
                xfree(srf_tbl_up[i]);
                xfree(srf_tbl_dn[i]);
        }
        xfree(srf_tbl_up);
        xfree(srf_tbl_dn);
}

static void
srf_downsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len, srf_filter_state_t *sf)
{
        sample *src_c, *src_l, *src_r, *src_e;
        int *h_c, *h, *h_e;
        int win_sz, result;
        int src_step;

        if (sf->last == NULL) {
                sf->last_sz = dst_len * IDX2SF(sf->tbl_idx) - src_len;
                assert(sf->last_sz > 0);
                sf->last    = (sample*)xmalloc(sizeof(sample) * sf->last_sz);
                memset(sf->last, 0, sizeof(sample) * sf->last_sz);
        }

        win_sz = 2 * tbl_sz_dn[sf->tbl_idx] - 1;
        h_c = srf_tbl_dn[sf->tbl_idx];
        h_e = srf_tbl_dn[sf->tbl_idx] + tbl_sz_dn[sf->tbl_idx];
        
        src_c    = sf->last + offset + (win_sz * channels)/2;
        src_e    = sf->last + sf->last_sz + offset;     
        src_step = channels * IDX2SF(sf->tbl_idx);

        assert(win_sz * channels == sf->last_sz);
        dst   += offset;

        /* Stage 1: RHS of filter overlaps with last and current block */
        while(src_c < src_e) {
                result = 0;
                h      = h_c;
                src_l  = src_c;
                while(h != h_e) {
                       result += (*h)*(*src_l);
                       src_l  -= channels;
                       h ++;
                }
                src_r = src_c + channels;
                h     = h_c + 1;
                while(src_r < src_e) {
                       result += (*h)*(*src_r);
                       src_r  += channels;
                       h ++; 
                }
                src_r = src + offset;
                while(h<h_e) {
                       result += (*h)*(*src_r);
                       src_r  += channels;
                       h ++; 
                }
                result /= SRF_SCALE;
                *dst = (sample)result;
                dst   += channels;
                src_c += IDX2SF(sf->tbl_idx) * channels;
        }

        /* Stage 2: LHS of filter overlaps last and current */
        src_c = src + offset;
        src_e = src_c + (win_sz * channels)/2;
        
        while(src_c < src_e) {
                result = 0;
                h     = h_c;
                src_r = src_c;
                while(h_c != h_e) {
                      result += (*h) * (*src_r);
                      src_r  += channels;
                      h++;
                }
                src_l = src_c - channels;
                h     = h_c + 1;
                while(src_l > src_e) {
                      result += (*h) * (*src_l);
                      src_l  -= channels;
                      h++;
                }
                src_l = sf->last + sf->last_sz - channels + offset;
                while(h != h_e) {
                      result += (*h) * (*src_l);
                      src_l  -= channels;
                      h++;
                }
                result /= SRF_SCALE;
                *dst = (sample)result;
                dst   += channels;
                src_c += IDX2SF(sf->tbl_idx) * channels;
        }
        
        /* Stage 3: Filter completely within last block */
        src_e = src + src_len - win_sz / 2;
        while(src_c < src_e) {
                h = h_c;
                result = (*h) * (*src_c);
                h++;
                src_l = src_c + channels;
                src_r = src_c - channels;
                while(h != h_e) {
                        result += (*h) * ((*src_l) + (*src_r));
                        src_r += channels;
                        src_l -= channels;
                        h++;
                }
                *dst   = (sample)(result/SRF_SCALE);
                dst   += channels;
                src_c += IDX2SF(sf->tbl_idx) * channels;
        }
        /* we should test whether we zero last dst sample to avoid overlap
         * in mixer 
         */

}

static void
srf_upsample(int offset, int channels, sample *src, int src_len, sample *dst, int dst_len, srf_filter_state_t *sf)
{
        sample *src_c, *src_l = NULL, *src_r = NULL, *src_e = NULL;
        int win_sz, result;
        int *h_c, *h_l, *h_r, *h_e;

        UNUSED(dst_len);
        
        win_sz = 2 * tbl_sz_up[sf->tbl_idx] - 1;
        h_c = srf_tbl_up[sf->tbl_idx];
        h_e = srf_tbl_up[sf->tbl_idx] + tbl_sz_up[sf->tbl_idx];

        src_c  = sf->last + offset + (tbl_sz_up[sf->tbl_idx] * channels)/2; 
        src_e  = sf->last + win_sz * channels;
        assert(win_sz * channels == sf->last_sz);

        dst   += offset;

        /* Stage One: Right hand side of window overlaps with last buffer and current,
         *            left hand side of window completely within last.
         */
        assert(sf->phase == 0);
        
        while(src_c < src_e) {
                if (sf->phase == 0) {
                        *dst      = *src_c;
                        dst      += channels;
                        sf->phase = 1;
                }
                h_l = h_c + sf->phase;
                h_r = h_c + sf->scale - sf->phase;
                src_l = src_c;
                result = 0;
                
                while((unsigned)h_l < (unsigned)h_e) {
                        result += (*h_l) * (*src_l);
                        src_l  -= channels;
                        h_l    += sf->scale;
                }
                src_r = src_c + channels;
                assert(src_r <= src_e);
                
                while((unsigned)src_r < (unsigned)src_e && (unsigned)h_r < (unsigned)h_e) {
                        assert((unsigned)src_r < (unsigned)src_e);
                        result += (*src_r) * (*h_r);
                        src_r  += channels;
                        h_r    += sf->scale;
                }
                
                src_r = src + offset;
                while((unsigned)h_r < (unsigned)h_e) {
                        result += (*src_r) * (*h_r);
                        src_r  += channels;
                        h_r    += sf->scale;
                }

                *dst = (short)(result/SRF_SCALE);
                dst += channels;
                sf->phase++;
                if (sf->phase == sf->scale) {
                        sf->phase =0; 
                        src_c+= channels;
                }
        }

        /* Stage Two: Left hand side of window overlaps with last buffer and current,
         *            right hand side of window completely within current.
         */
        assert(sf->phase == 0);

        src_c = src   + offset;
        src_e = src_c + win_sz * channels;

        while(src_c < src_e) {
                if (sf->phase == 0) {
                        *dst      = *src_c;
                        dst      += channels;
                        sf->phase = 1;
                }
                h_l = h_c + sf->phase;
                h_r = h_c + sf->scale - sf->phase;
                src_r = src_c + channels;
                
                result = 0;
                while((unsigned)h_r < (unsigned)h_e) {
                        result += (*src_r) * (*h_r);
                        src_r  += channels;
                        h_r    += sf->scale;
                }

                src_l = src_c;
                while((unsigned)src_l >= (unsigned)src && (unsigned)h_l < (unsigned)h_e) {
                        result += (*src_l) * (*h_l);
                        src_l  -= channels;
                        h_l    += sf->scale;
                }

                src_l = sf->last + sf->last_sz - channels + offset;
                while((unsigned)h_l < (unsigned)h_e) {
                        result += (*src_l) * (*h_l);
                        src_l  -= channels;
                        h_l    ++;
                }
                
                *dst = (short)(result/SRF_SCALE);
                dst += channels;
                sf->phase++;
                if (sf->phase == sf->scale) {
                        sf->phase = 0;
                        src_c += channels;
                }
        }

        /* Stage Three: All of window within current buffer. */
        assert(sf->phase == 0);
        src_e = src + src_len - tbl_sz_up[sf->tbl_idx] * channels;

        while((unsigned)src_c < (unsigned)src_e) {
                if (sf->phase == 0) {
                        *dst = *src_c;
                        dst += channels;
                }
                src_l = src_c;
                src_r = src_c + channels;
                h_l   = h_c   + sf->phase;
                h_r   = h_c   + sf->scale - sf->phase;
                result = 0;
                while ((unsigned)h_r < (unsigned)h_e) {
                        result += (*src_l)*(*h_l) + (*src_r)*(*h_r);
                        src_l  -= channels;
                        src_r  += channels;
                        h_l    += sf->scale;
                        h_r    += sf->scale;
                }
                *dst = (short)(result / SRF_SCALE);
                dst += channels;
                sf->phase++;
                if (sf->phase == sf->scale) {
                        sf->phase = 1;
                        src_c += channels;
                }
        }

        src_c = src + src_len - win_sz;
        if (offset == 0) {
                memcpy(sf->last,src_c,win_sz);
        } else {
                src_c++;
                dst = sf->last++;
                while(src_c < src + src_len) {
                        *dst = *src_c;
                        src_c += channels;
                        dst   += channels;
                }
        }

        /* a cheat - definitely break things */
        sf->phase = 0;

        xmemchk();
}

static 
void srf_init_filter(srf_filter_state_t *sf, int src_freq, int dst_freq, int channels)
{
        if (src_freq > dst_freq) {
                sf->scale     = src_freq / dst_freq;
                sf->tbl_idx   = (SF2IDX(src_freq/dst_freq));
                sf->tbl       = srf_tbl_dn[sf->tbl_idx];
                sf->convert_f = srf_downsample;
                sf->last_sz   = 0;
                sf->last      = NULL;
                sf->phase     = 0;
                sf->dst_freq  = dst_freq;
        } else {
                sf->scale     = dst_freq / src_freq;
                sf->tbl_idx   = (SF2IDX(dst_freq/src_freq));
                sf->tbl       = srf_tbl_up[sf->tbl_idx];
                sf->convert_f = srf_upsample;
                sf->last_sz   = (2 * tbl_sz_up[sf->tbl_idx] - 1) * channels;
                sf->last      = (sample *) xmalloc (sf->last_sz * sizeof(sample));
                memset(sf->last, 0, sizeof(sample) * sf->last_sz);
                sf->phase     = 0;
                sf->dst_freq  = dst_freq;
        }
        
}

/* This code is written so that if we do non-integer rate conversion 
 * we downsample to lowest common divisor, and then upsample again.
 * 
 * This is really the wrong thing to do, we should upsample to a 
 * common multiple, then downsample, but this requires much more
 * work and memory.  The reality for RAT is that we only ever convert 
 * between multiples of 8K, so the only problem is with conversion 
 * between 32K and 48K.  For sake of argument, most computer audio 
 * hardware isn't good enough too tell either way, so we'll take the 
 * cheap ticket.
 */

static int 
srf_init (converter_t *c)
{
        srf_state_t *s;
        int denom, src_freq, dst_freq, src_channels, dst_channels;

        assert(c->conv_fmt->from_freq % 8000 == 0);
        assert(c->conv_fmt->to_freq   % 8000 == 0);
        
        if (c->conv_fmt->to_freq == c->conv_fmt->from_freq) {
                c->data     = NULL;
                c->data_len = 0;
                return TRUE;
        }

        src_freq     = c->conv_fmt->from_freq;
        src_channels = c->conv_fmt->from_channels; 
        dst_freq     = c->conv_fmt->to_freq;
        dst_channels = c->conv_fmt->to_channels; 

        denom = gcd(src_freq, dst_freq);
        s     = (srf_state_t*)c->data;

        if (denom == 8000) {
                s->fs    = (srf_filter_state_t*)xmalloc(sizeof(srf_filter_state_t));
                s->steps = 1;
                srf_init_filter(s->fs,    src_freq, dst_freq, src_channels);
        } else {
                s->fs    = (srf_filter_state_t*)xmalloc(sizeof(srf_filter_state_t) * 2);
                s->steps = 2;
                srf_init_filter(s->fs,    src_freq, denom,    src_channels);
                srf_init_filter(s->fs+ 1, denom,    dst_freq, dst_channels);
        }

        return TRUE;
}

static void
srf_convert (converter_t  *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        int channels = c->conv_fmt->from_channels;
        
        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1); 
                src_len /= 2;
                channels = 1;
        }

        if (c->data) {
                srf_state_t *s = (srf_state_t*)c->data;
                int i;
                if (s->steps == 1) {
                        for (i = 0; i < channels; i++)  {
                                s->fs[0].convert_f(i, channels, src_buf, src_len, dst_buf, dst_len, s->fs);
                        }
                } else {
                        assert(s->steps == 2);
                        if (s->tmp_buf == NULL) {
                                s->tmp_sz  = src_len * c->conv_fmt->from_freq / s->fs->dst_freq;
                                s->tmp_buf = (sample*)xmalloc(sizeof(sample) * s->tmp_sz);
                        }
                        for(i = 0; i < channels; i++) {
                                s->fs[0].convert_f(i, channels, src_buf, src_len, s->tmp_buf, s->tmp_sz/sizeof(sample), s->fs);
                        }
                        for(i = 0; i < channels; i++) {
                                s->fs[1].convert_f(i, channels, s->tmp_buf, s->tmp_sz, dst_buf, dst_len, s->fs);
                        }
                }
        }

        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
                /* sample rate change before mono-> stereo */
                converter_change_channels(dst_buf, dst_len, 1, dst_buf, dst_len * 2, 2);
        }
}

static void 
srf_free (converter_t *c)
{
        int i;
        srf_state_t *s = (srf_state_t*)c->data;
        assert(c->data_len == sizeof(srf_state_t));
        for(i = 0; i < s->steps; i++)  {
                if (s->fs[i].last) xfree(s->fs[i].last);
        }
        xfree(s->fs);
        if (s->tmp_buf) xfree(s->tmp_buf);
        xfree(s);
        c->data     = NULL;
        c->data_len = 0;
}

/* Linear Interpolation Conversion *******************************************/

static int 
linear_init (converter_t *c)
{
        UNUSED(c);
        return TRUE;
}

static void
linear_convert (converter_t  *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        UNUSED(c);
        UNUSED(src_buf);
        UNUSED(src_len);
        UNUSED(dst_buf);
        UNUSED(dst_len);
}

static void 
linear_free (converter_t *c)
{
        UNUSED(c);
}

/* Extrusion *************************************************************
 * This is the cheap and nasty, for upsampling we drag out samples and 
 * downsamping we just subsample and suffer aliasing effects (v. dumb).
 */

static int 
extra_init (converter_t *c)
{
        UNUSED(c);
        return TRUE;
}

static void
extra_convert (converter_t  *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        register short *sp, *se, *dp;
        int i, channels;

        UNUSED(dst_len);

        channels = c->conv_fmt->from_channels;
        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1); 
                src_len /= 2;
                channels = 1;
        }

        if (c->conv_fmt->from_freq < c->conv_fmt->to_freq) {
                register int lim, dup;
                lim = c->conv_fmt->to_freq / c->conv_fmt->from_freq;
                se = src_buf + src_len;
                for(i = 0; i < channels; i++) {
                        sp = src_buf + i;
                        dp = dst_buf + i;
                        while(sp < se) {
                                dup = 0;
                                while(dup < lim) {
                                        *dp = *sp;
                                        dp += channels;
                                        dup++;
                                }
                                sp += channels;
                        }
                }
        } else if (c->conv_fmt->from_freq > c->conv_fmt->to_freq) {
                register int src_step;
                src_step = channels * c->conv_fmt->from_freq / c->conv_fmt->to_freq;
                se = src_buf + src_len;
                for (i = 0; i < channels; i++) {
                        sp = src_buf + i;
                        dp = dst_buf + i;
                        while(sp < se) {
                                *dp = *sp;
                                dp += channels;
                                sp += src_step;
                        }
                }
        }
        
        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
                /* stereo->mono then sample rate change */
                converter_change_channels(src_buf, src_len, 2, src_buf, src_len / 2, 1); 
                src_len /= 2;
                channels = 1;
        }
}

static void 
extra_free (converter_t *c)
{
        UNUSED(c);
}

typedef int  (*pcm_startup)     (void);  /* converter specific one time initialization */
typedef void (*pcm_cleanup)     (void);  /* converter specific one time cleanup */
typedef int  (*pcm_conv_init_f) (converter_t *c);
typedef void (*pcm_conv_do_f)   (converter_t *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len);
typedef void (*pcm_conv_free_f) (converter_t *c);

typedef struct s_pcm_converter{
        u_char  id;
        char    *name;
        u_char  enabled;
        pcm_startup     cf_start;
        pcm_cleanup     cf_clean;
        pcm_conv_init_f cf_init;
        pcm_conv_do_f   cf_convert;
        pcm_conv_free_f cf_free;
        int      data_len;
} pcm_converter_t;

#define CONVERT_NONE     255
#define CONVERT_PLATFORM 1
#define CONVERT_SRF      2
#define CONVERT_LINEAR   3
#define CONVERT_EXTRA    4

/* In this table of converters the platform specific converters should go at the
 * beginning, before the default (and worst) linear interpolation conversion.  The
 * intension is to have a mechanism which enables/disables more complex default schemes
 * such as interpolation with filtering, cubic interpolation, etc...
 */

pcm_converter_t converter_tbl[] = {
#ifdef WIN32
        {CONVERT_PLATFORM, 
         "MS Converter", 
         FALSE, 
         acm_conv_load, 
         acm_conv_unload, 
         acm_conv_init, 
         acm_convert,  
         acm_conv_free, 
         sizeof(HACMSTREAM)},
#endif
        {CONVERT_SRF, 
         "High Quality",
         TRUE,
         srf_tbl_init,
         srf_tbl_free,
         srf_init,
         srf_convert,
         srf_free,
         sizeof(srf_state_t)},
        {CONVERT_LINEAR,
         "Intermediate Quality",
         TRUE,  
         NULL,
         NULL,
         linear_init,
         linear_convert,
         linear_free,
         0},
        {CONVERT_EXTRA,
         "Low Quality",
         TRUE,
         NULL,
         NULL,
         extra_init,
         extra_convert,
         extra_free,
         0},
        {CONVERT_NONE,
         "None",
         TRUE,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         0}
};

converter_t *
converter_create(pcm_converter_t *pc, int from_channels, int from_freq, int to_channels, int to_freq)
{
        converter_t     *c  = NULL;
        converter_fmt_t *cf = NULL;
        
        if (pc == NULL || pc->id == CONVERT_NONE) return NULL;
        
        c  = (converter_t*)xmalloc(sizeof(converter_t));

        if (c == NULL) {
                return NULL;
        }
        memset(c, 0, sizeof(converter_t));

        cf = (converter_fmt_t*)xmalloc(sizeof(converter_fmt_t));
        if (cf == NULL) {
                converter_destroy(&c); 
                return NULL;
        }

        cf->from_channels = from_channels;
        cf->from_freq     = from_freq;
        cf->to_channels   = to_channels;
        cf->to_freq       = to_freq;

        c->pcm_conv = pc;
        c->conv_fmt = cf;

        if (pc->data_len) {
                c->data     = (char*)xmalloc(pc->data_len);
                c->data_len = pc->data_len;
                memset(c->data, 0, c->data_len);
                if (c->data == NULL) {
                        converter_destroy(&c);
                        return NULL;
                }
        }

        if ((pc->cf_init) && (pc->cf_init(c) == FALSE)) {
                converter_destroy(&c);
                return NULL;
        }
        return c;
}

void 
converter_destroy(converter_t **c)
{
        if (*c == NULL) return;
        if ((*c)->pcm_conv->cf_free) (*c)->pcm_conv->cf_free(*c);
        if ((*c)->conv_fmt)          xfree((*c)->conv_fmt);
        if ((*c)->data != NULL)      xfree((*c)->data);
        xfree(*c); (*c) = NULL;
}

int
converter_format (converter_t *c, rx_queue_element_struct *ip)
{
        converter_fmt_t *cf;
        assert(c);
        assert(c->pcm_conv);
        assert(c->pcm_conv->cf_convert);
        assert(ip->native_data);
        assert(ip->native_count);
        
        cf = c->conv_fmt;
        ip->native_size[ip->native_count] = cf->to_channels * ceil((double)ip->native_size[ip->native_count - 1] * (double)cf->to_freq / (double)cf->from_freq) / cf->from_channels;
        ip->native_data[ip->native_count] = (sample*)block_alloc(ip->native_size[ip->native_count]);
        c->pcm_conv->cf_convert(c,
                                ip->native_data[ip->native_count - 1], 
                                ip->native_size[ip->native_count - 1] / sizeof(sample),
                                ip->native_data[ip->native_count], 
                                ip->native_size[ip->native_count] / sizeof(sample));
        ip->native_count++;
        return TRUE;
}

void         
converters_init()
{
        pcm_converter_t *pc = converter_tbl;
        while(pc->id != CONVERT_NONE) {
                if (pc->cf_start) pc->enabled = pc->cf_start();
                pc++;
        }
}

void
converters_free()
{
        pcm_converter_t *pc = converter_tbl;
        while(pc->id != CONVERT_NONE) {
                if (pc->cf_clean) pc->cf_clean();
                pc++;
        }
}

pcm_converter_t *
converter_get_byname(char *name)
{
        pcm_converter_t *pc;

        pc = converter_tbl;
        while(strcmp(name, pc->name) && pc->id != CONVERT_NONE) pc++;
        return pc;
}

int 
converter_get_names(char *buf, int buf_len)
{
        pcm_converter_t *pc;
        char *bp;
        int len = 0;
        
        pc = converter_tbl;
        for(;;) {
                if (pc->enabled) len += strlen(pc->name);
                if (pc->id == CONVERT_NONE) break;
                pc++;
        } 

        if (buf_len < len) return FALSE;

        pc = converter_tbl;
        bp = buf;
        for(;;) {
                if (pc->enabled) {
                        sprintf(bp, "%s/", pc->name);
                        bp += strlen(pc->name) + 1;
                } 
                if (pc->id == CONVERT_NONE) break;
                pc++;

        }

        if (bp != buf) *(bp-1) = 0;

        return TRUE;
}
