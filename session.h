/*
 * FILE:    session.h
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#ifndef _session_h_
#define _session_h_

#include "net_udp.h"
#include "ts.h"
#include "converter.h"
#include "rtp_queue.h"

/* This will have to be raised in the future */
#define MAX_LAYERS      2

#define MAX_ENCODINGS	7
#define MAX_NATIVE      4

#define MAX_PACKET_SAMPLES	1280
#define PACKET_LENGTH		MAX_PACKET_SAMPLES + 100

#define PORT_UNINIT     0

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

typedef struct s_session {
        short           id;   /* unique session id */
	int		mode; /* audio tool, transcoder */
        char            title[SESSION_TITLE_LEN+1];
	char            asc_address[MAX_LAYERS][MAXHOSTNAMELEN+1];  
	u_short	        rx_rtp_port[MAX_LAYERS];
	u_short	        tx_rtp_port[MAX_LAYERS];
	u_short	        rx_rtcp_port;
	u_short	        tx_rtcp_port;
	int             ttl;
        struct rtp     *rtp_session[MAX_LAYERS];
        int             rtp_session_count;
	u_int8          layers; /* number of layers == rtp_session_count */
        rtp_queue_t    *rtp_pckt_queue;
        int             filter_loopback;
	struct s_fast_time	*clock;
	struct s_time		*device_clock;
        struct s_cushion_struct *cushion;
        struct s_mix_info       *ms; 
        struct s_audio_config   *new_config;
	u_int16		         rtp_seq;
	u_char		         encodings[MAX_ENCODINGS];
	int                      num_encodings; /* no of unique encs in used */
        struct s_channel_state  *channel_coder;
	int                 playing_audio;
	int		    repair;           /* Packet repair */
	int		    lecture;          /* UI lecture mode */
	int		    render_3d;
        int                 echo_suppress;
        int                 echo_was_sending; /* Mute state when suppressing */
	int                 auto_lecture;     /* Used for dummy lecture mode */
 	int                 receive_audit_required;
	int                 detect_silence;
	int                 meter;       /* if powermeters are on */
        u_int32             meter_period; 
	int		    ui_activated;
	int		    sync_on;
	int		    agc_on;
        int                 ui_on;
	char		   *ui_addr;
        converter_id_t      converter;
	struct s_sndfile   *in_file;
	struct s_sndfile   *out_file;
	audio_desc_t	    audio_device;
	struct s_tx_buffer *tb;
        struct s_pdb       *pdb; /* persistent participant */
                                 /* information database.  */
        struct s_source_lis *active_sources;
        ts_sequencer        decode_sequencer;
        int                 limit_playout;
        u_int32             min_playout;
        u_int32             max_playout;
        u_int32             last_depart_ts;
	char		   *mbus_engine_addr;
	struct mbus	   *mbus_engine;
	char		   *mbus_ui_addr;
	struct mbus	   *mbus_ui;
	char		   *mbus_video_addr;
        ts_t                cur_ts; /* current device time as timestamp */
	int		    loopback_gain;
} session_t;

void session_init(session_t *sp);
void session_exit(session_t *sp);
int  session_parse_early_options(int argc, char *argv[], session_t *sp[]);
void session_parse_late_options(int argc, char *argv[], session_t *sp[]);

#endif /* _session_h_ */
