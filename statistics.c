/*
 * FILE:	statistics.c
 * 
 * PROGRAM:	RAT
 * 
 * AUTHOR: V.J.Hardman + I.Kouvelas
 * 
 * CREATED: 23/03/95
 * 
 * $Id$
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

#include "statistics.h"
#include "session.h"
#include "receive.h"
#include "interfaces.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "audio.h"
#include "speaker_table.h"
#include "codec.h"
#include "ui.h"

static rtcp_dbentry *
update_database(session_struct *sp, u_int32 ssrc, u_int32 cur_time)
{
	rtcp_dbentry   *dbe_source;

	/* This function gets the relevant data base entry */
	dbe_source = rtcp_get_dbentry(sp, ssrc);
	if (dbe_source == NULL) {
		/* We haven't received an RTCP packet for this source, so we must throw the   */
		/* packets away. This seems a little extreme, but there are actually a couple */
		/* of good reasons for it:                                                    */
		/*   1) If we're receiving encrypted data, but we don't have the decryption   */
		/*      key, then every RTP packet we receive will have a different SSRC      */
		/*      and if we create a new database entry for it, we fill up the database */
		/*      with garbage (which is then displayed in the list of participants...) */
		/*   2) The RTP specification says that we should do it this way (sec 6.2.1)  */
		/*                                                                     [csp]  */
		return NULL;
	}
	dbe_source->last_active = cur_time;
	dbe_source->is_sender   = 1;

	sp->db->pckts_received++;

	return dbe_source;
}

static int
split_block(u_int32 playout_pt, int pt, char *data_ptr, int len,
	    rtcp_dbentry *src, rx_queue_struct *unitsrx_queue_ptr,
            int talks, rtp_hdr_t *hdr, session_struct *sp, u_int32 cur_time)
{
	codec_t *cp;
	int	units, ul, i, j, k;
	char	*buf;
	rx_queue_element_struct	*p;

	if ((cp = get_codec(pt)) == NULL) {
		/* Log as bad encoding... XXX */
		return 0;
	}

	/* This is a problem with vbr codecs */
	units = (len - cp->sent_state_sz) / cp->max_unit_sz;

	for (i = 0; i < units; i++) {
		ul = cp->max_unit_sz;
		if (i == 0)
			ul += cp->sent_state_sz;

		buf = xmalloc(ul);
		memcpy(buf, data_ptr, ul);
		data_ptr += ul;

		p = (rx_queue_element_struct *)block_alloc(sizeof(rx_queue_element_struct));
		p->next_ptr         = NULL;
		p->prev_ptr         = NULL;
		p->talk_spurt_start = (i == 0 ? talks: FALSE);
		p->talk_spurt_end   = FALSE;
		p->unit_size        = cp->unit_len;
		p->units_per_pckt   = units;
		p->unit_position    = i + 1;
		p->mixed            = FALSE;
		p->dbe_source[0]    = src;

		mark_active_sender(src, sp);
		for (j = 0, k = 1; j < hdr->cc; j++) {
			p->dbe_source[k] = update_database(sp, ntohl(hdr->csrc[j]), cur_time);
			if (p->dbe_source[k] != NULL) {
				mark_active_sender(p->dbe_source[k], sp);
				k++;
			}
		}
		p->dbe_source_count = k;
		p->playoutpt        = playout_pt + (i * cp->unit_len);
                p->comp_count       = 1;

		p->comp_data[0].cp = cp;
		if (i == 0 && cp->sent_state_sz > 0) {
			p->comp_data[0].state = buf;
			p->comp_data[0].state_len = cp->sent_state_sz;
			p->comp_data[0].data = buf + cp->sent_state_sz;
			p->comp_data[0].data_len = cp->max_unit_sz;
		} else {
			p->comp_data[0].state = NULL;
			p->comp_data[0].state_len = 0;
			p->comp_data[0].data = buf;
			p->comp_data[0].data_len = cp->max_unit_sz;
		}
		p->native_count = 0;
		put_on_rx_queue(p, unitsrx_queue_ptr);
	}

	return (units);
}

