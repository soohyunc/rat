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

/* All this code can be greatly simplified and reduced by making
 * better use of the playout buffer structure in playout.h.
 */

typedef struct s_tx_unit {
        struct s_tx_unit *next;
        struct s_tx_unit *prev;
        sample  *data;             /* pointer to raw data in read_buf */
        u_int32  dur_used;         /* number of time intervals filled */
        u_int16  energy;
        u_char   silence;          /* First pass */
        u_char   send;             /* Silence second pass */
        u_int32  time;             /* timestamp */
} tx_unit;

typedef struct s_tx_buffer {
        struct s_sd   *sd_info;
        struct s_vad  *vad;
        struct s_agc  *agc;
        struct s_time *clock;
        struct s_playout_buffer *media_buf;    /* A place forcoded audio           */
        struct s_playout_buffer *channel_buf;  /* And a place for the channel data */
        u_int32        channels;
        u_int32        unit_dur; /* duration in sampling intervals 
                                  * (excludes channels) */
        /* These are pointers into same chain of tx_units */
        tx_unit        *head_ptr;
        tx_unit        *tx_ptr;             /* Where transmission is */
        tx_unit        *silence_ptr;        /* Where rules based silence is */
        tx_unit        *last_ptr;           /* Where reading is */
        /* Separate chain for spares */
        tx_unit        *spare_ptr;     
        u_int32         spare_cnt;
        u_int32         alloc_cnt;
        /* Statistics log */
        double          mean_read_dur;
        /* These are a hack because we use playout buffer
         * which expects time units of type ts_t so we need
         * to be able to map to and from 32 bit no for
         * packet timestamp */
        ts_sequencer    down_seq;  /* used for 32 -> ts_t */
        ts_sequencer    up_seq;    /* used for ts_t -> 32 */

} tx_buffer;

static sample dummy_buf[DEVICE_REC_BUF];

/* read buffer does own recycling of data so that if we change device 
 * unit size we do not tie up unnecessary memory as would happen with 
 * block_alloc.
 */

static tx_unit *
tx_unit_get(tx_buffer *tb)
{
        tx_unit *u;

        if (tb->spare_ptr) {
                u = tb->spare_ptr;
                tb->spare_ptr = tb->spare_ptr->next;
                if (tb->spare_ptr) tb->spare_ptr->prev = NULL;
                assert(u->prev == NULL);
                u->next = NULL;
                tb->spare_cnt--;
        } else {
                u       = (tx_unit*) xmalloc (sizeof(tx_unit));
                u->data = (sample*)  xmalloc (sizeof(sample) * tb->channels *tb->unit_dur);
                tb->alloc_cnt++;
        }

        u->time   = get_time(tb->clock);
        u->next = u->prev = NULL;
        u->dur_used = 0;

        return u;
}

static void
tx_unit_release(tx_buffer *tb, tx_unit *u)
{
        assert(u);

        if (u->next) u->next->prev = u->prev;
        if (u->prev) u->prev->next = u->next;

        u->prev = NULL;
        u->next = tb->spare_ptr;

        if (tb->spare_ptr) tb->spare_ptr->prev = u;
        tb->spare_ptr = u;

        tb->spare_cnt++;
}

static void
tx_unit_destroy(tx_unit *u)
{
        assert(u != NULL);
        assert(u != u->next);
        assert(u != u->prev);
        
        if (u->next != NULL) u->next->prev = u->prev;
        if (u->prev != NULL) u->prev->next = u->next;
        xfree(u->data);
        xfree(u);
}

/* Clear and reset buffer to a starting position */
static void
transmit_audit(tx_buffer *tb)
{
        tx_unit *u, *u_next;

        u = tb->head_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_release(tb, u);
                u = u_next;
        }
        tb->head_ptr = tb->tx_ptr = tb->silence_ptr = tb->last_ptr = NULL;
        vad_reset(tb->vad);
}

