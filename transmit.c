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
#include "agc.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "speaker_table.h"
#include "net.h"
#include "rat_time.h"
#include "transmit.h"

typedef struct s_tx_unit {
	struct s_tx_unit *next;
	struct s_tx_unit *prev;
	sample		 *data;			/* pointer to raw data in read_buf */
        u_int32          dur_used;              /* number of time intervals filled */
	int		 energy;
	int		 silence;		/* First pass */
	int		 send;			/* Silence second pass */
	u_int32		 time;			/* timestamp */
} tx_unit;

typedef struct s_read_buffer {
	struct s_sd   *sd_info;
        struct s_vad  *vad;
	struct s_time *clock;
        u_int32        channels;
        u_int32        unit_dur; /* duration in sampling intervals (excludes channels) */
        /* These are pointers into same chain of tx_units */
        tx_unit *head_ptr;
        tx_unit	*tx_ptr;	/* Where transmission is */
	tx_unit	*silence_ptr;	/* Where rules based silence is */
        tx_unit	*last_ptr;	/* Where reading is */
        /* Separate chain for spares */
        tx_unit *spare_ptr;     
        u_int32  spare_cnt;
        u_int32  alloc_cnt;
} read_buffer;

static sample	dummy_buf[DEVICE_REC_BUF];

/* read buffer does own recycling of data so that if we change device 
 * unit size we do not tie up unnecessary memory as would happen with 
 * block_alloc.
 */

static tx_unit *
tx_unit_get(read_buffer *rb)
{
        tx_unit *u;

        if (rb->spare_ptr) {
                u = rb->spare_ptr;
                rb->spare_ptr = rb->spare_ptr->next;
                if (rb->spare_ptr) rb->spare_ptr->prev = NULL;
                assert(u->prev == NULL);
                u->next = NULL;
                rb->spare_cnt--;
        } else {
                u       = (tx_unit*) xmalloc (sizeof(tx_unit));
                u->data = (sample*)  xmalloc (sizeof(sample) * rb->channels *rb->unit_dur);
                rb->alloc_cnt++;
        }

        u->time   = get_time(rb->clock);
        u->next = u->prev = NULL;
        u->dur_used = 0;

        return u;
}

static void
tx_unit_release(read_buffer *rb, tx_unit *u)
{
        assert(u);

        if (u->next) u->next->prev = u->prev;
        if (u->prev) u->prev->next = u->next;

        u->prev = NULL;
        u->next = rb->spare_ptr;

        if (rb->spare_ptr) rb->spare_ptr->prev = u;
        rb->spare_ptr = u;

        rb->spare_cnt++;
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
transmit_audit(read_buffer *rb)
{
        tx_unit *u, *u_next;

        u = rb->head_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_release(rb, u);
                u = u_next;
        }
        rb->head_ptr = rb->tx_ptr = rb->silence_ptr = rb->last_ptr = NULL;
        vad_reset(rb->vad);
}

static void
read_buffer_trim(read_buffer *rb)
{
        tx_unit *u, *end;
        int safety;

        safety = vad_max_could_get(rb->vad);

        end = rb->tx_ptr;
        while(end != NULL && safety != 0) {
                end = end->prev;
                safety --;
        }
        
        if (end) {
                for(u = rb->head_ptr; u != end; u = rb->head_ptr) {
                        rb->head_ptr = u->next;
                        tx_unit_release(rb, u);
                }
        }
}

/* These routines are called when the button on the interface is toggled */
void
start_sending(session_struct *sp)
{
        read_buffer *rb;
	if (sp->sending_audio)
		return;

        rb = sp->rb;

        if (sp->transmit_audit_required == TRUE) {
                transmit_audit(rb);
                sp->transmit_audit_required = FALSE;
        }

        rb->head_ptr = rb->last_ptr = tx_unit_get(rb);

	sp->sending_audio = TRUE;
	sp->auto_lecture = 1;		/* Turn off */
        sd_reset(rb->sd_info);
}

void
stop_sending(session_struct *sp)
{
	struct timeval tv;

	if (sp->sending_audio == FALSE)
		return;
	sp->sending_audio = FALSE;
	gettimeofday(&tv, NULL);
	sp->auto_lecture = tv.tv_sec;
        sp->transmit_audit_required = TRUE;
        reset_channel_encoder(sp,sp->cc_encoding);
	ui_input_level(0);
	ui_info_deactivate(sp->db->my_dbe);
}

