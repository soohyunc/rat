/*
 * FILE:    interfaces.h
 * PROGRAM: RAT
 * AUTHOR:  V.J.Hardman
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

#ifndef _interfaces_h_
#define _interfaces_h_

#include "rat_types.h"

/* Packets off the net */
typedef struct pckt_queue_element_tag {
	struct pckt_queue_element_tag *next_pckt_ptr;
	struct pckt_queue_element_tag *prev_pckt_ptr;
	u_int32                        addr;
	int                            len;
	u_int32                        arrival_timestamp;
	u_int8                          *pckt_ptr;
} pckt_queue_element_struct;

typedef struct pckt_queue_tag {
	int                        queue_empty;
	pckt_queue_element_struct *head_ptr;
	pckt_queue_element_struct *tail_ptr;
} pckt_queue_struct;

struct rx_element_tag;
struct rx_queue_tag;
struct pckt_queue_element_tag;
struct pckt_queue_tag;

void	put_on_rx_queue(struct rx_element_tag* p_ptr, struct rx_queue_tag *q_ptr);
struct rx_element_tag *get_unit_off_rx_queue(struct rx_queue_tag *q_ptr);
void	free_rx_unit(struct rx_element_tag **temp_ptr);

void	free_pckt_queue_element(struct pckt_queue_element_tag **temp_ptr);
void    put_on_pckt_queue(struct pckt_queue_element_tag *pckt,
			  struct pckt_queue_tag *q_ptr);
struct pckt_queue_element_tag *get_pckt_off_queue(struct pckt_queue_tag *q_ptr);

void receive_pkt_audit(struct pckt_queue_tag *pkt_queue_ptr);
void receive_unit_audit(struct rx_queue_tag *unit_queue_ptr);

#endif	/* _interfaces_h_ */