static u_int32
adapt_playout(rtp_hdr_t *hdr, int arrival_ts, rtcp_dbentry *src,
	      session_struct *sp, cushion_struct *cushion)
{
	u_int32	playout, var;
	int	delay, diff;
	codec_t	*cp;

	arrival_ts = convert_time(arrival_ts, sp->device_clock, src->clock);
	delay = arrival_ts - hdr->ts;

	if (src->first_pckt_flag == TRUE) {
		src->first_pckt_flag = FALSE;
		diff                 = 0;
		src->delay           = delay;
		src->jitter          = 80;
		src->last_ts         = hdr->ts - 1;
		hdr->m               = TRUE;
	} else {
		/* This gives a smoothed average */
		diff       = abs(delay - src->delay);
		src->delay = delay;

		/* Jitter calculation as in RTP draft 07 */
		src->jitter = src->jitter + (((double) diff - src->jitter) / 16);
		src->last_diff = diff;
	}

	if (ts_gt(hdr->ts, src->last_ts)) {
		/* If TS start then adjust playout delay estimation */
		/* The 8 is the number of units in a 160ms packet (nasty hack!) */
		cp = get_codec(src->encs[0]);
		if ((hdr->m) || src->cont_toged > 4 || (ts_gt(hdr->ts, (src->last_ts + (hdr->seq - src->last_seq) * cp->unit_len * 8 + 1)))) {
			var = (u_int32) src->jitter * 3;
			if (var > 8000) {
				var = 8000;
			}
			var += cushion->cushion_size * get_freq(src->clock) / get_freq(sp->device_clock);
			if (src->clock!=sp->device_clock) 
				var += cp->unit_len;
			if (src->encoding == sp->redundancy_pt)
				var += cp->unit_len * src->units_per_packet;
			src->playout = src->delay + var;
		} else {
			/* Do not set encoding on TS start packets as they do not show if redundancy is used...   */
			src->encoding = hdr->pt;
		}
		src->last_ts  = hdr->ts;
		src->last_seq = hdr->seq;
	}

	/* Calculate the playout point in local source time for this packet */
	playout = hdr->ts + src->playout;

	return (playout);
}

int
rtp_header_validation(rtp_hdr_t *hdr, int length, session_struct *sp)
{
	/* This function checks the header info to make sure that the packet */
	/* is valid. We return TRUE if the packet is valid, FALSE otherwise. */
	/* This follows from page 52 of RFC1889.            [csp 22-10-1996] */
	int	pt = 0;
	int	pl = 0;
	int     *data = 0;
	u_int32	red_hdr;
	codec_t	*cp;

	/* We only accept RTPv2 packets... */
	if (hdr->type != 2) {
#ifdef DEBUG
		printf("rtp_header_validation: version != 2\n");
#endif
		return FALSE;
	}

	/* Check for valid audio payload types... */
	if (((hdr->pt > 23) && (hdr->pt < 96)) || (hdr->pt > 127)) {
#ifdef DEBUG
		printf("rtp_header_validation: payload-type out of audio range\n");
#endif
		return FALSE;
	}

	/* If padding or header-extension is set, we punt on this one... */
	/* We should really deal with it though...                       */
	if (hdr->p || hdr->x) {
#ifdef DEBUG
		printf("rtp_header_validation: p or x bit set\n");
#endif
		return FALSE;
	}

	/* Check the length is consistent... We have to check for 20,40,60,80ms packets*/
	pl = (length - 12) - (hdr->cc * 4);	/* Size of the payload... */
	if (hdr->pt == sp->redundancy_pt) {
		data  = (int *)hdr + 3 + hdr->cc;
		while ((red_hdr = ntohl(*data)) & 0x80000000) {
			pt  = RED_PT(red_hdr);
			pl -= RED_LEN(red_hdr) + 4;
			data++;
		}
		pl -= 1;
		pt  = RED_PT(red_hdr) & 0x7f;
	} else {
		pt = hdr->pt;
	}

	if ((cp = get_codec(pt)) == 0) {
		/* bad encoding */
		return (FALSE);
	}

	if ((pl - cp->sent_state_sz) % cp->max_unit_sz != 0) {
#ifdef DEBUG
		printf("rtp_header_validation: %s packet length is wrong\n", cp->name);
#endif
		return (FALSE);
	}

	return (TRUE);
}

static void
receiver_change_format(rtcp_dbentry *dbe, codec_t *cp)
{
	dbe->first_pckt_flag = TRUE;
	change_freq(dbe->clock, cp->freq);
}

