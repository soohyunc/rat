/*
 * FILE:	statistics.c
 * PROGRAM:	RAT
 * AUTHOR(S):   O.Hodson, I.Kouvelas, C.Perkins, D.Miras, and V.J.Hardman
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-99 University College London
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
#include "new_channel.h"
#include "convert.h"
#include "codec.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "ts.h"
#include "timers.h"
#include "session.h"
#include "source.h"
#include "timers.h"
#include "pckt_queue.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "audio.h"
#include "cushion.h"
#include "codec.h"
#include "ui.h"
#include "statistics.h"

static rtcp_dbentry *
update_database(session_struct *sp, u_int32 ssrc)
{
	rtcp_dbentry   *dbe_source;

	/* This function gets the relevant data base entry */
	dbe_source = rtcp_get_dbentry(sp, ssrc);
	if (dbe_source == NULL) {
		/* We haven't received an RTCP packet for this source,
		 * so we must throw the packets away. This seems a
		 * little extreme, but there are actually a couple of
		 * good reasons for it: 1) If we're receiving
		 * encrypted data, but we don't have the decryption
		 * key, then every RTP packet we receive will have a
		 * different SSRC and if we create a new database
		 * entry for it, we fill up the database with garbage
		 * (which is then displayed in the list of
		 * participants...)  2) The RTP specification says
		 * that we should do it this way (sec 6.2.1) [csp] 
                 */

		return NULL;
	}

	dbe_source->last_active = get_time(sp->device_clock);
	dbe_source->is_sender   = 1;

	sp->db->pckts_received++;

	return dbe_source;
}

/* Returns new playout timestamp */