static void
tx_buffer_trim(tx_buffer *tb)
{
        tx_unit *u, *end;
        int safety;

        safety = vad_max_could_get(tb->vad);

        end = tb->tx_ptr;
        while(end != NULL && safety != 0) {
                end = end->prev;
                safety --;
        }
        
        if (end) {
                for(u = tb->head_ptr; u != end; u = tb->head_ptr) {
                        tb->head_ptr = u->next;
                        tx_unit_release(tb, u);
                }
        }
}

/* These routines are called when the button on the interface is toggled */
void
tx_start(session_struct *sp)
{
        tx_buffer *tb;
        if (sp->sending_audio == TRUE || !sp->audio_device)
                return;

        tb = sp->tb;

        if (sp->transmit_audit_required == TRUE) {
                transmit_audit(tb);
                sp->transmit_audit_required = FALSE;
        }

        tb->head_ptr = tb->last_ptr = tx_unit_get(tb);
        sp->sending_audio = TRUE;

        /* Turn off auto lecture */
        sp->auto_lecture = 1;       
        sd_reset(tb->sd_info);
        agc_reset(tb->agc);
        
        /* Clear any audio that may have accumulated */
        audio_drain(sp->audio_device);  
}

void
tx_stop(session_struct *sp)
{
        struct timeval tv;

        if (sp->sending_audio == FALSE || !sp->audio_device)
                return;
        sp->sending_audio              = FALSE;
        sp->last_tx_service_productive = 0;
        sp->transmit_audit_required    = TRUE;
        gettimeofday(&tv, NULL);
        sp->auto_lecture               = tv.tv_sec;
        channel_encoder_reset(sp->channel_coder);
        ui_input_level(sp, 0);
        tx_update_ui(sp);
}

tx_buffer *
tx_create(session_struct *sp, u_int16 unit_dur, u_int16 channels)
{
        tx_buffer *tb;
        ts_t       no_history;

        tb = (tx_buffer*)xmalloc(sizeof(tx_buffer));
        memset(tb, 0, sizeof(tx_buffer));

        debug_msg("Unit duration %d channels %d\n", unit_dur, channels);
        
        tb->clock    = sp->device_clock;
        tb->sd_info  = sd_init    (unit_dur, (u_int16)get_freq(tb->clock));
        tb->vad      = vad_create (unit_dur, (u_int16)get_freq(tb->clock));
        tb->agc      = agc_create(sp);

        tb->unit_dur = unit_dur;
        tb->channels = channels;
        tb->mean_read_dur = unit_dur;

        if (sp->mode != TRANSCODER) {
                /*       audio_drain(sp->audio_device);
                audio_read(sp->audio_device, dummy_buf, DEVICE_REC_BUF); */
        }

        if (!sp->state_store) {
                codec_state_store_create(&sp->state_store, ENCODER);
        }

        /* We don't want to store any of this data here,
         * use a dummy history length 
         */
        no_history = ts_map32(8000, 0);
        playout_buffer_create(&tb->media_buf,
                              (playoutfreeproc)media_data_destroy,
                              no_history);
        playout_buffer_create(&tb->channel_buf,
                              (playoutfreeproc)channel_data_destroy,
                              no_history);

        return (tb);
}

void
tx_destroy(session_struct *sp)
{
        tx_buffer *tb;
        tx_unit *u, *u_next;

        tb = sp->tb;

        sd_destroy(tb->sd_info);
        vad_destroy(tb->vad);
        agc_destroy(tb->agc);

        u = tb->head_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_destroy(u);
                u = u_next;
        }

        u = tb->spare_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_destroy(u);
                u = u_next;
        }

        codec_state_store_destroy(&sp->state_store);
        channel_encoder_reset(sp->channel_coder);
        playout_buffer_destroy(&tb->media_buf);
        playout_buffer_destroy(&tb->channel_buf);

        xfree(tb);
        sp->tb = NULL;
}

