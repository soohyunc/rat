/*
 * FILE:    time.c
 * PROGRAM: RAT
 * AUTHOR:  I.Kouvelas
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
#include "rat_types.h"
#include "rat_time.h"
#include "util.h"

typedef struct s_fast_time {
	int freq;
	u_int32	low;
	u_int32 high;
} ft_t;

typedef struct s_time {
	struct s_fast_time *ft;
	int	freq;
	int	shift;
	u_int32	offset;
} frtime_t;

ft_t *
new_fast_time(int freq)
{
	ft_t *ft;

	ft = (ft_t*)xmalloc(sizeof(ft_t));
	ft->freq = freq;
	ft->low = ft->high = 0;
	return (ft);
}

void
time_advance(ft_t *ft, int freq, u_int32 time)
{
	u_int32 tmp = ft->low;
	ft->low += ft->freq / freq * time;
	if (ft->low < tmp)
		ft->high++;
}

frtime_t *
new_time(ft_t *ft, int freq)
{
	frtime_t	*tp;

	tp = (frtime_t*)xmalloc(sizeof(frtime_t));
	tp->offset = 0;
	tp->freq = freq;
	tp->ft = ft;
	change_freq(tp, freq);

	return (tp);
}

void
free_time(frtime_t *tp)
{
	xfree(tp);
}

void
change_freq(frtime_t *tp, int freq)
{
	int	i, step;
	u_int32	old;

	assert(freq <= tp->ft->freq);

	old = get_time(tp);
	step = tp->ft->freq / freq;
	for (i = -1; step > 0; i++)
		step >>= 1;
	tp->shift = i;
	tp->freq = freq;
	tp->offset += old - get_time(tp);
}

int get_freq(frtime_t *tp)
{
	return (tp->freq);
}

u_int32
get_time(frtime_t *tp)
{
	u_int32	t;
	t = (tp->ft->low >> tp->shift) | (tp->ft->high << (sizeof(u_int32) * 8 - tp->shift));
	t += tp->offset;
	return (t);
}

/* This function assumes that the time converted is "close" to current time */
u_int32
convert_time(u_int32 ts, frtime_t *from, frtime_t *to)
{
	u_int32	now, diff, conv;
	int	s;

	now = get_time(from);
	if (ts_gt(ts, now))
		diff = ts - now;
	else
		diff = now - ts;

	s = to->shift - from->shift;
	if (s > 0)
		diff >>= s;
	else
		diff <<= -s;

	if (ts_gt(ts, now))
		conv = get_time(to) + diff;
	else
		conv = get_time(to) - diff;

	return (conv);
}

#define HALF_TS_CYCLE	0x80000000

/*
 * Compare two timestamps and return TRUE if t1 > t2 Assume that they are
 * close together (less than half a cycle) and handle wraparounds...
 */
int
ts_gt(u_int32 t1, u_int32 t2)
{
	u_int32         diff;

	diff = t1 - t2;
	return (diff < HALF_TS_CYCLE && diff != 0);
}

/*
 * Return the abolute difference of two timestamps. As above assume they are
 * close and handle wraprounds...
 */
u_int32
ts_abs_diff(u_int32 t1, u_int32 t2)
{
	u_int32         diff;

	diff = t1 - t2;
	if (diff > HALF_TS_CYCLE)
		diff = t2 - t1;

	return (diff);
}
