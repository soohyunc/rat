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

#include "config.h"
#include "codec.h"
#include "channel.h"
#include "session.h"
#include "audio.h"
#include "parameters.h"
#include "ui_control.h"
#include "util.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "net.h"
#include "rat_time.h"
#include "transmit.h"

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
        u_int32        channels;
        u_int32        unit_dur; /* duration in sampling intervals (excludes channels) */
        /* These are pointers into same chain of tx_units */
        tx_unit        *head_ptr;
        tx_unit        *tx_ptr;             /* Where transmission is */
        tx_unit        *silence_ptr;        /* Where rules based silence is */
        tx_unit        *last_ptr;           /* Where reading is */
        /* Separate chain for spares */
        tx_unit        *spare_ptr;     
        u_int32         spare_cnt;
        u_int32         alloc_cnt;
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
        if (sp->sending_audio == TRUE || sp->have_device == FALSE)
                return;

        tb = sp->tb;

        if (sp->transmit_audit_required == TRUE) {
                transmit_audit(tb);
                sp->transmit_audit_required = FALSE;
        }

        tb->head_ptr = tb->last_ptr = tx_unit_get(tb);

        sp->sending_audio = TRUE;
        sp->auto_lecture = 1;                /* Turn off */
        sd_reset(tb->sd_info);
        agc_reset(tb->agc);
}

void
tx_stop(session_struct *sp)
{
        struct timeval tv;

        if (sp->sending_audio == FALSE || sp->have_device == FALSE)
                return;
        sp->sending_audio              = FALSE;
        sp->last_tx_service_productive = 0;
        sp->transmit_audit_required    = TRUE;
        gettimeofday(&tv, NULL);
        sp->auto_lecture               = tv.tv_sec;
        channel_encoder_reset(sp,sp->cc_encoding);
        ui_input_level(0);
        ui_info_deactivate(sp->db->my_dbe);
}

tx_buffer *
tx_create(session_struct *sp, u_int16 unit_dur, u_int16 channels)
{
        tx_buffer *tb;

        tb = (tx_buffer*)xmalloc(sizeof(tx_buffer));
        memset(tb, 0, sizeof(tx_buffer));

        tb->clock    = sp->device_clock;
        tb->sd_info  = sd_init    (unit_dur, (u_int16)get_freq(tb->clock));
        tb->vad      = vad_create (unit_dur, (u_int16)get_freq(tb->clock));
        tb->agc      = agc_create(sp);
        tb->unit_dur = unit_dur;
        tb->channels = channels;

        if (sp->mode != TRANSCODER) {
                audio_drain(sp->audio_fd);
                audio_read(sp->audio_fd, dummy_buf, DEVICE_REC_BUF);
        }

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

        xfree(tb);
        sp->tb = NULL;

        clear_encoder_states    (&sp->state_list);
        clear_cc_encoder_states (&sp->cc_state_list);
}

int
tx_read_audio(session_struct *sp)
{
        tx_unit 	*u;
        unsigned int	 read_dur = 0;

        if (sp->sending_audio) {
                do {
                        u = sp->tb->last_ptr;
                        assert(u);
                        u->dur_used += audio_device_read(sp, u->data + u->dur_used * sp->tb->channels,
                                                         (sp->tb->unit_dur - u->dur_used) * sp->tb->channels) / sp->tb->channels;
                        if (u->dur_used == sp->tb->unit_dur) {
                                read_dur += sp->tb->unit_dur;
                                time_advance(sp->clock, get_freq(sp->tb->clock), sp->tb->unit_dur);
                                sp->tb->last_ptr = tx_unit_get(sp->tb);
                                u->next = sp->tb->last_ptr;
                                u->next->prev = u;
                        } 
                } while (u->dur_used == sp->tb->unit_dur);
        } else {
		if (sp->have_device) {
			/* We're not sending, but have access to the audio device. Read the audio anyway. */
			/* to get exact timing values, and then throw the data we've just read away...    */
			read_dur = audio_device_read(sp, dummy_buf, DEVICE_REC_BUF) / sp->tb->channels;
			time_advance(sp->clock, get_freq(sp->tb->clock), read_dur);
	 	} else {
			/* Fake the timing using gettimeofday... We don't have the audio device, so this */
			/* can't rely on any of the values in sp->tb                                     */
			/* This is hard-coded to 8kHz right now! */
			struct timeval	curr_time;

			gettimeofday(&curr_time, NULL);
			read_dur = ((u_int32)((curr_time.tv_sec - sp->device_time.tv_sec) * 1e6)
				+ (curr_time.tv_usec - sp->device_time.tv_usec)) / 125;
			sp->device_time = curr_time;
			if (read_dur > DEVICE_REC_BUF) {
				read_dur = DEVICE_REC_BUF;
			}
			memset(dummy_buf, 0, DEVICE_REC_BUF * BYTES_PER_SAMPLE);
			time_advance(sp->clock, 8000, read_dur);
		}
        }
        return read_dur;
}