read_buffer *
read_device_init(session_struct *sp, u_int16 unit_dur, u_int16 channels)
{
	read_buffer *rb;

	rb = (read_buffer*)xmalloc(sizeof(read_buffer));
        memset(rb, 0, sizeof(read_buffer));

	rb->clock    = sp->device_clock;
	rb->sd_info  = sd_init(unit_dur, get_freq(rb->clock));
        rb->vad      = vad_create(unit_dur, get_freq(rb->clock));
        rb->unit_dur = unit_dur;
        rb->channels = channels;

	if (sp->mode != TRANSCODER) {
		audio_drain(sp->audio_fd);
                audio_read(sp->audio_fd, dummy_buf, DEVICE_REC_BUF);
	}

	return (rb);
}

void
read_device_destroy(session_struct *sp)
{
        read_buffer *rb;
        tx_unit *u, *u_next;

        rb = sp->rb;

        sd_destroy(rb->sd_info);
        vad_destroy(rb->vad);

        u = rb->head_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_destroy(u);
                u = u_next;
        }

        u = rb->spare_ptr;
        while(u) {
                u_next = u->next;
                tx_unit_destroy(u);
                u = u_next;
        }

        xfree(rb);
        sp->rb = NULL;
}

int
read_device(session_struct *sp)
{
        tx_unit *u;
	unsigned int	read_dur;
	read_buffer	*rb;

	rb = sp->rb;
        read_dur = 0;

	if (sp->sending_audio == FALSE) {
		read_dur = audio_device_read(sp, dummy_buf, DEVICE_REC_BUF) / rb->channels;
                time_advance(sp->clock, get_freq(rb->clock), read_dur);
	} else {
                do {
                        u = rb->last_ptr;
                        assert(u);
                        u->dur_used += audio_device_read(sp, 
                                                     u->data + u->dur_used * rb->channels,
                                                     (rb->unit_dur - u->dur_used) * rb->channels) / rb->channels;
                        if (u->dur_used == rb->unit_dur) {
                                read_dur += rb->unit_dur;
                                time_advance(sp->clock, 
                                             get_freq(rb->clock), 
                                             rb->unit_dur);
                                rb->last_ptr = tx_unit_get(rb);
                                u->next = rb->last_ptr;
                                u->next->prev = u;
                        } 
                } while (u->dur_used == rb->unit_dur);
        }

        return (read_dur);
}

