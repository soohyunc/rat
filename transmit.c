/*
 * FILE:    transmit.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996,1997 University College London
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
#include "session.h"
#include "audio.h"
#include "parameters.h"
#include "ui.h"
#include "util.h"
#include "agc.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "speaker_table.h"
#include "net.h"
#include "rat_time.h"

#define POST_HANG_LENGTH	8	/* 160ms */
#define FILTER_LENGTH		2
#define MAX_BUF_LEN		(DEVICE_REC_BUF * 3)

typedef struct s_tx_unit {
	struct s_tx_unit *next;
	struct s_tx_unit *prev;
	struct s_minibuf *buf;
	sample		*data;			/* pointer to raw data in read_buf */
	int		energy;
	coded_unit	coded[MAX_ENCODINGS];
	int		silence;		/* First pass */
	int		send;			/* Silence second pass */
	int		talkspurt_start;
	u_int32		time;			/* TX timestamp */
}tx_unit;

/* The point of a minibuf is that the unit size and sampling freq
 * remain the same throughout. */
typedef struct s_minibuf {
	struct s_minibuf *next;
	struct s_minibuf *prev;

	sample	buf[MAX_BUF_LEN];
	int	len;		/* In samples */
	int	head;		/* Where we have read up to */

	int	unit_size;	/* In samples (*= channels) */
	u_int32	start_time;	/* TX timestamp */
} minibuf;

typedef struct s_read_buffer {
	struct s_minibuf *lbuf;	/* Last minibuf in list ALWAYS THERE! */
	sd_t	*sd_info;
	int	talkspurt;	/* Whether we are in a talkspurt */
	int	posthang;	/* Posthang packet counter */
	int	units_per_pckt;
	tx_unit	*last_ptr;	/* Where reading is */
	tx_unit	*silence_ptr;	/* Where rules based silence is */
	tx_unit	*tx_ptr;	/* Where transmission is */
	struct s_time *clock;
} read_buffer;

static void
clear_tx_unit(tx_unit *u)
{
	int l;

	for (l = 0; l < MAX_ENCODINGS; l++) {
		if (u->coded[l].data != NULL)
			clear_coded_unit(u->coded + l);
	}

	u->talkspurt_start = FALSE;
	u->send = FALSE;
	u->data = NULL;			/* Mark as clear */
}

/* Used to clear old minibufs */
static void
remove_minibuf(read_buffer *rb, minibuf *mbp)
{
	tx_unit	*u, *next;

	if (rb->silence_ptr) {
		assert(rb->silence_ptr->buf != mbp);
		assert(rb->tx_ptr->buf != mbp);
	}
	assert(mbp->prev == NULL);
	if (mbp->next)
		mbp->next->prev = NULL;

	for (u = rb->last_ptr; u && u->buf != mbp; u = u->prev)
		;
	if (u->next)
		u->next->prev = NULL;
	for (; u && u->buf == mbp; u = next) {
		next = u->prev;
		clear_tx_unit(u);
		block_free(u, sizeof(tx_unit));
	}
	assert(u == NULL);
	block_free(mbp, sizeof(minibuf));
}

static void
add_new_buf(read_buffer *rb, int unit_size)
{
	minibuf *mbp;
	int	dist, remain;

	mbp = (minibuf *)block_alloc(sizeof(minibuf));
	mbp->len = MAX_BUF_LEN - (MAX_BUF_LEN % unit_size);
	mbp->head = 0;

	if (rb->lbuf) {
		assert(rb->last_ptr->buf == rb->lbuf);
		dist = rb->last_ptr->time - rb->lbuf->start_time + rb->lbuf->unit_size;
		remain = rb->lbuf->head - dist;
		mbp->start_time = rb->lbuf->start_time + dist;

		if (remain > 0 && rb->lbuf->unit_size == unit_size) {
			memcpy(mbp->buf, rb->lbuf->buf + dist, remain);
			mbp->head = remain;
		} else {
			mbp->start_time += remain;
		}
		rb->lbuf->head = dist;
	}

	mbp->unit_size = unit_size;

	if (rb->lbuf)
		rb->lbuf->next = mbp;
	mbp->prev = rb->lbuf;
	mbp->next = NULL;
	rb->lbuf = mbp;
}

static void
init_rb(read_buffer *rb, int unit_size)
{
	add_new_buf(rb, unit_size);
	/* Initialise time */
	rb->lbuf->start_time = get_time(rb->clock);

	/* Create an initial dummy unit to make life easier */
	rb->last_ptr = rb->silence_ptr = rb->tx_ptr = (tx_unit*)block_alloc(sizeof(tx_unit));
	memset(rb->last_ptr, 0, sizeof(tx_unit));
	rb->last_ptr->buf = rb->lbuf;
	rb->last_ptr->time = rb->lbuf->start_time;
	rb->last_ptr->data = rb->lbuf->buf;
	rb->lbuf->head = rb->lbuf->unit_size;
	/* XXX should memset the buffer to silence! */
}

