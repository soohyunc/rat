/*
 * FILE:    render_3D.c
 * PROGRAM: RAT
 * AUTHORS: Marcus Iken
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1998 University College London
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
#include "assert.h"
#include "memory.h"
#include "util.h"
#include "mix.h"
#include "session.h"
#include "audio.h"
#include "receive.h"
#include "timers.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "convert.h"
#include "codec.h"
#include "parameters.h"
#include "ui.h"
#include "render_3D.h"

typedef struct s_render_3D_dbentry {
        short    azimuth;        /* lateral angle */
        short    i_time_d;       /* interaural time difference (ITD); derived from 'azimuth' */
        float    i_intensity_d;  /* interaural intensity difference (IID); derived from 'azimuth' */
        double   filter[RESPONSE_LENGTH];     /* filter used for convolution */
        double   overlap[RESPONSE_LENGTH];    /* overlap buffer due to filter operation on the mono signal */
        sample   delay[32];      /* delay buffer due to ITD in the contra-lateral channel */
} render_3D_dbentry;


render_3D_dbentry *
render_3D_init()
{
        int               i;
        render_3D_dbentry *render_3D_data;

        render_3D_data = (render_3D_dbentry *) xmalloc(sizeof(render_3D_dbentry));

        render_3D_data->azimuth = 0;
        render_3D_data->i_time_d = 0;
        render_3D_data->i_intensity_d = 0.0;
        memset(render_3D_data->filter, 0, sizeof render_3D_data->filter);
        memset(render_3D_data->overlap, 0, sizeof render_3D_data->overlap);
        memset(render_3D_data->delay, 0, sizeof render_3D_data->delay);

	render_3D_data->filter[0] = 0.063113;
	render_3D_data->filter[1] = -0.107530;
	render_3D_data->filter[2] = 0.315168;
	render_3D_data->filter[3] = 0.015218;
	render_3D_data->filter[4] = -0.300535;
	render_3D_data->filter[5] = 1.000000;
	render_3D_data->filter[6] = 0.359786;
	render_3D_data->filter[7] = -0.601145;
	render_3D_data->filter[8] = -0.676947;
	render_3D_data->filter[9] = -0.167251;
	render_3D_data->filter[10] = 0.203305;
	render_3D_data->filter[11] = 0.261645;
	render_3D_data->filter[12] = 0.059649;
	render_3D_data->filter[13] = 0.026661;
	render_3D_data->filter[14] = -0.011648;
	render_3D_data->filter[15] = -0.335958;
	render_3D_data->filter[16] = -0.276208;
	render_3D_data->filter[17] = 0.037719;
	render_3D_data->filter[18] = 0.154546;
	render_3D_data->filter[19] = 0.141399;
	render_3D_data->filter[20] = -0.000902;
	render_3D_data->filter[21] = -0.031835;
	render_3D_data->filter[22] = -0.098318;
	render_3D_data->filter[23] = -0.058072;
	render_3D_data->filter[24] = -0.033449;
	render_3D_data->filter[25] = 0.030325;
	render_3D_data->filter[26] = 0.041670;
	render_3D_data->filter[27] = -0.001182;
	render_3D_data->filter[28] = -0.019692;
	render_3D_data->filter[29] = -0.031318;
	render_3D_data->filter[30] = -0.028427;
	render_3D_data->filter[31] = -0.003031;

        for (i=0; i<RESPONSE_LENGTH; i++) {
                fprintf(stdout, "\t%f\n", render_3D_data->filter[i]);
        }

        return render_3D_data;
}

void
render_3D_free(render_3D_dbentry **data)
{
        assert(*data);
        xfree(*data);
        *data = NULL;
}

void
finger_exercise(sample *signal, sample *answer, int signal_length)
{
        int     i;

        fprintf(stdout, "\tStepped into finger exercise.\n");

        for(i=0; i<signal_length; i++) {
                answer[i] = signal[i];
        }
}

