/*
 * FILE:    receive.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Orion Hodson
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

#include "config.h"
#include "receive.h"
#include "interfaces.h"
#include "util.h"
#include "rat_time.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "mix.h"
#include "audio.h"
#include "lbl_confbus.h"
#include "transmit.h"

typedef struct s_participant_playout_buffer {
	struct s_participant_playout_buffer *next;
	struct s_rtcp_dbentry *src;
	rx_queue_element_struct *head_ptr;
	rx_queue_element_struct *tail_ptr;
	rx_queue_element_struct *last_to_get;
} ppb_t;

rx_queue_element_struct *
new_rx_unit()
{
	rx_queue_element_struct *u;
	u = (rx_queue_element_struct *)block_alloc(sizeof(rx_queue_element_struct));
	memset(u, 0, sizeof(rx_queue_element_struct));
	return (u);
}

static void
add_unit_to_interval(rx_queue_element_struct *ip, rx_queue_element_struct *ru)
{
        int i;
        
	assert(ru->comp_count == 1);
	ip->talk_spurt_start |= ru->talk_spurt_start;
	ip->talk_spurt_end |= ru->talk_spurt_end;
	if (ip->comp_count == MAX_ENCODINGS) {
		free_rx_unit(&ru);
		return;
	}

	if (ip->comp_count > 0
	    && !codec_compatible(ip->comp_data[0].cp, ru->comp_data[0].cp)) {
#ifdef DEBUG
		fprintf(stderr, "Incompatible unit received for same interval!\n");
#endif
		free_rx_unit(&ru);
		return;
	}

	for (i = ip->comp_count - 1; i >= 0; i--) {
		if (ru->comp_data[0].cp->value < ip->comp_data[i].cp->value)
			break;
		memcpy(&ip->comp_data[i + 1], &ip->comp_data[i], sizeof(coded_unit));
	}
	memcpy(&ip->comp_data[i + 1], &ru->comp_data[0], sizeof(coded_unit));
	memset(&ru->comp_data[0], 0, sizeof(coded_unit));
	ru->comp_count = 0;
	ip->comp_count++;
	free_rx_unit(&ru);
}

static rx_queue_element_struct *
add_or_get_interval(ppb_t *buf, rx_queue_element_struct *ru)
{
	rx_queue_element_struct	**ipp;

	ipp = &buf->head_ptr;
	while (*ipp && ts_gt(ru->playoutpt, (*ipp)->playoutpt))
		ipp = &((*ipp)->next_ptr);

	if (*ipp == NULL || (*ipp)->playoutpt != ru->playoutpt) {
		ru->next_ptr = *ipp;
		if (*ipp == NULL) {
			ru->prev_ptr = buf->tail_ptr;
			buf->tail_ptr = ru;
		} else {
			ru->prev_ptr = (*ipp)->prev_ptr;
			(*ipp)->prev_ptr = ru;
		}
		*ipp = ru;
	}
	return (*ipp);
}

static rx_queue_element_struct *
playout_buffer_add(ppb_t *buf, rx_queue_element_struct *ru)
{
	int			i;
	rx_queue_element_struct	*tp, *ip;

	if ((ip = add_or_get_interval(buf, ru)) != ru) {
		add_unit_to_interval(ip, ru);
	}

	/* Check here if there is any unit overlap */

	ru = ip;
	if (ru->talk_spurt_start == FALSE) {
		for (i = 0; i < MAX_DUMMY; i++) {
			if (ip->prev_ptr == NULL || ts_gt(ip->playoutpt - ip->unit_size, ip->prev_ptr->playoutpt + ip->prev_ptr->unit_size)) {
				tp = new_rx_unit();
				tp->dbe_source_count = ru->dbe_source_count;
				memcpy(tp->dbe_source, ru->dbe_source, ru->dbe_source_count * sizeof(rtcp_dbentry *));
				tp->playoutpt = ip->playoutpt - ip->unit_size;
				tp->comp_count = 0;
				tp->dummy = FALSE;
				tp->unit_size = ru->unit_size;
				assert(add_or_get_interval(buf, tp) == tp);
				ip = tp;
			} else
				break;
		}
	}
	return (ru);
}

static rx_queue_element_struct *
playout_buffer_get(ppb_t *buf, u_int32 from, u_int32 to)
{
	rx_queue_element_struct *ip;

	if (buf->last_to_get == NULL) {
		ip = buf->head_ptr;
	} else {
		ip = buf->last_to_get->next_ptr;
	}
	while (ip && ts_gt(from, ip->playoutpt)) {
		buf->last_to_get = ip;
		ip = ip->next_ptr;
	}

	if (ip) {
		if (ts_gt(ip->playoutpt, to))
			return (NULL);

		buf->last_to_get = ip;
		decode_unit(ip);
	}
	return (ip);
}