/* Clear and reset buffer to a starting position */
static void
transmit_audit(read_buffer *rb)
{
	minibuf	*mbp, *next;
	int	unit_size;

	unit_size = rb->lbuf->unit_size;
	rb->silence_ptr = rb->tx_ptr = NULL;
	for (mbp = rb->lbuf; mbp->prev; mbp = mbp->prev)
		;
	for (; mbp; mbp = next) {
		next = mbp->next;
		remove_minibuf(rb, mbp);
	}
	rb->lbuf = NULL;
	rb->talkspurt = FALSE;
	rb->posthang = 0;
	init_rb(rb, unit_size);
}

/* These routines are called when the button on the interface is toggled */
void
start_sending(session_struct *sp)
{
	if (sp->sending_audio)
		return;
	if (sp->transmit_audit_required) {
		transmit_audit(sp->rb);
		sp->transmit_audit_required = FALSE;
	}
	sp->sending_audio = TRUE;
	sp->auto_lecture = 1;		/* Turn off */
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
	sp->rb->talkspurt = FALSE;
	ui_input_level(0, sp);
	ui_info_deactivate(sp->db->my_dbe, sp);
}

/*
 * I really did not see the point for the read buffer to understand number
 * of channels and sampling frequencies so instead it takes a unit size that
 * includes everything. The only place where the rest might be useful is codec
 * validation during transitions but who cares... [IK]
 */
read_buffer *
read_device_init(session_struct *sp, int unit_size)
{
	read_buffer *rb;

	rb = (read_buffer*)xmalloc(sizeof(read_buffer));
	memset(rb, 0, sizeof(read_buffer));
	rb->clock = sp->device_clock;
	rb->sd_info = sd_init();
	rb->units_per_pckt = 2;
	init_rb(rb, unit_size);

	if (sp->mode != TRANSCODER) {
		audio_drain(sp->audio_fd);
	}
	return (rb);
}

void
read_change_unit_size(read_buffer *rb, int unit_size)
{
	if (rb->lbuf->unit_size == unit_size)
		return;
	add_new_buf(rb, unit_size);
}

void
set_units_per_packet(read_buffer *rb, int units)
{
	rb->units_per_pckt = units;
}

int
get_units_per_packet(read_buffer *rb)
{
	return (rb->units_per_pckt);
}

int
read_device(session_struct *sp)
{
	int		read_len;
	read_buffer	*rb;
	static sample	dummy_buf[DEVICE_REC_BUF];
	codec_t		*cp;

	rb = sp->rb;
	if (sp->sending_audio == FALSE) {
		read_len = audio_device_read(sp, dummy_buf, DEVICE_REC_BUF);
	} else {
		if (rb->lbuf->len - rb->lbuf->head < DEVICE_REC_BUF)
			add_new_buf(rb, rb->lbuf->unit_size);

		read_len = audio_device_read(sp, rb->lbuf->buf + rb->lbuf->head, DEVICE_REC_BUF);
		if (read_len > 0 && sp->bc)
			audio_unbias(&sp->bc, rb->lbuf->buf + rb->lbuf->head, read_len);

		if ((sp->mode == FLAKEAWAY) && (sp->flake_go > 0)) {
			sp->flake_go -= read_len;
			if (sp->flake_go < 0)
				sp->flake_go = 0;
		}
        
		if ((sp->in_file) && sp->flake_go == 0) {
			if (fread(rb->lbuf->buf + rb->lbuf->head, BYTES_PER_SAMPLE, read_len, sp->in_file) < read_len) {
				fclose(sp->in_file);
				sp->in_file = NULL;
			}
			sp->flake_os += read_len;
		}

		rb->lbuf->head += read_len;
	}

	cp = get_codec(sp->encodings[0]);
	read_len /= cp->channels;
	time_advance(sp->clock, cp->freq, read_len);
	return (read_len);
}

