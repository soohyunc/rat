/*
 * FILE:    playout_calc.c
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

#define PLAYOUT_JITTER_SCALE 2

static ts_t
playout_variable_component(session_t *sp, pdb_entry_t *e)
/*****************************************************************************/
/* playout_variable component works out the variable components that RAT has */
/* in it's playout calculation.  It works returns the maximum of:            */
/* the interpacket gap, the cushion size, and the user requested limits on   */
/* the playout point.                                                        */
/*****************************************************************************/
{
        u_int32 var32, freq, cushion;

        freq  = get_freq(e->clock);
        var32 = e->inter_pkt_gap / 2;

        cushion = cushion_get_size(sp->cushion);
        if (var32 < cushion) {
                var32 = cushion;
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
playout_calc(session_t *sp, u_int32 ssrc, ts_t transit, int new_spurt)
/*****************************************************************************/
/* The primary purpose of this function is to calculate the playout point    */
/* for new talkspurts (new_spurt).  It also maintains the jitter and transit */
/* time estimates from the source to us.  The value returned is the local    */
/* playout time.                                                             */
/*****************************************************************************/
{
        ts_t delta_transit;
        pdb_entry_t *e;

        pdb_item_get(sp->pdb, ssrc, &e);
        assert(e != NULL);

        delta_transit = ts_abs_diff(transit, e->avg_transit);
        if (ts_gt(transit, e->avg_transit)) {
                e->avg_transit = ts_add(e->avg_transit, ts_div(delta_transit,16));
        } else {
                e->avg_transit = ts_sub(e->avg_transit, ts_div(delta_transit,16));
        }

        if (ts_gt(ts_abs_diff(transit, e->avg_transit), ts_mul(e->jitter, 5)) && new_spurt) {
                e->avg_transit = transit;
        }

        if (new_spurt == TRUE) {
                ts_t hvar, jvar; /* Host and jitter components       */
                debug_msg("New talkspurt\n");
                hvar = playout_variable_component(sp, e);
                jvar = ts_mul(e->jitter, PLAYOUT_JITTER_SCALE);
                if (ts_gt(hvar, jvar)) {
                        e->playout = hvar;
                } else {
                        e->playout = jvar;
                }
                e->transit = e->avg_transit;
        } else {
                /* delta_transit is abs((s[j+1] - d[j+1]) - (s[j] - d[j]))  */
                delta_transit   = ts_abs_diff(transit, e->last_transit);
                /* Update jitter estimate using                             */
                /*                  jitter = (7/8)jitter + (1/8) new_jitter */
                e->jitter = ts_mul(e->jitter, 7);
                e->jitter = ts_add(e->jitter, delta_transit);     
                e->jitter = ts_div(e->jitter, 8);

        }
        e->last_transit = transit;

        return e->playout;
}
