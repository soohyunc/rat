/*
 * FILE:     receive.h
 * PROGRAM:  RAT
 * AUTHOR:   Isidor Kouvelas
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

#ifndef _RECEIVE_H_
#define _RECEIVE_H_

#include "codec.h"
#include "channel.h"
#include "session.h"	

/* max forward fill in */
#define MAX_DUMMY 4	

/* Unit throughout receiver */
typedef struct rx_element_tag {
	struct rx_element_tag  *next_ptr;
	struct rx_element_tag  *prev_ptr;
	u_int32        	playoutpt;		     /* target play out point */
        u_int32         src_ts;                      /* source timestamp used to prevent jumps in playout
                                                      * breaking sequence of units into channel coders */
	int		dbe_source_count;	     /* Num elements of the following array that are used. Will be >= 1 */
	struct s_rtcp_dbentry   *dbe_source[16];     /* originator info */
	u_int32         unit_size;		     /* in samples */
        cc_unit        *ccu[MAX_CC_PER_INTERVAL];    /* channel coded units */
        int             ccu_cnt;                     /* number of channel coded units */
        int             cc_pt;                       /* payload of channel coded unit (needed by proding rx units only) */
        coded_unit	comp_data[MAX_ENCODINGS];    /* compressed data */
        int             comp_count;
	int             native_count;                /* Number of different types of decompressed data (only envisage 2) */
	sample*	        native_data[MAX_NATIVE];     /* 0 used for whatever codec uses, others for conversion output */ 
        u_int16         native_size[MAX_NATIVE];     /* size of raw audio blocks */
	int             dummy;                       /* Is a dummy unit */
	int		mixed;
	int             units_per_pckt;
	int             talk_spurt_start;	     /* talk spurt start marker */
} rx_queue_element_struct;

typedef struct rx_queue_tag {
	int                      queue_empty;
	rx_queue_element_struct *head_ptr;
	rx_queue_element_struct *tail_ptr;
} rx_queue_struct;

struct s_mix_info;
struct s_cushion_struct;
struct s_participant_playout_buffer;

rx_queue_element_struct*
new_rx_unit(void);

void playout_buffer_remove   (struct s_participant_playout_buffer **playout_buf_list, 
                              struct s_rtcp_dbentry *src);
void service_receiver        (struct session_tag *sp,
		              struct rx_queue_tag *receive_queue,
		              struct s_participant_playout_buffer **buf_list,
		              struct s_mix_info *ms);
void clear_old_history       (struct s_participant_playout_buffer **buf);
void destroy_playout_buffers (struct s_participant_playout_buffer **buf_list);
int  playout_buffer_endtime  (struct s_participant_playout_buffer *buf_list, 
                              struct s_rtcp_dbentry *src, 
                              u_int32* end_time);
int32 playout_buffer_duration(struct s_participant_playout_buffer *buf_list, 
                              struct s_rtcp_dbentry *src);
#endif /* _RECEIVE_H_ */
