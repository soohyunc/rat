/*
 * FILE:    playout_calc.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "ts.h"
#include "session.h"
#include "cushion.h"
#include "pdb.h"
#include "playout_calc.h"

static ts_t
playout_variable_component(session_t *sp, pdb_entry_t *e)
{
        u_int32 var32, freq, cushion;

        freq  = get_freq(e->clock);
        var32 = e->inter_pkt_gap;
        
        cushion = cushion_get_size(sp->cushion);
        if (var32 < cushion) {
                var32 = 3 * cushion / 2;
        }
        
        if (sp->limit_playout) {
                u_int32 minv, maxv;
                minv = sp->min_playout * freq / 1000;
                maxv = sp->max_playout * freq / 1000;
                var32 = max(minv, var32);
                var32 = min(maxv, var32);
        }

        return ts_map32(freq, var32);
} 


ts_t 
playout_calc(session_t *sp, u_int32 ssrc, ts_t src_ts, int new_spurt)
{
        pdb_entry_t *e;
        ts_t         transit, var;

        pdb_item_get(sp->pdb, ssrc, &e);
        assert(e != NULL);

        transit = ts_sub(sp->cur_ts, src_ts);
        if (new_spurt == TRUE) {
                /* Wildly optimistic jitter estimate */
                e->jitter       = ts_map32(8000, 10); 
                e->transit      = transit;
                e->last_transit = transit;
                var = playout_variable_component(sp, e);
                if (ts_gt(var, e->jitter)) {
                        e->playout = var;
                } else {
                        e->playout = e->jitter;
                }
                e->playout = ts_add(transit, e->playout);
        } else {
                ts_t delta_transit;
                delta_transit   = ts_abs_diff(transit, e->last_transit);
                e->last_transit = e->transit;
                /* Update jitter estimate using                             */
                /*                  jitter = (7/8)jitter + (1/8) new_jitter */
                e->jitter = ts_mul(e->jitter, 7);
                e->jitter = ts_add(e->jitter, delta_transit);     
                e->jitter = ts_div(e->jitter, 8);
        }
        return ts_add(src_ts, e->playout);
}
