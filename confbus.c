/*
 * FILE:    confbus.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#include "confbus_addr.h"
#include "confbus_cmnd.h"
#include "confbus_misc.h"
#include "confbus_ack.h"
#include "confbus.h"
#include "ui.h"
#include "net.h"
#include "util.h"
#include "transmit.h"
#include "audio.h"
#include "lbl_confbus.h"
#include "codec.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "crypt.h"
#include "session.h"

/* This is used during the parsing of the conference bus messages.  */
/* The routines in this file create an empty struct which is filled */
/* in by the yacc parser in confbus_parser.y, and then used here.   */
cb_mesg		*cb_msg;
/* If an error is detected during parsing, this is set to TRUE...   */
int		 cb_error;

/* This is the buffer where incoming conference bus packets are     */
/* placed. It is a global variable like this, so that the lexer can */
/* access it.                                                       */
char		*cb_buffer;
int		 cb_buffer_len;
int		 cb_buffer_pos;

/* Prototype for the yacc parser function... */
void cbparse(void);

#define CB_ADDR 	0xe0ffdeef	/* 224.255.222.239 */
#define CB_PORT 	47000
#define CB_BUF_SIZE	1024
#define CB_NUM_CMND	19

/*************************************************************************************/
/* Receive functions, one for each message we can receive from the conference bus... */
/*************************************************************************************/

static void cb_recv_toggle_send(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	if (cmnd->num_params != 0) {
		printf("ConfBus: toggle_send does not require parameters\n");
		return;
	}

        if (sp->sending_audio) {
		stop_sending(sp);
	} else {
		start_sending(sp);
	}
	ui_update_input_port(sp);
}

static void cb_recv_toggle_play(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	if (cmnd->num_params != 0) {
		printf("ConfBus: toggle_play does not require parameters\n");
		return;
	}

	if (sp->playing_audio) {
		/* Stop playing */
        	sp->playing_audio          = FALSE;
        	sp->receive_audit_required = TRUE;
	} else {
		/* Start playing */
        	sp->playing_audio          = TRUE;
        	sp->receive_audit_required = TRUE;
	}
	ui_update_output_port(sp);
}

static void cb_recv_get_audio(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	if (cmnd->num_params != 0) {
		printf("ConfBus: get_audio does not require parameters\n");
		return;
	}

	if (sp->have_device) {
		/* We already have the device! */
		return;
	}

	if (audio_device_take(sp) == FALSE) {
		lbl_cb_send_demand(sp);
	}
}

static void cb_recv_toggle_input_port(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	if (cmnd->num_params != 0) {
		printf("ConfBus: toggle_input_port does not require parameters\n");
		return;
	}

	audio_next_iport(sp->audio_fd);
	sp->input_mode = audio_get_iport(sp->audio_fd);
	ui_update_input_port(sp);
}

static void cb_recv_toggle_output_port(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	if (cmnd->num_params != 0) {
		printf("ConfBus: toggle_output_port does not require parameters\n");
		return;
	}

	audio_next_oport(sp->audio_fd);
	sp->output_mode = audio_get_oport(sp->audio_fd);
	ui_update_output_port(sp);
}

static void cb_recv_powermeter(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int i;

	if (cb_cmnd_get_param_int(cmnd, &i)) {
		sp->meter = i;
		ui_input_level(0, sp);
		ui_output_level(0, sp);
	} else {
		printf("ConfBus: usage \"powermeter <boolean>\"\n");
	}
}

static void cb_recv_silence(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int i;

	if (cb_cmnd_get_param_int(cmnd, &i)) {
		sp->detect_silence = i;
	} else {
		printf("ConfBus: usage \"silence <boolean>\"\n");
	}
}

static void cb_recv_lecture(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int i;

	if (cb_cmnd_get_param_int(cmnd, &i)) {
		sp->lecture = i;
	} else {
		printf("ConfBus: usage \"lecture <boolean>\"\n");
	}
}

static void cb_recv_agc(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int i;

	if (cb_cmnd_get_param_int(cmnd, &i)) {
		sp->agc_on = i;
	} else {
		printf("ConfBus: usage \"agc <boolean>\"\n");
	}
}

static void cb_recv_rate(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int	 i;
	codec_t	*cp;

	if (cb_cmnd_get_param_int(cmnd, &i)) {
		cp = get_codec(sp->encodings[0]);
		set_units_per_packet(sp->rb, (i * cp->freq) / (1000 * cp->unit_len));
	} else {
		printf("ConfBus: usage \"rate <integer>\"\n");
	}
}

