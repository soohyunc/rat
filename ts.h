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

__inline ts_t     ts_map32(u_int32 freq, u_int32 ts32);

/* Addition and subtraction operations */
__inline ts_t     ts_add      (ts_t ts1, ts_t ts2);
__inline ts_t     ts_sub      (ts_t ts1, ts_t ts2);
__inline ts_t     ts_abs_diff (ts_t ts1, ts_t ts2);

/* ts_gt = timestamp greater than */
__inline int      ts_gt(ts_t t1, ts_t t2);
__inline int      ts_eq(ts_t t1, ts_t t2);

/* ts_convert changes timebase of a timestamp */
__inline ts_t     ts_convert(u_int32 new_freq, ts_t ts);

/* Debugging functions */
__inline int      ts_valid(ts_t t1);
__inline u_int32  ts_get_freq(ts_t t1);

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
