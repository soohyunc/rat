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

typedef struct {
	int	 seq;
	char	*rel;
	char	*src;
	char	*dst;
	char	*msg;
} cb_header;

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

void (*cb_recv_func[CB_NUM_CMND])(char *srce, char *mesg, session_struct *sp) = {
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

static int cb_name_match(char *s1, char *s2)
{
	/* s1 and s2 are names of the form foo/bar/ibble. Each "/" separated component of s1 must be a */
	/* component of s2 for the names to match. It is not a requirement that all components of s2   */
	/* matched.                                                                                    */
	char	*n = xstrdup(s1); /* We have to strdup these, since strtok wrecks the originals! */
	char	*c;

	for (c = strtok(n, "/"); c != NULL; c = strtok(NULL, "/")) {
		if (strstr(s2, c) == NULL) {
			xfree(n);
			return FALSE;
		}
	}
	xfree(n);
	return TRUE;
}

static cb_header *cb_parse_header(char *buffer)
{
	cb_header	*hdr;

	/* mbus/1.0 <seqnum> <reliable> <srce> <dest> */
	/* command <param> ...                        */
	/* command <param> ...                        */
	/* ...                                        */
	if (strncmp(buffer, "mbus/1.0", 8) != 0) {
		printf("WARNING: Bogus conference bus message received!\n");
		return NULL;
	}
	hdr = (cb_header *) xmalloc(sizeof(cb_header));
	hdr->seq = atoi(strtok(buffer+8, " "));
	hdr->rel = strtok(NULL, " ");
	hdr->src = strtok(NULL, " ");
	hdr->dst = strtok(NULL, "\n");
	hdr->msg = strtok(NULL, "\n");
	return hdr;
}

void cb_init(session_struct *sp)
{
	char loop = 1;
	char addr = 1;
	
	sp->cb_myaddr = xmalloc(16); sprintf(sp->cb_myaddr, "rat/%d", (int) getpid());
	sp->cb_uiaddr = xmalloc(16); sprintf(sp->cb_uiaddr,  "ui/%d", (int) getpid());

	sp->cb_socket = sock_init(CB_ADDR, CB_PORT, 0);

	setsockopt(sp->cb_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	setsockopt(sp->cb_socket, IPPROTO_IP,      SO_REUSEADDR, &addr, sizeof(addr));
}

int cb_send(session_struct *sp, char *srce, char *dest, char *mesg, int reliable)
{
	char			*buffer;
	struct sockaddr_in	 saddr;
	u_long			 addr = CB_ADDR;

	assert(dest != NULL);
	assert(mesg != NULL);
	assert(strlen(dest) != 0);
	assert(strlen(mesg) != 0);

	sp->cb_seqnum++;

	if (sp->cb_shortcut) {
		if (cb_name_match(dest, sp->cb_myaddr)) {
			cb_recv(srce, mesg, sp);
			if (reliable) cb_send_ack(sp, dest, srce, sp->cb_seqnum);
		}
		if (cb_name_match(dest, sp->cb_uiaddr)) {
			ui_recv(srce, mesg, sp);
			if (reliable) cb_send_ack(sp, dest, srce, sp->cb_seqnum);
		}
	} else {
		memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
		saddr.sin_family = AF_INET;
		saddr.sin_port   = htons(CB_PORT);
		buffer           = (char *) xmalloc(strlen(dest) + strlen(mesg) + strlen(sp->cb_myaddr) + 80);
		sprintf(buffer, "mbus/1.0 %d %c %s %s\n%s\n", sp->cb_seqnum, reliable?'R':'U', srce, dest, mesg);
		if ((sendto(sp->cb_socket, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
			perror("cb_send: sendto");
		}
		xfree(buffer);
	}
	return sp->cb_seqnum;
}

void cb_send_ack(struct session_tag *sp, char *srce, char *dest, int seqnum)
{
	char			buffer[80];
	struct sockaddr_in	saddr;
	u_long			addr = CB_ADDR;

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(CB_PORT);
	sprintf(buffer, "mbus/1.0 %d A %s %s\n%d\n", ++(sp->cb_seqnum), srce, dest, seqnum);
	if ((sendto(sp->cb_socket, buffer, strlen(buffer)+1, 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("cb_send: sendto");
	}
}

void cb_poll(session_struct *sp)
{
	/* Check for new messages on the conference bus. If any are received */
	/* it calls cb_recv(...) or ui_recv(...) to process the message.     */
	fd_set		 fds;
	struct timeval 	 timeout;
	char		*buffer;
	int		 read_len;
	cb_header	*hdr;

	timeout.tv_sec  = 0;
	timeout.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(sp->cb_socket, &fds);

#ifdef HPUX
	if (select(sp->cb_socket+1, (int *) &fds, NULL, NULL, &timeout) > 0) {
#else
	if (select(sp->cb_socket+1, &fds, NULL, NULL, &timeout) > 0) {
#endif
		if (FD_ISSET(sp->cb_socket, &fds)) {
			buffer = (char *) xmalloc(CB_BUF_SIZE);
			read_len = read(sp->cb_socket, buffer, CB_BUF_SIZE);
			if (read_len > 0) {
				hdr = cb_parse_header(buffer);
				if (strcmp(hdr->rel, "A") == 0) {
					/* It's an acknowledgement for a reliable message we sent out... */
					if (cb_name_match(hdr->dst, sp->cb_myaddr)) {
						printf("Got an ACK for the media engine...\n");
					}
					if (cb_name_match(hdr->dst, sp->cb_uiaddr)) {
						ui_recv_ack(hdr->src, atoi(hdr->msg), sp);
					}
				} else {
					/* It's a normal mbus message... */
					if (cb_name_match(hdr->dst, sp->cb_myaddr)) {
						if (strcmp(hdr->rel, "R") == 0) {
							cb_send_ack(sp, hdr->dst, hdr->src, hdr->seq);
						}
						cb_recv(hdr->src, hdr->msg, sp);
					}
					if (cb_name_match(hdr->dst, sp->cb_uiaddr)) {
						if (strcmp(hdr->rel, "R") == 0) {
							cb_send_ack(sp, hdr->dst, hdr->src, hdr->seq);
						}
						ui_recv(hdr->src, hdr->msg, sp);
					}
				}
				xfree(hdr);
			}
			xfree(buffer);
		}
	}
}

void cb_recv(char *srce, char *mesg, session_struct *sp)
{
	/* This function is called by cb_poll() when a message is received. */
	/* It should not be called directly.                                */
	int	 i;
	char	*cmnd, *args;

#ifdef DEBUG_CONFBUS
	printf("ConfBus: %s --> %s : %s\n", srce, sp->cb_myaddr, mesg);
#endif

	cmnd = strtok(mesg, " ");
	args = strtok(NULL, "\0");  
	for (i=0; i<CB_NUM_CMND; i++) {
		if (strcmp(cb_recv_cmnd[i], cmnd) == 0) {
			cb_recv_func[i](srce, args, sp);
			return;
		}
	}
#ifdef DEBUG
	printf("Unknown ConfBus message: src=[%s] msg=[%s]\n", srce, mesg);
#endif
}

void cb_recv_toggle_send(char *srce, char *mesg, session_struct *sp)
{
        if (sp->sending_audio) {
		stop_sending(sp);
	} else {
		start_sending(sp);
	}
	ui_update_input_port(sp);
}

void cb_recv_toggle_play(char *srce, char *mesg, session_struct *sp)
{
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

void cb_recv_get_audio(char *srce, char *mesg, session_struct *sp)
{
	if (sp->have_device) {
		/* We already have the device! */
		return;
	}

	if (audio_device_take(sp) == FALSE) {
		lbl_cb_send_demand(sp);
	}
}

void cb_recv_redundancy(char *srce, char *mesg, session_struct *sp)
{
	char	comm[CB_BUF_SIZE];
	codec_t *cp, *pcp;

	/* XXX what is mesg? Do I need to take only first three chars? */
	cp = get_codec_byname(mesg,sp);
	if (cp == NULL) {
		sp->num_encodings = 1;
		return;
	}

	pcp = get_codec(sp->encodings[0]);

	if (pcp->value < cp->value) {
		/* Current primary scheme and requested redundancy */
		/* do not match so do not change redundancy scheme */
		cp = get_codec(sp->encodings[1]);
		if (sp->num_encodings > 1) {
			sprintf(comm, "redundancy %s", cp->name);
			cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, comm, FALSE);
		} else {
			cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, "redundancy NONE", FALSE);
		}
	} else {
		sp->encodings[1]  = cp->pt;
		sp->num_encodings = 2;
	}
	sprintf(comm, "redundancy %s", cp->name);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	return;
}

void cb_recv_primary(char *srce, char *mesg, session_struct *sp)
{
	char	comm[CB_BUF_SIZE];
	codec_t *cp, *scp;

	cp = get_codec_byname(mesg,sp);
	if (cp == NULL) {
		sp->num_encodings = 1;
		return;
	}

	sp->encodings[0] = cp->pt;

	if (sp->num_encodings > 1) {
		scp = get_codec(sp->encodings[1]);
		if (cp->value < scp->value) {
			/* Current redundancy scheme and requested primary do not match so change redundancy scheme */
			sp->encodings[1] = sp->encodings[0];
			sprintf(comm, "redundancy %s", cp->name);
			cb_send(sp, sp->cb_myaddr, sp->cb_myaddr, comm, FALSE);
		}
	}
	sprintf(comm, "primary %s", cp->name);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	return;
}

void cb_recv_ssrc(char *srce, char *mesg, session_struct *sp)
{
	/* Deal with changes to our SDES fields, etc... */
	char		*ssrc = strtok(mesg, " ");
	char		*cmd  = strtok(NULL, " ");
	char 		*arg  = strtok(NULL, "\0");  
	u_int32	 	 SSRC = strtol(ssrc, NULL, 16);
	int		 index;
	rtcp_dbentry	*e;
	char		 comm[CB_BUF_SIZE];

	if (sp->db->myssrc == SSRC) {
		if (strcmp(cmd,  "name") == 0) rtcp_set_attribute(sp, RTCP_SDES_NAME, arg);
		if (strcmp(cmd, "email") == 0) rtcp_set_attribute(sp, RTCP_SDES_EMAIL, arg);
		if (strcmp(cmd, "phone") == 0) rtcp_set_attribute(sp, RTCP_SDES_PHONE, arg);
		if (strcmp(cmd,   "loc") == 0) rtcp_set_attribute(sp, RTCP_SDES_LOC, arg);
		if (strcmp(cmd,  "tool") == 0) rtcp_set_attribute(sp, RTCP_SDES_TOOL, arg);
	} else {
		if (strcmp(cmd,   "mute") == 0) {
			index = (int) strtol(mesg, NULL, 16);
			for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
				if (e->ssrc == index) {
					e->mute = TRUE;
					sprintf(comm, "ssrc %x mute", index);
					cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
					return;
				}
			}
		}
		if (strcmp(cmd, "unmute") == 0) {
			index = (int) strtol(mesg, NULL, 16);
			for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
				if (e->ssrc == index) {
					e->mute = FALSE;
					sprintf(comm, "ssrc %x unmute", index);
					cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
					return;
				}
			}
		}
	}
}

void cb_recv_output(char *srce, char *mesg, session_struct *sp)
{
	char	*cmd	= strtok(mesg, " ");
	char    *arg    = strtok(NULL, "\0");

	if (strcmp(cmd, "gain") == 0) {
		sp->output_gain=atoi(arg);
		audio_set_volume(sp->audio_fd, sp->output_gain);
	}
	if (strcmp(cmd, "mode") == 0) {
		switch(arg[0]) {
		case 'N':
			sp->voice_switching = NET_MUTES_MIKE;
			return;
		case 'M':
			sp->voice_switching = MIKE_MUTES_NET;
			return;
		case 'F':
			sp->voice_switching = FULL_DUPLEX;
			return;
		default:
			printf("Invalid output mode!\n");
			return;
		}
	}
}

void cb_recv_input(char *srce, char *mesg, session_struct *sp)
{
	char	*cmd	= strtok(mesg, " ");
	char    *arg    = strtok(NULL, "\0");

	if (strcmp(cmd, "gain") == 0) {
		sp->input_gain=atoi(arg);
		audio_set_gain(sp->audio_fd, sp->input_gain);
	}
}

void cb_recv_toggle_input_port(char *srce, char *mesg, session_struct *sp)
{
	audio_next_iport(sp->audio_fd);
	sp->input_mode = audio_get_iport(sp->audio_fd);
	ui_update_input_port(sp);
}

void cb_recv_toggle_output_port(char *srce, char *mesg, session_struct *sp)
{
	audio_next_oport(sp->audio_fd);
	sp->output_mode = audio_get_oport(sp->audio_fd);
	ui_update_output_port(sp);
}

void cb_recv_silence(char *srce, char *mesg, session_struct *sp)
{
	/* silence 0|1 */
	sp->detect_silence = atoi(mesg);
}

void cb_recv_lecture(char *srce, char *mesg, session_struct *sp)
{
	/* lecture 0|1 */
	sp->lecture = atoi(mesg);
}

void cb_recv_agc(char *srce, char *mesg, session_struct *sp)
{
	/* agc 0|1 */
	sp->agc_on = atoi(mesg);
}

void cb_recv_repair(char *srce, char *mesg, session_struct *sp)
{
	char	comm[CB_BUF_SIZE];

        if (!strcmp(mesg, "None")) {
		sp->repair = REPAIR_NONE;
                sprintf(comm, "repair {None}");
        } else if (!strcmp(mesg,"Packet")){
		sp->repair = REPAIR_REPEAT;
                sprintf(comm, "repair {Packet Repetition}");
        } else if (!strcmp(mesg,"Pattern")){
		sp->repair = REPAIR_PATTERN_MATCH;
                sprintf(comm, "repair {Pattern Match}");
	} else {
		fprintf(stderr,"cv_recv_repair: %s unrecognized",mesg);
		return;
	}
	
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void cb_recv_powermeter(char *srce, char *mesg, session_struct *sp)
{
	/* powermeter 0|1 */
	sp->meter = atoi(mesg);
	ui_input_level(0, sp);
	ui_output_level(0, sp);
}

void cb_recv_rate(char *srce, char *mesg, session_struct *sp)
{
	int	d = atoi(strtok(mesg," "));
	codec_t	*cp;

	cp = get_codec(sp->encodings[0]);
	d *= cp->freq;
	d /= 1000;
	d /= cp->unit_len;
#ifdef DEBUG
	printf("cb_recv_rate: setting units per pckt to %d\n", d);
#endif
	set_units_per_packet(sp->rb, d);
}

void cb_recv_update_key(char *srce, char *mesg, session_struct *sp)
{
	assert(mesg != NULL);
	Set_Key(mesg);
}

void cb_recv_play(char *srce, char *mesg, session_struct *sp)
{
	if (sp->in_file != NULL) {
		fclose(sp->in_file);
		sp->in_file = NULL;
	}
	if (strncmp(mesg, "stop", 4)) {
		sp->in_file = fopen(mesg, "r");
		if (sp->in_file == NULL) {
			perror("fopen infile");
		}
	}
}

void cb_recv_rec(char *srce, char *mesg, session_struct *sp)
{
	if (sp->out_file != NULL) {
		fclose(sp->out_file);
		sp->out_file = NULL;
	}
	if (strncmp(mesg, "stop", 4)) {
		sp->out_file = fopen(mesg, "w");
		if (sp->out_file == NULL) {
			perror("fopen outfile");
		}
	}
}

/*****************************************************************************/

void cbparse(void);

mbus *mesg;

void cberror(char *s)
{
	printf("Error \"%s\"\n", s);
}

int cbwrap(void)
{
	return 1;
}

