/*
 * FILE:    transmit.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Isidor Kouvelas
 * 
 * $Revision$
 * $Date$
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

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "debug.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "playout.h"
#include "channel_types.h"
#include "new_channel.h"
#include "session.h"
#include "audio.h"
#include "sndfile.h"
#include "parameters.h"
#include "ui.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "net.h"
#include "timers.h"
#include "transmit.h"
#include "util.h"

/* All this code can be greatly simplified and reduced by making
 * better use of the playout buffer structure in playout.h.
 */

typedef struct s_tx_unit {
        sample  *data;             /* pointer to raw data in read_buf */
        u_int32  dur_used;         /* number of time intervals filled */
        u_int16  energy;
        u_char   silence;          /* First pass */
        u_char   send;             /* Silence second pass */
} tx_unit;

typedef struct s_tx_buffer {
        struct session_tag   *sp;
        struct s_sd          *sd_info;
        struct s_vad         *vad;
        struct s_agc         *agc;
        struct s_time        *clock;

        struct s_pb          *media_buffer; 
        struct s_pb          *channel_buffer; 
        struct s_pb          *audio_buffer; /* Audio buffer and it's iterators */
        struct s_pb_iterator *reading;  /* Current read point iterator     */
        struct s_pb_iterator *silence;  /* Silence classification iterator */
        struct s_pb_iterator *transmit; /* Transmission point iterator     */
        struct s_codec_state_store *state_store;    /* Encoder states        */
        u_int32        sending_audio:1;
        u_int16 channels;
        u_int16 unit_dur; /* dur. in sampling intervals (excludes channels) */

        /* Statistics log */
        double          mean_read_dur;
        /* These are a hack because we use playout buffer
         * which expects time units of type ts_t so we need
         * to be able to map to and from 32 bit no for
         * packet timestamp */
        ts_sequencer    down_seq;  /* used for 32 -> ts_t */
        ts_sequencer    up_seq;    /* used for ts_t -> 32 */

        /* place for the samples */
        sample samples[DEVICE_REC_BUF];
        int    last_sample; /* Stores the index of the last read buffer */
} tx_buffer;

static sample dummy_buf[DEVICE_REC_BUF];

static int
tx_unit_create(tx_buffer *tb, tx_unit  **ptu, int n_samples)
{
        tx_unit *tu;
        tu = block_alloc(sizeof(tx_unit));
        if (tu) {
                memset(tu, 0, sizeof(tx_unit));
                *ptu = tu;
                /* Position sample pointer */
                if (tb->last_sample + n_samples > DEVICE_REC_BUF) {
                        tb->last_sample = 0;
                }
                tu->data = tb->samples + tb->last_sample;
                tu->energy = 555;
                tb->last_sample += n_samples;
                return TRUE;
        }
        debug_msg("Failed to allocate tx_unit\n");
        return FALSE;
}

static void
tx_unit_destroy(tx_unit **ptu, u_int32 len)
{
        tx_unit *tu = *ptu;
        assert(tu != NULL);
        assert(len == sizeof(tx_unit));

        block_free(tu, sizeof(tx_unit));
        *ptu = NULL;
}

int
tx_create(tx_buffer     **ntb, 
          session_struct *sp,
          struct s_time  *clock,
          u_int16         unit_dur, 
          u_int16         channels)
{
        tx_buffer *tb;

        tb = (tx_buffer*)xmalloc(sizeof(tx_buffer));
        if (tb) {
                memset(tb, 0, sizeof(tx_buffer));
                debug_msg("Unit duration %d channels %d\n", 
                          unit_dur, 
                          channels);
                tb->sp      = sp;
                tb->clock   = clock;
                tb->sd_info = sd_init    (unit_dur, 
                                          (u_int16)get_freq(clock));
                tb->vad     = vad_create (unit_dur, 
                                          (u_int16)get_freq(clock));
                tb->agc     = agc_create(sp);
                tb->unit_dur = unit_dur;
                tb->channels = channels;
                tb->mean_read_dur = unit_dur;
                
                pb_create(&tb->audio_buffer,
                          (playoutfreeproc)tx_unit_destroy);

                pb_create(&tb->media_buffer,
                          (playoutfreeproc)media_data_destroy);

                pb_create(&tb->channel_buffer,
                          (playoutfreeproc)channel_data_destroy);

                *ntb = tb;
                return TRUE;
        }
        return FALSE;
}

