/*
 * FILE:    repair.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson + Isidor Kouvelas
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

#include <sys/types.h>
#ifndef WIN32
#include <netdb.h>
#endif
#include "util.h"
#include "repair.h"
#include "receive.h"
#include "gsm.h"
#include "codec_lpc.h"

/* Fades by 0.2 in 160 linear steps */
#define FADE_SIZE          0.2
#define FADE_STEP          0.00125

#define NO_FADE            0
#define FADE_TO_LEFT       1
#define FADE_TO_RIGHT      2

#define UNBOUND            0
#define BOUND              1

#define FORWARD           +1
#define BACKWARD          -1

typedef struct s_rep_seg {
        short start;
        short end;
} rep_seg;

/*
 * When we arrive here we know that participant has an audio sample from
 * previous interval and a dummy for this interval.
 */

char *
get_repair_name(int id)
{
        switch(id) {
        case REPAIR_NONE:
                return "NONE";
        case REPAIR_REPEAT:
                return "REPEAT";
        case REPAIR_WAVEFORM_PM:
                return "PM";
        default:
                return "UNKNOWN";
        }
}

#ifdef ucacoxh

/* Consecutive dummies behind */
static int
count_dummies_back(rx_queue_element_struct *up)
{
        int n = 0;

	while (up->prev_ptr) {
		up = up->prev_ptr;
		if (up->comp_count != 0)
			break;
		n++;
	}
        return (n);
}

/* Consecutive dummies ahead */
static int
count_dummies_ahead(rx_queue_element_struct *up)
{
        int n = 0; 

        while (up->next_ptr) {
		up = up->next_ptr;
		if (up->comp_count != 0)
			break;
		n++;
	}
	return (n);
}

static void
repeat(rx_queue_element_struct *srcp, rx_queue_element_struct *up)
{
/*	int	i, len;
	float	sf;
	sample	*s, *d;
	
	if (up->decomp_data == NULL)
		up->decomp_data = (sample*)block_alloc(up->unit_size * BYTES_PER_SAMPLE);

	len = min(up->unit_size, srcp->unit_size);

	sf = 1.0 - FADE_SIZE * count_dummies_back(ip, up);
	d = up->decomp_data;
	s = srcp->decomp_data;
	for(i = 0; i < len; i++) {
		*d++ = (short)((float)*s++ * sf);
		sf -= FADE_STEP;
	}
	*/
        return;
}

static void
repeat_lpc(rx_queue_element_struct *pp, rx_queue_element_struct *up)
{
	lpc_txstate_t	*lp=NULL;

	assert(pp->comp_data != NULL);
        if (up->comp_count == 0) {
		/* XXX */
		up->comp_count = 1;
                up->comp_data[0].data = (char*)xmalloc(LPCTXSIZE);
	}
	up->comp_data[0].data_len = LPCTXSIZE;
	memcpy(up->comp_data[0].data, pp->comp_data[0].data, LPCTXSIZE);
	up->comp_data[0].cp = pp->comp_data[0].cp;

	lp = (lpc_txstate_t*)up->comp_data[0].data;
	lp->gain = (short)((float)lp->gain * 0.8);
	decode_unit(up);
}

static void
repeat_gsm(rx_queue_element_struct *pp, rx_queue_element_struct *up, gsm s)
{
	/* GSM 06.11 repair mechanism */
/*	int		i;
	gsm_byte	*rep;
	char		xmaxc;
    
	assert(pp->comp_data != NULL);
	
	if (up->comp_count == 0) {
		up->comp_data[0] = (sample*)block_alloc(33);
		up->comp_count = 1;
	}

	up->comp_dataformat[0].scheme = GSM;
	rep = (gsm_byte*)up->comp_data[0];
	memcpy(rep, pp->comp_data[0], 33);
#ifdef DEBUG
	printf("last %d cur %d - ", (int)s->lrep_time, 
	       (int)up->interval->interval_start);
#endif
	if (up->interval->interval_start - s->lrep_time <= 160) {
		for(i=6;i<28;i+=7) {
			xmaxc  = (rep[i] & 0x1f) << 1;
			xmaxc |= (rep[i+1] >> 7) & 0x01;
			if (xmaxc > 4) { 
				xmaxc -= 4;
			} else { 
				xmaxc = 0;
			}
			rep[i]   = (rep[i] & 0xe0) | (xmaxc >> 1);
			rep[i+1] = (rep[i+1] & 0x7f) | ((xmaxc & 0x01) << 7);
		}
#ifdef DEBUG
		printf("fade\n");
#endif
	}
#ifdef DEBUG
	else printf("replica\n");
#endif
	s->lrep_time = up->interval->interval_start;
	decode_unit(up);
	*/
	return;
}

void
repair(int repair, rx_queue_element_struct *up)
{
	static codec_t *gsmcp, *lpccp;
	static virgin = 1;
	int i;

	if (virgin) {
		gsmcp = get_codec_byname("GSM",NULL);
		lpccp = get_codec_byname("LPC",NULL);
		virgin = 0;
	}

	rx_queue_element_struct *pp;

	pp = up->prev_ptr;
	if (!pp) return;
	if (!pp->native_count) decode_unit(pp);
	
	switch(repair) {
	case REPAIR_REPEAT:
		switch (pp->comp_format[0].cp) {
		case lpccp:
			repeat_lpc(pp, up);
			break;
		case gsmcp: 
			repeat_gsm(pp, up, up->dbe_source[0]->rx_gsm_state);
			break;
		default:
			for(i=0;i<pp->comp_format[0].cp->channels;i++)
				repeat(pp,ip,i);
			break;
		}
		break;
	case REPAIR_WAVEFORM_PM:
		for(i=0;i<pp->comp_format[0].cp->channels;i++)
			waveform_repair(pp,ip,i);
		break;
        } 
        return;
}
#else
void 
repair(int repair, struct rx_element_tag *up)
{
	return;
}
#endif








