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
 */

#include "config_unix.h"
#include "config_win32.h"
#include "codec_types.h"
#include "channel.h"
#include "converter.h"
#include "codec.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "ts.h"
#include "timers.h"
#include "session.h"
#include "timers.h"
#include "pckt_queue.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "audio.h"
#include "cushion.h"
#include "codec.h"
#include "pdb.h"
#include "mix.h"
#include "ui.h"
#include "source.h"
#include "statistics.h"


static converter_id_t null_converter;

/* We cache jitter between talkspurts unless we have not heard from
 * source for discard_jitter_period.
 */

static ts_t discard_jitter_period;

void
statistics_init()
{
        null_converter        = converter_get_null_converter();
        discard_jitter_period = ts_map32(8000, 3 * 60 * 8000); /* 3 minutes */
}

static rtcp_dbentry *
update_database(session_t *sp, u_int32 ssrc)
{
	rtcp_dbentry   *dbe_source;

	/* This function gets the relevant data base entry */
	dbe_source = rtcp_get_dbentry(sp->db, ssrc);
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

static u_int32
statistics_variable_component(rtcp_dbentry            *dbe,
                              pdb_entry_t             *pdbe,
                              session_t          *sp, 
                              struct s_cushion_struct *cushion) 
{
        u_int32 var, cush;

        var = (u_int32) dbe->jitter * 3;

        debug_msg("var (jitter) %d\n", var);
        
        if (var < (unsigned)pdbe->inter_pkt_gap) {
                var = pdbe->inter_pkt_gap;
                debug_msg("jitter < pkt_gap -> %d\n", var);
        }
        
        cush = cushion_get_size(cushion);
        if (var < cush) {
                var = 3 * cush / 2;
                debug_msg("cushion %d\n", cushion_get_size(cushion));
        }

        if (sp->limit_playout) {
                u_int32 minv, maxv;
                minv = sp->min_playout * get_freq(pdbe->clock) / 1000;
                maxv = sp->max_playout * get_freq(pdbe->clock) / 1000; 
                assert(maxv > minv);
                var = max(minv, var);
                var = min(maxv, var);
        }

        return var;
}

/* Returns new playout timestamp */

static ts_t 
adapt_playout(rtp_hdr_t               *hdr, 
              ts_t                     arr_ts,
              ts_t                     src_ts,
              rtcp_dbentry            *src,
              pdb_entry_t             *pdbe,
	      session_t          *sp, 
              struct s_cushion_struct *cushion, 
	      u_int32                  ntp_time)
{
	u_int32	var;
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
        src_freq = get_freq(pdbe->clock);
        assert(src_freq % 8000 == 0 || src_freq % 11025 == 0);

	if (pdbe->first_pckt_flag == TRUE) {
		pdbe->delay                 = delay_ts;
                pdbe->delay_in_playout_calc = delay_ts;
		hdr->m                      = TRUE;
                cid = codec_get_by_payload(pdbe->enc);

                if ((ts_valid(pdbe->last_arr) == FALSE) ||
                    (ts_gt(ts_sub(arr_ts, pdbe->last_arr), discard_jitter_period))) {
                        /* Guess suitable playout only if we have not
                         *  heard from this source recently...  
                         */
                        
                        if (cid) {
                                u_int32 spf;
                                cf = codec_get_format(cid);
                                spf = codec_get_samples_per_frame(cid);
                                src->jitter  = 1.0 * (double)spf;
                                debug_msg("jitter (1 frame) %d\n", (int)src->jitter);
                        } else {
                                src->jitter  = 160; 
                                debug_msg("jitter (guess) %d\n", (int)src->jitter);
                        }
                }
	} else {
                if (hdr->seq != src->last_seq) {
                        diff_ts      = ts_abs_diff(delay_ts, pdbe->delay);
                        /* Jitter calculation as in RTP spec */
                        src->jitter += (((double) diff_ts.ticks - src->jitter) / 16.0);
#ifdef DEBUG
                        if (src->jitter == 0) {
                                debug_msg("Jitter zero\n");
                        }
#endif
                        pdbe->delay = delay_ts;
                }
	}

	/* 
	 * besides having the lip-sync option enabled, we also need to have
	 * a SR received [dm]
	 */	
	if (sp->sync_on && pdbe->mapping_valid) {
		/* calculate delay in absolute (real) time [dm] */ 
		ntptime = (pdbe->last_ntp_sec & 0xffff) << 16 | pdbe->last_ntp_frac >> 16;
		if (hdr->ts > pdbe->last_rtp_ts) {
			since_last_sr = hdr->ts - pdbe->last_rtp_ts;	
		} else {
			since_last_sr = pdbe->last_rtp_ts - hdr->ts;
		}
		since_last_sr = (since_last_sr << 16) / get_freq(pdbe->clock);
		sendtime = (u_int32)(ntptime + since_last_sr); /* (since_last_sr << 16) / get_freq(src->clock); */

		ntp_delay = ntp_time - sendtime; 

		if (pdbe->first_pckt_flag == TRUE) { 
			pdbe->sync_playout_delay = ntp_delay;
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
                    pdbe->cont_toged || 
                    (hdr->seq - src->last_seq > 8) ) {
#ifdef DEBUG
                        if (hdr->m) {
                                debug_msg("New talkspurt\n");
                        } else if (pdbe->cont_toged) {
                                debug_msg("Cont_toged\n");
                        } else {
                                debug_msg("Seq jump %ld %ld\n", hdr->seq, src->last_seq);
                        }
#endif
                        var = statistics_variable_component(src, pdbe, sp, cushion);
                        debug_msg("var = %d\n", var);

                        if (!ts_eq(pdbe->delay_in_playout_calc, pdbe->delay)) {
                                debug_msg("Old delay (%u) new delay (%u) delta (%u)\n", 
                                          pdbe->delay_in_playout_calc.ticks, 
                                          pdbe->delay.ticks, 
                                          max(pdbe->delay_in_playout_calc.ticks, pdbe->delay.ticks) - 
                                          min(pdbe->delay_in_playout_calc.ticks, pdbe->delay.ticks));
                        }

                        pdbe->delay_in_playout_calc = pdbe->delay;
                        
			debug_msg("delay (%lu) var (%lu)\n", pdbe->delay.ticks, var);
			pdbe->playout = ts_add(pdbe->delay, ts_map32(src_freq, var));
			debug_msg("src playout %lu\n", pdbe->playout.ticks);

			if (sp->sync_on && pdbe->mapping_valid) {
				/* use the jitter value as calculated
                                 * but convert it to a ntp_ts freq
                                 * [dm] */
				pdbe->sync_playout_delay = ntp_delay + ((var << 16) / get_freq(pdbe->clock));
                                
				/* Communicate our playout delay to
                                   the video tool... */
                                ui_update_video_playout(sp, src->sentry->ssrc, pdbe->sync_playout_delay);
		
				/* If the video tool is slower than
                                 * us, then adjust to match it...
                                 * src->video_playout is the delay of
                                 * the video in real time */
				debug_msg("ad=%d\tvd=%d\n", pdbe->sync_playout_delay, pdbe->video_playout);
                                if (pdbe->video_playout_received == TRUE &&
                                    pdbe->video_playout > pdbe->sync_playout_delay) {
                                        pdbe->sync_playout_delay = pdbe->video_playout;
                                }
			}
                }
        }
        src->last_seq       = hdr->seq;

	/* Calculate the playout point in local source time for this packet. */
        if (sp->sync_on && pdbe->mapping_valid) {
		/* 	
		 * Use the NTP to RTP ts mapping to calculate the playout time 
		 * converted to the clock base of the receiver
		 */
		play_time = sendtime + pdbe->sync_playout_delay;
		rtp_time = sp->db->map_rtp_time + (((play_time - sp->db->map_ntp_time) * get_freq(pdbe->clock)) >> 16);
                playout  = ts_map32(src_freq, rtp_time);
		pdbe->playout = ts_sub(playout, src_ts);
	} else {
		playout = ts_add(src_ts, pdbe->playout);
	}

        if (pdbe->cont_toged > 12) {
                /* something has gone wrong if this assertion fails*/
                if (!ts_gt(playout, ts_map32(src_freq, get_time(pdbe->clock)))) {
                        debug_msg("playout before now.\n");
                        pdbe->first_pckt_flag = TRUE;
                }
        }

        pdbe->first_pckt_flag = FALSE;

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
	if (((hdr->pt > 23) && (hdr->pt < 33)) || ((hdr->pt > 71) && (hdr->pt < 77)) || (hdr->pt > 127)) {
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
statistics_channel_extract(session_t     *sp,
                           rtcp_dbentry       *dbe,
                           pdb_entry_t        *pdbe,
                           const audio_format *afout,
                           u_int8              pt,
                           u_char*             data,
                           u_int32             len)
{
        cc_id_t ccid;
        const codec_format_t *cf;
        codec_id_t            id;
        u_int16 upp;
        u_int8  codec_pt;

        assert(dbe != NULL);
        assert(pdbe != NULL);

        ccid = channel_coder_get_by_payload(pt);
        if (!ccid) {
                debug_msg("No channel decoder\n");
                return FALSE;
        }
        
        if (!channel_verify_and_stat(ccid,pt,data,len,&upp,&codec_pt)) {
                debug_msg("Failed verify and stat\n");
                return FALSE;
        }

        if (pdbe->units_per_packet != upp) {
                pdbe->units_per_packet = upp;
                pdbe->update_req       = TRUE;
        }

        id = codec_get_by_payload(codec_pt);
        cf = codec_get_format(id);

        if (mix_compatible(sp->ms, cf->format.sample_rate, cf->format.channels) == FALSE) { 

                /* Won't go into mixer without some form of
                 * conversion.  converter will convert sample_rate and
                 * sample_channels if enabled and 3d renderer will
                 * convert number of channels from 1 to 2 if enabled
                 */
                
                if (sp->converter == null_converter &&
                    !(sp->render_3d == TRUE && 
                      cf->format.sample_rate == afout->sample_rate &&
                      afout->channels == 2)) {
                        
                        debug_msg("Rejected - needs sample rate conversion\n");
                        return FALSE;
                }
        }

        if (pdbe->channel_coder_id != ccid) {
                debug_msg("Channel coding changed\n");
                pdbe->channel_coder_id = ccid;
                pdbe->update_req = TRUE;
        }


        if (pdbe->enc != codec_pt) {
                debug_msg("Format changed\n");
                change_freq(pdbe->clock, cf->format.sample_rate);
                pdbe->enc             = codec_pt;
                pdbe->inter_pkt_gap   = pdbe->units_per_packet * (u_int16)codec_get_samples_per_frame(id);
                pdbe->first_pckt_flag = TRUE;
                pdbe->update_req      = TRUE;
        }

        if (pdbe->update_req) {
                /* Format len must be long enough for at least 2
                 * redundant encodings, a separator and a zero */
                int fmt_len = 2 * CODEC_LONG_NAME_LEN + 2;
                if (pdbe->enc_fmt == NULL) {
                        pdbe->enc_fmt = (char*)xmalloc(fmt_len);
                }
                channel_describe_data(ccid, pt, data, len, pdbe->enc_fmt, fmt_len);
                ui_update_stats(sp, pdbe);
                pdbe->update_req = FALSE;
        }

        return TRUE;
}

void
statistics_process(session_t          *sp,
                   struct s_pckt_queue     *rtp_pckt_queue,
                   struct s_cushion_struct *cushion,
                   u_int32	            ntp_time,
                   ts_t                     curr_ts)
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

        ts_t                  arr_ts, src_ts;

	rtp_hdr_t	     *hdr;
	u_char		     *data_ptr;
	int		      len;
	rtcp_dbentry	     *sender = NULL;
        pdb_entry_t          *pdbe;
        const audio_format   *afout;

	pckt_queue_element *pckt;
        struct s_source *src;
        int pkt_cnt = 0;

        afout = audio_get_ofmt(sp->audio_device);

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

                if (pdb_item_get(sp->pdb, hdr->ssrc, &pdbe) == FALSE) {
                        debug_msg("Could not get pdbe entry\n");
                        abort();
                }

                if (pdbe->mute) {
                        debug_msg("Packet to muted participant discarded\n");
                        goto release;
                }

                if (hdr->cc) {
                        int k;
                        for(k = 0; k < hdr->cc; k++) {
                                update_database(sp, ntohl(hdr->csrc[k]));
                        }
                }

                data_ptr =  (unsigned char *)pckt->pckt_ptr + 4 * (3 + hdr->cc) + pckt->extlen;
                len      = pckt->len - 4 * (3 + hdr->cc) - pckt->extlen;

                if (statistics_channel_extract(sp, 
                                               sender,
                                               pdbe,
                                               afout, 
                                               (u_char)hdr->pt, 
                                               data_ptr, 
                                               (u_int32)len) == FALSE) {
                        debug_msg("Failed channel check\n");
                        goto release;
                }

                pckt->sender  = sender;

                if ((src = source_get_by_ssrc(sp->active_sources, 
                                              pckt->sender->ssrc)) == NULL) {
			ui_info_activate(sp, pckt->sender);
                        src = source_create(sp->active_sources, 
                                            pckt->sender->ssrc,
					    sp->pdb,
                                            sp->converter,
                                            sp->render_3d,
                                            (u_int16)afout->sample_rate,
                                            (u_int16)afout->channels); 
                        assert(src != NULL);
                }

                arr_ts = pckt->arrival;

                /* Convert originator timestamp in ts_t */
                src_ts = ts_seq32_in(source_get_sequencer(src), 
                                     get_freq(pdbe->clock), 
                                     hdr->ts);

                pckt->playout = adapt_playout(hdr, 
                                              arr_ts,
                                              src_ts,
                                              sender, 
                                              pdbe,
                                              sp, 
                                              cushion, 
                                              ntp_time);

                if (source_add_packet(src,
                                      pckt->pckt_ptr, 
                                      pckt->len, 
                                      data_ptr - pckt->pckt_ptr, 
                                      (u_char)hdr->pt, 
                                      pckt->playout) == TRUE) {
                        /* Source is now repsonsible for packet so
                         * empty pointers */
                        pckt->pckt_ptr = NULL;
                        pckt->len      = 0;
                }

                pdbe->last_arr = curr_ts;
        release:
                block_trash_check();
                pckt_queue_element_free(&pckt);
        }

        if (pkt_cnt > 5) {
                debug_msg("Processed lots of packets(%d).\n", pkt_cnt);
        }
}