void
tx_destroy(tx_buffer **ptb)
{
        tx_buffer *tb;

        assert(ptb != NULL);
        tb = *ptb;
        assert(tb != NULL);

        sd_destroy(tb->sd_info);
        vad_destroy(tb->vad);
        agc_destroy(tb->agc);

        pb_destroy(&tb->audio_buffer);
        pb_destroy(&tb->media_buffer);
        pb_destroy(&tb->channel_buffer);

        xfree(tb);
        *ptb = NULL;
}

/* These routines are called when the button on the interface is toggled */
void
tx_start(tx_buffer *tb)
{
        tx_unit *tu_new;
        ts_t     unit_start;

        /* Not sure why this is here (?) */
        if (tb->sending_audio) {
                debug_msg("Why? Fix");
                return;
        }

        tb->sending_audio = TRUE;

        /* Turn off auto lecture */
        tb->sp->auto_lecture = 1;       

        /* Reset signal classification and auto-scaling */
        sd_reset(tb->sd_info);
        vad_reset(tb->vad);
        agc_reset(tb->agc);

        /* Attach iterator for silence classification */
        pb_iterator_create(tb->audio_buffer, &tb->transmit);
        pb_iterator_create(tb->audio_buffer, &tb->silence);
        pb_iterator_create(tb->audio_buffer, &tb->reading);

        /* Add one unit to media buffer to kick off audio reading */
        unit_start = ts_map32(get_freq(tb->clock), rand());
        tx_unit_create(tb, &tu_new, tb->unit_dur * tb->channels);
        assert(ts_valid(unit_start));
        pb_add(tb->audio_buffer, 
               (u_char*)tu_new,
               sizeof(tx_unit),
               unit_start);

        /* And then put reading iterator on it */
        pb_iterator_advance(tb->reading);

        assert(tb->state_store == NULL);
        codec_state_store_create(&tb->state_store, ENCODER);
}

void
tx_stop(tx_buffer *tb)
{
        struct timeval tv;

        /* Again not sure why this is here */
        if (tb->sending_audio == FALSE) {
                return;
        }

        gettimeofday(&tv, NULL);
        tb->sp->auto_lecture  = tv.tv_sec;
        codec_state_store_destroy(&tb->state_store);
        channel_encoder_reset(tb->sp->channel_coder);
        ui_input_level(tb->sp, 0);
        tb->sending_audio = FALSE;
        tx_update_ui(tb);
        
        /* Detach iterators      */
        pb_iterator_destroy(tb->audio_buffer, &tb->transmit);
        pb_iterator_destroy(tb->audio_buffer, &tb->silence);
        pb_iterator_destroy(tb->audio_buffer, &tb->reading);

        /* Drain playout buffers */
        pb_flush(tb->audio_buffer);
        pb_flush(tb->media_buffer);
        pb_flush(tb->channel_buffer);
}


