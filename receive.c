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

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "debug.h"
#include "util.h"
#include "receive.h"
#include "interfaces.h"
#include "timers.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "mix.h"
#include "audio.h"
#include "convert.h"
#include "cushion.h"
#include "transmit.h"
#include "ui.h"
#include "render_3D.h"

typedef struct s_participant_playout_buffer {
	struct s_participant_playout_buffer *next;
	struct s_rtcp_dbentry *src;
	rx_queue_element_struct *head_ptr;
	rx_queue_element_struct *tail_ptr;
	rx_queue_element_struct *last_got;
        u_int32 creation_time;
        u_int32 len;
        u_int32 age;
        u_int32 hist; /* bitmap of additions and removals */
} ppb_t;

/* this is still trial and error */
#define PB_GROW(x)     (x)->hist<<=1; (x)->len++
#define PB_SHRINK(x)   (x)->hist = (((x)->hist << 1)|1); (x)->len--
#define PB_DRAINING(x) (((x->hist)&0x0f) == 0x0f)

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
        /* XXX we should detect duplicates here [oth] 
         * if we have reached limit of number of ccu's or we have two or 
         * more with identical headers then we know we've had a duplicate.
         */
        if (ip->mixed   == FALSE && 
            ru->cc_pt   == ip->cc_pt && 
            ip->ccu_cnt == 0) {
                while(ru->ccu_cnt>0 && 
                      ip->ccu_cnt < ru->ccu[ru->ccu_cnt-1]->cc->max_cc_per_interval) {
                        ip->ccu[ip->ccu_cnt++] = ru->ccu[--ru->ccu_cnt];
                        ru->ccu[ru->ccu_cnt] = NULL;
                }
        } else {
                debug_msg("duplicate\n");        ip->dbe_source[0]->duplicates++;
        
        } 
	free_rx_unit(&ru);
}

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
		*ipp = ru;
                PB_GROW(buf);
	}
	return (*ipp);
}

#define MAX_FILLIN_UNITS 32

static int
fillin_playout_buffer(ppb_t *buf,
                      rx_queue_element_struct *from, 
                      rx_queue_element_struct *to)
{
        rx_queue_element_struct *last, *curr; 
        u_int32 playout_step, units_made = 0;
        
        assert(ts_abs_diff(from->src_ts,to->src_ts) % to->unit_size == 0);

        playout_step = ts_abs_diff(from->playoutpt, to->playoutpt) * 
                from->unit_size / ts_abs_diff(from->src_ts, to->src_ts);

#ifdef DEBUG_PLAYOUT
        if (playout_step > 4*from->unit_size) {
                debug_msg("playout_step is large %d.\n",
                        playout_step);
        }
#endif

        last = from;
        while(last->src_ts + last->unit_size != to->src_ts &&
              units_made < MAX_FILLIN_UNITS) {
                curr = new_rx_unit();
                PB_GROW(buf);
                buf->age ++;
                curr->src_ts    = last->src_ts    + last->unit_size;
                curr->playoutpt = last->playoutpt + playout_step;
                curr->unit_size = last->unit_size;
                curr->cc_pt     = last->cc_pt;

                curr->dbe_source_count = last->dbe_source_count;
                memcpy(curr->dbe_source,
                       last->dbe_source,
                       curr->dbe_source_count * sizeof(struct s_rtcp_dbentry*));

                curr->next_ptr = to;
                to->prev_ptr   = curr;
                curr->prev_ptr = last;
                last->next_ptr = curr;
                
                last = curr;
                units_made++;
        }

        assert(units_made>0);
#ifdef DEBUG_PLAYOUT
        debug_msg("Allocated %d new units with separation %d\n",
                units_made,
                playout_step);
#endif /* DEBUG_PLAYOUT */
        return units_made;
}

#ifdef DEBUG_PLAYOUT
static void
verify_playout_buffer(ppb_t* buf)
{
        rx_queue_element_struct *el;
        u_int32 src_diff, playout_diff, buf_len = 0;

        el = buf->head_ptr;
        while( el && el->next_ptr ) {
                if (ts_gt(el->src_ts, el->next_ptr->src_ts)) {
                        src_diff = ts_abs_diff( el->next_ptr->src_ts, 
                                                el->src_ts );
                        debug_msg( "src_ts jump %08u.\n", 
                                 src_diff );
                }
                if (ts_gt(el->playoutpt, el->next_ptr->playoutpt)) {
                        playout_diff = ts_abs_diff( el->next_ptr->playoutpt,
                                                    el->playoutpt );
                        debug_msg( "out of order playout units by %08u.\n",
                                 playout_diff );
                }
                el = el->next_ptr;
                buf_len++;
        }

        if (buf->len != buf_len + 1) {
                debug_msg("Buffer length estimate is wrong (%d %d)!\n",
                        buf->len,
                        buf_len + 1);
        }
}
#endif /* DEBUG_PLAYOUT */