#define HISTORY_LEN	60	/* ms */

static void
clear_old_participant_history(ppb_t *buf)
{ 
	rx_queue_element_struct *ip;
	u_int32	cutoff, cur_time;

	cur_time = get_time(buf->src->clock);
	cutoff = cur_time - get_freq(buf->src->clock) * HISTORY_LEN / 1000;
	while (buf->head_ptr && ts_gt(cutoff, buf->head_ptr->playoutpt)) {
		ip = buf->head_ptr;
		buf->head_ptr = ip->next_ptr;

		if (buf->last_to_get == ip) {
			buf->last_to_get = NULL;
		}

		free_rx_unit(&ip);
	}

	if (buf->head_ptr) {
		buf->head_ptr->prev_ptr = NULL;
	} else {
		buf->tail_ptr = NULL;
	}
}

void
clear_old_history(ppb_t **buf, session_struct *sp)
{
	ppb_t	*p;

	while (*buf) {
		clear_old_participant_history(*buf);
		if ((*buf)->head_ptr == NULL) {
			p = *buf;
			*buf = p->next;
			block_free(p, sizeof(ppb_t));
		} else
			buf = &(*buf)->next;
	}
}

static ppb_t *
find_participant_queue(ppb_t **list, rtcp_dbentry *src)
{
	ppb_t *p;

	for (p = *list; p; p = p->next) {
		if (p->src == src)
			return (p);
	}
#ifdef DEBUG
	printf("allocating new receive buffer\n");
#endif
	p = (ppb_t*)block_alloc(sizeof(ppb_t));
	memset(p, 0, sizeof(ppb_t));
	p->src = src;
	p->next = *list;
	*list = p;

	return (p);
}

void 
service_receiver(cushion_struct *cushion, session_struct *sp,
		 rx_queue_struct *receive_queue, ppb_t **buf_list, struct s_mix_info *ms)
{
	rx_queue_element_struct	*up;
	ppb_t			*buf, **bufp;
	u_int32			cur_time, cs;
        
	while (receive_queue->queue_empty_flag == FALSE) {
		up = get_unit_off_rx_queue(receive_queue);
		buf = find_participant_queue(buf_list, up->dbe_source[0]);
		cur_time = get_time(buf->src->clock);
		/* This is to compensate for clock drift.
		 * Same check should be made in case it is too early.
		 */
		if (ts_gt(cur_time, up->playoutpt)) {
			up->dbe_source[0]->jit_TOGed++;
			up->dbe_source[0]->cont_toged++;
		} else {
			up->dbe_source[0]->cont_toged = 0;
		}
		up = playout_buffer_add(buf, up);
		/*
		 * If we have already worked past this point then mix it!
		 */
		if (up && buf->last_to_get && up->mixed == FALSE
		    && ts_gt(buf->last_to_get->playoutpt, up->playoutpt)
		    && ts_gt(up->playoutpt, cur_time)){
			decode_unit(up);
			if (up->native_count) {
				mix_do_one_chunk(sp, ms, up);
				if (!sp->have_device && (audio_device_take(sp) == FALSE)) {
					/* I hope the cb code suppresses some of these */
					lbl_cb_send_request(sp);
				}
				up->mixed = TRUE;
			}
		}
	}

	for (buf = *buf_list; buf; buf = buf->next) {
		cur_time = get_time(buf->src->clock);
		cs = cushion->cushion_size * get_freq(buf->src->clock) / get_freq(sp->device_clock);
		while ((up = playout_buffer_get(buf, cur_time, cur_time + cs))) {
			if (!up->comp_count  && sp->repair != REPAIR_NONE 
     			    && up->prev_ptr != NULL && up->next_ptr != NULL) 
				repair(sp->repair, up);

			if (up->native_count && up->mixed == FALSE) {
				mix_do_one_chunk(sp, ms, up);
				if (!sp->have_device && (audio_device_take(sp) == FALSE)) {
					/* I hope the cb code suppresses some of these */
					lbl_cb_send_request(sp);
				}
				up->mixed = TRUE;
			}
		}
		clear_old_participant_history(buf);
	}

	for (bufp = buf_list; *bufp;) {
		if ((*bufp)->head_ptr == NULL) {
			buf = *bufp;
			*bufp = buf->next;
			block_free(buf, sizeof(ppb_t));
		} else
			bufp = &(*bufp)->next;
	}
}