int
tx_process_audio(session_struct *sp)
{
        tx_unit *u, *u_mark;
        int to_send;

        tx_buffer *tb = sp->tb;

        if (tb->silence_ptr == NULL) {
                tb->silence_ptr = tb->head_ptr;
        }

        for(u = tb->silence_ptr; u != tb->last_ptr; u = u->next) {
                /* Audio unbias not modified for stereo yet! */
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

        return TRUE;
}

void
tx_send(session_struct *sp)
{
        int                units, i, n, ready, send, num_encodings;
        tx_unit               *u;
        rtp_hdr_t       rtp_header;
        cc_unit        *out;
        cc_unit        *collated[MAX_ENCODINGS];
        coded_unit      coded[MAX_ENCODINGS+1];
        tx_buffer      *tb = sp->tb;
        struct iovec    ovec[CC_UNITS];
        int             ovec_elem;

        if (tb->silence_ptr == NULL) {
                /* Don't you just hate fn's that do this! */
                return;
        }

        if (tb->tx_ptr == NULL) {
                tb->tx_ptr = tb->head_ptr;
        }

        assert(tb->silence_ptr != NULL);
        n = (tb->silence_ptr->time - tb->tx_ptr->time) / tb->unit_dur;

        rtp_header.cc = 0;

/*
        if (sp->mode == TRANSCODER) {
                speaker_table        *cs;
                for (cs = sa; cs && rtp_header.cc < 16; cs = cs->next) {
                        if (cs->state == 2)
                                rtp_header.csrc[rtp_header.cc++] = htonl(cs->dbe->ssrc);
                }
        }
        */
        sp->last_tx_service_productive = 0;    
        units = collator_get_units(sp->collator);
        
        while(n > units) {
                send = FALSE;
                for (i = 0, u = tb->tx_ptr; i < units; i++, u = u->next) {
                        if (u->send) {
                                send = TRUE;
                                break;
                        }
                }
                
                num_encodings = 0;
                if (send == TRUE) {
                        for (i = 0, u = tb->tx_ptr; i < units; i++, u=u->next) {
                                num_encodings = 0;
                                assert(u != tb->silence_ptr);
                                while(num_encodings != sp->num_encodings) {
                                        encoder(sp, u->data, sp->encodings[num_encodings], &coded[num_encodings]);
                                        collated[num_encodings] = collate_coded_units(sp->collator, &coded[num_encodings], num_encodings);
                                        num_encodings++;
                                }
                        }
                } else {
                        for(i=0, u = tb->tx_ptr; i < units; i++, u = u->next) 
                                assert(u != tb->silence_ptr);
                        /* if silence we pass dummy to encoder.  
                         * If the encoder has latency, like an 
                         * interleaver, it may need a pulse in 
                         * order to sync behaviour.
                         */
                        num_encodings = 0;
                        collated[0]   = NULL;
                }

                for(i = 0; i< num_encodings; i++) assert(collated[i]->pt == sp->encodings[i]);
                ready = channel_encode(sp, sp->cc_encoding, collated, num_encodings, &out);

                if (ready && out) {

                        assert(out->iovc > 0);

                        /* through everything into iovec */
                        ovec[0].iov_base = (caddr_t)&rtp_header;
                        ovec[0].iov_len  = 12 + rtp_header.cc*4;
                        ovec_elem        = 1 + out->iovc;
                        assert(ovec_elem < 20);
                        memcpy(ovec + 1, out->iov, sizeof(struct iovec) * out->iovc);
                        rtp_header.type = 2;
                        rtp_header.seq  = (u_int16)htons(sp->rtp_seq++);
                        rtp_header.ts   = htonl(u->time);
                        rtp_header.p    = rtp_header.x = 0;
                        rtp_header.ssrc = htonl(rtcp_myssrc(sp));
                        rtp_header.pt   = out->pt;
                        if (ready & CC_NEW_TS) {
                                rtp_header.m = 1;
                                debug_msg("new talkspurt\n");
                        } else {
                                rtp_header.m = 0;
                        }   
                        sp->last_depart_ts = u->time;
                        sp->db->pkt_count  += 1;
                        sp->db->byte_count += get_bytes(out);
                        sp->db->sending     = TRUE;
                        sp->last_tx_service_productive = 1;
                                
                        if (sp->drop == 0.0 || drand48() >= sp->drop) {
                                net_write_iov(sp->rtp_fd, sp->net_maddress, sp->rtp_port, ovec, ovec_elem, PACKET_RTP);
                        }
                }
                
                /* hook goes here to check for asynchonous channel coder data */
                n -= units;
                tb->tx_ptr = u;
        }
}

void
tx_update_ui(session_struct *sp)
{
        if (sp->meter && sp->tb->silence_ptr && sp->tb->silence_ptr->prev) {
                if (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->detect_silence == FALSE) {
                        ui_input_level(lin2vu(sp->tb->silence_ptr->prev->energy, 100, VU_INPUT));
                } else {
                        ui_input_level(0);
                }
        }

        if (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->detect_silence == FALSE) {
                sp->lecture = FALSE;
                ui_update_lecture_mode(sp);
                ui_info_activate(sp->db->my_dbe);
        } else {
		ui_info_deactivate(sp->db->my_dbe);
        }
}

void
tx_igain_update(session_struct *sp)
{
        sd_reset(sp->tb->sd_info);
        agc_reset(sp->tb->agc);
}


