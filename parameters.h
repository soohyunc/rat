/*
 *	FILE: parameters.h
 *	PROGRAM: BAT
 *	AUTHOR:	Vicky Hardman
 *		Colin Perkins
 *
 *	$Revision$
 *	$Date$
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


#ifndef _RAT_PARAMETERS_H_
#define _RAT_PARAMETERS_H_

/*
 * This version of the code can only work with a constant device
 * encoding. To use different encodings the parameters below have
 * to be changed and the program recompiled.
 * The clock translation problems have to be taken into consideration
 * if different users use different base encodings...
 */

/*From Nevot code - Henning Schulzerinne*/

typedef struct {
	int	echo_th;	/* additional energy needed if echo cancellation is on */
	int	hyst;		/* non-silent if this much above long-term average */
	int	max_avg;	/* maximum tracked average (upper limit on silence) */
	int	interval;	/* adjustment interval (packets) */
	int	avg;		/* current average energy */
	int	count;		/* consecutive packets above threshold */
} sd_t;

typedef struct {
	int	interval;	/* update every interval (measurements) */
	int	hyst;		/* hysteresis */
	int	count;		/* counts seen */
	int	old_value;	/* old display value */
	int     energy;         /* value to be displayed on screen VJH*/
	int     show_flag;      /* update required indicator VJH*/
} peakmeter_t;

int	audio_energy(short *buf, size_t bytes);
sd_t	*sd_init(void);
void	set_silence_params(char *s);
int	sd(sd_t *s, int energy, int echo);
peakmeter_t *peakmeter_init(void);
void	peakmeter(peakmeter_t *p);
void	zero_peakm(peakmeter_t *p);

#endif /* _RAT_PARAMETERS_H_ */