int
process_read_audio(session_struct *sp)
{
	tx_unit		*u;
	int		last_ind, dist;
	read_buffer	*rb = sp->rb;
	minibuf		*mbp;

	if (rb->last_ptr->buf != rb->lbuf)
		last_ind = 0;
	else
		last_ind = rb->last_ptr->time - rb->lbuf->start_time + rb->lbuf->unit_size;

	dist = rb->lbuf->head - last_ind;

	if (dist < rb->lbuf->unit_size)
		return (FALSE);

	/* If there is a complete unit... */
	while (dist >= rb->lbuf->unit_size) {
		u = (tx_unit*)block_alloc(sizeof(tx_unit));
		memset(u, 0, sizeof(tx_unit));
		u->prev = rb->last_ptr;
		rb->last_ptr->next = u;
		u->buf = rb->lbuf;
		u->data = rb->lbuf->buf + last_ind;
		u->time = rb->lbuf->start_time + last_ind;

		/* Do first pass of silence supression */
		u->energy = audio_energy(u->data, rb->lbuf->unit_size);
		if (sp->detect_silence) {
			u->silence = sd(rb->sd_info, u->energy, FALSE);
		} else {
			u->silence = FALSE;
		}
		/* Automatic Gain Control... */
		agc_table_update(sp, u->energy, u->silence);

		rb->last_ptr = u;
		dist -= rb->lbuf->unit_size;
		last_ind += rb->lbuf->unit_size;
	}

	/* Clear old history */
	if (rb->tx_ptr->buf->prev != NULL) {
		for (mbp = rb->tx_ptr->buf->prev; mbp->prev; mbp = mbp->prev)
			;
		do {
			mbp = mbp->next;
			remove_minibuf(rb, mbp->prev);
		} while (mbp != rb->tx_ptr->buf);
	}

	return (TRUE);
}

#define SIGNIFICANT_LECT	3
#define SIGNIFICANT_CONF	1
#define PREHANG_LECT		3
#define PREHANG_CONF		1
#define POSTHANG		8

void
rules_based_silence(session_struct *sp)
{
	tx_unit	*u;
	int	change, i, prehang, significant;
	read_buffer *rb = sp->rb;

	if (sp->lecture == TRUE) {
		prehang = PREHANG_LECT;
		significant = SIGNIFICANT_LECT;
	} else {
		prehang = PREHANG_CONF;
		significant = SIGNIFICANT_CONF;
	}

	while (rb->silence_ptr != rb->last_ptr) {
		if (rb->silence_ptr->silence == rb->talkspurt) {
			if (rb->talkspurt) {
				if (++rb->posthang > POSTHANG) {
					rb->talkspurt = FALSE;
					rb->posthang = 0;
				}
			} else {
				u = rb->silence_ptr;
				for (change = 0; u != rb->last_ptr
				     && u->silence == FALSE
				     && change < significant;) {
					change++;
					u = u->next;
				}
				if (change == significant) {
					rb->talkspurt = TRUE;
					u = rb->silence_ptr;
					for (i = 0; i < prehang && u != rb->tx_ptr; i++) {
						u = u->prev;
						u->send = TRUE;
					}
					u->talkspurt_start = TRUE;
				} else if (u == rb->last_ptr)
					break;
			}
		} else {
			if (rb->posthang > 0)
				rb->posthang--;
		}
		rb->silence_ptr->send = rb->talkspurt;
		rb->silence_ptr = rb->silence_ptr->next;
	}
}

static coded_unit *
get_coded_unit(session_struct *sp, tx_unit *u, int coding)
{
	codec_t		*cp;
	coded_unit	*c;
	int		l;

	cp = get_codec(coding);

	if (cp->unit_len * cp->channels != u->buf->unit_size) {
#ifdef DEBUG
		printf("%d != %d\n",cp->unit_len * cp->channels, u->buf->unit_size);
#endif
		return (NULL);
	}

	/* Find the first free coded_unit, and code into it! */
	for (l = 0, c = u->coded; l < MAX_ENCODINGS && c->data != NULL; l++, c++) {
		if (c->cp == cp) {
			break;
		}
	}
	assert(l != MAX_ENCODINGS);	/* ...else we've run out of space to add units into! */
	if (c->data == NULL)
		encoder(sp, u->data, coding, c);
	return (c);
}

static int
fill_one_level(session_struct *sp, tx_unit *u, int coding, int max_units, struct iovec *iovp, int *len)
{
	int i, iovc = 0;
	coded_unit *c;

	*len = 0;
	for (i = 0; i < max_units; i++, u = u->next) {
		if (u->send == FALSE)
			break;
		if ((c = get_coded_unit(sp, u, coding)) == NULL)
			break;
		if (i == 0 && c->state != NULL) {
			iovp[iovc].iov_base = (caddr_t)c->state;
			*len += iovp[iovc].iov_len = c->state_len;
			iovc++;
		}
		iovp[iovc].iov_base = (caddr_t)c->data;
		*len += iovp[iovc].iov_len = c->data_len;
		iovc++;
	}

	return (iovc);
}