int
tx_read_audio(tx_buffer *tb)
{
        session_struct  *sp;
        tx_unit 	*u;
        u_int32          ulen, freq;
        ts_t             u_ts;
        unsigned int	 read_dur = 0;
        unsigned int     this_read;

        assert(tb->channels > 0 && tb->channels <= 2);

        sp = tb->sp;
        if (tb->sending_audio) {
                freq = get_freq(tb->clock);
                do {
                        if (pb_iterator_get_at(tb->reading, (u_char**)&u, &ulen, &u_ts) == FALSE) {
                                debug_msg("Reading iterator failed to get unit!\n");
                        }
                        assert(u != NULL);

                        this_read = audio_read(sp->audio_device, 
                                               u->data + u->dur_used * tb->channels,
                                               (tb->unit_dur - u->dur_used) * tb->channels) / tb->channels;
                        if (sp->in_file) {
                                snd_read_audio(&sp->in_file, 
                                                u->data + u->dur_used * tb->channels,
                                                (u_int16)((tb->unit_dur - u->dur_used) * tb->channels));
                        }
                        
                        u->dur_used += this_read;
                        if (u->dur_used == tb->unit_dur) {
                                read_dur += tb->unit_dur;
                                time_advance(sp->clock, freq, tb->unit_dur);
                                u_ts = ts_add(u_ts, ts_map32(get_freq(tb->clock), tb->unit_dur));
                                tx_unit_create(tb, &u, tb->unit_dur * tb->channels);
                                pb_add(tb->audio_buffer, (u_char*)u, ulen, u_ts);
                                pb_iterator_advance(tb->reading);
                        } 
                } while (u->dur_used == tb->unit_dur);
        } else {
                /* We're not sending, but have access to the audio device. Read the audio anyway. */
                /* to get exact timing values, and then throw the data we've just read away...    */
                read_dur = audio_read(sp->audio_device, dummy_buf, DEVICE_REC_BUF / 4) / sp->tb->channels;
                time_advance(sp->clock, get_freq(sp->tb->clock), read_dur);
        }
        
        if ((double)read_dur > 5.0 * sp->tb->mean_read_dur) {
                debug_msg("Should clear transmit buffer because read_len big (%d cf %0.0f)\n",
                          read_dur,
                          sp->tb->mean_read_dur);
        } 

        if (read_dur) {
                sp->tb->mean_read_dur += ((double)read_dur - sp->tb->mean_read_dur) / 8.0;
        }

        return read_dur;
}

int
tx_process_audio(tx_buffer *tb)
{
        session_struct       *sp;
        struct s_pb_iterator *marker;
        tx_unit              *u;
        u_int32               u_len;
        ts_t                  u_ts;
        int                   to_send;
        
        sp = tb->sp;

	if (tb->sending_audio) {
                
                /* Do signal classification up until read point, that
                 * is not a complete audio frame so cannot be done 
                 */
                pb_iterator_get_at(tb->silence, (u_char**)&u, &u_len, &u_ts);
                while (pb_iterators_equal(tb->silence, tb->reading) == FALSE) {
			audio_unbias(sp->bc, u->data, u->dur_used * tb->channels);
			u->energy = avg_audio_energy(u->data, u->dur_used * tb->channels, tb->channels);
			u->send   = FALSE;

                        /* Silence classification on this block */
			u->silence = sd(tb->sd_info, (u_int16)u->energy);

                        /* Pass decision to voice activity detector (damps transients, etc) */
			to_send    = vad_to_get(tb->vad, 
                                                (u_char)u->silence, 
                                                (u_char)((sp->lecture) ? VAD_MODE_LECT : VAD_MODE_CONF));           
			agc_update(tb->agc, (u_int16)u->energy, vad_talkspurt_no(tb->vad));

			if (sp->detect_silence) {
                                if (to_send != 0) {
                                        pb_iterator_dup(&marker, tb->silence);
                                        while(u != NULL && to_send != 0) {
                                                u->send = TRUE;
                                                to_send --;
                                                pb_iterator_retreat(marker);
                                                pb_iterator_get_at(marker, (u_char**)&u, &u_len, &u_ts);
                                        }
                                        pb_iterator_destroy(tb->audio_buffer, &marker);
                                }
			} else {
				u->silence = FALSE;
				u->send    = TRUE;
			}
                        pb_iterator_advance(tb->silence);
                        pb_iterator_get_at(tb->silence, (u_char**)&u, &u_len, &u_ts);
                }

		if (sp->agc_on == TRUE && 
		    agc_apply_changes(tb->agc) == TRUE) {
			ui_update_input_gain(sp);
		}
	}
        return TRUE;
}

