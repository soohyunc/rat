/*
 * FILE:    pos_audio.c
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

/*==============================================================================
  convolve() - filters input array using FIR
              coefs (uses real time code)
  Prototype:  void Dig_FIR_Filt_RT(int *X,int *Y,
            double *M,double *C,int numb_coefs,int N);
  Return:     error value.
  Arguments:  X - ptr to input array
              Y - ptr to output array
              M - ptr to memory array
              C - ptr to coefs array
              numb_coefs - number of coefficients
              N - number of values in array
===============================================================================*/
void convolve(short *X, short *Y, double *M, double *C, int numb_coefs, int N)
{ 
        short   *x, *y;         /*  ptrs to in/out arrays */
        int     i, j;           /*  loop counters */
        double  *c, *m1, *m2;   /*  ptrs to coef/memory array */
        double  o;              /*  output value */

        /*  Make copies of input and output pointers */
        x = X;
        y = Y;
        /*  Start loop for number of data values */
        for(i = 0; i < N ;i++) {
                /*  Make copy of pointers and start loop */
                M[numb_coefs-1] = *x++;
                c = C;
                m1 = m2 = M;
                o = *m1++ * *c++;
                /*  Use convolution method for computation */
                for(j = 1; j < numb_coefs ;j++) {
                        *m2++ = *m1;
                        o += *m1++ * *c++;
                }
                /*  Multiply by gain, convert to int and store */
                if (o > 32767.0) o = 32767.0;
                if (o < -32768.0) o = -32768.0;
                o *= *c;
                *y++ = (int)ceil(o-0.5);
        }
}

void
localise_sound(rx_queue_element_struct *el)
{
        out_buf = el->native_data[el->native_count - 1];

        if (lat) {
                if ((!flt) && (!wbs)) memcpy(out_buf, in_buf, n_bytes);
                /* splice into two channels: ipsilateral and contralateral. */
                memcpy(ss->ipsi_buf, out_buf, n_bytes);
                memcpy(ss->contra_buf, out_buf, n_bytes);
                /* apply IID to contralateral buffer. */
                for (i=0; i<SAMPLES_PER_WBS_UNIT; i++) ss->contra_buf[i] *= ss->attenuation;
                /* apply ITD to contralateral buffer: delay mechanisam. */
                memcpy(ss->tmp_buf, ss->contra_buf+(SAMPLES_PER_WBS_UNIT-1)-ss->delay, ss->delay*sizeof(sample));
                memmove(ss->contra_buf+ss->delay, ss->contra_buf, (SAMPLES_PER_WBS_UNIT-ss->delay)*sizeof(sample));
                memcpy(ss->contra_buf, ss->excess_buf, ss->delay*sizeof(sample));
                memcpy(ss->excess_buf, ss->tmp_buf, ss->delay*sizeof(sample));
                /* Funnel ipsi- and contralateral buffers into out_buf. */
                for (i=0; i<SAMPLES_PER_WBS_UNIT; i++) {
                        out_buf[2*i]   = ss->ipsi_buf[i];
                        out_buf[2*i+1] = ss->contra_buf[i];
                }
        }

        if (!lat) {
                if ((!flt) && (!wbs)) memcpy(out_buf, in_buf, n_bytes);
                memcpy(ss->ipsi_buf, out_buf, n_bytes);
                memcpy(ss->contra_buf, out_buf, n_bytes);
                for (i=0; i<SAMPLES_PER_WBS_UNIT; i++) {
                        out_buf[2*i]   = ss->ipsi_buf[i];
                        out_buf[2*i+1] = ss->contra_buf[i];
                }
        }
}
