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
#include "debug.h"
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

#define MAX_RESPONSE_LENGTH 32
#define MIN_RESPONSE_LENGTH 8
#define DEFAULT_RESPONSE_LENGTH 32
#define LOWER_AZIMUTH -90
#define UPPER_AZIMUTH  90
#define IDENTITY_FILTER 0


static char * filter_names[] = {"Identity",
                                "HRTF",
                                "Echo"};

#define NUM_FILTERS (sizeof(filter_names) / sizeof(filter_names[0]))

typedef struct s_render_3D_dbentry {
        short    azimuth;                             /* lateral angle of sound source */
        short    delay;                               /* based on interaural time difference (ITD); derived from 'azimuth' */
        float    attenuation;                         /* based on interaural intensity difference (IID); derived from 'azimuth' */
        sample   ipsi_buf[MAX_PACKET_SAMPLES];        /* buffer for ipsi-lateral channel before merging into stereo buffer */
        sample   contra_buf[MAX_PACKET_SAMPLES];      /* buffer for contra-lateral channel before merging into stereo buffer */
        sample   tmp_buf[64];                         /* temporary storage for swapping samples */
        sample   excess_buf[64];                      /* buffer for excess samples due to delay */
        char     filter_name;
        double   filter[MAX_RESPONSE_LENGTH];         /* filter used for convolution */
        double   overlap_buf[MAX_RESPONSE_LENGTH];    /* overlap buffer due to filter operation on the mono signal */
        int      response_length;
} render_3D_dbentry;

int
render_3D_filter_get_count()
{
        return NUM_FILTERS;
}

char *
render_3D_filter_get_name(int id)
{
        if (id >= 0 && id < (signed)NUM_FILTERS) return filter_names[id];
        return filter_names[IDENTITY_FILTER];
}

int
render_3D_filter_get_by_name(char *name)
{
        int i;
        for(i = 0; i < (signed)NUM_FILTERS; i++) {
                if (!strcasecmp(name, filter_names[i])) return i;
        }
        return IDENTITY_FILTER;
}

/* At the present time there is only 1 possible filter length.  At a 
 * later date it may be desirable to have shorter filter lengths to
 * reduce processing load and a suitable length selection algorithm.
 */

int
render_3D_filter_get_lengths_count()
{
        return 1;
}

int
render_3D_filter_get_length(int idx)
{
        UNUSED(idx);
        return DEFAULT_RESPONSE_LENGTH;
}

int
render_3D_filter_get_lower_azimuth()
{
        return LOWER_AZIMUTH;
}

int
render_3D_filter_get_upper_azimuth()
{
        return UPPER_AZIMUTH;
}