static rx_queue_element_struct *
playout_buffer_add(ppb_t *buf, rx_queue_element_struct *ru)
{
	rx_queue_element_struct	*ip;

	if ((ip = add_or_get_interval(buf, ru)) != ru) {
		add_unit_to_interval(ip, ru);
	}

        assert(ip != NULL);

        /* If there's a gap between us and the units around us, because of 
         * loss or mis-ordering, fill in the units so that channel decoder
         * does not get out of sync.
         */
        if (ip->next_ptr != NULL && 
            ts_abs_diff(ip->src_ts, ip->next_ptr->src_ts) != ip->unit_size &&
            ip->next_ptr->talk_spurt_start == FALSE) {
                fillin_playout_buffer(buf, ip, ip->next_ptr);
        }

        if (ip->prev_ptr != NULL && 
            ts_abs_diff(ip->src_ts, ip->prev_ptr->src_ts) != ip->unit_size &&
            ip->prev_ptr->talk_spurt_start == FALSE) {
                fillin_playout_buffer(buf, ip->prev_ptr, ip);
        }

        /* If playout point has been adjusted due to loss, late arrivals,
         * or start of new talkspurt, shift any overlapping units to keep
         * channel decoder in sync.
         */
        while(ip && 
              ip->prev_ptr && 
              ts_gt(ip->prev_ptr->playoutpt, ip->playoutpt)) {
#ifdef DEBUG_PLAYOUT
                debug_msg("Shifting unit from %ld to %ld\n",
                        ip->prev_ptr->playoutpt,
                        ip->playoutpt);
#endif
                ip->prev_ptr->playoutpt = ip->playoutpt;
        }

#ifdef DEBUG_PLAYOUT
        verify_playout_buffer(buf);
#endif /* DEBUG_PLAYOUT */

	return (ru);
}

static rx_queue_element_struct *
playout_buffer_get(session_struct *sp, ppb_t *buf, u_int32 from, u_int32 to)
{
	rx_queue_element_struct *ip;

	if (buf->last_got == NULL) {
		ip = buf->head_ptr;
	} else {
		ip = buf->last_got->next_ptr;
	}
	while (ip && ts_gt(from, ip->playoutpt)) {
		buf->last_got = ip;
		ip = ip->next_ptr;
	}

	if (ip) {
		if (ts_gt(ip->playoutpt, to)) {
			return (NULL);
                }
		buf->last_got = ip;
                channel_decode(sp, ip);
		decode_unit(ip);
	}
	return (ip);
}

static void
playout_buffer_destroy(session_struct *sp, ppb_t **list, ppb_t *buf)
{
        ppb_t *pb, *lpb;
        rx_queue_element_struct *nrx;
        rtcp_dbentry *ssrc;

        ui_info_deactivate(sp, buf->src);
        ssrc = buf->src;

        debug_msg("Destroying playout buffer\n");

        while (buf->head_ptr) {
                nrx = buf->head_ptr->next_ptr;
                free_rx_unit(&buf->head_ptr);
                buf->head_ptr = nrx;
        }
        
        pb  = *list;
        lpb = NULL;
        while(pb) {
                if (buf == pb) {
                        pb = buf->next;
                        block_free(buf, sizeof(ppb_t));
                        if (buf == *list) {
                                *list = pb;
                                break;
                        } else {
                                lpb->next = pb;
                                break;
                        }
                }
                lpb = pb;
                pb = pb->next;
        }

        if (*list == NULL && sp->echo_was_sending && sp->echo_suppress) {
                debug_msg("Echo suppressor unmuting (%d).\n", sp->echo_was_sending);
                if (sp->echo_was_sending) {
                        tx_start(sp);
                }
                sp->echo_was_sending = FALSE;
        }
        /* Update playout buf length etc, otherwise figure on screen is garbage and persists */
        ui_update_stats(sp, ssrc);
}

#define HISTORY_LEN	60	/* ms */
#define SUPPRESS_LEN   200      /* ms */