static void cb_recv_input(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	int i;

	if (cb_cmnd_get_param_sym(cmnd, "gain") && cb_cmnd_get_param_int(cmnd, &i)) {
		sp->input_gain = i;
		audio_set_gain(sp->audio_fd, sp->input_gain);
	} else {
		printf("ConfBus: usage \"input gain <integer>\"\n");
	}
}

static void cb_recv_output(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*s;
	int	 i;

	if (cb_cmnd_get_param_sym(cmnd, "gain") && cb_cmnd_get_param_int(cmnd, &i)) {
		sp->output_gain = i;
		audio_set_volume(sp->audio_fd, sp->output_gain);
	} else if (cb_cmnd_get_param_sym(cmnd, "mode") && cb_cmnd_get_param_str(cmnd, &s)) {
		switch(s[0]) {
		case 'N': sp->voice_switching = NET_MUTES_MIKE;
			  return;
		case 'M': sp->voice_switching = MIKE_MUTES_NET;
			  return;
		case 'F': sp->voice_switching = FULL_DUPLEX;
			  return;
		}
	} else {
		printf("ConfBus: usage \"output gain <integer>\"\n");
		printf("               \"output mode <N|M|F>\"\n");
	}
}

static void cb_recv_repair(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*s;

	if (cb_cmnd_get_param_str(cmnd, &s)) {
		if (strcmp(s,              "None") == 0) sp->repair = REPAIR_NONE;
		if (strcmp(s, "Packet Repetition") == 0) sp->repair = REPAIR_REPEAT;
        	if (strcmp(s,  "Pattern Matching") == 0) sp->repair = REPAIR_PATTERN_MATCH;
	} else {
		printf("ConfBus: usage \"repair None|Repetition\"\n");
	}
}

static void cb_recv_update_key(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*key;

	if (cb_cmnd_get_param_str(cmnd, &key)) {
		Set_Key(key);
	} else {
		printf("ConfBus: usage \"update_key <key>\"\n");
	}
}

static void cb_recv_play(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*file;

	if (cb_cmnd_get_param_sym(cmnd, "stop")) {
		if (sp->in_file != NULL) {
			fclose(sp->in_file);
		}
		sp->in_file = NULL;
	} else if (cb_cmnd_get_param_str(cmnd, &file)) {
		sp->in_file = fopen(file, "r");
	} else {
		printf("ConfBus: usage \"play stop\"\n");
		printf("               \"play <filename>\"\n");
	}
}

static void cb_recv_rec(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*file;

	if (cb_cmnd_get_param_sym(cmnd, "stop")) {
		if (sp->out_file != NULL) {
			fclose(sp->out_file);
		}
		sp->out_file = NULL;
	} else if (cb_cmnd_get_param_str(cmnd, &file)) {
		sp->out_file = fopen(file, "w");
	} else {
		printf("ConfBus: usage \"rec stop\"\n");
		printf("               \"rec <filename>\"\n");
	}
}

static void cb_recv_ssrc(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	u_int32		 ssrc;
	rtcp_dbentry	*e;
	char		*s;

	if (cb_cmnd_get_param_int(cmnd, (int *) &ssrc)) {
		if (sp->db->myssrc == ssrc) {
			if (cb_cmnd_get_param_sym(cmnd,  "name") && cb_cmnd_get_param_str(cmnd, &s)) rtcp_set_attribute(sp, RTCP_SDES_NAME, s);
			if (cb_cmnd_get_param_sym(cmnd, "email") && cb_cmnd_get_param_str(cmnd, &s)) rtcp_set_attribute(sp, RTCP_SDES_EMAIL, s);
			if (cb_cmnd_get_param_sym(cmnd, "phone") && cb_cmnd_get_param_str(cmnd, &s)) rtcp_set_attribute(sp, RTCP_SDES_PHONE, s);
			if (cb_cmnd_get_param_sym(cmnd,   "loc") && cb_cmnd_get_param_str(cmnd, &s)) rtcp_set_attribute(sp, RTCP_SDES_LOC, s);
			if (cb_cmnd_get_param_sym(cmnd,  "tool") && cb_cmnd_get_param_str(cmnd, &s)) rtcp_set_attribute(sp, RTCP_SDES_TOOL, s);
		} else {
			for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
				if (e->ssrc == ssrc) break;
			}
			if (e != NULL) {
				if (cb_cmnd_get_param_sym(cmnd, "mute"))   e->mute = TRUE;
				if (cb_cmnd_get_param_sym(cmnd, "unmute")) e->mute = FALSE;
		 	}
		}
	} else {
		printf("ConfBus: usage \"ssrc <ssrc> <action> [<params>...]\"\n");
	}
}

