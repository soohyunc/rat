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
 * Copyright (c) 1995-99 University College London
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
#include "net_udp.h"
#include "net.h"
#include "crypt.h"
#include "session.h"
#include "pckt_queue.h"
#include "audio.h"
#include "util.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"
#include "ts.h"
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
	return udp_send(s, (char *) encrypted_msg, msglen);
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
	sp->rtp_socket  = udp_init(sp->asc_address, sp->rtp_port,  sp->ttl); assert(sp->rtp_socket  != NULL);
	sp->rtcp_socket = udp_init(sp->asc_address, sp->rtcp_port, sp->ttl); assert(sp->rtcp_socket != NULL);
}

void
read_and_enqueue(socket_udp *s, ts_t cur_ts, struct s_pckt_queue *queue, int type)
{
        unsigned char      *data_in, *data_out, *tmp_data;
        int                 read_len;
        pckt_queue_element *pckt;
        
        assert(type == PACKET_RTP || type == PACKET_RTCP);
	data_in  = block_alloc(PACKET_LENGTH);
	read_len = udp_recv(s, (char *) data_in, PACKET_LENGTH);
	if (read_len > 0) {
                data_out = block_alloc(PACKET_LENGTH);
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
                /* We should avoid this memcpy */
		pckt = pckt_queue_element_create();
		pckt->len               = read_len;
		pckt->pckt_ptr          = (u_char*)block_alloc(read_len);
                memcpy(pckt->pckt_ptr, data_out, read_len);
		pckt->arrival = cur_ts;
		pckt_enqueue(queue, pckt);
		block_free(data_out, PACKET_LENGTH);
	} 
        block_free(data_in, PACKET_LENGTH);
        return;
}

void network_process_mbus(session_struct *sp)
{
	/* Process outstanding Mbus messages. */
	int	rc;
	do {
		mbus_send(sp->mbus_ui);
		rc  = mbus_recv(sp->mbus_engine, (void *) sp); 
		mbus_send(sp->mbus_engine);
                rc |= mbus_recv(sp->mbus_ui, (void *) sp); 
	} while (rc);
}

