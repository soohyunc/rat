/*
 * FILE:    convert.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
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

static void (*ds)(short *,short *,short *,short *, int, int, int, int);
static void (*us)(short *,short *,short *,short *, int, int, int, int);

/* Linear interpolation (sperm of the devil) */
static void
upsample_linear(short *dst, 
                short *src_prev, 
                short *src, 
                short *src_next, 
                int    src_len, 
                int    scale, 
                int    src_step, 
                int    dst_step)
{
        short *p,*ep;
        float w1,w2,ws,tmp;

        UNUSED(src_next);
        UNUSED(src_prev);

        if (src_prev) {
                p = src_prev+(src_len-1)*src_step;
        } else {
                p = src;
        }
    
        ep = src+src_len*src_step;
        ws = 1.0f/((float)scale);
    
        while(src<ep){
                w1 = ws;
                w2 = 1.0f-ws;
                tmp = 0;
                while(w1<1.0) {
                        *dst = *p;
/*(short)(w1*((float)*p)+w2*((float)*src));*/
                        dst += dst_step;
                        w1  += ws;
                        w2  -= ws;
                }
                p    = src;
                src += src_step;
                *dst = *p;
                dst += dst_step;
        }
}

static void
downsample_linear(short *dst, 
                  short *src_prev, 
                  short *src, 
                  short *src_next, 
                  int src_len, 
                  int scale, 
                  int src_step, 
                  int dst_step)
{
        register int tmp,j;
        short *ep = src + src_len*src_step;

        UNUSED(src_next);
        UNUSED(src_prev);

        printf("downsample linear %d %d\n",src_len,scale);

        while(src<ep) {
                tmp = 0;
                j = 0;
                while(j++<scale) {
                        tmp += *src;
                        src += src_step;
		}
                tmp /= scale;
                *dst = tmp;
                dst += dst_step;
	}
}

void
set_converter(int mode)
{
        switch (mode) {
        case CONVERT_LINEAR:
                ds = &downsample_linear;
                us = &upsample_linear;
                break;
        default:
                debug_msg("Converter not implemented!\n");
        }
}

static short*
get_unconverted_audio(rx_queue_element_struct *ip)
{
        if (ip && ip->native_count>0) {
                return ip->native_data[0];
        } else {
                return NULL;
        }
}

int
convert_format(rx_queue_element_struct *ip, int to_freq, int to_channels)
{
        register int i;
        register short *p;
        codec_t *cp;
        short *prev, *next;
        int scale=0,size=0;
        void (*converter)(short *, short*, short*, short *, int, int, int, int) = NULL;

        /* get audio blocks from side for conversion methods that need them */
        prev = get_unconverted_audio(ip->prev_ptr);
        next = get_unconverted_audio(ip->next_ptr);
        cp = ip->comp_data[0].cp;

        if ( cp->freq > to_freq && 
             cp->freq % to_freq == 0) {
                scale = cp->freq/to_freq;
                size  = cp->unit_len * to_channels * sizeof(sample) / scale;
                converter = ds;
        } else if ( cp->freq < to_freq && 
                    to_freq % cp->freq == 0) {
                scale = to_freq/cp->freq;
                size  = cp->unit_len * to_channels * sizeof(sample) * scale;
                converter = us;
	} else {
                /* just doing number of channels conversion */
                size  = cp->unit_len * to_channels * sizeof(sample);
        }
        ip->native_size[ip->native_count] = size;
        ip->native_data[ip->native_count] = (sample*)block_alloc(size);
        ip->native_count++;

        if (converter && scale) {
                if (to_channels == cp->channels) {
                        /* m channels to m channels */
                        for(i=0;i<to_channels;i++) {
                                (*converter)(ip->native_data[ip->native_count-1]+i,
                                             prev,
                                             ip->native_data[ip->native_count-2]+i,
                                             next,
                                             cp->unit_len,
                                             scale,
                                             to_channels,
                                             to_channels);
                                if (prev) prev++;
                                if (next) next++;
                        }
                } else if (to_channels == 1) {
                        /* XXX 2 channels to 1 - we should do channel selection here [oth] */
                        assert(cp->channels == 2);
                        (*converter)(ip->native_data[ip->native_count-1],
                                     prev,
                                     ip->native_data[ip->native_count-2],
                                     next,
                                     cp->unit_len,
                                     scale,
                                     2,
                                     1);
                } else if (to_channels == 2) {
                        /* XXX 1 channel to 2 */
                        assert(cp->channels == 1);
                        (*converter)(ip->native_data[ip->native_count-1],
                                     prev,
                                     ip->native_data[ip->native_count-2],
                                     next,
                                     cp->unit_len,
                                     scale,
                                     1,
                                     2);
                        p = ip->native_data[ip->native_count-1];
                        for(i=0;i<cp->unit_len;i++,p+=2)
                                *(p+1) = *p;
                } else {
                        /* non-integer conversion attempted */
                        return 0;
		}
                return (size>>1);
        } else if (cp->freq == to_freq) {
                /* channel conversion only */
                if (to_channels > cp->channels) {
                        /* 1 to 2 channels */
                        printf("1 to 2 channels\n");
                        assert(to_channels == 2);
                        prev = ip->native_data[0];
                        p    = ip->native_data[1];
                        i = 0;
                        while(i++ < cp->unit_len) {
                                *p = *(p+1) = *prev++;
                                p += 2;
                        }
                        return (size / 2);
                } else if (to_channels < cp->channels) {
                        /* 2 to 1 channels */
                        printf("2 to 1 channels\n");
                        assert(cp->channels == 2);
                        prev = ip->native_data[0];
                        p    = ip->native_data[1];
                        i = 0;
                        while(i++<cp->unit_len) {
                                *p++  = *prev++;
                                prev += 2;
                        }
                        return (size / 2);
                } else {
#ifdef DEBUG
                        /* should never be here */
                        fprintf(stderr, "convert_format: neither sample rate conversion nor channel count conversion.");
#endif            
                }
        }
        return 0;
}






