/*
 * FILE:     audio_util.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson / Isidor Kouvelas / Colin Perkins
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "audio_util.h"
#include "debug.h"

/* Bias control code */

typedef struct s_bias_ctl {
        sample   lta;
        uint16_t step;
        uint16_t freq;
        uint32_t age;
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

static void
remove_lta(bias_ctl *bc, sample *buf, int len, int step)
{
        int  	i, samples;
	int32_t m = 0;

	/* NB m is buffer mean value.  It's 32 bits long.  During
	 * calculation we first let m equal sum of samples and then
	 * divide.  Sum of few thousand samples (limit of len) will
	 * not exceed range of m.  */

        samples = len / step;

        if (bc->age == 0) {
                /* On first pass do expensive task of calculating and
                 * removing bias in two steps.
                 */
                for (i = 0; i < len; i += step) {
                        m += buf[i];
                }
		m /= samples;
		bc->lta = (sample)m;
                bc->age++;
                for (i = 0; i < len; i += step) {
                        buf[i] -= bc->lta;
                }
        } else {
                for (i = 0; i < len; i += step) {
                        m      += buf[i];
                        buf[i] -= bc->lta;
                }
		m /= samples;
                bc->lta -= ((bc->lta - (sample)m) / 8);
        }
}

void
bias_remove(bias_ctl *bc, sample *buf, int len)
{
        if (bc->step == 1) {
                remove_lta(bc, buf, len, 1);
        } else {
                remove_lta(bc  , buf  , len / 2, 2);
                remove_lta(bc+1, buf+1, len / 2, 2);
        }
}

/* Audio processing utility functions */

void
audio_zero(sample *buf, int len, deve_e type)
{
	assert(len>=0);
	switch(type) {
	case DEV_PCMU:
		memset(buf, PCMU_AUDIO_ZERO, len);
		break;
	case DEV_PCMA:
		memset(buf, PCMA_AUDIO_ZERO, len);
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
	int i, tmp;

        for(i = 0; i < len; i++) {
                tmp = dst[i] + src[i];
                if (tmp > 32767) {
                        tmp = 32767;
                } else if (tmp < -32768) {
                        tmp = -32768;
                }
                dst[i] = tmp;
        }
}

#ifdef WIN32

/* mmx_present is (C) Intel 1998 */
BOOL
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
 * is marginally better than the x86 assembler generated by my hand [oh].
 */

void
audio_mix_mmx(sample *dst, sample *src, int len)
{
        int tmp, endq, i;

        /* The end of where we can do quad sample addition */
        endq  = ((len * sizeof(short)) / 8 - 1) * 8 ;
	if (endq > 0) {
	/* Order of these instructions is crucial for performance */
		__asm {
			mov esi, 0
			mov eax, dst
			mov ebx, src
			jmp START
LOOP_L1:
			add    esi, 8
START:
			movq   mm0, [eax + esi]
			paddsw mm0, [ebx + esi]
			movq  [eax + esi], mm0
			cmp    esi, endq
			jb     LOOP_L1
			emms
		}
		endq += 8;
	} else {
		endq = 0;
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

#define ENERGY_CALC_STEP	         1
uint16_t
audio_avg_energy(sample *buf, uint32_t samples, uint32_t channels)
{
        register uint32_t e1, e2;
        register sample *buf_end = buf + samples;

        assert (channels > 0);
        e1 = e2 = 0;
        switch (channels) {
        case 1:
                while(buf < buf_end) {
                        e1  += abs(*buf);
                        buf += ENERGY_CALC_STEP;
                }
                break;
        case 2:
                /* SIMD would improve this */
                while(buf < buf_end) {
                        e1 += abs(*buf++);
                        e2 += abs(*buf);
                        buf += ENERGY_CALC_STEP*channels - 1;
                }
                e1 = max(e1, e2);
                samples /= channels; /* No. of samples to no. of sampling intervals */
        }

        /* Return mean sampled energy:
         * no. of sampling points = samples/ENERGY_CALC_STEP;
         */
        return (uint16_t)(e1*ENERGY_CALC_STEP/samples);
}

sample
audio_abs_max(sample *buf, uint32_t samples)
{
        uint32_t i;
        sample   max;

        max = 0;
        for(i = 0; i < samples; i++) {
                if (abs(buf[i]) > max) {
                        max = abs(buf[i]);
                }
        }
        return max;
}

void
audio_scale_buffer(sample *buf, int len, double scale)
{
        int i;
        for(i = 0; i < len; i++) {
                buf[i] = (sample)((double)buf[i] * scale);
        }
}

void
audio_blend(sample *from, sample *to, sample *out, int samples, int channels)
{
        int i;
        int32_t tmp, sf, m;

        m = samples / channels;
        for (i = 0; i < samples; i++) {
                sf   = i / channels;
                tmp  = from[i] * (m - sf) + to[i] * sf;
                tmp /= m;
                out[i] = (sample)tmp;
        }
}
