/*
 * FILE:    net.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996 University College London
 * All rights reserved.
 *
 */



#ifndef _RAT_NET_H_
#define _RAT_NET_H_

#define PACKET_RTP    1
#define PACKET_RTCP   2

#include "ts.h"

struct session_tag;
struct s_pckt_queue;

void	network_init(struct session_tag *session);
int	net_write(socket_udp *s, unsigned char *msg, int msglen, int type);
int	net_write_iov(socket_udp *s, struct iovec *iov, int iovlen, int type);
void    network_process_mbus(struct session_tag *sp);
void 	read_and_enqueue(socket_udp *s, 
                         ts_t        now_ts,
                         struct s_pckt_queue *queue, 
                         int type);
int     read_and_discard(socket_udp *s);

#endif /* _RAT_NET_H_ */


