/*
 *	FILE:    parameters.c
 *	PROGRAM: RAT
 *	AUTHOR:	 O. Hodson
 *
 *	$Revision$
 *	$Date$
 *
 * Copyright (c) 1995,1998 University College London
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

#include "config.h"
#include "rat_types.h"
#include "parameters.h"
#include "audio.h"
#include "util.h"
#include "math.h"

#define SD_BINS   256
#define SD_SCALE  (32768/SD_BINS)
#define SD_COUNTS 0.25f 

typedef struct s_sd {
        u_char  sbins[SD_BINS];      /* loose measure of freq dist */
} sd_t;

#define STEP	16
#define SD_MAX_CHANNELS 5

u_int16 
avg_audio_energy(sample *buf, u_int32 samples, u_int32 channels)
{
        u_int32 e[SD_MAX_CHANNELS];
        register u_int32 i=0,j=0, step;

        memset(e, 0, sizeof(u_int32) * SD_MAX_CHANNELS);
        assert (channels > 0);

        switch (channels) {
        case 1:
                while(i<samples) {
                        (*e) += abs(*buf);
                        buf  += STEP;
                        i    += STEP;
                }
                return ((u_int16)((*e)*STEP/samples));
        default:
                /* SIMD would improve this */
                step = STEP - channels + 1;
                while(i<samples) {
                        for(j=0;j<channels;j++) {
                                *(e+j) = *buf++;
                        }
                        buf += j;
                        i++;
                }
                for(j=1;j<channels;j++) {
                        e[0] = max(e[0], e[j]);
                }
                return (u_int16)((*e)*STEP/samples);
        }
}

/* ad hoc values - aesthetically better than 0.1, 0.01, 0.001, and so on */
#define DB_BIAS     (0.005)
#define DB_BIAS_LOG (-2.3f)

int 
lin2db(u_int16 energy, double peak)
{
        float quasi_db;

        quasi_db = ( -DB_BIAS_LOG + log10(DB_BIAS+(float)energy/65535.0f) ) / -DB_BIAS_LOG;
        return (int) (peak * quasi_db);
}

sd_t *
sd_init(void)
{
	sd_t *s = (sd_t *)xmalloc(sizeof(sd_t));
        memset(s->sbins,0,SD_BINS);
	return (s);
}

int
sd(sd_t *s, int energy)
{
        int maxb,c,thresh;
        assert(energy>=0 && energy<65535);

        maxb = energy/SD_SCALE;

        c = 0;
        while(c<=maxb) {
                s->sbins[maxb]++;
                c++;
        }
                
        if (s->sbins[maxb]==255) {
                for(c=0;c<SD_BINS;c++) {
                        s->sbins[c]>>=1;
                }
        } 
          
        c = 1;
        while((c<SD_BINS) && (s->sbins[c]>SD_COUNTS*s->sbins[0])) {
                c++;
        }
        
        thresh = 5*c*SD_SCALE/4;

        if (energy>thresh) {
                return FALSE;
        } else {
                return TRUE;
        }
}