int
process_read_audio(session_struct *sp)
{
        tx_unit *u, *u_mark;
        int to_send;

	read_buffer *rb = sp->rb;

        if (rb->silence_ptr == NULL) {
                rb->silence_ptr = rb->head_ptr;
        }

        for(u = rb->silence_ptr; u != rb->last_ptr; u = u->next) {
                /* Audio unbias not modified for stereo yet! */
                audio_unbias(&sp->bc, u->data, u->dur_used * rb->channels);

		u->energy = avg_audio_energy(u->data, u->dur_used, rb->channels);

                u->send   = FALSE;
		if (sp->detect_silence) {
			u->silence = sd(rb->sd_info, u->energy);
                        if (sp->lecture == TRUE) {
                                to_send = vad_to_get(rb->vad, u->silence, VAD_MODE_LECT);           
                        } else {
                                to_send = vad_to_get(rb->vad, u->silence, VAD_MODE_CONF);           
                        }
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

		/* Automatic Gain Control... */
		agc_table_update(sp, u->energy);
        }
        rb->silence_ptr = u;

        if (rb->tx_ptr != NULL) {
                read_buffer_trim(rb);
        }

	return TRUE;
}

static int
new_ts(u_int32 last_time, u_int32 this_time, int encoding, int upp)
{
        codec_t *cp;
        int diff, delta;
        diff = this_time - last_time;
        cp = get_codec(encoding);
        delta = upp*cp->unit_len;
        return (delta != diff);
}  

static void
compress_transmit_audio(session_struct *sp, speaker_table *sa)
{
        int		units, i, n, ready, send;
        tx_unit		*u;
        rtp_hdr_t	rtp_header;
        cc_unit             cu;
        read_buffer	*rb = sp->rb;
        
        if (rb->tx_ptr == NULL) {
                rb->tx_ptr = rb->head_ptr;
        }

        assert(rb->silence_ptr != NULL);
        n = (rb->silence_ptr->time - rb->tx_ptr->time) / rb->unit_dur;

        rtp_header.cc=0;
        if (sp->mode == TRANSCODER) {
                speaker_table	*cs;
                for (cs = sa; cs; cs = cs->next) {
                        /* 2 is a magic number, WHITE in speaker_table.c */
                        if (cs->state == 2)
                                rtp_header.csrc[rtp_header.cc++] = htonl(cs->dbe->ssrc);
                        if (rtp_header.cc == 15)
                                break;
                }
        }

        sp->last_tx_service_productive = 0;    
        units = get_units_per_packet(sp);
        
        /* When channel coders have enough data they fill in the iovec cu
         * and return 1.  They are responsible for clearing items pointed to
         * by iovec.  This is necessary because iovec is pointing to coded data and 
         * redundancy (for example) needs earlier info and so we cannot free it
         * here in case it is still needed by the channel coder.
         */

        memset(&cu,0,sizeof(cc_unit));
        cu.iov[0].iov_base = (caddr_t)&rtp_header;
        cu.iov[0].iov_len  = 12+rtp_header.cc*4;
        cu.iovc            = 1;
        
        while(n > units) {

                send = FALSE;
                for (i = 0, u = rb->tx_ptr; i < units; i++, u = u->next) {
                        if (u->send) {
                                send = TRUE;
                                break;
                        }
                }

                for (i = 0, u = rb->tx_ptr; i < units; i++, u=u->next) {
                        assert(u != rb->silence_ptr);
                        if (send == FALSE) 
                                reset_encoder(sp, sp->encodings[0]);
                        
                        /* if silence we pass dummy to encoder.  If the encoder 
                         * has latency, like an interleaver, it may need a pulse 
                         * in order to sync behaviour.
                         */
                        if (send) {          
                                ready = channel_code(sp, &cu, sp->cc_encoding, u->data);
                        } else {
                                ready = channel_code(sp, &cu, sp->cc_encoding, NULL);
                        }
                        
                        if (ready && cu.iovc>1) {
                                /* rtp hdr fill in.
                                 * nb talkspurt as seen by classifer maybe behind that seen by 
                                 * channel coder so it is set later. 
                                 */
                                rtp_header.type = 2;
                                rtp_header.seq  = htons(sp->rtp_seq++);
                                rtp_header.ts   = htonl(u->time);
                                rtp_header.p = rtp_header.x = 0;
                                rtp_header.ssrc = htonl(rtcp_myssrc(sp));
                                
                                if (sp->cc_encoding == PT_VANILLA) 
                                        rtp_header.pt = sp->encodings[0];
                                else
                                        rtp_header.pt = sp->cc_encoding;
                                
                                rtp_header.m = new_ts(sp->last_depart_ts, u->time, sp->encodings[0], sp->units_per_pckt);
                                
                                sp->last_depart_ts = u->time;
                                sp->db->pkt_count  += 1;
                                sp->db->byte_count += get_bytes(&cu);
                                sp->db->sending     = TRUE;
                                sp->last_tx_service_productive = 1;
                                
                                if (sp->drop == 0.0 || drand48() >= sp->drop) {
                                        net_write_iov(sp->rtp_fd, sp->net_maddress, sp->rtp_port, cu.iov, cu.iovc, PACKET_RTP);
                                }
                        }
                        
                        /* hook goes here to check for asynchonous channel coder data */
                        
                }
                n -= units;
                rb->tx_ptr = u;
        }

}
 
void
service_transmitter(session_struct *sp, speaker_table *sa)
{
	compress_transmit_audio(sp, sa);
}

void
transmitter_update_ui(session_struct *sp)
{
	if (sp->meter && sp->rb->silence_ptr && sp->rb->silence_ptr->prev) {
                if (vad_talkspurt(sp->rb->vad) == TRUE || sp->detect_silence == FALSE) {
                        ui_input_level(lin2db(sp->rb->silence_ptr->prev->energy, 100.0));
                } else {
                        ui_input_level(0);
                }
        }

	if (vad_talkspurt(sp->rb->vad) == TRUE || sp->detect_silence == FALSE) {
		ui_info_activate(sp->db->my_dbe);
		sp->lecture = FALSE;
		update_lecture_mode(sp);
	} else {
		ui_info_deactivate(sp->db->my_dbe);
        }
}

void
read_device_igain_update(session_struct *sp)
{
        sd_reset(sp->rb->sd_info);
}


