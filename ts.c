/*
 * FILE:    ts.c
 * AUTHORS: Orion Hodson
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
#include "ts.h"

typedef struct {
        u_int32 freq;
        u_int32 wrap;
} ticker;

/* Each timebase has a range corresponding to 0..N seconds.  Depending
 * on the frequency this represents a differing number of ticks.  So
 * an 8 kHz clock has ticks ranging from 0..M, a 16kHz clock has ticks
 * ranging from 0..2M.  We can compare timestamps simply by scaling up
 * from lower frequency clocks to higher frequency clocks.
 *
 * As defined in ts.h we use 25 bits as full range of ticks.  In
 * reality, the highest frequency clock coded (90k) uses just under
 * the full 25 bit range ,0..floor (2^25-1 / 90000). All other clocks use
 * less than this.  The range corresponds to 372 seconds which is ample for
 * media playout concerns.
 *
 * NB. The tickers must be frequency ordered - comparison code depends
 * on it!  
 */
 
ticker tickers[] = {
        {   8000, 0x002d6900 },
        {  11025, 0x003e94b4 },
        {  16000, 0x005ad200 },
        {  22050, 0x007d2968 },
        {  24000, 0x00883b00 },
        {  32000, 0x00b5a400 },
        {  40000, 0x00e30d00 },
        {  44100, 0x00fa52d0 },
        {  48000, 0x01107600 },
        {  90000, 0x01fedd40 }
};

#define TS_NUM_TICKERS (sizeof(tickers)/sizeof(ticker))

#define TS_CHECK_BITS 0x07

ts_t
ts_map32(u_int32 freq, u_int32 ticks32)
{
        u_int32 i;
        ts_t out;

        /* Make invalid timestamp */
        out.check = ~TS_CHECK_BITS;

        for(i = 0; i < TS_NUM_TICKERS; i++) {
                if (tickers[i].freq == freq) {
                        out.ticks = ticks32 % tickers[i].wrap;
                        out.check = TS_CHECK_BITS;
                        out.idx   = i;
                        break;
                }
        }
        assert(ts_valid(out));
        return out;
}

static ts_t
ts_rebase(u_int32 new_idx, ts_t t)
{
        /* Use 64 bit quantity as temporary since 
         * we are multiplying a 25 bit quantity by a
         * 16 bit one.  Only have to do this as
         * frequencies are not all multiples of each
         * other.
         */

        int64 new_ticks;

        assert(new_idx < TS_NUM_TICKERS);

        /* new_ticks = old_ticks * new_freq / old_freq */
        new_ticks  = (int64)t.ticks * tickers[new_idx].freq;
        new_ticks /= tickers[t.idx].freq;

        /* Bound tick range */
        new_ticks %= (u_int32)tickers[new_idx].wrap;

        /* Update ts fields */
        t.ticks   = (u_int32)new_ticks;
        t.idx     = new_idx;

        return t;
}

int
ts_gt(ts_t t1, ts_t t2)
{
        u_int32 half_range, x1, x2;
        
        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase((unsigned)t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase((unsigned)t2.idx, t1);
        }

        half_range = tickers[t1.idx].wrap >> 1;        

        x1 = t1.ticks;
        x2 = t2.ticks;

        if (x1 > x2) {
                return (x1 - x2) < half_range;
        } else {
                return (x2 - x1) > half_range;
        }
}

int
ts_eq(ts_t t1, ts_t t2)
{
        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase((unsigned)t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase((unsigned)t2.idx, t1);
        }

        return (t2.ticks == t1.ticks);
}

ts_t
ts_add(ts_t t1, ts_t t2)
{
        u_int32 ticks;
        assert(ts_valid(t1));        
        assert(ts_valid(t2));
        
        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase(t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase(t2.idx, t1);
        }
        assert(t1.idx == t2.idx);

        ticks    = (t1.ticks + t2.ticks) % tickers[t1.idx].wrap;
        t1.ticks = ticks;

        return t1;
}

