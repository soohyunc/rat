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
 */

#include "config_unix.h"
#include "config_win32.h"
#include "net_udp.h"
#include "net.h"
#include "crypt.h"
#include "session.h"
#include "pckt_queue.h"
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

/* discard buffered packets and return how many were dumped */
int read_and_discard(socket_udp *s)
{
        struct timeval no_time_at_all;
        unsigned char *data;
        int done = 0;

        memset(&no_time_at_all, 0, sizeof(no_time_at_all));

        data = block_alloc(PACKET_LENGTH);
        do {
                udp_fd_zero();
                udp_fd_set(s);
                done++;
        } while (( udp_select(&no_time_at_all) > 0 ) && 
                 ( udp_recv(s, (char *) data, PACKET_LENGTH) > 0));

        block_free(data, PACKET_LENGTH);
        return (done - 1);
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