static void cb_recv_redundancy(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*codec;
	char	 comm[CB_BUF_SIZE];
	codec_t *cp, *pcp;

	if (cb_cmnd_get_param_str(cmnd, &codec)) {
		cp = get_codec_byname(codec, sp);
		if (cp != NULL) {
			pcp = get_codec(sp->encodings[0]);
			if (pcp->value < cp->value) {
				/* Current primary scheme and requested redundancy */
				/* do not match so do not change redundancy scheme */
				cp = get_codec(sp->encodings[1]);
				if (sp->num_encodings > 1) {
					sprintf(comm, "redundancy %s", cb_encode_str(cp->name));
					cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, comm, FALSE);
				} else {
					cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, "redundancy \"NONE\"", FALSE);
				}
			} else {
				sp->encodings[1]  = cp->pt;
				sp->num_encodings = 2;
			}
			sprintf(comm, "redundancy %s", cb_encode_str(cp->name));
			cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
		} else {
			sp->num_encodings = 1;
		}
	} else {
		printf("ConfBus: usage \"redundancy <codec>\"\n");
	}
}

static void cb_recv_primary(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	char	*codec;
	char	 comm[CB_BUF_SIZE];
	codec_t *cp, *scp;

	if (cb_cmnd_get_param_str(cmnd, &codec)) {
		cp = get_codec_byname(codec, sp);
		if (cp != NULL) {
			sp->encodings[0] = cp->pt;
			if (sp->num_encodings > 1) {
				scp = get_codec(sp->encodings[1]);
				if (cp->value < scp->value) {
					/* Current redundancy scheme and requested primary do not match so change redundancy scheme */
					sp->encodings[1] = sp->encodings[0];
					sprintf(comm, "redundancy %s", cb_encode_str(cp->name));
					cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, comm, FALSE);
				}
			}
			sprintf(comm, "primary %s", cb_encode_str(cp->name));
			cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
		} else {
			sp->num_encodings = 1;
		}
	} else {
		printf("ConfBus: usage \"primary <codec>\"\n");
	}
}

/* Jump table for access to the above functions... */

char *cb_recv_cmnd[CB_NUM_CMND] = {
	"toggle_send",
	"toggle_play",
	"get_audio",
	"primary",
	"redundancy",
	"ssrc",
	"input",
	"output",
	"toggle_input_port",
	"toggle_output_port",
	"silence",
	"lecture",
	"agc",
	"repair",
	"powermeter",
	"rate",
	"update_key",
	"play",
	"rec",
};

void (*cb_recv_func[CB_NUM_CMND])(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp) = {
	cb_recv_toggle_send,
	cb_recv_toggle_play,
	cb_recv_get_audio,
	cb_recv_primary,
	cb_recv_redundancy,
	cb_recv_ssrc,
	cb_recv_input,
	cb_recv_output,
	cb_recv_toggle_input_port,
	cb_recv_toggle_output_port,
	cb_recv_silence,
	cb_recv_lecture,
	cb_recv_agc,
	cb_recv_repair,
	cb_recv_powermeter,
	cb_recv_rate,
	cb_recv_update_key,
	cb_recv_play,
	cb_recv_rec,
};

static void cb_recv(struct s_cbaddr *srce, cb_cmnd *cmnd, session_struct *sp)
{
	/* This function is called by cb_poll() when a message is received. */
	/* It should not be called directly.                                */
	int	 i;

	for (i=0; i<CB_NUM_CMND; i++) {
		if (strcmp(cb_recv_cmnd[i], cmnd->cmnd) == 0) {
			cb_recv_func[i](srce, cmnd, sp);
			return;
		}
	}
#ifdef DEBUG_CONFBUS
	printf("Unknown ConfBus command \"%s\"\n", cmnd->cmnd);
#endif
}

/**************************************************************************************************/

