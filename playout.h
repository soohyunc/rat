/*
 * FILE:    playout.h
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


#ifndef __UCLMM_PLAYOUT_BUFFER_H__
#define __UCLMM_PLAYOUT_BUFFER_H__

#include "ts.h"

struct  s_pb;
struct  s_pb_iterator;

typedef void (*playoutfreeproc)(u_char**, u_int32);

/* All functions return TRUE on success, and FALSE on failure */
int pb_create  (struct s_pb     **pb, 
                playoutfreeproc   callback);

int pb_destroy (struct s_pb **pb);

int pb_add    (struct s_pb *pb, 
               u_char*      data, 
               u_int32      datalen,
               ts_t         playout);

void pb_flush (struct s_pb *pb);

/*
 * These following three functions return data stored in the playout buffer.  
 * The playout buffer has a playout point iterator.  playout_buffer_get 
 * returns the data at that point, advance steps to the next unit and 
 * returns that, and rewind steps to the previous unit
 * and returns that.
 */

int
pb_iterator_create (struct s_pb           *pb,
                 struct s_pb_iterator **pbi);
  
void
pb_iterator_destroy (struct s_pb           *pb,
                     struct s_pb_iterator **pbi);

int
pb_iterator_dup (struct s_pb_iterator **pbi_dst,
                 struct s_pb_iterator *pbi_src);

int
pb_iterator_get_at (struct s_pb_iterator *pbi,
                    u_char              **data,
                    u_int32              *datalen, 
                    ts_t                 *playout);

int
pb_iterator_detach_at (struct s_pb_iterator *pbi,
                       u_char              **data,
                       u_int32              *datalen, 
                       ts_t                 *playout);

/* Single step movements */
int
pb_iterator_advance (struct s_pb_iterator *pbi);

int
pb_iterator_retreat (struct s_pb_iterator *pbi);

/* Shift to head / tail */

int
pb_iterator_ffwd (struct s_pb_iterator *pbi);

int
pb_iterator_rwd  (struct s_pb_iterator *pbi);

/* Trims data more than history_len before iterator */
int 
pb_iterator_audit (struct s_pb_iterator *pi,
                   ts_t                  history_len);

/* Return whether 2 iterators refer to same time interval */
int
pb_iterators_equal(struct s_pb_iterator *pi1,
                   struct s_pb_iterator *pi2);

/* Returns whether playout buffer has data to be played out */
int 
pb_relevant (struct s_pb *pb, 
             ts_t         now);

/* Return the times of interest for playout buffer in ts, returns
 * TRUE or FALSE depending on whether request successful
 */
__inline int pb_get_start_ts     (struct s_pb *pb, ts_t *ts);
__inline int pb_get_end_ts       (struct s_pb *pb, ts_t *ts);
__inline int pb_iterator_get_ts  (struct s_pb_iterator *pbi, ts_t *ts);

#endif /* __UCLMM_PLAYOUT_BUFFER_H__ */
