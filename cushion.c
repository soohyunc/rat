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
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "cushion.h"
#include "codec_types.h"
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
	uint32_t         cushion_estimate;
	uint32_t         cushion_size;
	uint32_t         cushion_step;
	uint32_t        *read_history;	/* Circular buffer of read lengths */
	int             last_in;	/* Index of last added value */
	int            *histogram;	/* Histogram of read lengths */
        uint32_t         histbins;      /* Number of bins in histogram */
} cushion_t;

int 
cushion_create(cushion_t **c, int blockdur)
{
        int i;
        uint32_t *ip;
        cushion_t *nc;

        nc = (cushion_t*) xmalloc (sizeof(cushion_t));
        if (nc == NULL) goto bail_cushion;

        /* cushion operates independently of the number of channels */
        assert(blockdur > 0);

        nc->cushion_size     = 2 * blockdur;
	nc->cushion_estimate = blockdur;
	nc->cushion_step     = blockdur / 2;
	nc->read_history     = (uint32_t *) xmalloc (HISTORY_SIZE * sizeof(uint32_t));
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
cushion_destroy(cushion_t **ppc)
{
        cushion_t *pc;
        assert(ppc);
        pc = *ppc;
        assert(pc);
        xfree(pc->read_history);
        xfree(pc->histogram);
        xfree(pc);
        *ppc = NULL;
}

void
cushion_update(cushion_t *c, uint32_t read_dur, int mode)
{
        uint32_t idx, cnt, cover_idx, cover_cnt; 
        uint32_t lower, upper; 

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

uint32_t 
cushion_get_size(cushion_t *c)
{
        return c->cushion_size;
}

uint32_t
cushion_set_size(cushion_t *c, uint32_t new_size)
{
        c->cushion_size = new_size;
        cushion_size_check(c);
#ifdef DEBUG_CUSHION
        debug_msg("cushion size %ld\n", new_size);
#endif
        return c->cushion_size;
}

uint32_t
cushion_step_up(cushion_t *c)
{
        c->cushion_size += c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

uint32_t
cushion_step_down(cushion_t *c)
{
        c->cushion_size -= c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

uint32_t
cushion_get_step(cushion_t *c)
{
        return c->cushion_step;
}

uint32_t 
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

int32_t 
cushion_diff_estimate_size(cushion_t *c)
{
        return (c->cushion_estimate - c->cushion_size);
}

