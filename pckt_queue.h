/*
 * FILE:    pckt_queue.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
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

#ifndef _pckt_queue_h_
#define _pckt_queue_h_

#define PCKT_QUEUE_RTP_LEN  64
#define PCKT_QUEUE_RTCP_LEN 32

#include "ts.h"

struct s_pckt_queue;
struct s_rtcp_dbentry;

typedef struct {
	u_int8                 *pckt_ptr;
	int32                   len;
	u_int32                 arrival_timestamp;
        u_int32                 extlen;
        ts_t                    playout;
        struct  s_rtcp_dbentry *sender;
} pckt_queue_element;

pckt_queue_element*  pckt_queue_element_create (void);
void                 pckt_queue_element_free   (pckt_queue_element **pe);

struct s_pckt_queue* pckt_queue_create  (int len);
void                 pckt_queue_destroy (struct s_pckt_queue **p);
void                 pckt_queue_drain   (struct s_pckt_queue *p);
void                 pckt_enqueue       (struct s_pckt_queue *q, pckt_queue_element *pe);
pckt_queue_element*  pckt_dequeue       (struct s_pckt_queue *q);

#endif	/* _pckt_queue_h_ */

