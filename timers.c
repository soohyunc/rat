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
 */

#include "config_unix.h"
#include "config_win32.h"
#include "timers.h"
#include "memory.h"

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