int
tx_read_audio(session_struct *sp)
{
        tx_unit 	*u;
        unsigned int	 read_dur = 0;
        unsigned int     this_read;
        if (sp->sending_audio) {
                do {
                        u = sp->tb->last_ptr;
                        assert(u);
                        assert(sp->tb->channels > 0 && sp->tb->channels <= 2);
                        this_read = audio_read(sp->audio_device, 
                                               u->data + u->dur_used * sp->tb->channels,
                                               (sp->tb->unit_dur - u->dur_used) * sp->tb->channels) / sp->tb->channels;
                        if (sp->in_file) {
                                snd_read_audio(&sp->in_file, 
                                                u->data + u->dur_used * sp->tb->channels,
                                                (u_int16)((sp->tb->unit_dur - u->dur_used) * sp->tb->channels));
                        }
                        
                        u->dur_used += this_read;
                        if (u->dur_used == sp->tb->unit_dur) {
                                read_dur += sp->tb->unit_dur;
                                time_advance(sp->clock, get_freq(sp->tb->clock), sp->tb->unit_dur);
                                sp->tb->last_ptr = tx_unit_get(sp->tb);
                                u->next = sp->tb->last_ptr;
                                u->next->prev = u;
                        } 
                } while (u->dur_used == sp->tb->unit_dur);
        } else {
                /* We're not sending, but have access to the audio device. Read the audio anyway. */
                /* to get exact timing values, and then throw the data we've just read away...    */
                read_dur = audio_read(sp->audio_device, dummy_buf, DEVICE_REC_BUF / 4) / sp->tb->channels;
                time_advance(sp->clock, get_freq(sp->tb->clock), read_dur);
        }
        
        if ((double)read_dur > 5.0 * sp->tb->mean_read_dur) {
                debug_msg("Cleared transmit buffer because read_len big (%d cf %0.0f)\n",
                          read_dur,
                          sp->tb->mean_read_dur);
                transmit_audit(sp->tb);
                /* Make tx buffer ready for next read */
                sp->tb->head_ptr = sp->tb->last_ptr = tx_unit_get(sp->tb);
        } 

        if (read_dur) {
                sp->tb->mean_read_dur += ((double)read_dur - sp->tb->mean_read_dur) / 8.0;
        }

        return read_dur;
}

