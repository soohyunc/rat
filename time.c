/*
 * FILE:    time.c
 * PROGRAM: RAT
 * AUTHOR:  I.Kouvelas + O.Hodson
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

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "rat_types.h"
#include "rat_time.h"
#include "util.h"

#define HALF_TS_CYCLE	0x80000000

typedef struct s_fast_time {
	int freq;
	u_int32	low;
	u_int32 high;
} ft_t;

typedef struct s_time {
	struct s_fast_time *ft;
	int	freq;
	int	scale;
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
free_fast_time(ft_t *ft)
{
        xfree(ft);
}

__inline void
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
	tp->freq = freq;
	tp->ft = ft;
        tp->scale = 1;
	change_freq(tp, freq);
        assert(tp->scale != 0);
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
	assert(freq <= tp->ft->freq);

        tp->scale = tp->ft->freq/freq;
	tp->freq = freq;
}

__inline int 
get_freq(frtime_t *tp)
{
	return (tp->freq);
}

/* Calculates time scaled to tp->freq units */
__inline u_int32
get_time(frtime_t *tp)
{
	u_int32	t;
	t = tp->ft->low / tp->scale + tp->ft->high * tp->scale;
	return (t);
}

u_int32
convert_time(u_int32 ts, frtime_t *from, frtime_t *to)
{
        assert(to->freq % from->freq == 0 || from->freq % to->freq == 0);

        if (from->freq == to->freq) {
                return ts;
        } else if (from->freq < to->freq) {
                return ts * (to->freq / from->freq);
        } else {
                return ts / (from->freq / to->freq);
        }
}

/*
 * Compare two timestamps and return TRUE if t1 > t2 Assume that they are
 * close together (less than half a cycle) and handle wraparounds...
 */
__inline int
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
__inline u_int32
ts_abs_diff(u_int32 t1, u_int32 t2)
{
	u_int32         diff;

	diff = t1 - t2;
	if (diff > HALF_TS_CYCLE)
		diff = t2 - t1;

	return (diff);
}

