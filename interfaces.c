/*
 * FILE:    interfaces.c
 * PROGRAM: RAT
 * AUTHOR:  V.J.Hardman
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

#include "config_unix.h"
#include "config_win32.h"
#include "codec_types.h"
#include "codec.h"
#include "channel.h"
#include "session.h"
#include "receive.h"
#include "util.h"
#include "audio.h"

#include "interfaces.h"

/* This routine assumes a doubly linked list */
void
put_on_rx_queue(rx_queue_element_struct *p_ptr, rx_queue_struct *q_ptr)
{
	rx_queue_element_struct *s;
	/* put packet on the queue */
        assert(p_ptr->cc_pt < 128);
	p_ptr->next_ptr = q_ptr->head_ptr;
	q_ptr->head_ptr = p_ptr;
	if (q_ptr->tail_ptr == NULL) {
		q_ptr->tail_ptr = q_ptr->head_ptr;	/* one packet on queue */
		q_ptr->queue_empty = FALSE;
	} else {		/* must set up second element on queue to
				 * point to first */
		s = q_ptr->head_ptr->next_ptr;
		s->prev_ptr = p_ptr;
	}
	p_ptr = NULL;		/* Just for checking purposes */
}

rx_queue_element_struct *
get_unit_off_rx_queue(rx_queue_struct * q_ptr)
{
	rx_queue_element_struct *p;
	rx_queue_element_struct *s;

	/* The rx queue must exist [csp] */
	assert(q_ptr != NULL);

	/* Get packet off queue */
	p = q_ptr->tail_ptr;
	if (p == NULL) {			/* queue is emtpy? */
		return NULL;
	} else if (p->prev_ptr != NULL) {	/* other units on the queue */
		s = p->prev_ptr;
		s->next_ptr = NULL;		/* patch up end of queue */
		q_ptr->tail_ptr = s;
	} else {				/* pckt is only one on queue */
		q_ptr->head_ptr = NULL;
		q_ptr->tail_ptr = NULL;
		/* packets not waiting to be encoded */
		q_ptr->queue_empty = TRUE;
	}

	p->next_ptr = NULL;	/* no home to go to */
	p->prev_ptr = NULL;	/* or to come from */
	return (p);
}

void
free_rx_unit(rx_queue_element_struct **temp_ptr)
{
        int i;
        
        for(i=0;i<(*temp_ptr)->comp_count;i++) 
            codec_clear_coded_unit(&(*temp_ptr)->comp_data[i]);

        if ((*temp_ptr)->ccu[0] != NULL) assert((*temp_ptr)->ccu_cnt);
        for(i=0;i<(*temp_ptr)->ccu_cnt;i++) {
            clear_cc_unit((*temp_ptr)->ccu[i],0);
            block_free((*temp_ptr)->ccu[i], sizeof(cc_unit));
        }

	for (i = 0; i < (*temp_ptr)->native_count; i++) {
		if ((*temp_ptr)->native_data[i]) {
			block_free((*temp_ptr)->native_data[i], (*temp_ptr)->native_size[i]);
		}
	}

        if ((*temp_ptr)->next_ptr != NULL) {
                (*temp_ptr)->next_ptr->prev_ptr = NULL;
        }

        if ((*temp_ptr)->prev_ptr != NULL) {
                (*temp_ptr)->prev_ptr->next_ptr = NULL;
        }
        
	block_free(*temp_ptr, sizeof(rx_queue_element_struct));
	(*temp_ptr) = (rx_queue_element_struct *) 1;	/* for debugging purposes */
}

void 
receive_unit_audit(rx_queue_struct * unit_queue_ptr)
{
	rx_queue_element_struct *temp_ptr;
	while (unit_queue_ptr->queue_empty != TRUE) {
		temp_ptr = get_unit_off_rx_queue(unit_queue_ptr);
		free_rx_unit(&temp_ptr);
	}
}