void
render_3D(rx_queue_element_struct *el, int no_channels)
{
        int      n_samples;
        sample   *raw_buf, *proc_buf;
        struct   s_render_3D_dbentry  *part_3D_data;

        /* - take rx_queue_element_struct el
         * - set size of buffer by filling in 'el->native_size[el->native_count]'
         * - add extra native_data buffer of that size
         * - increment 'el->native_count'
         * - render audio into that buffer created in step 3
         */
        if (el->native_count < MAX_NATIVE) {
                el->native_size[el->native_count] = el->native_size[el->native_count-1];
                el->native_data[el->native_count] = (sample *) block_alloc(el->native_size[el->native_count]);
        }
        el->native_count++;

        part_3D_data = el->dbe_source[0]->render_3D_data;

        assert(el->native_size[el->native_count - 1] == el->native_size[el->native_count - 2]);

        n_samples = (int) el->native_size[el->native_count-1] / BYTES_PER_SAMPLE;
        
#ifdef NDEF
        /* check if mixer is stereo using 'no_channels' ('1' is mono, '2' ist stereo). */
        if (no_channels == 2) {
                /* extract mono buffer from stereo buffer */
                raw_buf = (sample *) block_alloc(el->native_size[el->native_count-1] / 2);
                proc_buf = (sample *) block_alloc(el->native_size[el->native_count-1] / 2);
                for (i=0; i<n_samples/2; i++) {
                        raw_buf[i] = el->native_data[el->native_count - 2] + (2 * i);
                }
                /* Externalisation */
                convolve(raw_buf, proc_buf, part_3D_data->overlap, part_3D_data->filter, RESPONSE_LENGTH, n_samples);

                /* Lateralisation */
                lateralise();
        }
#endif

        if (no_channels == 1) {
                raw_buf = el->native_data[el->native_count - 2];
                proc_buf = el->native_data[el->native_count - 1];
                memset(proc_buf, 0, n_samples * sizeof(sample));
                xmemchk();
                convolve(raw_buf, proc_buf, part_3D_data->overlap, part_3D_data->filter, RESPONSE_LENGTH, n_samples);
                xmemchk();
        }
        block_check((char*)el->native_data[el->native_count - 1]);
        block_check((char*)el->native_data[el->native_count - 2]);
        xmemchk();
}

/*=============================================================================================
  convolve()   time-domain, on-the-fly convolution

  Arguments:  signal           pointer to signal vector ('input')
              answer           pointer to answer vector (answer of the system)
              overlap          pointer to the overlap buffer
              response         pointer to coefficients vector (transfer function of the system)
              response_length  number of coefficients
              signal_length    number of values in 'signal'
=============================================================================================*/
void
convolve(sample *signal, sample *answer, double *overlap, double *response, int response_length, int signal_length)
{
        short   *signal_rptr, *answer_rptr;       /* running pointers within signal and answer vector */
        int     i, j;                             /* loop counters */
        double  *response_rptr;                   /* running pointer within response vector */
        double  *overlap_rptr_1, *overlap_rptr_2; /* running pointer within the overlap buffer */
        double  current;                          /* currently calculated answer value */

        /* Initialise the running pointers for 'signal' and 'answer'. */
        signal_rptr = signal;
        answer_rptr = answer;
        /*  Loop over the length of the signal vector. */
        for(i = 0; i < signal_length ;i++) {
                overlap[response_length-1] = *signal_rptr++;
                response_rptr = response;
                overlap_rptr_1 = overlap_rptr_2 = overlap;
                current = *overlap_rptr_1++ * *response_rptr++;
                /*  Use convolution method for computation */
                for(j = 1; j < response_length ; j++) {
                        *overlap_rptr_2++ = *overlap_rptr_1;
                        current += *overlap_rptr_1++ * *response_rptr++;
                }
                /* Clamping */
                if (current > 32767.0) current = 32767.0;
                if (current < -32768.0) current = -32768.0;
                /* store 'current' in answer vector. */
                *answer_rptr++ = ceil(current-0.5);
        }
}

#ifdef NDEF
/* expects a mono buffer (raw_buf) and processes into stereo buffer (proc_buf).
   n_samples is number of samples in mono_buffer.
   ipsi_buf:  buffer for channel closer to the sound source.
   contra_buf: buffer for channel further away from the sound source.
   tmp_buf: temporary buffer for storing samples before they go into excess_buf.
   excess_buf: storage of samples that couldn't be played out due to the delay in the
               contra-lateral channel. Go first next time 'round. */
void
lateralise(sample *raw_buf, sample *proc_buf, int n_samples)
{
        size_t   n_bytes;    /* number of bytes in unspliced (mono!) buffer */

        /* 'n_samples' is number of samples in _stereo_ buffer */
        n_bytes = sizeof(sample) * n_samples / 2;

        /* splice into two channels: ipsilateral and contralateral. */
        memcpy(ss->ipsi_buf, out_buf, n_bytes);
        memcpy(ss->contra_buf, out_buf, n_bytes);
        /* apply IID to contralateral buffer. */
        for (i=0; i<n_samples; i++) ss->contra_buf[i] *= ss->attenuation;
        /* apply ITD to contralateral buffer: delay mechanisam. */
        memcpy(ss->tmp_buf, ss->contra_buf+(n_samples-1)-ss->delay, ss->delay*sizeof(sample));
        memmove(ss->contra_buf+ss->delay, ss->contra_buf, (n_samples-ss->delay)*sizeof(sample));
        memcpy(ss->contra_buf, ss->excess_buf, ss->delay*sizeof(sample));
        memcpy(ss->excess_buf, ss->tmp_buf, ss->delay*sizeof(sample));
        /* Funnel ipsi- and contralateral buffers into out_buf. */
        for (i=0; i<n_samples; i++) {
                out_buf[2*i]   = ss->ipsi_buf[i];
                out_buf[2*i+1] = ss->contra_buf[i];
        }
}
#endif
