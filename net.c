/*
 * FILE:    net.c 
 * PROGRAM: rat - Version 3.0
 * 
 * Modified by Isidor Kouvelas from NT source.
 * 
 * $Revision$ $Date$
 * 
 * Modified by V.J.Hardman 2/3/95 All the network specific routines have been
 * moved to this module
 * 
 * Add queues functionality to simplify interface to rest of program
 * 
 * 21/3/95 Isidor fixed the network select to work for Solaris
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

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "net_udp.h"
#include "net.h"
#include "crypt.h"
#include "session.h"
#include "interfaces.h"
#include "audio.h"
#include "util.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"
#include "ui.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

int
net_write(socket_udp *s, unsigned char *msg, int msglen, int type)
{
	unsigned char	*encrypted_msg = NULL;

	assert(type == PACKET_RTP || type == PACKET_RTCP);

	if (Null_Key()) {
		encrypted_msg = msg;
	} else {
		switch (type) {
		case PACKET_RTP  : encrypted_msg=Encrypt(msg, &msglen);
				   break;
		case PACKET_RTCP : encrypted_msg=Encrypt_Ctrl(msg, &msglen);
				   break;
		}
	}
	return udp_send(s, encrypted_msg, msglen);
}

#define MAXPACKETSIZE    (1500-28)
int net_write_iov(socket_udp *s, struct iovec *iov, int len, int type)
{
	unsigned char  wrkbuf[MAXPACKETSIZE];
	unsigned char *cp;
	unsigned char *ep;

	for (cp = wrkbuf, ep = wrkbuf + MAXPACKETSIZE; --len >= 0; ++iov) {
		int plen = iov->iov_len;
		if (cp + plen >= ep) {
			errno = E2BIG;
			return -1;
		}
		memcpy(cp, iov->iov_base, plen);
		cp += plen;
	}
	return net_write(s, wrkbuf, cp - wrkbuf, type);
}

void 
network_init(session_struct *sp)
{
	struct in_addr in;
	struct hostent *h;

	if (inet_addr(sp->asc_address) != INADDR_NONE) {
		strcpy(sp->maddress, sp->asc_address);
	} else if ((h = gethostbyname(sp->asc_address))!=NULL) {
		memcpy(&in.s_addr, *(h->h_addr_list), sizeof(in.s_addr));
		sprintf(sp->maddress, "%s", inet_ntoa(in));
	} else {
		fprintf(stderr, "Could not resolve hostname (h_errno = %d): %s", h_errno, sp->asc_address);
		exit(-1);
	}
	
	sp->rtp_socket  = udp_init(sp->asc_address, sp->rtp_port,  sp->ttl);
	sp->rtcp_socket = udp_init(sp->asc_address, sp->rtcp_port, sp->ttl);
}

static void
read_and_enqueue(socket_udp *s, u_int32 cur_time, pckt_queue_struct *queue, int type)
{
	unsigned char			*data_in, *data_out, *tmp_data;
	int             		 read_len;
	pckt_queue_element_struct 	*pckt;

	assert(type == PACKET_RTP || type == PACKET_RTCP);
	while (1) {
		data_in  = block_alloc(PACKET_LENGTH);
		data_out = block_alloc(PACKET_LENGTH);
		read_len = udp_recv(s, data_in, PACKET_LENGTH);
		if (read_len > 0) {
			if (Null_Key()) {
				tmp_data      = data_out;
				data_out      = data_in;
				data_in       = tmp_data;
			} else {
				switch (type) {
					case PACKET_RTP:  Decrypt(data_in, data_out, &read_len);
							  break;
					case PACKET_RTCP: Decrypt_Ctrl(data_in, data_out, &read_len);
							  break;
				}
			}
			pckt = (pckt_queue_element_struct *) block_alloc(sizeof(pckt_queue_element_struct));
			pckt->len               = read_len;
			pckt->pckt_ptr          = data_out;
			pckt->next_pckt_ptr     = NULL;
			pckt->prev_pckt_ptr     = NULL;
			pckt->arrival_timestamp = cur_time;
			put_on_pckt_queue(pckt, queue);
			block_free(data_in, PACKET_LENGTH);
		} else {
			block_free(data_in, PACKET_LENGTH);
			block_free(data_out, PACKET_LENGTH);
			return;
		}
	}
}

void
network_read(session_struct    *sp,
	     pckt_queue_struct *netrx_pckt_queue_ptr,
	     pckt_queue_struct *rtcp_pckt_queue_ptr,
	     u_int32       cur_time)
{
	read_and_enqueue(sp->rtp_socket, cur_time, netrx_pckt_queue_ptr, PACKET_RTP);
	read_and_enqueue(sp->rtcp_socket, cur_time, rtcp_pckt_queue_ptr, PACKET_RTCP);

	mbus_recv(mbus_engine(FALSE), (void *) sp);
	mbus_recv(mbus_ui(FALSE), (void *) sp);
	if (sp->mbus_channel != 0) {
		mbus_recv(mbus_engine(TRUE), (void *) sp);
		mbus_recv(mbus_ui(TRUE), (void *) sp);
	}
}

void network_process_mbus(session_struct *sp)
{
	/* Process outstanding Mbus messages. */
	int	rc;

	do {
		rc  = mbus_recv(mbus_engine(0), (void *) sp); mbus_send(mbus_engine(0));
		rc |= mbus_recv(    mbus_ui(0), (void *) sp); mbus_send(mbus_ui(0));
		if (sp->mbus_channel != 0) {
			rc |= mbus_recv(mbus_engine(TRUE), (void *) sp); mbus_send(mbus_engine(TRUE));
			rc |= mbus_recv(    mbus_ui(TRUE), (void *) sp); mbus_send(mbus_ui(TRUE));
		}
	} while (rc);
}