void
statistics(session_struct    *sp,
	   pckt_queue_struct *netrx_pckt_queue,
	   rx_queue_struct   *unitsrx_queue_ptr,
	   cushion_struct    *cushion,
	   u_int32       cur_time)
{
	/*
	 * We expect to take in an RTP packet, and decode it - read fields
	 * etc. This module should do statistics, and keep information on
	 * losses, and re-orderings. Duplicates will be dealt with in the
	 * receive buffer module.
	 * 
	 * Late packets will not be counted as lost. RTP stats reports count
	 * duplicates in with the number of packets correctly received, thus
	 * sometimes invalidating stats reports. We can, if necessary, keep
	 * track of some duplicates, and throw away in the receive module. It
	 * has not yet been decided whether or not loss will be 'indicated'
	 * to later modules - put a dummy unit(s) on the queue for the receive
	 * buffer
	 */

	rtp_hdr_t	*hdr;
	u_char		*hdr_ptr, *data_ptr;
	int		len, block_len, blocks, pri_pt;
	rtcp_dbentry	*src;
	u_int32		red_hdr, block_ts, playout_pt;
	pckt_queue_element_struct *e_ptr;
	codec_t		*pcp, *scp;

	char update_req = FALSE;

	/* Get a packet to process */
	e_ptr = get_pckt_off_queue(netrx_pckt_queue);
	assert(e_ptr != NULL);

	/* Impose RTP formating on it... */
	hdr = (rtp_hdr_t *) (e_ptr->pckt_ptr);

	if (rtp_header_validation(hdr, e_ptr->len, sp) == FALSE) {
#ifdef DEBUG
		printf("RTP Packet failed header validation!\n");
#endif
		free_pckt_queue_element(&e_ptr);
		/* XXX log as bad packet */
		return;
	}
	/* Convert from network byte-order */
	hdr->seq  = ntohs(hdr->seq);
	hdr->ts   = ntohl(hdr->ts);
	hdr->ssrc = ntohl(hdr->ssrc);

	/* Get database entry of participant that sent this packet */
	src = update_database(sp, hdr->ssrc, cur_time);
	/* Uncknown participant */
	if (src == NULL) {
		free_pckt_queue_element(&e_ptr);
		return;
	}

	rtcp_update_seq(src, hdr->seq);

	hdr_ptr = (char *)e_ptr->pckt_ptr + 4 * (3 + hdr->cc);
	data_ptr = hdr_ptr; 
	len = e_ptr->len - 4 * (3 + hdr->cc);

	if (hdr->pt == sp->redundancy_pt) {
		/* Advance data_ptr past the headers... */
		for (blocks = 1; len >= 4 && (*data_ptr & 0x80); blocks++) {
			data_ptr += 4;
			len -= 4;
		}
		pri_pt = *data_ptr & 0x7f;
		data_ptr++;
		len--;
	} else
		pri_pt = hdr->pt;
	
	if ((pcp = get_codec(pri_pt)) == NULL) {
		/* We do not understand the primary encoding so forget about it */
		free_pckt_queue_element(&e_ptr);
		return;
	}

	if (src->encs[0] == -1 || !codec_compatible(pcp, get_codec(src->encs[0])))
		receiver_change_format(src, pcp);

	if (src->encs[0] != pri_pt) {
		src->encs[0] = pri_pt;
		update_req   = TRUE;
	}

	playout_pt = adapt_playout(hdr, e_ptr->arrival_timestamp, src, sp, cushion);
	
	/* Continue redundancy processing */
	if (hdr_ptr != data_ptr) {
		/* Process the data... */
		for (blocks = 1; *hdr_ptr & 0x80; blocks++) {
			red_hdr = ntohl(*((int *)hdr_ptr));
			if (src->encs[blocks] != RED_PT(red_hdr)) {
				src->encs[blocks] = RED_PT(red_hdr);
				update_req = TRUE;
			}
			block_len = RED_LEN(red_hdr);
			block_ts = hdr->ts - RED_OFF(red_hdr);
			if (block_len > len) {
#ifdef DEBUG
				fprintf(stderr, "Bad redundancy packet!\n");
#endif
				free_pckt_queue_element(&e_ptr);
				return;
			}
			scp = get_codec(src->encs[blocks]);
			if (scp != NULL) {
				if (codec_compatible(scp, pcp))
					split_block(playout_pt - RED_OFF(red_hdr), src->encs[blocks], data_ptr, block_len, src, unitsrx_queue_ptr, FALSE, hdr, sp, cur_time);
				else {
#ifdef DEBUG
					fprintf(stderr, "Bad block in redundancy packet!\n");
#endif
				}
			}
			len      -= block_len;
			data_ptr += block_len;
			hdr_ptr  += 4;
		}
	} else {
		blocks = 1;
	}
	if (src->encs[blocks] != -1) {
		src->encs[blocks] = -1;
		update_req        = TRUE;
	}
	src->units_per_packet = split_block(playout_pt, src->encs[0], data_ptr, len, src, unitsrx_queue_ptr, hdr->m, hdr, sp, cur_time);
	if (update_req) update_stats(src, sp);
	free_pckt_queue_element(&e_ptr);
}

