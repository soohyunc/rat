/*
 * FILE:    session.h
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
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

#ifndef _session_h_
#define _session_h_

#include "rat_types.h"

#define MAX_ENCODINGS	7
#define MAX_NATIVE      2

#define FULL_DUPLEX 	0
#define NET_MUTES_MIKE 	1
#define MIKE_MUTES_NET 	2

#define MAX_PACKET_SAMPLES	1280
#define PACKET_LENGTH		MAX_PACKET_SAMPLES + 100

/* Rat mode def's */
#define AUDIO_TOOL              1
#define TRANSCODER              2
#define FLAKEAWAY               4

extern int thread_pri;

typedef struct session_tag {
	int		mode;                           /* audio tool, transcoder, flakeaway */
	char            asc_address[MAXHOSTNAMELEN+1];  /* their ascii name if unicast */
	char            maddress[16];
	u_long          net_maddress;			/* Same as above, can be used in a sendto */
	u_long          our_address;			/* our unicast address */
	int             rtp_port;
	int             rtcp_port;
	int             ttl;
	int             rtp_fd;
	int             rtcp_fd;
        u_long          ipaddr;
	struct s_fast_time	*clock;
	struct s_time		*device_clock;
	int		rtp_seq;
	int		encodings[MAX_ENCODINGS];
	int		num_encodings;			/* 1 - MAX_ENC */
	int             sending_audio;
	int             playing_audio;
	int		repair;				/* Packet repair */
	int		lecture;			/* UI lecture mode */
	int		auto_lecture;			/* Used for dummy lecture mode */
	int             transmit_audit_required;
	int             receive_audit_required;
	int             voice_switching;		/* NETMUTESMIKE etc. */
	int		redundancy_pt;
	int		detect_silence;
	int             meter;                      /* if powermeters are on */
	struct s_bias_ctl *bc;
	int		agc_on;
        int             ui_on;
        int		ui_response;
        float           drop;                       /* Flakeaway drop percentage [0,1] */
	FILE		*in_file;
	FILE		*out_file;
	int		have_device;
	int		keep_device;
        int             input_gain;                 /* mike gain */
        int             output_gain;                /* speaker volume */
        int             input_mode;                 /* mike/line input */
        int             output_mode;                /* speaker/line/head out */
	struct timeval	device_time;
	int		audio_fd;
	struct s_read_buffer	*rb;
	int			mix_count;
	struct rtp_db_tag	*db;
	struct s_participant_playout_buffer	*playout_buf_list;

	struct s_dpt	*dpt_list;		/* Dynamic payload types */
	struct s_codec_state *state_list;

	struct s_speaker_table	*speakers_active;
	int		last_zero;		/* audio.c */
	long		loop_delay;
	long		loop_estimate;
	int 		lbl_cb_base_socket;
	int 		lbl_cb_channel_socket;
	int 		lbl_cb_channel;
	int		lbl_cb_priority;
        int             flake_go;                   /* counter used to avoid dropping at start */
        int             flake_os;                   /* number outstanding */
        char            *ui_script;
        struct s_cbaddr	*cb_myaddr;
        struct s_cbaddr	*cb_uiaddr;
        int		 cb_socket;
        int		 cb_shortcut;
	unsigned int	 cb_seqnum;
	struct s_cb_ack	*cb_ack_list;
} session_struct;

void init_session(session_struct *sp);
int  parse_options(int argc, char *argv[], session_struct *sp[]);

#endif /* _session_h_ */