void
compress_transmit_audio(session_struct *sp, speaker_table *sa)
{
	int		units, red, i, blocks, iovc, *pt, len, l;
	tx_unit		*u;
	rtp_hdr_t	rtp_header;
	u_int32		red_hdr[MAX_ENCODINGS], tmph;
	struct iovec	send_ptrs[MAX_ENCODINGS * 8 + 2];
	read_buffer	*rb = sp->rb;
	codec_t		*cp;

	rtp_header.type = 2;
	rtp_header.x    = 0;
	rtp_header.p    = 0;
	rtp_header.cc   = 0;
	rtp_header.ssrc = htonl(rtcp_myssrc(sp));

	units = 0;
	u = rb->tx_ptr;
	while (u != rb->silence_ptr) {
		units++;
		u = u->next;
	}

	while (units > rb->units_per_pckt) {
		if (rb->tx_ptr->send == FALSE) {
			rb->tx_ptr = rb->tx_ptr->next;
			units--;
			continue;
		}

		u = rb->tx_ptr;
		i = rb->units_per_pckt * (sp->num_encodings - 1);
		red = 0;
		if (i > 0) {
			for (; red < i && u->prev != NULL && u->prev->send == TRUE; red++)
				u = u->prev;
			/* Round to the nearest whole packet */
			i = red % rb->units_per_pckt;
			red -= i;
			for (; i > 0; i--)
				u = u->next;
		}

		send_ptrs->iov_base = (caddr_t)&rtp_header;
		send_ptrs->iov_len = 12;
		iovc = 1;

		pt = sp->encodings + red / rb->units_per_pckt;
		if (red > 0) {
			rtp_header.pt = sp->redundancy_pt;
			send_ptrs[1].iov_base = (caddr_t)red_hdr;
			iovc++;
			blocks = 0;
			for (i = red; i > 0; i -= rb->units_per_pckt) {
				cp = get_codec(*pt);
				iovc += fill_one_level(sp, u, *pt, rb->units_per_pckt, send_ptrs + iovc, &len);
				tmph = len;
				tmph |= (i * cp->unit_len) << 10;
				tmph |= (0x80 | *pt) << 24;
				red_hdr[blocks++] = htonl(tmph);
				pt--;
				for (l = 0; l < rb->units_per_pckt; l++)
					u = u->next;
			}
			(*(char *)(red_hdr + blocks)) = *pt;
			send_ptrs[1].iov_len = blocks * 4 + 1;
		} else {
			rtp_header.pt = *pt;
		}
		rtp_header.m = u->talkspurt_start;
		rtp_header.seq = htons(sp->rtp_seq);
		sp->rtp_seq++;
		rtp_header.ts = htonl(u->time);

		/* Fill in CSRC information... */
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

		send_ptrs->iov_len = 12 + (rtp_header.cc * 4);

		iovc += fill_one_level(sp, u, *pt, rb->units_per_pckt, send_ptrs + iovc, &len);
		if (sp->drop != 0.0) {
			if (drand48() >= sp->drop)
				net_write_iov(sp->rtp_fd, sp->net_maddress, sp->rtp_port, send_ptrs, iovc, PACKET_RTP);
                } else {
			net_write_iov(sp->rtp_fd, sp->net_maddress, sp->rtp_port, send_ptrs, iovc, PACKET_RTP);
		}
		/* Update RTP/RTCP statistics... [csp] */
		sp->db->pkt_count  += 1;
		sp->db->byte_count += len;
		sp->db->sending     = TRUE;

		units -= rb->units_per_pckt;
		for (i = 0; i < rb->units_per_pckt; i++)
			rb->tx_ptr = rb->tx_ptr->next;
	}
}

void
service_transmitter(session_struct *sp, speaker_table *sa)
{
	/*
	 * For load adaption etc we can modify these functions to do only
	 * one unit at a time and look at the rb pointers to figure out if
	 * any need servicing... [ik]
	 */
	rules_based_silence(sp);
	compress_transmit_audio(sp, sa);
}

void
transmitter_update_ui(session_struct *sp)
{
	if (sp->meter && sp->rb->silence_ptr && sp->rb->silence_ptr->prev)
		ui_input_level(log10((double)1.0 + (double)sp->rb->silence_ptr->prev->energy / (double)127) * 67, sp);
	if (sp->rb->talkspurt) {
		ui_info_activate(sp->db->my_dbe, sp);
#ifdef NDEF
		if (sp->mode == AUDIO_TOOL) {
			lbl_cb_send_focus(sp, cname);
		}
#endif
		/* Disable lecture mode... */
		sp->lecture = FALSE;
		update_lecture_mode(sp);
	} else
		ui_info_deactivate(sp->db->my_dbe, sp);
}
