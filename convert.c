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

/* Perform sample rate and channel conversion. From length is in samples and
 * does not take into account the number of channels (i.e. you have to work
 * on twice the data for stereo).
 * conv_buf_len is length of buffer to put output and includes stereo etc.
 * Return value is number of samples to mix (i.e real length of buffer) and
 * includes number of channels.
 */

#define sgn(x) ((x>0.0) ? 1.0 : -1.0)

#define WIN_WIDTH   24
#define DOWN_WIDTH   6
/* tables to 2x,3x,4x,5x,6x up and downsample */
#define N_WINDOWS    5

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif /* M_PI */

static float h_up[N_WINDOWS][2*WIN_WIDTH+1];
static float h_down[N_WINDOWS][2*DOWN_WIDTH+1];

void
init_converter()
{
	int i,j;
	for(i=0;i<N_WINDOWS;i++) {
		h_down[i][DOWN_WIDTH] = 0.0;
		for(j=1;j<=DOWN_WIDTH;j++)
			h_down[i][DOWN_WIDTH-j] = h_down[i][DOWN_WIDTH+j] = 
				(1.0/(M_PI*(float)j))*sin(M_PI*(float)j/(float)(i+2));
	}
	return;
}

#ifdef ucacoxh

static float* 
get_converter_table(int from, int to)
{
	if (from>to) {
		if (from%to) {
			fprintf(stderr,"Cannot locate freq conversion table for %d to %d Hz.\n", from, to);
			return NULL;
		}
		return h_down[from/to-2];
	} else {
		if (to%from) {
			fprintf(stderr,"Cannot locate freq conversion table for %d to %d Hz.\n", from, to);
			return NULL;
		}
		return h_up[to/from-2];
	}
}

static void
mono_upsample(rx_queue_element_struct *ip, int scale, float *h)
{
	float *hsp, *hcp, *hep;
	short *ssp, *sep, *scp, *sbp;
	short *ossp, *osep, *oscp, *osbp;
	short *dp;
	float tmp;
	int offset;
	int lstep = WIN_WIDTH/scale;
	hsp = h; 
	hep = h+2*WIN_WIDTH+1;
	/* first the left edge */
#ifdef ucacoxh
	ssp = ip->native_data[0];
	dp  = ip->native_data[1]; 
	if (ip->prev_ptr &&
	    ip->prev_ptr->native_count &&
	    ip->prev_ptr->native_data[0]) {
		oscp = osbp = ossp = ip->prev_ptr->native_data[0] 
			+ ip->prev_ptr->comp_data[0].cp->unit_len
			- lstep;
		osep = ossp + lstep;
		sbp = scp = ssp;
		while(osbp<osep) {
			*dp++ = *sbp++;
			osbp ++;
			for(offset = scale-1;offset>0;offset--) {
				oscp = osbp;
				hcp = hsp + offset;
				tmp = 0.0;
				while(oscp<osep) {
					tmp += (*hcp)*(*oscp);
					hcp += scale;
					oscp++;
				}
				scp = ssp;
				while(hcp<hep) {
					tmp += (*hcp)*(*scp++);
					hcp += scale;
				}
			}
		}
	}
#endif ucacoxh
	/* now the body ...*/
	dp  = ip->native_data[1]+WIN_WIDTH;
	sbp = scp = ip->native_data[0];
	sep = ip->native_data[0]+ip->comp_data[0].cp->unit_len-2*lstep;
	while(sbp<sep){
		*dp++ = *(sbp+lstep);
		assert(dp<=ip->native_data[1]+scale*ip->comp_data[0].cp->unit_len);
		sbp++;
		for(offset=scale-1;offset>0;offset--){
			hcp = hsp + offset;
			scp = sbp;
			tmp = 0.0;
			while(hcp<hep){
				tmp += (*hcp)*(*scp);
				hcp += scale;
				scp ++;
			}
#ifdef DEBUG
			if (tmp<-32768.0||tmp>32767.0) 
				fprintf(stderr, "clipping % .4f\n",tmp);
#endif
			assert(tmp>=-32768.0 && tmp<32768);
			*dp++ = tmp;

			assert(dp<=ip->native_data[1]+scale*ip->comp_data[0].cp->unit_len);
		}
	}
	sep = sep + lstep;

	if (ip->next_ptr &&
	    ip->next_ptr->native_count &&
	    ip->next_ptr->native_data[0]) {
		ossp = ip->next_ptr->native_data[0];
		while(sbp<sep) {
			*dp++ = *(sbp+lstep);
			assert(dp<=ip->native_data[1]+scale*ip->comp_data[0].cp->unit_len);
			sbp++;
			for(offset=scale-1;offset>0;offset--){
				scp = sbp;
				hcp = hsp+offset;
				tmp = 0.0;
				while(scp<sep){
					tmp += (*hcp)*(*scp);
					hcp += scale;
					scp++;
				}
				oscp = ossp;
				while(hcp<hep) {
					tmp += (*hcp)*(*oscp);
					hcp += scale;
					oscp++;
				}
				assert(tmp>=-32768.0 && tmp<32768);
				*dp++=tmp;
				assert(dp<=ip->native_data[1]+scale*ip->comp_data[0].cp->unit_len);
			}
		}
	}
}

