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

#ifdef WIN32

/* WINDOWS ACM CONVERSION CODE **********************************************/

static HACMDRIVER hDrv;

static BOOL CALLBACK 
getPCMConverter(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
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
acm_conv_load(void)
{
     acmDriverEnum(getPCMConverter, 0L, 0L);
     if (hDrv) return TRUE;
     return FALSE;                /* Failed initialization, entry disabled in table */
}

static void 
acm_conv_unload(void)
{
        if (hDrv) acmDriverClose(hDrv, 0);
        hDrv = 0;
}

void
acm_conv_init_fmt(WAVEFORMATEX *pwfx, u_int16 nChannels, u_int16 nSamplesPerSec)
{
       pwfx->wFormatTag = WAVE_FORMAT_PCM;
       pwfx->nChannels       = nChannels;
       pwfx->nSamplesPerSec  = nSamplesPerSec;
       pwfx->nAvgBytesPerSec = nSamplesPerSec * nChannels * sizeof(sample);
       pwfx->nBlockAlign     = nChannels * sizeof(sample);
       pwfx->wBitsPerSample  = 8 * sizeof(sample);
}

static int
acm_conv_init(converter_t *c)
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
acm_conv_do(converter_t *c, sample *src_buf, int src_len, sample *dst_buf, int dst_len)
{
        ACMSTREAMHEADER ash;
        LPHACMSTREAM    lphs;

        memset(&ash, 0, sizeof(ash));
        ash.cbStruct        = sizeof(ash);
        ash.pbSrc           = (LPBYTE)src_buf;
        ash.cbSrcLength     = src_len;
        ash.pbDst           = (LPBYTE)dst_buf;
        ash.cbDstLength     = dst_len;

        lphs = (LPHACMSTREAM)c->data;

        if (acmStreamPrepareHeader(*lphs, &ash, 0) || 
            acmStreamConvert(*lphs, &ash, ACM_STREAMCONVERTF_BLOCKALIGN)) {
                memset(dst_buf, 0, dst_len * sizeof(sample));
        }
        xmemchk();
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

#define CONVERT_NONE     0
#define CONVERT_PLATFORM 1
#define CONVERT_LINEAR   2
#define CONVERT_LINEARF  3
#define CONVERT_CUBIC    4

/* In this table of converters the platform specific converters should go at the
 * beginning, before the default (and worst) linear interpolation conversion.  The
 * intension is to have a mechanism which enables/disables more complex default schemes
 * such as interpolation with filtering, cubic interpolation, etc...
 */

pcm_converter_t converter_tbl[] = {
#ifdef WIN32
        {CONVERT_PLATFORM, "MS PCM Converter", FALSE, acm_conv_load, acm_conv_unload, acm_conv_init, acm_conv_do,  acm_conv_free, sizeof(HACMSTREAM)},
#endif
        {CONVERT_LINEAR  , "Linear",           TRUE , NULL,          NULL,            linear_init, linear_convert, linear_free,   0},
        {CONVERT_NONE    , "None",             FALSE, NULL,          NULL,            NULL,        NULL,           NULL,          0}
};

converter_t *
converter_create(int from_channels, int from_freq, int to_channels, int to_freq)
{
        converter_t     *c  = NULL;
        converter_fmt_t *cf = NULL;
        pcm_converter_t *pc = converter_tbl;

        while(pc->id != CONVERT_NONE) {
                if (pc->enabled) break;
                pc++;
        }
        
        if (pc->id == CONVERT_NONE) return NULL;
        
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
        ip->native_size[ip->native_count] = sizeof(sample) * ip->native_size[ip->native_count - 1] * cf->to_channels * cf->to_freq / (cf->from_channels * cf->from_freq);
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