int
tx_process_audio(session_struct *sp)
{
        tx_unit 	*u, *u_mark;
        int 		 to_send;
        tx_buffer 	*tb = sp->tb;

	if (sp->sending_audio) {
		if (tb->silence_ptr == NULL) {
			tb->silence_ptr = tb->head_ptr;
		}

		for(u = tb->silence_ptr; u != tb->last_ptr; u = u->next) {
			audio_unbias(sp->bc, u->data, u->dur_used * tb->channels);
			u->energy = avg_audio_energy(u->data, u->dur_used * tb->channels, tb->channels);
			u->send   = FALSE;
			
			/* we do silence detection and voice activity detection
			 * all the time.  agc depends on them and they are all 
			 * cheap.
			 */
			u->silence = sd(tb->sd_info, (u_int16)u->energy);
			to_send    = vad_to_get(tb->vad, (u_char)u->silence, (u_char)((sp->lecture) ? VAD_MODE_LECT : VAD_MODE_CONF));           
			agc_update(tb->agc, (u_int16)u->energy, vad_talkspurt_no(tb->vad));

			if (sp->detect_silence) {
				u_mark = u;
				while(u_mark != NULL && to_send > 0) {
					u_mark->send = TRUE;
					u_mark = u_mark->prev;
					to_send --;
				}
			} else {
				u->silence = FALSE;
				u->send    = TRUE;
			}
		}
		tb->silence_ptr = u;

		if (sp->agc_on == TRUE && 
		    agc_apply_changes(tb->agc) == TRUE) {
			ui_update_input_gain(sp);
		}

		if (tb->tx_ptr != NULL) {
			tx_buffer_trim(tb);
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
        native.id        = codec_get_native_coding(cf->format.sample_rate, 
                                                   cf->format.channels);
        native.state     = NULL;
        native.state_len = 0;
        native.data      = (u_char*)buf;
        native.data_len  = dur_used * sizeof(sample) * cf->format.channels;

        /* Get codec state from those stored for us */
        cs = codec_state_store_get(css, id);
        return codec_encode(cs, &native, coded);
}

void
tx_send(session_struct *sp)
{
        int             units, i, n, send, encoding;
        tx_unit        *u;
        rtp_hdr_t       rtp_header;
        tx_buffer      *tb = sp->tb;
        channel_data   *cd;
        channel_unit   *cu;
        struct iovec    ovec[2];
        ts_t            time_ts;
        u_int32         time_32, cd_len, freq;

        if (tb->silence_ptr == NULL) {
                /* Don't you just hate fn's that do this! */
                return;
        }

        if (tb->tx_ptr == NULL) {
                tb->tx_ptr = tb->head_ptr;
        }

        /* Silence pointer time should always be ahead of transmitted
         * time since we can't make a decision to send without having
         * done silence determination first.  
         */

        n = (tb->silence_ptr->time - tb->tx_ptr->time) / tb->unit_dur;

        assert((unsigned)n <= tb->alloc_cnt); 

        rtp_header.cc = 0;

        sp->last_tx_service_productive = 0;    
        units = channel_encoder_get_units_per_packet(sp->channel_coder);
        freq  = get_freq(tb->clock);
        
        while(n > units) {
                send = FALSE;
                for (i = 0, u = tb->tx_ptr; i < units; i++, u = u->next) {
                        if (u->send) {
                                send = TRUE;
                                break;
                        }
                }
                
                u = tb->tx_ptr;
                for (i = 0;i < units; i++) {
                        media_data *m;
                        if (send) {
                                media_data_create(&m, sp->num_encodings);
                                for(encoding = 0; encoding < sp->num_encodings; encoding ++) {
                                        tx_encode(sp->state_store, 
                                                  u->data, 
                                                  u->dur_used,
                                                  sp->encodings[encoding], 
                                                  m->rep[encoding]);
                                }
                        } else {
                                media_data_create(&m, 0);
                        }
                        time_ts = ts_seq32_in(&tb->down_seq, freq, u->time);
                        playout_buffer_add(tb->media_buf, 
                                           (u_char*)m, 
                                           sizeof(media_data), 
                                           time_ts);
                        u = u->next;
                }
                n -= units;
                tb->tx_ptr = u;
        }

        channel_encoder_encode(sp->channel_coder, tb->media_buf, tb->channel_buf);

        while(playout_buffer_get(tb->channel_buf, (u_char**)&cd, &cd_len, &time_ts)) {
                playout_buffer_remove(tb->channel_buf, (u_char**)&cd, &cd_len, &time_ts);
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
                sp->last_tx_service_productive = 1;
                channel_data_destroy(&cd, sizeof(channel_data));
        }
}

void
tx_update_ui(session_struct *sp)
{
        static int active = FALSE;

        if (sp->meter && sp->tb->silence_ptr && sp->tb->silence_ptr->prev) {
                if (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->detect_silence == FALSE) {
                        ui_input_level(sp, lin2vu(sp->tb->silence_ptr->prev->energy, 100, VU_INPUT));
                } else {
                        if (active == TRUE) ui_input_level(sp, 0);
                }
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

        if (sp->sending_audio == FALSE) {
                ui_info_deactivate(sp, sp->db->my_dbe);
                active = FALSE;
        }
}

void
tx_igain_update(session_struct *sp)
{
        sd_reset(sp->tb->sd_info);
        agc_reset(sp->tb->agc);
}