static void
mono_downsample(rx_queue_element_struct *ip, int scale, float *h)
{
	short *scp, *sep, *ssp;
	short *ossp, *osep;
	short *dp,*dsp;
	float *hcp,*hep;
	float tmp;


	dsp  = dp   = ip->native_data[1]; 
	ssp  = scp = ip->native_data[0];
	sep = ssp + ip->comp_data[0].cp->unit_len;
	hep = h+2*DOWN_WIDTH+1;
	hcp = h;

	while(scp<sep) {
		*dp++ = (short)(((int)(*scp++)+(*scp++))/2);
	}

/*	if (ip->prev_ptr &&
	    ip->prev_ptr->native_count &&
	    ip->prev_ptr->native_data[0]) {
		ossp = ip->prev_ptr->native_data[0]+ip->prev_ptr->comp_data[0].cp->unit_len-DOWN_WIDTH;
		osep = ip->prev_ptr->native_data[0]+ip->prev_ptr->comp_data[0].cp->unit_len;
		while(ossp<osep){
			tmp = 0.0;
			scp = ossp;
			hcp = h;
			while(scp<osep)
				tmp += (*scp++)*(*hcp++);
			scp = ssp;
			while(hcp<hep)
				tmp += (*scp++)*(*hcp++);
			*dp++=tmp;
			ossp += scale;
			
		}
		assert(dp==dsp+DOWN_WIDTH/scale);
	}
	*/

	sep = ssp + ip->comp_data[0].cp->unit_len - 2*DOWN_WIDTH;
	dp = dsp + DOWN_WIDTH/scale;
	while(ssp<sep) {
		scp = ssp;
		hcp = h;
		tmp = 0.0;
		while(hcp<hep) 
			tmp += (*hcp++)*(*scp++);

		if (tmp<-32768.0) {
			tmp = -32768.0;
			printf("clipping -\n");
		} else if (tmp>32767.0) {
			tmp = 32767.0;
			printf("clipping +\n");
		}

		*dp++ = (short) tmp;
		ssp += scale;
	}

}

#endif /* ucacoxh */

int
convert_format(rx_queue_element_struct *ip, int to_freq, int to_channels)
{
	int    len;
	float *h;
	assert(ip->native_data[0]);
	assert(to_channels == 1);
	assert(ip->comp_data[0].cp->freq != to_freq);
	
	len = ip->comp_data[0].cp->unit_len * to_freq / ip->comp_data[0].cp->freq;
#ifdef ucacoxh
	h = get_converter_table(ip->comp_data[0].cp->freq,to_freq);
	if (h==NULL) {
#ifdef DEBUG
		fprintf(stderr, "Non integer sample rate conversion attempted %d -> %d\n",
		       ip->comp_data[0].cp->freq,to_freq);
#endif /* DEBUG */
		return 0;
	}
	ip->native_data[1] = (short*)xmalloc(len*sizeof(sample));
	memset(ip->native_data[1],0x70,len*sizeof(sample));
	ip->native_count=2;
	if (ip->comp_data[0].cp->freq < to_freq)
		mono_upsample(ip,to_freq/ip->comp_data[0].cp->freq,h);
	else 
		mono_downsample(ip,ip->comp_data[0].cp->freq/to_freq,h);

#endif /* ucacoxh */
	return (len);
}