void cb_init(session_struct *sp)
{
	char loop = 1;
	char addr = 1;
	
	sp->cb_socket = sock_init(CB_ADDR, CB_PORT, 0);

	setsockopt(sp->cb_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	setsockopt(sp->cb_socket, IPPROTO_IP,      SO_REUSEADDR, &addr, sizeof(addr));
}

int cb_send(session_struct *sp, struct s_cbaddr *srce, struct s_cbaddr *dest, char *mesg, int reliable)
{
	char			*buffer;
	struct sockaddr_in	 saddr;
	u_long			 addr = CB_ADDR;

	assert(dest != NULL);
	assert(mesg != NULL);
	assert(strlen(mesg) != 0);

	sp->cb_seqnum++;

	if (reliable) {
		cb_ack_list_insert(&(sp->cb_ack_list), srce, dest, mesg, sp->cb_seqnum);
	}

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(CB_PORT);
	buffer           = (char *) xmalloc(strlen(cb_addr2str(dest)) + strlen(mesg) + strlen(cb_addr2str(sp->cb_myaddr)) + 80);
	sprintf(buffer, "mbus/1.0 %d %c %s %s\n%s\n", sp->cb_seqnum, reliable?'R':'U', cb_addr2str(srce), cb_addr2str(dest), mesg);
	if ((sendto(sp->cb_socket, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("cb_send: sendto");
	}
	xfree(buffer);
	return sp->cb_seqnum;
}

void cb_send_ack(struct session_tag *sp, struct s_cbaddr *srce, struct s_cbaddr *dest, int seqnum)
{
	char			buffer[80];
	struct sockaddr_in	saddr;
	u_long			addr = CB_ADDR;

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(CB_PORT);
	sprintf(buffer, "mbus/1.0 %d A %s %s\n%d\n", ++(sp->cb_seqnum), cb_addr2str(srce), cb_addr2str(dest), seqnum);
	if ((sendto(sp->cb_socket, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("cb_send: sendto");
	}
}

void cb_poll(session_struct *sp)
{
	/* Check for new messages on the conference bus. If any are received */
	/* it calls cb_recv(...) or ui_recv(...) to process the message.     */
	cb_cmnd		*cmnd, *next;

	cb_buffer = (char *) xmalloc(CB_BUF_SIZE);
	memset(cb_buffer, 0, CB_BUF_SIZE);
	cb_buffer_len = read(sp->cb_socket, cb_buffer, CB_BUF_SIZE);
#ifdef DEBUG_CONFBUS
	printf("%s", cb_buffer);
#endif
	cb_buffer_pos = 0;
	if (cb_buffer_len > 0) {
		cb_msg = (cb_mesg *) xmalloc(sizeof(cb_mesg));
		cb_msg->srce_addr = NULL;
		cb_msg->dest_addr = NULL;
		cb_msg->commands  = NULL;

		/* Call the yacc generated confbus message parser. */
		/* This will fill out the "cb_msg" variable...     */
		cb_error = FALSE;
		cbparse();	
		if (!cb_error) {
			if (cb_msg->msg_type == CB_MSG_ACK) {
				if (cb_addr_match(cb_msg->dest_addr, sp->cb_myaddr)) {
					cb_ack_list_remove(&(sp->cb_ack_list), cb_msg->dest_addr, cb_msg->srce_addr, cb_msg->ack);
					abort();
				}
				if (cb_addr_match(cb_msg->dest_addr, sp->cb_uiaddr)) {
					cb_ack_list_remove(&(sp->cb_ack_list), cb_msg->dest_addr, cb_msg->srce_addr, cb_msg->ack);
				}
			} else {
				/* It's a normal mbus message... */
				if (cb_addr_match(cb_msg->dest_addr, sp->cb_myaddr)) {
					if (cb_msg->msg_type == CB_MSG_RELIABLE) {
						cb_send_ack(sp, cb_msg->dest_addr, cb_msg->srce_addr, cb_msg->seq_num);
					}
					for (cmnd = cb_msg->commands; cmnd != NULL; cmnd=cmnd->next) {
						cb_recv(cb_msg->srce_addr, cmnd, sp);
					}
				}
				if (cb_addr_match(cb_msg->dest_addr, sp->cb_uiaddr)) {
					if (cb_msg->msg_type == CB_MSG_RELIABLE) {
						cb_send_ack(sp, cb_msg->dest_addr, cb_msg->srce_addr, cb_msg->seq_num);
					}
					for (cmnd = cb_msg->commands; cmnd != NULL; cmnd=cmnd->next) {
						ui_recv(cb_msg->srce_addr, cmnd, sp);
					}
				}
			}
		}
		cb_addr_free(cb_msg->srce_addr);
		cb_addr_free(cb_msg->dest_addr);
		cmnd = cb_msg->commands;
		while (cmnd != NULL) {
			next = cmnd->next;
			cb_cmnd_free(cmnd);
			cmnd = next;
		}
		xfree(cb_msg);
	}
	xfree(cb_buffer);
}