ts_t
ts_sub(ts_t t1, ts_t t2)
{
        ts_t out;
        u_int32 ticks;

        assert(ts_valid(t1));        
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase(t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase(t2.idx, t1);
        }

        assert(t1.idx == t2.idx);

        if (t1.ticks < t2.ticks) {
                /* Handle wrap */
                ticks = t1.ticks + tickers[t1.idx].wrap - t2.ticks; 
        } else {
                ticks = t1.ticks - t2.ticks;
        }
        out.idx   = t1.idx;
        out.check = TS_CHECK_BITS;
        assert(ticks < tickers[t1.idx].wrap);
        assert((ticks & 0xfe000000) == 0);
        out.ticks = ticks;
        assert((unsigned)out.ticks == ticks);
        assert(ts_valid(out));
        return out;
}

ts_t
ts_abs_diff(ts_t t1, ts_t t2)
{
        if (ts_gt(t1, t2)) {
                return ts_sub(t1, t2);
        } else {
                return ts_sub(t2, t1);
        }
}

ts_t 
ts_convert(u_int32 new_freq, ts_t ts)
{
        u_int32 i;
        ts_t out;
        
        out.check = 0;

        for(i = 0; i < TS_NUM_TICKERS; i++) {
                if (tickers[i].freq == new_freq) {
                        out = ts_rebase(i, ts);
                        break;
                }
        }

        assert(ts_valid(out));

        return out;
}

u_int32
ts_to_ms(ts_t t1)
{
        u_int32 r;
        assert(ts_valid(t1));
        r = t1.ticks / (ts_get_freq(t1) / 1000);
        return r;
}


int 
ts_valid(ts_t t1)
{
        return ((unsigned)t1.idx < TS_NUM_TICKERS && 
                (t1.check == TS_CHECK_BITS) &&
                (unsigned)t1.ticks < tickers[t1.idx].wrap);
}

u_int32
ts_get_freq(ts_t t1)
{
        assert(ts_valid(t1));
        return tickers[t1.idx].freq;
}

/* ts_map32_in and ts_map32_out are used to map between 32bit clock
 * and timestamp type which is modulo M.  Because the boundaries of
 * the timestamping wraps do not coincide, we cache last translated
 * value and add relative difference to other timestamp.  The application
 * does not then have to deal with discontinuities in timestamps.
 */

#define TS_WRAP_32 0x7fffffff

static 
int ts32_gt(u_int32 a, u_int32 b)
{
        u_int32 diff;
        diff = a - b;
        return (diff < TS_WRAP_32 && diff != 0);
}

ts_t
ts_seq32_in(ts_sequencer *s, u_int32 freq, u_int32 curr_32)
{
        u_int32 delta_32;
        ts_t    delta_ts; 

        /* Inited or freq changed check */
        if (s->freq != freq || !ts_valid(s->last_ts)) {
                s->last_ts = ts_map32(freq, lrand48());
                s->last_32 = curr_32;
                s->freq    = freq;
                return s->last_ts;
        }

        /* Find difference in 32 bit timestamps, scale to ts_t size
         * and add to last returned timestamp.
         */
        
        if (ts32_gt(curr_32, s->last_32)) {
                delta_32   = curr_32 - s->last_32;
                delta_ts   = ts_map32(freq, delta_32);
                s->last_ts = ts_add(s->last_ts, delta_ts);
        } else {
                delta_32   = s->last_32 - curr_32;
                delta_ts   = ts_map32(freq, delta_32);
                s->last_ts = ts_sub(s->last_ts, delta_ts);
        }
        
        s->last_32 = curr_32;
        return s->last_ts;
}

u_int32
ts_seq32_out(ts_sequencer *s, u_int32 freq, ts_t curr_ts)
{
        u_int32 delta_32;
        ts_t    delta_ts; 

        /* Inited or freq change check */
        if (s->freq != freq || !ts_valid(s->last_ts)) {
                s->last_ts = curr_ts;
                s->last_32 = lrand48();
                s->freq    = freq;
                return s->last_32;
        }

        if (ts_gt(curr_ts, s->last_ts)) {
                delta_ts   = ts_sub(curr_ts, s->last_ts);
                delta_32   = delta_ts.ticks * ts_get_freq(delta_ts) / freq;
                s->last_32 = s->last_32 + delta_32;
        } else {
                delta_ts   = ts_sub(s->last_ts, curr_ts);
                delta_32   = delta_ts.ticks * ts_get_freq(delta_ts) / freq;
                s->last_32 = s->last_32 - delta_32;
        }

        s->last_ts = curr_ts;
        return s->last_32;
}