static ts_t 
adapt_playout(rtp_hdr_t               *hdr, 
              ts_t                     arr_ts,
              ts_t                     src_ts,
              rtcp_dbentry            *src,
	      session_struct          *sp, 
              struct s_cushion_struct *cushion, 
	      u_int32                  ntp_time)
{
	u_int32	var;
        u_int32 minv, maxv; 

	int	src_freq;
	codec_id_t            cid;
        const codec_format_t *cf;
	u_int32	ntptime, play_time;
	u_int32	sendtime = 0;
        int     ntp_delay = 0;
	u_int32	rtp_time;
        ts_t    delay_ts, diff_ts, playout;

        int64 since_last_sr;

        /* This is kind of disgusting ... the device clock is a 64-bit 96kHz 
         * clock.  First map it to a 32-bit timestamp of the source 
         */
    
        delay_ts = ts_sub(arr_ts, src_ts);

        /* Keep a note a source freq */
        src_freq = get_freq(src->clock);
        assert(src_freq % 8000 == 0 || src_freq % 11025 == 0);

	if (src->first_pckt_flag == TRUE) {
		src->delay                 = delay_ts;
                src->delay_in_playout_calc = delay_ts;
		hdr->m                     = TRUE;
                cid = codec_get_by_payload(src->enc);
                if (cid) {
                        cf = codec_get_format(cid);
                        src->jitter  = 3 * 20 * cf->format.sample_rate / 1000;
                } else {
                        src->jitter  = 240; 
                }
	} else {
                if (hdr->seq != src->last_seq) {
                        diff_ts      = ts_abs_diff(delay_ts, src->delay);
                        /* Jitter calculation as in RTP spec */
                        src->jitter += (((double) diff_ts.ticks - src->jitter) / 16.0);
                        src->delay = delay_ts;
                }
	}

	/* 
	 * besides having the lip-sync option enabled, we also need to have
	 * a SR received [dm]
	 */	
	if (sp->sync_on && src->mapping_valid) {
		/* calculate delay in absolute (real) time [dm] */ 
		ntptime = (src->last_ntp_sec & 0xffff) << 16 | src->last_ntp_frac >> 16;
		if (hdr->ts > src->last_rtp_ts) {
			since_last_sr = hdr->ts - src->last_rtp_ts;	
		} else {
			since_last_sr = src->last_rtp_ts - hdr->ts;
		}
		since_last_sr = (since_last_sr << 16) / get_freq(src->clock);
		sendtime = (u_int32)(ntptime + since_last_sr); /* (since_last_sr << 16) / get_freq(src->clock); */

		ntp_delay = ntp_time - sendtime; 

		if (src->first_pckt_flag == TRUE) { 
			src->sync_playout_delay = ntp_delay;
		}
	}

	if (hdr->seq > src->last_seq) {
		/* IF (a) TS start 
                   OR (b) we've thrown 4 consecutive packets away 
                   OR (c) ts have jumped by 8 packets worth 
                   OR (e) a new/empty playout buffer.
                   THEN adapt playout and communicate it
                   */
		if (hdr->m || 
                    src->cont_toged || 
                    (hdr->seq - src->last_seq > 8) ) {
#ifdef DEBUG
                        if (hdr->m) {
                                debug_msg("New talkspurt\n");
                        } else if (src->cont_toged) {
                                debug_msg("Cont_toged\n");
                        } else {
                                debug_msg("Seq jump %ld %ld\n", hdr->seq, src->last_seq);
                        }
#endif
			var = (u_int32) src->jitter * 3;
                        
                        debug_msg("jitter %d\n", var / 3);

                        if (var < (unsigned)src->inter_pkt_gap) {
                                debug_msg("var (%d) < inter_pkt_gap (%d)\n", var, src->inter_pkt_gap);
                                var = src->inter_pkt_gap;
                        }

                        var = max(var, 3 * cushion_get_size(cushion) / 2);

                        debug_msg("Cushion %d\n", cushion_get_size(cushion));
                        minv = sp->min_playout * get_freq(src->clock) / 1000;
                        maxv = sp->max_playout * get_freq(src->clock) / 1000; 

                        assert(maxv > minv);
                        if (sp->limit_playout) {
                                var = max(minv, var);
                                var = min(maxv, var);
                        }

                        assert(var > 0);

                        if (!ts_eq(src->delay_in_playout_calc, src->delay)) {
                                debug_msg("Old delay (%u) new delay (%u) delta (%u)\n", 
                                          src->delay_in_playout_calc.ticks, 
                                          src->delay.ticks, 
                                          max(src->delay_in_playout_calc.ticks, src->delay.ticks) - 
                                          min(src->delay_in_playout_calc.ticks, src->delay.ticks));
                        }

                        src->delay_in_playout_calc = src->delay;
                        
                        if (src->first_pckt_flag != TRUE &&
                            source_get_by_rtcp_dbentry(sp->active_sources, src) != 0) {
                                /* If playout buffer is not empty
                                 * or, difference in time stamps is less than 1 sec,
                                 * we don't want playout point to be before that of existing data.
                                 */
                                ts_t new_playout;
                                new_playout = ts_add(src->delay, 
                                                     ts_map32(src_freq, var));
                                debug_msg("Buf exists (%u) (%u)\n", 
                                          src->playout.ticks, 
                                          new_playout.ticks);
                                if (ts_gt(new_playout, src->playout)) {
                                        src->playout = new_playout;
                                }
                        } else {
                                debug_msg("delay (%lu) var (%lu)\n", src->delay.ticks, var);
                                src->playout = ts_add(src->delay, ts_map32(src_freq, var));
                                debug_msg("src playout %lu\n", src->playout.ticks);
                        }

			if (sp->sync_on && src->mapping_valid) {
				/* use the jitter value as calculated
                                 * but convert it to a ntp_ts freq
                                 * [dm] */
				src->sync_playout_delay = ntp_delay + ((var << 16) / get_freq(src->clock));
                                
				/* Communicate our playout delay to
                                   the video tool... */
                                ui_update_video_playout(sp, src->sentry->cname, src->sync_playout_delay);
		
				/* If the video tool is slower than
                                 * us, then adjust to match it...
                                 * src->video_playout is the delay of
                                 * the video in real time */
				debug_msg("ad=%d\tvd=%d\n", src->sync_playout_delay, src->video_playout);
                                if (src->video_playout_received == TRUE &&
                                    src->video_playout > src->sync_playout_delay) {
                                        src->sync_playout_delay = src->video_playout;
                                }
			}
                        src->skew_adjust = 0;
                } else {
                        /* No reason to adapt playout unless too
                         * little or too much audio buffered in
                         * comparison to to amount of audio in a
                         * packet. 
                        codec_id_t id;
                        u_int32 buffered, audio_per_pckt, adjust, samples_per_frame, cs;
                        static u_int32 last_adjust;

                        id = codec_get_by_payload(src->enc);
                        if (id) {
                                samples_per_frame = codec_get_samples_per_frame(id);
                        } else {
                                samples_per_frame = 160;
                        }

                        audio_per_pckt    = src->units_per_packet * samples_per_frame;

                        buffered = receive_buffer_duration_ms(sp->receive_buf_list, src) * 8;
                        cs = cushion_get_size(cushion);
                        if (ts_gt(get_time(sp->device_clock), last_adjust + 4000)) {
                                if (buffered < cs + audio_per_pckt) {
                                        adjust            = samples_per_frame * src->units_per_packet;
                                        src->playout     += adjust;
                                        src->skew_adjust += adjust;
                                        debug_msg("*** shifted %d +samples, previous time %08u\n", adjust, get_time(sp->device_clock) - last_adjust);
                                        last_adjust = get_time(sp->device_clock);
                                } else if (buffered > cs + 3 * audio_per_pckt) {
                                        adjust            = samples_per_frame / 2;
                                        src->playout     -= samples_per_frame;
                                        src->skew_adjust -= adjust;
                                        debug_msg("*** shifted %d -samples, previous time %08u\n", adjust, get_time(sp->device_clock) - last_adjust);
                                        last_adjust = get_time(sp->device_clock);
                                }
                                
                        }
                        */
                }
        }
        src->last_seq       = hdr->seq;

	/* Calculate the playout point in local source time for this packet. */
        if (sp->sync_on && src->mapping_valid) {
		/* 	
		 * Use the NTP to RTP ts mapping to calculate the playout time 
		 * converted to the clock base of the receiver
		 */
		play_time = sendtime + src->sync_playout_delay;
		rtp_time = sp->db->map_rtp_time + (((play_time - sp->db->map_ntp_time) * get_freq(src->clock)) >> 16);
                playout  = ts_map32(src_freq, rtp_time);
		src->playout = ts_sub(playout, src_ts);
	} else {
		playout = ts_add(src_ts, src->playout);
	}

        if (ts_gt(arr_ts, playout)) {
                debug_msg("Will be discarded %u %u.\n", arr_ts.ticks, playout.ticks);
        }

        if (src->cont_toged > 12) {
                /* something has gone wrong if this assertion fails*/
                if (!ts_gt(playout, ts_map32(src_freq, get_time(src->clock)))) {
                        debug_msg("playout before now.\n");
                        src->first_pckt_flag = TRUE;
                }
        }

        src->first_pckt_flag = FALSE;

	return playout;
}