static void
clear_old_participant_history(session_struct *sp, ppb_t *buf)
{ 
	rx_queue_element_struct *ip;
	u_int32	cutoff, cur_time, adj;

	cur_time = get_time(buf->src->clock);
        if (sp->echo_suppress) {
                adj = (get_freq(buf->src->clock)/1000) * SUPPRESS_LEN;
        } else {
                adj = (get_freq(buf->src->clock)/1000) * HISTORY_LEN;
        }
	cutoff = cur_time - adj;
        assert(cutoff!=cur_time);

	while (buf->head_ptr && ts_gt(cutoff, buf->head_ptr->playoutpt)) {
		ip = buf->head_ptr;
		buf->head_ptr = ip->next_ptr;

		if (buf->last_got == ip) {
			buf->last_got = NULL;
		}
		free_rx_unit(&ip);
                PB_SHRINK(buf);
	}

	if (buf->head_ptr) {
		buf->head_ptr->prev_ptr = NULL;
	} else {
		buf->tail_ptr = NULL;
                playout_buffer_destroy(sp, &sp->playout_buf_list, buf);
	}
}

int 
playout_buffer_exists (ppb_t *list, rtcp_dbentry *src)
{
        ppb_t *p;

        p = list;
        while(p != NULL) {
                if (p->src == src) {
                        return TRUE;
                }
                p = p->next;
        }
        
        return FALSE;
}

int32
playout_buffer_duration (ppb_t *list, rtcp_dbentry *src)
{
        ppb_t *p;

        p = list;
        while(p != NULL) {
                if (p->src == src && p->last_got) {
                        return (p->tail_ptr->playoutpt - p->last_got->playoutpt) * 1000 / get_freq(p->src->clock);
                }
                p = p->next;
        }
        return 0;
}

void
playout_buffers_destroy(session_struct *sp, ppb_t **list)
{
        ppb_t *p;
        while((p = *list)) {
                playout_buffer_destroy(sp, list, p);
        }
}

static ppb_t *
find_participant_queue(session_struct *sp, ppb_t **list, rtcp_dbentry *src, int dev_pt, int src_pt, struct s_pcm_converter *pc)
{
	ppb_t *p;
        codec_t *cp_dev, *cp_src;

	for (p = *list; p; p = p->next) {
		if (p->src == src)
			return (p);
	}

        /* Echo suppression */
        if (*list == NULL && sp->echo_suppress) {
                /* We are going to create a playout buffer so mute mike */
                debug_msg("Echo suppressor muting.\n");
                sp->echo_was_sending = sp->sending_audio;
                if (sp->sending_audio) {
                        tx_stop(sp);
                }
        }

	p = (ppb_t*)block_alloc(sizeof(ppb_t));
	memset(p, 0, sizeof(ppb_t));
	p->src           = src;
        p->creation_time = get_time(src->clock);
	p->next          = *list;
	*list            = p;
        
        ui_info_activate(sp, src);
        
        cp_dev = get_codec_by_pt(dev_pt);
        cp_src = get_codec_by_pt(src_pt);
        assert(cp_dev);
        assert(cp_src);
        if (!codec_compatible(cp_dev,cp_src)) {
                if (src->converter) converter_destroy(&src->converter);
                assert(src->converter == NULL);
                src->converter = converter_create(pc, 
                                                  cp_src->channels, 
                                                  cp_src->freq,
                                                  cp_dev->channels,
                                                  cp_dev->freq);
        } 

        if (sp->externalise_audio) {
                if (src->render_3D_data) render_3D_free(&src->render_3D_data);
                src->render_3D_data = render_3D_init();
        }

	return (p);
}

void
playout_buffer_remove(session_struct *sp, ppb_t **list, rtcp_dbentry *src)
{
	/* We don't need to free "src", that's done elsewhere... [csp] */
	ppb_t *curr, *tmp;

        if (src->converter)      converter_destroy(&src->converter);
        if (src->render_3D_data) render_3D_free(&src->render_3D_data);
	assert(list != NULL);

        curr = *list;
        while(curr) {
                tmp = curr->next;
                if (curr->src == src) {
                        playout_buffer_destroy(sp, list, curr);
                }
                curr = tmp;
        }
}

#define PLAYOUT_SAFETY 5

