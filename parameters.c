/*
 *	FILE:	parameters.c
 *	PROGRAM: RAT
 *	AUTHOR:	Isidor Kouvelas + V.J.Hardman + Colin Perkins + O. Hodson
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

#include "config.h"
#include "parameters.h"
#include "audio.h"
#include "util.h"

/*
 * audio_stats  --  compute audio statistics
 * Copyright 1994 by Henning Schulzrinne
 *
 * Since this routine is called very frequently, we do the minimum necessary.
 * We sample every tenth sample for a statistical impression of the volume.
 * 
 * For mu-law, the energy estimate is 1/n sum log (|x_i|) = 1/n log prod |x_i| =
 * log prod |x_i|^(1/n), which is the log of the geometric mean of the
 * amplitudes.  for linear, the energy estimate is log (1/n sum |x_i|), the
 * log of the average of the amplitudes.
 */

#define STEP	16

int 
audio_energy(short *buf, size_t bytes)
{
	int i;
	sample *cp;
	long energy = 0;
	int count   = 0;
	
	for(i = 0, cp = buf; i<bytes; i+=STEP, cp+=STEP) {
		energy += (~s2u((unsigned short)*cp)) & 0x7f;
		count++;
	}
	return ((int)(energy / count));
}

/*
 * sd  --  silence detector
 * 
 * Algorithm: Use as input the average of the absolute buffer values.  The
 * threshold is set to be the minimum average plus 'hysteresis'.  If the
 * packet average is greater than the threshold for 'interval' packets, the
 * 'avg' (minimum average) value is incremented by one, but not beyond
 * 'max_avg'.
 * 
 * Return TRUE if average is less than threshold.  In that case, update the
 * minimum average, if necessary.  Silence detection copied almost verbatim
 * from VT ((c) isi).
 * 
 * Copyright 1994 by AT&T Bell Laboratories; all rights reserved
 */

static int sd_max_avg = 55;
static int sd_hyst = 12;
static int sd_interval = 8;

void
set_silence_params(char *s)
{
	char	*p;
	int	i;

	p = strtok(s, "/");
	if (p == NULL)
		return;
	i = atoi(p);
	if (i <= 0)
		return;
	sd_max_avg = i;

	p = strtok(NULL, "/");
	if (p == NULL)
		return;
	i = atoi(p);
	if (i <= 0)
		return;
	sd_hyst = i;

	p = strtok(NULL, "/");
	if (p == NULL)
		return;
	i = atoi(p);
	if (i <= 0)
		return;
	sd_interval = i;
}

/*
 * Initialize sd structure to reasonable defaults.
 */
sd_t *
sd_init(void)
{
	sd_t	*s = (sd_t *)xmalloc(sizeof(sd_t));

	if (!s)
		return (NULL);
	s->max_avg = sd_max_avg;
	s->avg = 0;
	s->count = 0;
	s->interval = sd_interval;
	s->hyst = sd_hyst;
	return (s);
}

/*
 * Flag 'echo' indicates whether echo suppression is on.
 * Return TRUE if silent.
 */
int
sd(sd_t *s, int energy, int echo)
{
	int	thresh;

	thresh = s->avg + s->hyst + (echo ? s->echo_th : 0);

	if (energy > thresh) {	/* non-silence: adjust after interval */
		if (s->count > 1)
			s->count--;
		else {
			if (s->avg < s->max_avg)
				s->avg++;

			s->count = s->interval;
		}
		return (FALSE);
	} else {		/* silence: adjust min. average immediately */
		if (energy < s->avg /* + 5 && s->avg > 10 */) {
			s->avg = energy; 
			s->count = s->interval;
		}

		return (TRUE);
	}
}

/************Peakmeter has been modified to work with BAT - VJH****/
/*
 * peak_meter  --  measure peak audio value
 * 
 * Copyright 1993 by AT&T Bell Laboratories; all rights reserved
 */

/*
 * Allocated peakmeter datastructure.
 */

peakmeter_t *peakmeter_init(void)
{
/* This routine inits the peak meter storage
 * I think that interval should be variable
 */
  peakmeter_t *p;

  p = (peakmeter_t *)xmalloc(sizeof(peakmeter_t));
  p->interval = 10;
  p->hyst = 5;
  p->old_value = 0;
  p->count = 0;/*VJH*/
  p->energy = 0;/*VJH*/
  p->show_flag = FALSE;
  return (p);
}

/*
 * Peak meter with logarithmic display. Subtract DC estimate (as->avg).
 * Values sent to display range from 0 to 127.
 */

void 
peakmeter(peakmeter_t *p)
{
  /*removed the count and interval stuff from this routine - VJH*/
  if (++(p->count) >= p->interval)
    {
      if (abs(p->energy - p->old_value) > p->hyst || 
	  p->energy == 0) 
	{
	  p->old_value = p->energy;	
	  /*p->energy = s2u(p->energy)>>1;*/
	  p->show_flag = TRUE;
	}
      p->count = 0;
    }
}

/*Extra routine added by VJH*/
void
zero_peakm(peakmeter_t *p)
{
  if (++(p->count) >= p->interval)
    {
      if (p->energy != 0)
	{
	  p->energy = 0;
	  p->old_value = 0;
	  p->show_flag = TRUE;
	}
      p->count = 0;
    }
}