static int
rtp_header_validation(rtp_hdr_t *hdr, int32 *len, int *extlen)
{
	/* This function checks the header info to make sure that the packet */
	/* is valid. We return TRUE if the packet is valid, FALSE otherwise. */
	/* This follows from page 52 of RFC1889.            [csp 22-10-1996] */

	/* We only accept RTPv2 packets... */
	if (hdr->type != 2) {
		debug_msg("rtp_header_validation: version != 2\n");
		return FALSE;
	}

	/* Check for valid audio payload types... */
	if (((hdr->pt > 23) && (hdr->pt < 96)) || (hdr->pt > 127)) {
		debug_msg("rtp_header_validation: payload-type out of audio range\n");
		return FALSE;
	}

	/* If padding or header-extension is set, we punt on this one... */
	/* We should really deal with it though...                       */
	if (hdr->p) {
                int pad = *((unsigned char *)hdr + *len - 1);
                if (pad < 1) {
                        debug_msg("rtp_header_validation: padding but 0 len\n");
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

static int
statistics_channel_extract(rtcp_dbentry *dbe,
                           u_int8        pt,
                           u_char*       data,
                           u_int32       len)
{
        cc_id_t ccid;
        u_int16 upp;
        u_int8  codec_pt;
        assert(dbe != NULL);

        ccid = channel_coder_get_by_payload(pt);
        if (!ccid) {
                debug_msg("No channel decoder\n");
                return FALSE;
        }

        if (!channel_verify_and_stat(ccid,pt,data,len,&upp,&codec_pt)) {
                debug_msg("Failed verify and stat\n");
                return FALSE;
        }

        if (dbe->units_per_packet != upp) {
                dbe->units_per_packet = upp;
                dbe->update_req       = TRUE;
        }

        if (dbe->enc != codec_pt) {
                const codec_format_t *cf;
                codec_id_t            id;

                id = codec_get_by_payload(codec_pt);
                cf = codec_get_format(id);

                change_freq(dbe->clock, cf->format.sample_rate);

                dbe->enc             = codec_pt;
                dbe->inter_pkt_gap   = dbe->units_per_packet * codec_get_samples_per_frame(id);
                dbe->first_pckt_flag = TRUE;
                dbe->update_req      = TRUE;
        }

        return TRUE;
}

void
statistics(session_struct          *sp,
	   struct s_pckt_queue     *rtp_pckt_queue,
	   struct s_cushion_struct *cushion,
	   u_int32	            ntp_time)
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
         * 
         */

        ts_t                arr_ts, src_ts;

	rtp_hdr_t	   *hdr;
	u_char		   *data_ptr;
	int		    len;
	rtcp_dbentry	   *sender = NULL;
        const audio_format* af;

	pckt_queue_element *pckt;
        struct s_source *src;
        int pkt_cnt = 0;

        af = audio_get_ofmt(sp->audio_device);

	/* Process incoming packets */
        while( (pckt = pckt_dequeue(rtp_pckt_queue)) != NULL ) {
                pkt_cnt++;
                block_trash_check();
                /* Impose RTP formating on it... */
                hdr = (rtp_hdr_t *) (pckt->pckt_ptr);
        
                if (rtp_header_validation(hdr, &pckt->len, (int*)&pckt->extlen) == FALSE) {
                        debug_msg("RTP Packet failed header validation!\n");
                        block_trash_check();
                        goto release;
                }

                if (sp->playing_audio == FALSE) {
                        /* Don't decode audio if we are not playing it! */
                        goto release;
                }
        
                /* Convert from network byte-order */
                hdr->seq  = ntohs(hdr->seq);
                hdr->ts   = ntohl(hdr->ts);
                hdr->ssrc = ntohl(hdr->ssrc);
        
                if ((hdr->ssrc == sp->db->myssrc) && sp->filter_loopback) {
                        /* Discard loopback packets...unless we have asked for them ;-) */
                        block_trash_check();
                        goto release;
                }
        
                /* Get database entry of participant that sent this packet */
                sender = update_database(sp, hdr->ssrc);
                if (sender == NULL) {
                        debug_msg("Packet from unknown participant discarded\n");
                        goto release;
                }
                rtcp_update_seq(sender, hdr->seq);

                if (hdr->cc) {
                        int k;
                        for(k = 0; k < hdr->cc; k++) {
                                update_database(sp, ntohl(hdr->csrc[k]));
                        }
                }

                data_ptr =  (unsigned char *)pckt->pckt_ptr + 4 * (3 + hdr->cc) + pckt->extlen;
                len      = pckt->len - 4 * (3 + hdr->cc) - pckt->extlen;

                if (statistics_channel_extract(sender, 
                                               hdr->pt, 
                                               data_ptr, 
                                               len) == FALSE) {
                        debug_msg("Failed channel check\n");
                        goto release;
                }

                pckt->sender  = sender;

                if ((src = source_get_by_rtcp_dbentry(sp->active_sources, 
                                                      pckt->sender)) == NULL) {
                        src = source_create(sp->active_sources, 
                                            pckt->sender,
                                            sp->converter,
                                            sp->render_3d,
                                            af->sample_rate,
                                            af->channels);
                        assert(src != NULL);
                }

                arr_ts = pckt->arrival;

                /* Convert originator timestamp in ts_t */
                src_ts = ts_seq32_in(source_get_sequencer(src), 
                                     get_freq(sender->clock), 
                                     hdr->ts);

                pckt->playout = adapt_playout(hdr, 
                                              arr_ts,
                                              src_ts,
                                              sender, 
                                              sp, 
                                              cushion, 
                                              ntp_time);

                if (source_add_packet(src, 
                                      pckt->pckt_ptr, 
                                      pckt->len, 
                                      data_ptr - pckt->pckt_ptr, 
                                      hdr->pt, 
                                      pckt->playout) == TRUE) {
                        /* Source is now repsonsible for packet so
                         * empty pointers */
                        pckt->pckt_ptr = NULL;
                        pckt->len      = 0;
                }
        release:
                block_trash_check();
                pckt_queue_element_free(&pckt);
        }

        if (pkt_cnt > 5) {
                debug_msg("Processed lots of packets(%d).\n", pkt_cnt);
        }
}

