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
#include "transmit.h"

typedef struct s_participant_playout_buffer {
	struct s_participant_playout_buffer *next;
	struct s_rtcp_dbentry *src;
	rx_queue_element_struct *head_ptr;
	rx_queue_element_struct *tail_ptr;
	rx_queue_element_struct *last_to_get;
} ppb_t;

rx_queue_element_struct *
new_rx_unit(void)
{
	rx_queue_element_struct *u;
	u = (rx_queue_element_struct *)block_alloc(sizeof(rx_queue_element_struct));
	memset(u, 0, sizeof(rx_queue_element_struct));
	return (u);
}

static void
add_unit_to_interval(rx_queue_element_struct *ip, rx_queue_element_struct *ru)
{
	ip->talk_spurt_start |= ru->talk_spurt_start;

        if (ru->cc_pt == ip->cc_pt) {
            /* XXX we should detect duplicates here [oth] 
             * if we have reached limit of number of ccu's or we have two or more with
             * identical headers then we know we've had a duplicate
             */
            while(ru->ccu_cnt>0 && 
                  ip->ccu_cnt < ru->ccu[--ru->ccu_cnt]->cc->max_cc_per_interval) {
                ip->ccu[ip->ccu_cnt++] = ru->ccu[ru->ccu_cnt];
                ru->ccu[ru->ccu_cnt] = NULL;
            }
	}
	free_rx_unit(&ru);
}

#ifdef ucacoxh

#define HALF_TS_CYCLE 0x80000000
int
ts_cmp(u_int32 ts1, u_int32 ts2)
{
        u_int32 d1,d2,dmin;

        d1 = ts2 - ts1;
        d2 = ts1 - ts2;

        /* look for smallest difference between ts (timestamps are unsigned) */
        if (d1<d2) {
                dmin = d1;
        } else if (d1>d2) {
                dmin = d2;
        } else {
                dmin = 0;
        }

        /* if either d1 or d2 have wrapped dmin > HALF_TS_CYCLE */

        if (dmin<HALF_TS_CYCLE) {
                if (ts1>ts2) return  1;
                if (ts1<ts2) return -1;
                return 0;
        } else if (ts1<ts2) {
                /* Believe t1 to have been wrapped and therefore greater */
                return 1; 
        } else {
                /* believe t2 to have been wrapped and therefore greater */
                return -1;
        }
}

void 
verify_playout_buffer(ppb_t *buf)
{
        rx_queue_element_struct *ip;
        ip = buf->head_ptr;
        while(ip != NULL && ip->next_ptr != NULL) {
                assert(ts_cmp(ip->src_ts,ip->next_ptr->src_ts) < 0); 
                assert(ts_cmp(ip->playoutpt,ip->next_ptr->playoutpt) <= 0); 
                ip = ip->next_ptr;
        }
}

#endif

static rx_queue_element_struct *
add_or_get_interval(ppb_t *buf, rx_queue_element_struct *ru)
{
	rx_queue_element_struct	**ipp;

	ipp = &buf->head_ptr;
        /* we look on src_ts to ensure correct ordering through decode path */
	while (*ipp && ts_gt(ru->src_ts, (*ipp)->src_ts))
		ipp = &((*ipp)->next_ptr);

	if (*ipp == NULL || (*ipp)->src_ts != ru->src_ts) {
                /* interval didn't already exist */
		ru->next_ptr = *ipp;
		if (*ipp == NULL) {
			ru->prev_ptr = buf->tail_ptr;
			buf->tail_ptr = ru;
		} else {
			ru->prev_ptr = (*ipp)->prev_ptr;
			(*ipp)->prev_ptr = ru;
		}
                /* remove overlap in playout points if existent */
                
                if ((ru->prev_ptr != NULL) &&
                    ts_gt(ru->prev_ptr->playoutpt,ru->playoutpt)) {
                        dprintf("cur playout shifted %u (%u) -> %u (%u)\n", 
                                ru->playoutpt, 
                                ru->src_ts,
                                ru->prev_ptr->playoutpt,
                                ru->prev_ptr->src_ts);
                        ru->playoutpt = ru->prev_ptr->playoutpt;
                } 
                if ((ru->next_ptr != NULL) &&
                    ts_gt(ru->playoutpt,ru->next_ptr->playoutpt)) {
                        dprintf("next playout shifted %u (%u) -> %u (%u)\n", 
                                ru->next_ptr->playoutpt, 
                                ru->next_ptr->src_ts,
                                ru->playoutpt,
                                ru->src_ts);
                        ru->next_ptr->playoutpt = ru->playoutpt;
                }
                
#ifdef ucacoxh
                verify_playout_buffer(buf);
#endif

		*ipp = ru;
	}
	return (*ipp);
}