void 
playout_buffers_process(session_struct *sp, rx_queue_struct *receive_queue, ppb_t **buf_list, struct s_mix_info *ms)
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
	ppb_t			*buf, *buf_next;
	u_int32			cur_time, cs, cu;
        
        cs = cu = 0;
        
	while (receive_queue->queue_empty == FALSE) {
		up       = get_unit_off_rx_queue(receive_queue);
		buf      = find_participant_queue(sp, buf_list, up->dbe_source[0], sp->encodings[0], up->dbe_source[0]->enc, sp->converter);
		cur_time = get_time(buf->src->clock);
                
		/* This is to compensate for clock drift.
		 * Same check should be made in case it is too early.
		 */
                
		if (ts_gt(cur_time, up->playoutpt)) {
/* 
   if (cur_time == buf->creation_time) this is the first block of a new playout buffer.
   It is silly to throw first packet away, however what normally causes this is that
   RAT has gone away (UI locked, or somesuch) and we have lots of audio.  Shifting it 
   adds lots of delay which is not desirable
   */
                        up->dbe_source[0]->jit_TOGed++;
                        up->dbe_source[0]->cont_toged++;
                        debug_msg("cont_toged %d\n",
                                  up->dbe_source[0]->cont_toged); 
		} else {
			up->dbe_source[0]->cont_toged = 0;
		}
#ifdef DEBUG_PLAYOUT
                verify_playout_buffer(buf);
#endif
		up = playout_buffer_add(buf, up);
#ifdef DEBUG_PLAYOUT
                verify_playout_buffer(buf);
#endif
		/*
		 * If we have already worked past this point then mix it!
		 */
                
		if (up && buf->last_got && up->mixed == FALSE
		    && ts_gt(buf->last_got->playoutpt, up->playoutpt) 
		    && ts_gt(up->playoutpt, cur_time)){
                        debug_msg("Mixing late audio\n");
                        channel_decode(sp, up);
			decode_unit(up);
			if (up->native_count) {
				mix_do_one_chunk(sp, ms, up);
			}
		}
	}
        
	/* If sp->cushion is NULL, it probably means we don't have access to the audio device... */
	if (sp->cushion != NULL) {
        	cs = cushion_get_size(sp->cushion);
        	cu = cushion_get_step(sp->cushion);
	}
        
        buf_next = NULL;
	for (buf = *buf_list; buf; buf = buf_next) {
                buf_next = buf->next; /* because buf maybe freed by clear_old_participant history
                                       * and we should not dereference through freed memory.
                                       */
		cur_time = get_time(buf->src->clock);
#ifdef DEBUG_PLAYOUT_BROKEN
                {
                        static struct timeval last_foo;
                        struct timeval foo;
                        gettimeofday(&foo, NULL);
                        debug_msg("%08ld: playout range: %ld - %ld\n\tbuffer playout range %ld - %ld\n\tbuffer ts range %ld - %ld\n",
                                  (foo.tv_sec  - last_foo.tv_sec) * 1000 +
                                  (foo.tv_usec - last_foo.tv_usec)/1000, 
                                  cur_time, 
                                  cur_time + cs,
                                  buf->head_ptr->playoutpt,
                                  buf->tail_ptr->playoutpt,
                                  buf->head_ptr->src_ts,
                                  buf->tail_ptr->src_ts
                                );
                        memcpy(&last_foo, &foo, sizeof(struct timeval));
                }
#endif /* DEBUG_PLAYOUT_BROKEN */
                if ((buf->tail_ptr->playoutpt - cur_time) < 5*cs/4) {
                        debug_msg("Less audio buffered (%ld) than cushion safety (%ld)!\n", 
                                  buf->tail_ptr->playoutpt - cur_time,
                                  5 * cs / 4);
                        buf->src->playout_danger = TRUE;
                }
                
		while ((up = playout_buffer_get(sp, buf, cur_time, cur_time + cs))) {
                        if (!up->comp_count  && sp->repair != REPAIR_NONE 
                            && up->prev_ptr != NULL && up->next_ptr != NULL
                            && up->prev_ptr->native_count) 
                        repair(sp->repair, up);
#ifdef DEBUG_PLAYOUT
                        if (up->prev_ptr) {
                                u_int32 src_diff = ts_abs_diff(up->prev_ptr->src_ts,up->src_ts);
                            if (src_diff != up->unit_size) {
                                    debug_msg("src_ts jump %08d\n",src_diff);
                            }
                        }
#endif /* DEBUG_PLAYOUT */
                        
                        if (up->native_count && up->mixed == FALSE) {
                                mix_do_one_chunk(sp, ms, up);
                        } else { 
                                if (up->native_count) {
                                        debug_msg("already mixed\n"); 
                                } else {
                                        if (up->comp_count) {
                                                debug_msg("Not decoded ?\n");
                                        } else {
                                                assert(up->comp_data[0].data == NULL);
                                                debug_msg("No data for block, buf len %ld, cushion size %ld\n", 
                                                          playout_buffer_duration(buf, buf->src), 
                                                          cs);
                                        }
                                }
                        }
		}
		clear_old_participant_history(sp, buf);
	}
}