render_3D_dbentry *
render_3D_init(session_struct *sp)
{
        int               i;
        int               sampling_rate;
        int               azimuth, length;
        int               default_filter_num;
        char              *default_filter_name;
        render_3D_dbentry *render_3D_data;

        sampling_rate = audio_get_freq(sp->audio_device);

        azimuth = 0;
        length = DEFAULT_RESPONSE_LENGTH;
        default_filter_name = "HRTF";
        default_filter_num  = render_3D_filter_get_by_name(default_filter_name);

        render_3D_data = (render_3D_dbentry *) xmalloc(sizeof(render_3D_dbentry));
        memset(render_3D_data, 0, sizeof(render_3D_dbentry));

        render_3D_set_parameters(render_3D_data, sampling_rate, azimuth, default_filter_num, length);

        fprintf(stdout, "\tdelay:\t%d\n", render_3D_data->delay);
        fprintf(stdout, "\tattenuation:\t%f\n", render_3D_data->attenuation);
        for (i=0; i<length; i++) {
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
render_3D_set_parameters(struct s_render_3D_dbentry *p_3D_data, int sampling_rate, int azimuth, int filter_number, int length)
{
        int i;
        double aux;
        double d_time;         /* delay in seconds. auxiliary to calculate delay in samples. */
        double d_intensity;    /* interaural intensity difference 0.0 <d_intensity < 1.0 */

        double   filters[][32] = { { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
                                   { 0.063113, -0.107530, 0.315168, 0.015218, -0.300535, 1.000000, 0.359786, -0.601145, -0.676947,
                                     -0.167251, 0.203305, 0.261645, 0.059649, 0.026661, -0.011648, -0.335958, -0.276208, 0.037719,
                                     0.154546, 0.141399, -0.000902, -0.031835, -0.098318, -0.058072, -0.033449, 0.030325, 0.041670,
                                     -0.001182, -0.019692, -0.031318, -0.028427, -0.003031 },
                                   { 0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                     0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                     0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                     0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 } };
        double   *filter_set[NUM_FILTERS];

        debug_msg("rate %d azimuth %d filter %d length %d\n",
                  sampling_rate, azimuth, filter_number, length);

        /* Identity filter */
        filter_set[0] = *(filters+0);

        /* HRTF filter */
        filter_set[1] = *(filters+1);

        /* simple echo filter */
        filter_set[2] = *(filters+2);

        p_3D_data->azimuth = azimuth;

        /* derive interaural time difference from azimuth */
        aux= azimuth * 0.017453203;                                /* conversion into radians */
        d_time = 2.72727 * sin(aux);
        p_3D_data->delay = abs((int) (sampling_rate * d_time / 1000));

        /* derive interaural intensity difference from azimuth */
        d_intensity = 1.0 - (0.3 * fabs(sin(aux)));
        p_3D_data->attenuation = d_intensity;

        /* fill up participant's response filter */
        p_3D_data->response_length = length;
        switch(filter_number) {
        case 0:
                /* filter_set[0] is a pointer to the identity filter (decimation and extraction of MST are superfluous */
                for (i=0; i<MAX_RESPONSE_LENGTH; i++) {
                        p_3D_data->filter[i] = *(filter_set[0] + i);
                }
                break;
        case 1:
                /* filter_set[1] is a pointer to the HRTF data set and serves as input to decimation and extraction of MST */
                /* the output of this operation goes into p_3D_data->filter[] */
                /* right now it's only a copying of values */
                for (i=0; i<MAX_RESPONSE_LENGTH; i++) {
                        p_3D_data->filter[i] = *(filter_set[1] + i);
                }
                break;
        case 2:
                /* filter_set[2] is a pointer to a simple echo filter */
                for (i=0; i<MAX_RESPONSE_LENGTH; i++) {
                        p_3D_data->filter[i] = *(filter_set[2] + i);
                }
                break;
        }
}

#ifdef NDEF
int
int
render_3D_get_lower_filter_length()
{
        return MIN_RESPONSE_LENGTH;
}

int
render_3D_get_upper_filter_length()
{
        return MAX_RESPONSE_LENGTH;
}
#endif

void
render_3D(rx_queue_element_struct *el, int no_channels)
{
        int      i;
        int      n_samples;
        size_t   n_bytes;    /* number of bytes in unspliced (mono!) buffer */
        sample   *raw_buf, *proc_buf;
        sample   *mono_raw, *mono_filtered;  /* auxiliary buffers in case of stereo */
        int       mono_buf_len;
        struct   s_render_3D_dbentry  *p_3D_data;

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

        p_3D_data = el->dbe_source[0]->render_3D_data;

        assert(el->native_size[el->native_count - 1] == el->native_size[el->native_count - 2]);

        n_samples = (int) el->native_size[el->native_count-1] / BYTES_PER_SAMPLE;

        raw_buf = el->native_data[el->native_count - 2];   /* buffer handed over _for_ 3D rendering */
        proc_buf = el->native_data[el->native_count - 1];  /* buffer returned _from_ 3D rendering */
        memset(proc_buf, 0, n_samples * sizeof(sample));

        /* check if mixer is stereo using 'no_channels' ('1' is mono, '2' ist stereo). */
        if (no_channels == 2) {
                /* extract mono buffer from stereo buffer */
                mono_buf_len = el->native_size[el->native_count-1] / 2;
                mono_raw      = (sample *) block_alloc(mono_buf_len);
                mono_filtered = (sample *) block_alloc(mono_buf_len);
                for (i=0; i<n_samples/2; i++) {
                        mono_raw[i] = raw_buf[2 * i];
                }
                /* EXTERNALISATION */
                convolve(mono_raw, mono_filtered, p_3D_data->overlap_buf, p_3D_data->filter, p_3D_data->response_length, n_samples/2);

                /* LATERALISATION */

                /* mono_filtered is input, and el->native_data[el->native_count-1] is the output (stereo). */
                /* 'n_samples' is number of samples in _stereo_ buffer */
                n_bytes = sizeof(sample) * n_samples / 2;

                /* splice into two channels: ipsilateral and contralateral. */
                memcpy(p_3D_data->ipsi_buf, mono_filtered, n_bytes);
                memcpy(p_3D_data->contra_buf, mono_filtered, n_bytes);
                /* apply IID to contralateral buffer. */
                for (i=0; i<(n_samples/2); i++) {
                        p_3D_data->contra_buf[i] = (short)((double)p_3D_data->contra_buf[i]*p_3D_data->attenuation);
                }
                /* apply ITD to contralateral buffer: delay mechanisam. */
                memcpy(p_3D_data->tmp_buf, p_3D_data->contra_buf+((n_samples/2)-1)-p_3D_data->delay, p_3D_data->delay*sizeof(sample));
                memmove(p_3D_data->contra_buf+p_3D_data->delay, p_3D_data->contra_buf, ((n_samples/2)-p_3D_data->delay)*sizeof(sample));
                memcpy(p_3D_data->contra_buf, p_3D_data->excess_buf, p_3D_data->delay*sizeof(sample));
                memcpy(p_3D_data->excess_buf, p_3D_data->tmp_buf, p_3D_data->delay*sizeof(sample));
                /* Merge ipsi- and contralateral buffers into proc_buf. */
                if (p_3D_data->azimuth > 0) {
                        for (i=0; i<n_samples/2; i++) {
                                proc_buf[2*i]   = p_3D_data->ipsi_buf[i];
                                proc_buf[2*i+1] = p_3D_data->contra_buf[i];
                        }
                } else {
                        for (i=0; i<n_samples/2; i++) {
                                proc_buf[2*i]   = p_3D_data->contra_buf[i];
                                proc_buf[2*i+1] = p_3D_data->ipsi_buf[i];
                        }
                }
                block_free(mono_raw, mono_buf_len);
                block_free(mono_filtered, mono_buf_len);
        }

        if (no_channels == 1) {
                xmemchk();
                convolve(raw_buf, proc_buf, p_3D_data->overlap_buf, p_3D_data->filter, p_3D_data->response_length, n_samples);
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
        sample  *signal_rptr, *answer_rptr;       /* running pointers within signal and answer vector */
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
                if (current > 32767.0) {
                        current = 32767.0;
                        debug_msg("clipping\n");
                } else if (current < -32767.0) {
                        current = -32767.0;
                        debug_msg("clipping\n");
                }
                /* store 'current' in answer vector. */
                current = ceil(current-0.5);
                *answer_rptr++ = (short)current;
        }
}
