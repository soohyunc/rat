/*
 * FILE:     audio_util.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson / Isidor Kouvelas / Colin Perkins 
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
#include "audio_util.h"

#define C0 +0.46363718
#define C1 -0.92724705
#define C2 +0.46363718
#define D1 -1.9059465
#define D2 +0.9114024

#define IC0 +475
#define IC1 -950
#define IC2 +475
#define ID1 -1952
#define ID2 +933

typedef struct s_bias_ctl {
        /* for 8k pre-filtering */
        sample y1, y2;
        sample x1, x2;
        /* for rest */
        sample lta;
        u_char step;
        int    freq;
} bias_ctl;

bias_ctl *
bias_ctl_create(int channels, int freq)
{
        bias_ctl *bc = (bias_ctl*)xmalloc(channels*sizeof(bias_ctl));
        memset(bc, 0, channels*sizeof(bias_ctl));
        bc->step = channels;
        bc->freq = freq;
        return bc;
}

void
bias_ctl_destroy(bias_ctl *bc)
{
        xfree(bc);
}

__inline static void
prefilter(bias_ctl *pf, sample *buf, register int len, int step)
{
        register int y0, y1, y2;
        register int x0, x1, x2;

        y1 = pf->y1;
        y2 = pf->y2;
        x1 = pf->x1;
        x2 = pf->x2;
        
        while(len-- != 0) {
                x0 = *buf;
                y0 = (IC0 * x0 + IC1 * x1 + IC2 * x2 - ID1 * y1 - ID2 * y2) >> 10;
                *buf = y0 << 1;
                buf += step;                
                y2 = y1; y1 = y0;
                x2 = x1; x1 = x0;
        }

        pf->y1 = y1;
        pf->y2 = y2;
        pf->x1 = x1;
        pf->x2 = x2;
}

static void
remove_lta(bias_ctl *bc, sample *buf, register int len, int step)
{
        int  m, samples;
        m = 0;
        samples = len;

        while (len-- > 0) {
                m += *buf;
                *buf -= bc->lta;
                buf += step;
        }

        bc->lta -= (bc->lta - m / samples) >> 3;
}

void
bias_remove(bias_ctl *bc, sample *buf, int len)
{
        if (bc->freq == 8000) {
                if (bc->step == 1) {
                        prefilter(bc, buf, len, 1);
                } else {
                        len /= bc->step;
                        prefilter(bc  , buf  , len, 2);
                        prefilter(bc+1, buf+1, len, 2);
                }
        } else {

                if (bc->step == 1) {
                        remove_lta(bc, buf, len, 1); 
                } else {
                        remove_lta(bc  , buf  , len / 2, 2);
                        remove_lta(bc+1, buf+1, len / 2, 2);
                }
        }
} 

void
audio_zero(sample *buf, int len, deve_e type)
{
	assert(len>=0);
	switch(type) {
	case DEV_PCMU:
		memset(buf, PCMU_AUDIO_ZERO, len);
		break;
	case DEV_S8:
		memset(buf, 0, len);
		break;
	case DEV_S16:
		memset(buf, 0, 2*len);
		break;
	default:
		fprintf(stderr, "%s:%d Type not recognized", __FILE__, __LINE__);
		break;
	}
}

void
audio_mix(sample *dst, sample *src, int len)
{
	int tmp;
        sample *src_e;

        src_e = src + len;
        while (src != src_e) {
                tmp = *dst + *src++;
                if (tmp > 32767)
                        tmp = 32767;
                else if (tmp < -32768)
                        tmp = -32768;
                *dst++ = tmp;
        }
}

#ifdef WIN32

/* mmx_present is (C) Intel 1998 */
static BOOL 
mmx_present(void)
{

        BOOL retval = TRUE;

        DWORD RegEDX;

        __try {
                _asm {
                        mov eax, 1      // set up CPUID to return processor version and features

                                        // 0 = vendor string, 1 = version info, 2 = cache info

                        CPUID           // code bytes = 0fh,  0a2h
                        mov RegEDX, edx // features returned in edx
                }

        } __except(EXCEPTION_EXECUTE_HANDLER) { retval = FALSE; }


        if (retval == FALSE)
                return FALSE;           // processor does not support CPUID

        if (RegEDX & 0x800000)          // bit 23 is set for MMX technology
        {
           __try { _asm emms }          // try executing the MMX instruction "emms"

           __except(EXCEPTION_EXECUTE_HANDLER) { retval = FALSE; }

        } else {
                return FALSE;           // processor supports CPUID but does not  support MMX technology
        }

        // if retval == 0 here, it means the processor has MMX technology but

        // floating-point emulation is on; so MMX technology is unavailable

        return retval;
}

/* audio_mix_mmx is a trivial bit of mmx code but it is six times
 * quicker than best optimized C compiled that msvc offers, and msvc
 * is marginally better than the x86 assembler generated by my hand.
 */

void
audio_mix_mmx(sample *dst, sample *src, int len)
{
        int tmp, endq, i;

        /* The end of where we can do quad sample addition */
        endq  = ((len * sizeof(short)) / 4) * 4;

        __asm {
                mov esi, 0
                mov eax, dst
                mov ebx, src
                jmp START_L1
LOOP_L1:
                add esi, 8
START_L1:
                movq   mm0, [eax + esi]
                paddsw mm0, [ebx + esi]
                movq [eax + esi], mm0
                cmp             esi, endq
                jb              LOOP_L1 
                emms
        }

        for (i = endq / 2; i < len; i++) {
                tmp = src[i] + dst[i];
                if (tmp > 32767) {
                        tmp = 32767;
                } else if (tmp < -32767) {
                        tmp = -32767;
                }
                dst[i] = (short)tmp;
        }
}

#endif /* WIN32 */




