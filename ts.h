/*
 * FILE:    ts.h
 * AUTHORS: Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#ifndef __TS_H__
#define __TS_H__

typedef struct {
        u_int32 ticks:25;
        u_int32 check:3;
        u_int32 idx:4;
} ts_t;

/* Maps a 32 bit unsigned integer into a valid timestamp.
 * This be used for mapping offsets, not timestamps (see
 * below) */

ts_t     ts_map32(u_int32 freq, u_int32 ts32);

/* Common Operations */
ts_t     ts_add      (ts_t ts1, ts_t ts2);
ts_t     ts_sub      (ts_t ts1, ts_t ts2);
ts_t     ts_abs_diff (ts_t ts1, ts_t ts2);

/* Operations for use on offets, i.e. small ts values */
ts_t     ts_mul      (ts_t ts,  u_int32 x);
ts_t     ts_div      (ts_t ts,  u_int32 x);

/* ts_gt = timestamp greater than */
int      ts_gt(ts_t t1, ts_t t2);
int      ts_eq(ts_t t1, ts_t t2);

/* ts_convert changes timebase of a timestamp */
ts_t     ts_convert(u_int32 new_freq, ts_t ts);

/* Conversion to milliseconds */
u_int32  ts_to_ms(ts_t t1);

/* Conversion to microseconds */
u_int32  ts_to_us(ts_t t1);

/* Debugging functions */
int      ts_valid(ts_t t1);
u_int32  ts_get_freq(ts_t t1);

typedef struct {
        ts_t    last_ts;
        u_int32 last_32;
        u_int32 freq;
} ts_sequencer;

/* These functions should be used for mapping sequences of
 * 32 bit timestamps to ts_t and vice-versa.
 */

ts_t    ts_seq32_in  (ts_sequencer *s, u_int32 f, u_int32 curr_32);
u_int32 ts_seq32_out (ts_sequencer *s, u_int32 f, ts_t    curr_ts);

#endif
