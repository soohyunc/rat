/*
 * FILE:    statistics.h
 * PROGRAM: RAT
 * AUTHOR:  V.J.Hardman
 * CREATED: 23/03/95
 *
 * $Revision$
 * $Date$
 *
 * This module houses the routine from the main execution loop that monitors
 * lost packets, duplicated packets etc.
 *
 * This routine fixes duplications - by throwing away the duplicates
 *
 * This module also contains irregularly executed stats analysis routines
 *
 * Input queue: netrx_queue Output Queue: receive_queue Stats Queue: Stats queue
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


#ifndef _rat_statistics_h_
#define _rat_statistics_h_

struct session_tag;
struct pckt_queue_tag;
struct rx_queue_tag;
struct s_cushion_struct;

void statistics(struct session_tag *session_pointer, 
		struct pckt_queue_tag *netrx_pckt_queue,
		struct rx_queue_tag *unitsrx_queue_ptr,
		struct s_cushion_struct *cushion,
		u_int32 real_time);

#endif /* _rat_statistics_h_ */