static int
tx_encode(struct s_codec_state_store *css, 
          sample     *buf, 
          u_int32     dur_used,
          u_char      payload, 
          coded_unit *coded)
{
        codec_id_t id;
        coded_unit native;

        codec_state *cs;
        const codec_format_t *cf;

        id = codec_get_by_payload(payload);
        assert(id);

        cf = codec_get_format(id);

        /* native is a temporary coded_unit that we use to pass to
         * codec_encode since this take a 'native' (raw) coded unit as
         * input and fills in coded with the transformed data.
         */
        native.id        = codec_get_native_coding((u_int16)cf->format.sample_rate, 
                                                   (u_int16)cf->format.channels);
        native.state     = NULL;
        native.state_len = 0;
        native.data      = (u_char*)buf;
        native.data_len  = (u_int16)(dur_used * sizeof(sample) * cf->format.channels);

        /* Get codec state from those stored for us */
        cs = codec_state_store_get(css, id);
        return codec_encode(cs, &native, coded);
}

void
tx_send(tx_buffer *tb)
{
        struct s_pb_iterator    *cpos;
        channel_data            *cd;
        channel_unit            *cu;

        session_struct *sp;

        tx_unit        *u;
        rtp_hdr_t       rtp_header;
        struct iovec    ovec[2];
        ts_t            u_ts, u_sil_ts, delta;
        ts_t            time_ts;
        u_int32         time_32, cd_len, freq;
        u_int32         u_len, units, i, n, send, encoding;
        int success;

        if (pb_iterators_equal(tb->silence, tb->transmit)) {
                /* Nothing to do */
                debug_msg("Nothing to do\n");
                return;
        }

        pb_iterator_get_at(tb->silence,  (u_char**)&u, &u_len, &u_sil_ts);
        pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);

        assert(ts_gt(u_sil_ts, u_ts));

        delta = ts_sub(u_sil_ts, u_ts);
        n = delta.ticks / tb->unit_dur;

        rtp_header.cc = 0;
        sp = tb->sp;
        units = channel_encoder_get_units_per_packet(sp->channel_coder);
        freq  = get_freq(tb->clock);
        
        while(n > units) {
                send = FALSE;

                /* Check whether we want to send this group of units */
                for (i = 0; i < units; i++) {
                        pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);
                        if (u->send) {
                                send = TRUE;
                                break;
                        }
                        pb_iterator_advance(tb->transmit);
                }

                /* Rewind transmit point to where it was before we did
                 * last check */
                while(i > 0) {
                        pb_iterator_retreat(tb->transmit);
                        i--;
                }
                
                for (i = 0;i < units; i++) {
                        media_data *m;
                        success = pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);
                        assert(success);
                        if (send) {
                                media_data_create(&m, sp->num_encodings);
                                for(encoding = 0; encoding < (u_int32)sp->num_encodings; encoding ++) {
                                        tx_encode(tb->state_store, 
                                                  u->data, 
                                                  u->dur_used,
                                                  sp->encodings[encoding], 
                                                  m->rep[encoding]);
                                }
                        } else {
                                media_data_create(&m, 0);
                        }
                        assert(m != NULL);
                        success = pb_add(tb->media_buffer, 
                                         (u_char*)m, 
                                         sizeof(media_data), 
                                         u_ts);
                        assert(success);
                        success = pb_iterator_advance(tb->transmit);
                        assert(success);
                }
                n -= units;
        }

        channel_encoder_encode(sp->channel_coder, 
                               tb->media_buffer, 
                               tb->channel_buffer);

        pb_iterator_create(tb->channel_buffer, &cpos);
        while(pb_iterator_get_at(cpos, (u_char**)&cd, &cd_len, &time_ts)) {
                if (pb_iterator_detach_at(cpos, 
                                          (u_char**)&cd, 
                                          &cd_len, 
                                          &time_ts) == FALSE){
                        debug_msg("Failed to detach\n");
                }
                assert(cd->nelem == 1);
                cu = cd->elem[0];
                rtp_header.type = 2;
                rtp_header.seq  = (u_int16)htons(sp->rtp_seq++);
                rtp_header.p    = rtp_header.x = 0;
                rtp_header.ssrc = htonl(rtcp_myssrc(sp));
                rtp_header.pt   = cu->pt;

                time_32 = ts_seq32_out(&tb->up_seq, freq, time_ts);
                rtp_header.ts   = htonl(time_32);
                
                if (time_32 - sp->last_depart_ts != units * tb->unit_dur) {
                        rtp_header.m = 1;
                        debug_msg("new talkspurt\n");
                } else {
                        rtp_header.m = 0;
                }   
                
                ovec[0].iov_base = (caddr_t)&rtp_header;
                ovec[0].iov_len  = 12 + rtp_header.cc*4;
                ovec[1].iov_base = cu->data;
                ovec[1].iov_len  = cu->data_len;

                if (sp->drop == 0.0 || drand48() >= sp->drop) {
                        net_write_iov(sp->rtp_socket, ovec, 2, PACKET_RTP);
                }
                sp->last_depart_ts  = time_32;
                sp->db->pkt_count  += 1;
                sp->db->byte_count += cu->data_len;
                sp->db->sending     = TRUE;
                channel_data_destroy(&cd, sizeof(channel_data));
        }
        pb_iterator_destroy(tb->channel_buffer, &cpos);

        /* Drain tb->audio, remove every older than silence position
         * by two packets worth of audio.  Note tb->media is drained
         * by the channel encoding stage and tb->channel is drained
         * in the act of transmission with pbi_detach_at call.
         */
        u_ts = ts_map32(get_freq(tb->clock), 2 * units * tb->unit_dur);
        n = pb_iterator_audit(tb->transmit, u_ts);
}