static rx_queue_element_struct *
playout_buffer_add(ppb_t *buf, rx_queue_element_struct *ru)
{
	rx_queue_element_struct	*ip;
        
	if ((ip = add_or_get_interval(buf, ru)) != ru) {
		add_unit_to_interval(ip, ru);
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
                channel_decode(ip);
		decode_unit(ip);
	}
	return (ip);
}

#define HISTORY_LEN	60	/* ms */

static void
clear_old_participant_history(ppb_t *buf)
{ 
	rx_queue_element_struct *ip;
	u_int32	cutoff, cur_time, adj;

	cur_time = get_time(buf->src->clock);
        adj = (get_freq(buf->src->clock)/1000) * HISTORY_LEN;
	cutoff = cur_time - adj;
        assert(cutoff!=cur_time);
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
clear_old_history(ppb_t **buf)
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
	printf("allocating new receive buffer (ssrc=%lx)\n", src->ssrc);
#endif
	p = (ppb_t*)block_alloc(sizeof(ppb_t));
	memset(p, 0, sizeof(ppb_t));
	p->src = src;
	p->next = *list;
	*list = p;

	return (p);
}

void
playout_buffer_remove(ppb_t **list, rtcp_dbentry *src)
{
	/* We don't need to free "src", that's done elsewhere... [csp] */
	ppb_t 			*curr, *prev, *tmp;
	rx_queue_element_struct	*rxu, *rxt;

	assert(list != NULL);

	if (*list == NULL) {
		return;
	}

	if ((*list)->src == src) {
		tmp = (*list)->next;
		rxu = (*list)->head_ptr;
		while(rxu != NULL) {
			rxt = rxu->next_ptr;
			free_rx_unit(&rxu);
			rxu = rxt;
		}
		block_free(*list, sizeof(ppb_t));
		*list = tmp;
	}

	if (*list == NULL) return;

	prev = *list;
	curr = (*list)->next;

	if (prev == NULL) return;

	assert(prev != NULL);
	while (curr != NULL) {
		if (curr->src == src) {
			prev->next = curr->next;
			rxu = curr->head_ptr;
			while(rxu != NULL) {
				rxt = rxu->next_ptr;
				free_rx_unit(&rxu);
				rxu = rxt;
			}
			block_free(curr, sizeof(ppb_t));
			curr = prev->next;
		} else {
			prev = curr;
			curr = curr->next;;
		}
	}
}

void 
service_receiver(cushion_struct *cushion, session_struct *sp, rx_queue_struct *receive_queue, ppb_t **buf_list, struct s_mix_info *ms)
{
	/* There is a nasty race condition in this function, which people
	 * should be aware of: when a participant leaves a session (either
	 * via a timeout or reception of an RTCP BYE packet), the RTCP
	 * database entries for that source are removed. However, there may
	 * still be some packets left for that source in the receiver
	 * pipeline. This means that the up->dbe_source[] and buf->src
	 * fields (and possibly some others) in the code below may be
	 * dangling pointers... Now, this should never happen, since the
	 * code removing a participant cleans up the receiver mess, right?
	 * Well, we hope anyway...... At least one bug has been caused by
	 * this already (a crash just after receiving a BYE packet from a
	 * participant who is sending). Don't say I didn't warn you! [csp]
	 */
	rx_queue_element_struct	*up;
	ppb_t			*buf, **bufp;
	u_int32			cur_time, cs;
        
	while (receive_queue->queue_empty_flag == FALSE) {
		up       = get_unit_off_rx_queue(receive_queue);
		buf      = find_participant_queue(buf_list, up->dbe_source[0]);
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
                        channel_decode(up);
			decode_unit(up);
			if (up->native_count) {
				mix_do_one_chunk(sp, ms, up);
				if (!sp->have_device && (audio_device_take(sp) == FALSE)) {
					/* Request device using the mbus... */
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
                        && up->prev_ptr != NULL && up->next_ptr != NULL
                        && up->prev_ptr->native_count) 
                        repair(sp->repair, up);
                    if (up->native_count && up->mixed == FALSE) {
                        mix_do_one_chunk(sp, ms, up);
                        if (!sp->have_device && (audio_device_take(sp) == FALSE)) {
				/* Request device using the mbus... */
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
		} else {
			bufp = &(*bufp)->next;
		}
	}
}
