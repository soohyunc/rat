/*
 * FILE:    cushion.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 * MODIFICATIONS: Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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
#include "debug.h"
#include "memory.h"
#include "cushion.h"
#include "audio.h"

/*
 * SAFETY is how safe we want to be with the device going dry. If we want to
 * cover for larger future jumps in workstation delay then SAFETY should be
 * larger. Sensible values are between 0.9 and 1
 */
#define SAFETY		0.90
#ifdef SunOS
#define CUSHION_STEP	160
#else
#define CUSHION_STEP	80
#endif
#define HISTORY_SIZE	250
#define MIN_COVER	((float)HISTORY_SIZE * SAFETY)

#define MAX_CUSHION	4000
#define MIN_CUSHION	320

/* All cushion measurements are in sampling intervals, not samples ! [oth] */

typedef struct s_cushion_struct {
	u_int32         cushion_estimate;
	u_int32         cushion_size;
	u_int32         cushion_step;
	u_int32        *read_history;	/* Circular buffer of read lengths */
	int             last_in;	/* Index of last added value */
	int            *histogram;	/* Histogram of read lengths */
        u_int32         histbins;      /* Number of bins in histogram */
} cushion_t;

int 
cushion_create(cushion_t **c, int blockdur)
{
        int i;
        u_int32 *ip;
        cushion_t *nc;

        nc = (cushion_t*) xmalloc (sizeof(cushion_t));
        if (nc == NULL) goto bail_cushion;

        /* cushion operates independently of the number of channels */
        assert(blockdur > 0);

        nc->cushion_size     = 2 * blockdur;
	nc->cushion_estimate = blockdur;
	nc->cushion_step     = blockdur;
	nc->read_history     = (u_int32 *) xmalloc (HISTORY_SIZE * sizeof(u_int32));
        if (nc->read_history == NULL) goto bail_history;

	for (i = 0, ip = nc->read_history; i < HISTORY_SIZE; i++, ip++)
		*ip = 4;

        nc->histbins  = 16000 / blockdur;
	nc->histogram = (int *)xmalloc(nc->histbins * sizeof(int));
        if (nc->histogram == NULL) goto bail_histogram;

	memset(nc->histogram, 0, nc->histbins * sizeof(int));
	nc->histogram[4] = HISTORY_SIZE;
	nc->last_in = 0;

        *c = nc;
        return TRUE;
        /* error cleanups... */
        bail_histogram: xfree(nc->read_history);
        bail_history:   xfree(nc); 
        bail_cushion:   debug_msg("Cushion allocation failed.\n");
                        return FALSE;
}

void
cushion_destroy(cushion_t *c)
{
        xfree(c->read_history);
        xfree(c->histogram);
        xfree(c);
}

void
cushion_update(cushion_t *c, u_int32 read_dur, int mode)
{
        u_int32 idx, cnt, cover_idx, cover_cnt; 
        u_int32 lower, upper; 

        /* remove entry we are about to overwrite from histogram */
        
        if (c->read_history[c->last_in] < c->histbins) {
                c->histogram[ c->read_history[c->last_in] ]--;
        } else {
                c->histogram[ c->read_history[c->histbins - 1] ]--;
        }

        /* slot in new entry and update histogram */
	c->read_history[c->last_in] = read_dur / c->cushion_step;
        if (c->read_history[c->last_in] < c->histbins) {
                c->histogram[ c->read_history[c->last_in] ]++;
        } else {
                c->histogram[ c->read_history[c->histbins - 1] ]++;
                debug_msg("WE ARE NOT KEEPING UP IN REAL-TIME\n");
        }
	c->last_in++;
	if (c->last_in == HISTORY_SIZE) {
		c->last_in = 0;
        }

        /* Find lower and upper bounds for cushion... */
        idx = cnt = cover_idx = cover_cnt = 0;
        while(idx < c->histbins && cnt < HISTORY_SIZE) {
                if (cover_cnt < MIN_COVER) {
                        cover_cnt += c->histogram[idx];
                        cover_idx  = idx;
                }
                cnt += c->histogram[idx];
                idx++;
        }
        
        if (mode == CUSHION_MODE_LECTURE) {
                lower = (cover_idx + 10) * c->cushion_step;
                upper = (idx       + 10) * c->cushion_step;
        } else {
                lower = (cover_idx + 2) * c->cushion_step;
                upper = idx * c->cushion_step;
        }

        /* it's a weird world :D lower can be above upper */
        c->cushion_estimate = min(lower,upper);

        if (c->cushion_estimate < 2 * c->cushion_step) {
                c->cushion_estimate = 2 * c->cushion_step;
        }

        /* Ignore first read from the device after startup */
	if (c->cushion_size == 0) {
		c->cushion_estimate = c->cushion_step;    
        }
}

static void
cushion_size_check(cushion_t *c)
{
        if (c->cushion_size < MIN_CUSHION) {
                c->cushion_size = MIN_CUSHION;
#ifdef DEBUG_CUSHION
                debug_msg("cushion boosted.");
#endif
        } else if (c->cushion_size > MAX_CUSHION) {
                c->cushion_size = MAX_CUSHION;
#ifdef DEBUG_CUSHION
                debug_msg("cushion clipped.\n");
#endif
        }
}

u_int32 
cushion_get_size(cushion_t *c)
{
        return c->cushion_size;
}

u_int32
cushion_set_size(cushion_t *c, u_int32 new_size)
{
        c->cushion_size = new_size;
        cushion_size_check(c);
#ifdef DEBUG_CUSHION
        debug_msg("cushion size %ld\n", new_size);
#endif
        return c->cushion_size;
}

u_int32
cushion_step_up(cushion_t *c)
{
        c->cushion_size += c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

u_int32
cushion_step_down(cushion_t *c)
{
        c->cushion_size -= c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

u_int32
cushion_get_step(cushion_t *c)
{
        return c->cushion_step;
}

u_int32 
cushion_use_estimate(cushion_t *c)
{
        c->cushion_size = c->cushion_estimate + c->cushion_step 
                - (c->cushion_estimate % c->cushion_step);
        cushion_size_check(c);
#ifdef DEBUG_CUSHION
        debug_msg("cushion using size %ld\n", c->cushion_size);
#endif
        return c->cushion_size;
}

int32 
cushion_diff_estimate_size(cushion_t *c)
{
        return (c->cushion_estimate - c->cushion_size);
}

