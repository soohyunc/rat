/*
 * FILE:	statistics.c
 * 
 * PROGRAM:	RAT
 * 
 * AUTHOR: V.J.Hardman + I.Kouvelas + O.Hodson
 * 
 * CREATED: 23/03/95
 * 
 * $Id$
 *
 * Copyright (c) 1995-98 University College London
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
#include "cushion.h"
#include "speaker_table.h"
#include "codec.h"
#include "channel.h"
#include "ui_control.h"
#include "mbus.h"

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
split_block(u_int32 playout_pt, 
            codec_t *cp,
            char *data_ptr, 
            int len,
	    rtcp_dbentry *src, 
            rx_queue_struct *unitsrx_queue_ptr,
            int talks, 
            rtp_hdr_t *hdr, 
            session_struct *sp, 
            u_int32 cur_time)
{
	int	units, i, j, k, trailing; 
	rx_queue_element_struct	*p;
        cc_unit *ccu;

        /* we no longer break data units across rx elements here.
         * instead we leave channel coded data in first block and
         * remove channel coding when we are ready to decode and play 
         * samples. 
         */

        ccu = (cc_unit*)block_alloc(sizeof(cc_unit));
        memset(ccu,0,sizeof(cc_unit));
        units = validate_and_split(hdr->pt, data_ptr, len, ccu, &trailing);

        if (units <=0) {
	    	dprintf("Validate and split failed!\n");
            	block_free(ccu,sizeof(cc_unit));
		return 0;
	}

        for(i=0;i<ccu->iovc;i++) {
                ccu->iov[i].iov_base = (caddr_t)block_alloc(ccu->iov[i].iov_len);
                memcpy(ccu->iov[i].iov_base, 
                       data_ptr,
                       ccu->iov[i].iov_len);
                data_ptr += ccu->iov[i].iov_len;
        }

        for(i=0;i<trailing;i++) {
		p = new_rx_unit();
		p->unit_size        = cp->unit_len;
		p->units_per_pckt   = units;
		p->mixed            = FALSE;
		p->dbe_source[0]    = src;
                p->playoutpt        = playout_pt + i * cp->unit_len;
                p->src_ts           = hdr->ts + i * cp->unit_len;
                p->comp_count       = 0;
                p->cc_pt            = ccu->cc->pt;
		for (j = 0, k = 1; j < hdr->cc; j++) {
			p->dbe_source[k] = update_database(sp, ntohl(hdr->csrc[j]), cur_time);
			if (p->dbe_source[k] != NULL) {
				mark_active_sender(p->dbe_source[k], sp);
				k++;
			}
		}
		p->dbe_source_count = k;
                p->native_count = 0;
                if (i==0) {
                    p->ccu[0]  = ccu;
                    p->ccu_cnt = 1;
                    p->talk_spurt_start = talks;
                } else {
                    p->ccu[0]  = NULL;
                    p->ccu_cnt = 0;
                    p->talk_spurt_start = FALSE;
                }
                put_on_rx_queue(p, unitsrx_queue_ptr);
	}
        mark_active_sender(src, sp);
	return (units);
}

static u_int32
adapt_playout(rtp_hdr_t *hdr, 
              int arrival_ts, 
              rtcp_dbentry *src,
	      session_struct *sp, 
              struct s_cushion_struct *cushion, 
              u_int32 cur_time)
{
	u_int32	playout, var;
        u_int32 minv, maxv; 

	int	delay, diff;
	codec_t	*cp;
	int	real_playout;
        
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
		diff       = abs(delay - src->delay);
		src->delay = delay;
		/* Jitter calculation as in RTP spec */
		src->jitter = src->jitter + (((double) diff - src->jitter) / 16);
	}

	if (ts_gt(hdr->ts, src->last_ts)) {
		cp = get_codec(src->encs[0]);
		/* IF (a) TS start 
                   OR (b) we've thrown 4 consecutive packets away 
                   OR (c) ts have jumped by 8 packets worth 
                   OR (d) playout buffer running dry.
                   THEN adapt playout and communicate it
                   */
		if ((hdr->m) || 
                    src->cont_toged > 4 || 
                    ts_gt(hdr->ts, (src->last_ts + (hdr->seq - src->last_seq) * cp->unit_len * 8 + 1)) ||
                    src->playout_danger) {
#ifdef DEBUG
                        if (hdr->m) {
                                dprintf("New talkspurt\n");
                        } else if (src->cont_toged > 4) {
                                dprintf("Cont_toged > 4\n");
                        } else if (src->playout_danger) {
                                dprintf("playout danger\n");
                        } else {
                                dprintf("Time stamp jump %ld %ld\n", hdr->ts, src->last_ts);
                        }
#endif
			var = (u_int32) src->jitter * 3;

                        minv = sp->min_playout * get_freq(src->clock) / 1000;
                        maxv = sp->max_playout * get_freq(src->clock) / 1000; 

                        assert(maxv > minv);
                        if (sp->limit_playout) {
                                var = max(minv, var);
                                var = min(maxv, var);
                        }

			var += cushion_get_size(cushion);
			if (src->clock!=sp->device_clock) {
				var += cp->unit_len;
			}

                        assert(var > 0);
			src->playout = src->delay + var;

			if (sp->sync_on) {
				/* Communicate our playout delay to the video tool... */
				real_playout = (convert_time(hdr->ts + src->playout - cur_time, src->clock, sp->device_clock) * 1000)/get_freq(sp->device_clock);
				update_video_playout(src->sentry->cname, real_playout);

				/* If the video tool is slower than us, then
				 * adjust to match it...  src->video_playout is
				 * the delay of the video, converted to the clock 
				 * base of that participant.
				 */
				if (src->video_playout_received == TRUE && 
                                    src->video_playout > src->playout) {
					src->playout = src->video_playout;
				}
			}
		} else {
			/* Do not set encoding on TS start packets as they do not show if redundancy is used...   */
			src->encoding = hdr->pt;
		}
		src->last_ts        = hdr->ts;
		src->last_seq       = hdr->seq;
                src->playout_danger = FALSE;
	}

	/* Calculate the playout point in local source time for this packet. */
	playout = hdr->ts + src->playout;

        if (src->cont_toged > 12) {
                /* something has gone wrong if this assertion fails*/
                assert(playout > get_time(src->clock));
        }

	return playout;
}

