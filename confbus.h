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

#ifndef _CONFBUS_H
#define _CONFBUS_H

#define CB_ADDR 	0xe0ffdeef	/* 224.255.222.239 */
#define CB_PORT 	47000
#define CB_BUF_SIZE	1024
#define CB_NUM_CMND	19

struct session_tag;

void cb_init(struct session_tag *sp);
int  cb_send(struct session_tag *sp, char *srce, char *dest, char *mesg, int reliable);
void cb_send_ack(struct session_tag *sp, char *srce, char *dest, int seqnum);
void cb_poll(struct session_tag *sp);
void cb_recv(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_toggle_send(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_toggle_play(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_get_audio(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_primary(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_redundancy(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_ssrc(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_input(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_output(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_toggle_input_port(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_toggle_output_port(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_silence(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_lecture(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_agc(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_repair(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_output(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_powermeter(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_rate(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_update_key(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_play(char *srce, char *mesg, struct session_tag *sp);
void cb_recv_rec(char *srce, char *mesg, struct session_tag *sp);

typedef struct {
	char	*media_type;
	char	*module_type;
	char	*media_handling;
	char	*app_name;
	char	*app_instance;
} mbus_addr;

typedef enum {
	RELIABLE, UNRELIABLE, ACK
} mbus_type;

typedef struct {
	int		 seq_num;
	mbus_type	 msg_type;
	mbus_addr	*srce_addr;
	mbus_addr	*dest_addr;
} mbus;

void cberror(char *s);

#endif
