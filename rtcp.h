/*
 * FILE:    rtcp.h 
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995,1996 University College London
 * All rights reserved.
 *
 */

#ifndef _service_rtcp_h_
#define _service_rtcp_h_

struct s_session;
struct s_pckt_queue;

char    *get_cname(socket_udp *s);

void 	 service_rtcp(struct s_session *sp, 
                      struct s_session *sp2, 
                      struct s_pckt_queue *rtcp_pckt_queue, 
                      u_int32 cur_time, u_int32 real_time);

#endif /* _service_rtcp_h_ */