static int
rtp_header_validation(rtp_hdr_t *hdr, int *len, int *extlen)
{
	/* This function checks the header info to make sure that the packet */
	/* is valid. We return TRUE if the packet is valid, FALSE otherwise. */
	/* This follows from page 52 of RFC1889.            [csp 22-10-1996] */

	/* We only accept RTPv2 packets... */
	if (hdr->type != 2) {
		dprintf("rtp_header_validation: version != 2\n");
		return FALSE;
	}

	/* Check for valid audio payload types... */
	if (((hdr->pt > 23) && (hdr->pt < 96)) || (hdr->pt > 127)) {
		dprintf("rtp_header_validation: payload-type out of audio range\n");
		return FALSE;
	}

	/* If padding or header-extension is set, we punt on this one... */
	/* We should really deal with it though...                       */
	if (hdr->p) {
                int pad = *((unsigned char *)hdr + *len - 1);
                if (pad < 1) {
                        dprintf("rtp_header_validation: padding but 0 len\n");
                        return FALSE;
                }
                *len -= pad;
        }

        if (hdr->x) {
                *extlen = *((u_int32*)((unsigned char*)hdr + 4*(3+hdr->cc)))&0x0000ffff;
	} else {
                *extlen = 0;
        }

	return (TRUE);
}

static void
receiver_change_format(rtcp_dbentry *dbe, codec_t *cp)
{
        dprintf("Changing Format.\n");
	dbe->first_pckt_flag = TRUE;
	change_freq(dbe->clock, cp->freq);
}

void
statistics(session_struct    *sp,
	   pckt_queue_struct *netrx_pckt_queue,
	   rx_queue_struct   *unitsrx_queue_ptr,
	   struct s_cushion_struct    *cushion,
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
	u_char		*data_ptr;
	int		len,extlen,compat;
	rtcp_dbentry	*src;
	u_int32		playout_pt;
	pckt_queue_element_struct *e_ptr;
	codec_t		*pcp;

	char update_req = FALSE;

	/* Process an incoming packets */
        while(netrx_pckt_queue->queue_empty == FALSE) {
                e_ptr = get_pckt_off_queue(netrx_pckt_queue);
                /* Impose RTP formating on it... */
                hdr = (rtp_hdr_t *) (e_ptr->pckt_ptr);
        
                if (rtp_header_validation(hdr, &e_ptr->len, &extlen) == FALSE) {
                        dprintf("RTP Packet failed header validation!\n");
                        /* XXX log as bad packet */
                        goto release;
                }
        
                /* Convert from network byte-order */
                hdr->seq  = ntohs(hdr->seq);
                hdr->ts   = ntohl(hdr->ts);
                hdr->ssrc = ntohl(hdr->ssrc);
        
                if ((hdr->ssrc == sp->db->myssrc) && 
                    sp->filter_loopback) {
                        /* Discard loopback packets...unless we have asked for them ;-) */
                        goto release;
                }
        
                /* Get database entry of participant that sent this packet */
                src = update_database(sp, hdr->ssrc, cur_time);
                if (src == NULL) {
                        dprintf("Packet from unknown participant\n");
                        /* Discard packets from unknown participant */
                        goto release;
                }
                rtcp_update_seq(src, hdr->seq);

                if (sp->have_device == FALSE) {
                        /* we don't have the audio device so there is no point
                         * processing data any further.
                         */
                        goto release;
                }

                data_ptr =  (char *)e_ptr->pckt_ptr + 4 * (3 + hdr->cc) + extlen;
                len      = e_ptr->len - 4 * (3 + hdr->cc) - extlen;
        
                if (!(pcp = get_codec(hdr->pt))) {
                        /* this is either a channel coded block or we can't decode it */
                        if (!(pcp = get_codec(get_wrapped_payload(hdr->pt, data_ptr, len)))) {
                                dprintf("Cannot decode data.\n");
                                goto release;
                        }
                }
        
                compat = codec_compatible(pcp, get_codec(sp->encodings[0]));
                if (!compat && !sp->auto_convert) {
                        dprintf("Format conversion not enabled (%s received).\n", pcp->name);
                        goto release;
                }
                
                if ((src->encs[0] == -1) || (compat==0))
                        receiver_change_format(src, pcp);
                
                if (src->encs[0] != pcp->pt) {
                        /* we should tell update more about coded format */
                        src->encs[0] = pcp->pt;
                        update_req   = TRUE;
                }
                
                playout_pt = adapt_playout(hdr, e_ptr->arrival_timestamp, src, sp, cushion, cur_time);
                src->units_per_packet = split_block(playout_pt, pcp, data_ptr, len, src, unitsrx_queue_ptr, hdr->m, hdr, sp, cur_time);
                
                if (!src->units_per_packet) {
                        goto release;
                }
        
                if (update_req) update_stats(src, sp);
        release:
                free_pckt_queue_element(&e_ptr);
        }
}

