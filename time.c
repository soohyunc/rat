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
	int	scale;
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
	ft->low += time * ft->freq / freq;
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
        tp->scale = 1;
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
	u_int32	old;

	assert(freq <= tp->ft->freq);

	old = get_time(tp);
        tp->scale = tp->ft->freq/freq;
	tp->freq = freq;
	tp->offset += old - get_time(tp);
}

int 
get_freq(frtime_t *tp)
{
	return (tp->freq);
}

/* Calculates time scaled to tp->freq units */
u_int32
get_time(frtime_t *tp)
{
	u_int32	t;
	t = tp->ft->low / tp->scale + tp->ft->high * tp->scale;
	t += tp->offset;
	return (t);
}

/* This function assumes that the time converted is "close" to current time */
u_int32
convert_time(u_int32 ts, frtime_t *from, frtime_t *to)
{
	u_int32	now, diff, conv;

	now = get_time(from);
	if (ts_gt(ts, now))
		diff = ts - now;
	else
		diff = now - ts;

        if (to->scale>from->scale)
            diff = diff * to->scale / from->scale;
        else 
            diff = diff * from->scale / to->scale;

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

int
ts_cmp(u_int32 ts1, u_int32 ts2)
{
        u_int32 d1,d2,dmin;

        d1 = ts2 - ts1;
        d2 = ts1 - ts2;

        /* look for smallest difference between ts (timestamps are unsigned) */
        if (d1<d2) {
                dmin = d1;
        } else if (d1>d2) {
                dmin = d2;
        } else {
                dmin = 0;
        }

        /* if either d1 or d2 have wrapped dmin > HALF_TS_CYCLE */

        if (dmin<HALF_TS_CYCLE) {
                if (ts1>ts2) return  1;
                if (ts1<ts2) return -1;
                return 0;
        } else if (ts1<ts2) {
                /* Believe t1 to have been wrapped and therefore greater */
                return 1; 
        } else {
                /* believe t2 to have been wrapped and therefore greater */
                return -1;
        }
}
