/*
 * FILE:    speaker_table.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins + Isidor Kouvelas  
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#include "speaker_table.h"
#include "receive.h"
#include "session.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "ui_control.h"
#include "rat_time.h"
#include "transmit.h"

#define GRAY_DELAY	60 /* ms */
#define REMOVE_DELAY	3000 /* ms */
/*#define LAST_ACTIVE	last_mixed_playout*/
#define LAST_ACTIVE	last_active

#define OFF	0
#define GRAY	1
#define WHITE	2

void mark_active_senders(rx_queue_element_struct *up, session_struct *sp)
{
	int i;

	for (i = 0; i < up->dbe_source_count; i++) {
		mark_active_sender(up->dbe_source[i], sp);
	}
}

void mark_active_sender(rtcp_dbentry *src, session_struct *sp)
{
	speaker_table *st = NULL;

	for (st = sp->speakers_active; st != NULL; st = st->next) {
		if (st->dbe == src)
			break;
	}
	if (st == NULL) {
		st = (speaker_table *)block_alloc(sizeof(speaker_table));
		st->next = sp->speakers_active;
		st->dbe = src;
		st->state = OFF;
		sp->speakers_active = st;
		if (sp->ui_on) {
			ui_info_activate(st->dbe);
		}
	}
	if (st->state != WHITE) {
		if (sp->ui_on) {
			ui_info_activate(st->dbe);
		}
		st->state = WHITE;
	}
}

void clear_active_senders(session_struct *sp)
{
	speaker_table 	*st, **stp;
	u_int32		cur_time;

	cur_time = get_time(sp->device_clock);

	/* Remove only one at a time.
	 * This smooths load + solves some problems with the loop :-) */
	for (stp = &sp->speakers_active; *stp != NULL; stp = &(*stp)->next) {
		if (ts_gt(cur_time, (*stp)->dbe->LAST_ACTIVE + get_freq(sp->device_clock) * REMOVE_DELAY / 1000)) {
			break;
		}
		if (ts_gt(cur_time, (*stp)->dbe->LAST_ACTIVE + get_freq(sp->device_clock) * GRAY_DELAY / 1000) && (*stp)->state != GRAY) {
			if (sp->ui_on) {
				ui_info_gray((*stp)->dbe);
			}
			(*stp)->state = GRAY;
		}
	}
	if (*stp) {
		st = *stp;
		*stp = st->next;
		if (sp->ui_on) {
			ui_info_deactivate(st->dbe);
		}
		block_free(st, sizeof(speaker_table));
	}
}

void check_active_leave(session_struct *sp, rtcp_dbentry *e)
{
	speaker_table *st, **stp;

	for (stp = &sp->speakers_active; *stp != NULL; stp = &(*stp)->next) {
		if ((*stp)->dbe == e)
			break;
	}
	if (*stp) {
		st = *stp;
		*stp = st->next;
		if (sp->ui_on) {
			ui_info_deactivate(st->dbe);
		}
		block_free(st, sizeof(speaker_table));
	}
}
