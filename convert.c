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
 */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "convert.h"
#include "debug.h"

typedef struct s_converter {
        struct s_pcm_converter *pcm_conv;
        struct s_converter_fmt *conv_fmt;
        char                   *data;
        int                     data_len;
} converter_t;

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

        /* Differing directions of conversions means we can do in place        
         * conversion if necessary.
         */

        switch(src_channels) {
        case 1:
                s = &src[src_len - 1]; /* clumsy syntax so not to break bounds-checker in debug */
                d = &dst[dst_len - 1];
                for(i = 0; i < src_len; i++) {
                        *d-- = *s;
                        *d-- = *s--;
                }
                break;
        case 2:
                s = src;
                d = dst;
                src_len /= 2;
                for(i = 0; i < src_len; i++) {
                        t    = *s++;
                        t   += *s++;
                        t   /= 2;
                        *d++ = t;
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

static int
conversion_steps(int f1, int f2) 
{
        if (f1 == f2) return 0;

        if (gcd(f1,f2) == 8000) {
                /* Integer conversion */
                return 1;
        } 
        /* Non-integer conversion */
        return 2;
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

static int 
linear_init (converter_t *c)
{
        li_state_t *l;
        int denom;
        
        l = (li_state_t*)c->data;
        l->steps = conversion_steps(c->conv_fmt->from_freq, c->conv_fmt->to_freq);
        switch(l->steps) {
        case 1:
                linear_init_state(l, c->conv_fmt->from_channels, c->conv_fmt->from_freq, c->conv_fmt->to_freq);
                break;
        case 2:
                denom = gcd(c->conv_fmt->from_freq, c->conv_fmt->to_freq);
                linear_init_state(l, c->conv_fmt->from_channels,     c->conv_fmt->from_freq, denom);
                linear_init_state(l + 1, c->conv_fmt->from_channels, denom, c->conv_fmt->to_freq);                
                break;
        }

        return TRUE;
}

static void
linear_convert (converter_t *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        li_state_t *l;
        int         channels, i;

        channels = c->conv_fmt->from_channels;

        l = (li_state_t*)c->data;

        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
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
        } else if (c->conv_fmt->from_channels == 1 && c->conv_fmt->to_channels == 2) {
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
        
        if (c->conv_fmt->from_channels == 1 && c->conv_fmt->to_channels == 2) {
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

static void 
linear_free (converter_t *c)
{
        int i;
        li_state_t *l = (li_state_t*)c->data;
        
        for(i = 0; i < l->steps; i++) {
                if (l[i].last)    xfree(l[i].last);
                if (l[i].tmp_buf) xfree(l[i].tmp_buf);
        }
}

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

static int 
extra_init (converter_t *c)
{
        extra_state_t *e;
        int denom;

        e = (extra_state_t*) c->data;
        
        e->steps = conversion_steps(c->conv_fmt->from_freq, c->conv_fmt->to_freq);

        switch(e->steps) {
        case 1:
                extra_init_state(e, c->conv_fmt->from_freq, c->conv_fmt->to_freq);
                break;
        case 2:
                denom = gcd(c->conv_fmt->from_freq, c->conv_fmt->to_freq);
                extra_init_state(e, c->conv_fmt->from_freq, denom);
                extra_init_state(e + 1, denom, c->conv_fmt->to_freq);                
                break;
        }
         
        return TRUE;
}

static void
extra_convert (converter_t  *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len)
{
        extra_state_t *e;
        int i, channels;

        channels = c->conv_fmt->from_channels;
        e = (extra_state_t*)c->data;

        if (c->conv_fmt->from_channels == 2 && c->conv_fmt->to_channels == 1) {
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
        } else if (c->conv_fmt->from_channels == 1 && c->conv_fmt->to_channels == 2) {
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
        
        if (c->conv_fmt->from_channels == 1 && c->conv_fmt->to_channels == 2) {
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
                e->tmp_len = 0;
        }
}

static void 
extra_free (converter_t *c)
{
        int i;
        extra_state_t *e;

        e = (extra_state_t*)c->data;
        for(i = 0; i < e->steps; i++) 
                extra_state_free(e + i);
}

typedef int  (*pcm_startup)     (void);  /* converter specific one time initialization */
typedef void (*pcm_cleanup)     (void);  /* converter specific one time cleanup */
typedef int  (*pcm_conv_init_f) (converter_t *c);
typedef void (*pcm_conv_do_f)   (converter_t *c, sample* src_buf, int src_len, sample *dst_buf, int dst_len);
typedef void (*pcm_conv_free_f) (converter_t *c);

typedef struct s_pcm_converter{
        char    *name;
        u_char  enabled;
        pcm_startup     cf_start;
        pcm_cleanup     cf_clean;
        pcm_conv_init_f cf_init;
        pcm_conv_do_f   cf_convert;
        pcm_conv_free_f cf_free;
        int      data_len;
} pcm_converter_t;

/* In this table of converters the platform specific converters should go at the
 * beginning, before the default (and worst) linear interpolation conversion.  The
 * intension is to have a mechanism which enables/disables more complex default schemes
 * such as interpolation with filtering, cubic interpolation, etc...
 */

pcm_converter_t converter_tbl[] = {
#ifdef WIN32
        {
         "Microsoft Converter", 
         FALSE, 
         acm_conv_load, 
         acm_conv_unload, 
         acm_conv_init, 
         acm_convert,  
         acm_conv_free, 
         sizeof(HACMSTREAM)
        },
#endif
#ifdef SRF_GOOD
        {
         "High Quality",
         TRUE,
         srf_tbl_init,
         srf_tbl_free,
         srf_init,
         srf_convert,
         srf_free,
         2 * sizeof(srf_state_t)
        },
#endif /* SRF_GOOD */
        {
         "Intermediate Quality",
         TRUE,  
         NULL,
         NULL,
         linear_init,
         linear_convert,
         linear_free,
         2 * sizeof(li_state_t)
        },
        {
         "Low Quality",
         TRUE,
         NULL,
         NULL,
         extra_init,
         extra_convert,
         extra_free,
         2 * sizeof(extra_state_t)
        },
        { /* This must be last converter */
         "None",
         TRUE,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         0
        }
};

#define NUM_CONVERTERS sizeof(converter_tbl)/sizeof(pcm_converter_t)
#define CONVERTER_NONE (NUM_CONVERTERS - 1)

/* Index to converter_id_t mapping macros */
#define CONVERTER_ID_TO_IDX(x) (((x)>>2) - 17)
#define IDX_TO_CONVERTER_ID(x) ((x+17) << 2)

converter_t *
converter_create(converter_id_t   cid, 
                 converter_fmt_t *in_fmt)
{
        converter_t     *c  = NULL;
        converter_fmt_t *cf = NULL;
        pcm_converter_t *pc;
        u_int32          tbl_idx;
        
        tbl_idx = CONVERTER_ID_TO_IDX(cid);

        if (tbl_idx >= NUM_CONVERTERS) {
                debug_msg("Converter ID invalid\n");
                return NULL;
        }

        if (tbl_idx == CONVERTER_NONE) return NULL;

        pc = converter_tbl + tbl_idx;
        
        assert(in_fmt != NULL);

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

        memcpy(cf, in_fmt, sizeof(converter_fmt_t));

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

void         
converters_init()
{
        pcm_converter_t *pc, *end;

        pc  = converter_tbl;
        end = converter_tbl + NUM_CONVERTERS;
        while(pc != end) {
                if (pc->cf_start) pc->enabled = pc->cf_start();
                pc++;
        }
}

void
converters_free()
{
        pcm_converter_t *pc, *end;

        pc  = converter_tbl;
        end = converter_tbl + NUM_CONVERTERS;
        while(pc != end) {
                if (pc->cf_clean) pc->cf_clean();
                pc++;
        }
}

int
converter_get_details(u_int32 idx, converter_details_t *cd)
{
        if (idx < NUM_CONVERTERS) {
                cd->id   = IDX_TO_CONVERTER_ID(idx);
                cd->name = converter_tbl[idx].name;
                return TRUE;
        }
        debug_msg("Getting invalid converter details\n");
        return FALSE;
}

u_int32 
converter_get_count()
{
        return NUM_CONVERTERS;
}

__inline converter_id_t
converter_get_null_converter()
{
        return IDX_TO_CONVERTER_ID(CONVERTER_NONE);
}

#include "codec_types.h"
#include "codec.h"

int
converter_process (converter_t *c, coded_unit *in, coded_unit *out)
{
        converter_fmt_t *cf;
        u_int32          n_in, n_out;
        assert(c);
        assert(c->pcm_conv);
        assert(c->pcm_conv->cf_convert);
        assert(in->data != NULL);
        assert(in->data_len != 0);

        cf = c->conv_fmt;
        n_in  = in->data_len / sizeof(sample);
        n_out = n_in * cf->to_channels * cf->to_freq / (cf->from_channels * cf->from_freq); 

        assert(out->state     == NULL);
        assert(out->state_len == 0);
        assert(out->data      == NULL);
        assert(out->data_len  == 0);

        out->id       = codec_get_native_coding(cf->to_freq, cf->to_channels);
        out->data_len = sizeof(sample) * n_out;
        out->data     = (u_char*)block_alloc(out->data_len);

        c->pcm_conv->cf_convert(c,
                                (sample*)in->data, 
                                n_in,
                                (sample*)out->data, 
                                n_out);

        return TRUE;
}

const converter_fmt_t*
converter_get_format (converter_t *c)
{
        assert(c != NULL);
        return c->conv_fmt;
}
