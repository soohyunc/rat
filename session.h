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

#include "net_udp.h"
#include "ts.h"
#include "convert.h"

#define MAX_ENCODINGS	7
#define MAX_NATIVE      4

#define MAX_PACKET_SAMPLES	1280
#define PACKET_LENGTH		MAX_PACKET_SAMPLES + 100

/* Rat mode def's */
#define AUDIO_TOOL              1
#define TRANSCODER              2
#define FLAKEAWAY               4

/*- global clock frequency -*/
#define GLOBAL_CLOCK_FREQ 96000

#define PT_VANILLA         -1
#define PT_INTERLEAVED    108
#define PT_REDUNDANCY     121		/* This has to be 121 for compatibility with RAT-3.0 */

#define SESSION_TITLE_LEN 40

extern int thread_pri;

typedef struct session_tag {
        short           id;                             /* idx of this session_tag - nasty hack - we know session_structs allocated as an array of 2 */
	int		mode;                           /* audio tool, transcoder */
        char            title[SESSION_TITLE_LEN+1];
	char            asc_address[MAXHOSTNAMELEN+1];  /* their ascii name if unicast */
	u_short		         rtp_port;
	u_short		         rtcp_port;
	socket_udp              *rtp_socket;
	socket_udp              *rtcp_socket;
        struct s_pckt_queue     *rtp_pckt_queue;
        struct s_pckt_queue     *rtcp_pckt_queue;
	int    ttl;
        int    filter_loopback;
        u_long ipaddr;
	struct s_fast_time	*clock;
	struct s_time		*device_clock;
        struct s_cushion_struct *cushion;
        struct s_mix_info       *ms; 
	u_int16		rtp_seq;
	u_char		encodings[MAX_ENCODINGS];
        struct s_channel_state  *channel_coder;
	int		num_encodings;	/* number of unique encodings being used */
        int             next_encoding;  /* used for changing device format */
	int             sending_audio;
	int             playing_audio;
        int             last_tx_service_productive;     /* channel coder can output after talksprt ends */
	int		repair;				/* Packet repair */
	int		lecture;			/* UI lecture mode */
	int		render_3d;
        int             echo_suppress;
        int             echo_was_sending;               /* Used to store mute state when suppressing */
	int		auto_lecture;			/* Used for dummy lecture mode */
	int             transmit_audit_required;
	int             receive_audit_required;
	int		detect_silence;
	int             meter;                      /* if powermeters are on */
        u_int32         meter_period; 
	struct s_bias_ctl *bc;
	int		sync_on;
	int		agc_on;
        int             ui_on;
	char		*ui_addr;
        converter_id_t  converter;
        float           drop;                       /* Flakeaway drop percentage [0,1] */
	struct s_snd_file *in_file;
	struct s_snd_file *out_file;
        int             input_gain;                 /* mike gain */
        int             output_gain;                /* speaker volume */
        int             input_mode;                 /* mike/line input */
        int             output_mode;                /* speaker/line/head out */
	struct timeval	device_time;
	audio_desc_t	audio_device;
        int             next_selected_device;       /* No to set selected device to when we want to change */
	struct s_tx_buffer	            *tb;
	struct rtp_db_tag	            *db;
        struct s_source_list                *active_sources;
        ts_sequencer                         decode_sequencer;
        int                                  limit_playout;
        u_int32                              min_playout;
        u_int32                              max_playout;
        struct s_codec_state_store *state_store;
        struct s_cc_state          *cc_state_list;  /* Channel coding states */
        int              cc_encoding;
        int              last_depart_ts;
	struct s_speaker_table	*speakers_active;
	struct mbus	*mbus_engine_base;
	struct mbus	*mbus_engine_conf;
	struct mbus	*mbus_ui_base;
	struct mbus	*mbus_ui_conf;
	int		 mbus_channel;
	int		 wait_on_startup;
} session_struct;

void init_session(session_struct *sp);
void end_session(session_struct *sp);
int  parse_early_options(int argc, char *argv[], session_struct *sp[]);
void parse_late_options(int argc, char *argv[], session_struct *sp[]);

#endif /* _session_h_ */