void
tx_update_ui(tx_buffer *tb)
{
        static int            active = FALSE;
        session_struct       *sp     = tb->sp;

        if (sp->meter && tb->sending_audio) {
                struct s_pb_iterator *prev;  
                tx_unit              *u;
                u_int32               u_len;
                ts_t                  u_ts;

                /* Silence point should be upto read point here so use last
                 * completely read unit.
                 */

                pb_iterator_dup(&prev, tb->silence);
                pb_iterator_retreat(prev);
                assert(!pb_iterators_equal(tb->silence, prev));
                if (pb_iterator_get_at(prev, (u_char**)&u, &u_len, &u_ts) &&
                    (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->detect_silence == FALSE)) {
                        ui_input_level(sp, lin2vu(u->energy, 100, VU_INPUT));
                } else {
                        if (active == TRUE) ui_input_level(sp, 0);
                }
                pb_iterator_destroy(tb->audio_buffer, &prev);
        }
        if (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->detect_silence == FALSE) {
                if (active == FALSE) {
                        ui_info_activate(sp, sp->db->my_dbe);
                        sp->lecture = FALSE;
                        ui_update_lecture_mode(sp);
                        active = TRUE;
                }
        } else {
                if (active == TRUE) {
                        ui_info_deactivate(sp, sp->db->my_dbe);
                        active = FALSE;
                }
        }

        if (tb->sending_audio == FALSE) {
                ui_info_deactivate(sp, sp->db->my_dbe);
                active = FALSE;
        }
}

void
tx_igain_update(tx_buffer *tb)
{
        sd_reset(tb->sd_info);
        agc_reset(tb->agc);
}

__inline int
tx_is_sending(tx_buffer *tb)
{
        return tb->sending_audio;
}
