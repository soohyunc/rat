/*
 * FILE  : agc.c - Automatic Gain Control
 * AUTHOR: Colin Perkins + Mark Handley
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#include "agc.h"
#include "session.h"
#include "audio.h"
#include "ui.h"

#define ENERGY_MAX 32

/*Threshold numbers chosen empirically*/
/*the highest value of minimum energy at which we increase gain*/
#define MIN_THRESH 3
/*different between hi and lo above which decide there's speech*/
#define RANGE_THRESH 8
/*maximum level for hi when increasing without definite speech*/
#define LO_THRESH 16
/*level above which we reduce gain*/
#define HI_THRESH 20
/*level above which we reduce gain rapidly!*/
#define OVERLOAD_THRESH 26

static int agc_table[ENERGY_MAX];
static int agc_count;
static int agc_window;

void agc_table_init(void)
{
	int i;
	for (i=0; i<ENERGY_MAX; i++) {
		agc_table[i] = 0;
	}
	agc_count = 0;
	agc_window= 25;
}

void agc_table_update(session_struct *sp, int energy, int silence)
{
	/* The "silence" parameter is a boolean, based on the result of the    */
	/* silence detection algorithm. The AGC code might find this useful... */
	int i, hi, lo, min, max;
	int agc_incr = 20;	/* Window increment */

	if (sp->agc_on == FALSE) return;

	assert((energy>=0) && ((energy/4)<ENERGY_MAX));
	agc_table[energy/4]++;
	agc_count++;
	if (agc_count > agc_window) {
		/* Find lower peak... */
		lo = 0;
		min=0;
		max=0;
		for(i=0;i<ENERGY_MAX; i++) {
		  if (agc_table[i]>0) 
		    break;
		  min=i+1;
		}
		for (i=ENERGY_MAX-1; i>0; i--) {
		  if (agc_table[i]>0)
		    break;
		  max=i-1;
		}
#ifdef DEBUG
		for(i=0;i<ENERGY_MAX; i++) {
		  printf("%d ", agc_table[i]);
		}
		printf("\n");
#endif
		for (i=1; i<ENERGY_MAX; i++) {
			if (agc_table[i] < agc_table[i-1]) {
				lo = i-1;
				break;
			}
		}
		/* Find upper peak... */
		hi = lo;
		for (i=ENERGY_MAX-2; i>lo; i--) {
			if (agc_table[i] < agc_table[i+1]) {
				hi = i+1;
				break;
			}
		}
#ifdef DEBUG
		printf("AGC: min=%d lo=%d hi=%d max=%d window=%d\n", min, lo, hi, max, agc_window);
#endif
		if (((hi-lo)>RANGE_THRESH)&&(max<HI_THRESH)&&(min<MIN_THRESH)) {
		    /*looks like we have signal but peak is still below
		      high thresh and we haven't increased background noise
		      to unacceptable levels*/
		    sp->input_gain++;
		    agc_incr = 5;
#ifdef DEBUG
		    printf("up b(%d)\n", sp->input_gain);
#endif
		    audio_set_gain(sp->audio_fd, sp->input_gain);
		    ui_update(sp);
		  } else if ((hi<LO_THRESH)&&(min==0)) {
		    /*be very cautious in increasing volume when we
		     don't seem to have a signal*/
		    sp->input_gain += 5;
		    agc_incr = 0;
#ifdef DEBUG
		    printf("up (%d)\n", sp->input_gain);
#endif
		    audio_set_gain(sp->audio_fd, sp->input_gain);
		    ui_update(sp);
		  } else if ((lo==hi)&&(min>=MIN_THRESH)) {
		    /*probably an unacceptable level of background noise*/
		    sp->input_gain--;
		    agc_incr -= 5;
#ifdef DEBUG
		    printf("down bg(%d)\n", sp->input_gain);
#endif
		    audio_set_gain(sp->audio_fd, sp->input_gain);
		    ui_update(sp);
		  } else if (hi>=OVERLOAD_THRESH) {
		    /*Definite overload - reduce plus speed up AGC*/
		    sp->input_gain=(sp->input_gain*8)/10;
#ifdef DEBUG
		    printf("down fast (%d)\n", sp->input_gain);
#endif
		    agc_incr = -10;
		    ui_update(sp);
		  } else if (hi>=HI_THRESH) {
		    /*Overload - reduce quickly*/
		    sp->input_gain=(sp->input_gain*9)/10;
#ifdef DEBUG
		    printf("down fast (%d)\n", sp->input_gain);
#endif
		    agc_window = -30;
		    ui_update(sp);
		  } else if (max>=HI_THRESH) {
		    /*High peak, reduce slowly*/
		    sp->input_gain-=1;
#ifdef DEBUG
		    printf("down (%d)\n", sp->input_gain);
#endif
		    ui_update(sp);
		  }
		  
		/* Clear the table... */
		for (i=0; i<ENERGY_MAX; i++) {
			agc_table[i] = 0;
		}
		agc_count = 0;
		agc_window += agc_incr;
		if (agc_window > 100) agc_window = 100;
		if (agc_window <  10) agc_window =  10;
	}
}

