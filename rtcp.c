/*
 * FILE:    rtcp.c 
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

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "pckt_queue.h"
#include "net_udp.h"
#include "rtcp.h"
#include "session.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "timers.h"

void 
service_rtcp(session_struct      *sp,
             session_struct      *sp2,
	     struct s_pckt_queue *rtcp_pckt_queue,
	     u_int32              cur_time,
	     u_int32		  real_time)
{
	double RTCP_SIZE_GAIN = (1.0/16.0); /* Copied from RFC1889 [csp] */
	pckt_queue_element *pckt;

	while ( (pckt = pckt_dequeue(rtcp_pckt_queue)) != NULL) {
		if (rtcp_check_rtcp_pkt(pckt->pckt_ptr, pckt->len)) {
			rtcp_decode_rtcp_pkt(sp, sp2, pckt->pckt_ptr, pckt->len, cur_time, real_time);
    			sp->db->avg_size += (int)((pckt->len - sp->db->avg_size)*RTCP_SIZE_GAIN);    /* Update the average RTCP packet size... */
			sp->db->report_interval = rtcp_interval(sp->db->members, sp->db->senders, sp->db->rtcp_bw, sp->db->sending, 
						       	0, &(sp->db->avg_size), sp->db->initial_rtcp, get_freq(sp->device_clock));
		} else {
#ifdef DEBUG
			int i;

			printf("RTCP packet failed header validity check!\n");
			for (i=0; i<pckt->len; i++) {
		        	printf("%02x ", (unsigned char) pckt->pckt_ptr[i]);
				if ((i % 16) == 15) {
					printf("\n");
				}
			 }
                         printf("\n");
#endif
		}
		pckt_queue_element_free(&pckt);
	}
	rtcp_update(sp, sp->rtcp_socket);
}

char *get_cname(socket_udp *s)
{
	/* Set the CNAME. This is "user@hostname" or just "hostname" if the username cannot be found. */
	char           		*uname;
	char	       		*hname;
	char			*cname;
#ifndef WIN32
	struct passwd  		*pwent;
#endif

	cname = (char *) xmalloc(MAXHOSTNAMELEN + 10);
	cname[0] = '\0';

	/* First, fill in the username... */
#ifdef WIN32
	uname = getenv("USER");
#else
	pwent = getpwuid(getuid());
	uname = pwent->pw_name;
#endif
	if (uname != NULL) {
		sprintf(cname, "%s@", uname);
	}

	/* Now the hostname. Must be dotted-quad IP address. */
	hname = udp_host_addr(s);
	strcpy(cname + strlen(cname), hname);
	xfree(hname);
	return cname;
}

u_int32 get_ssrc(void)
{
#ifdef WIN32
	srand48((unsigned int)GetTickCount());
        return (lrand48()<<16)|(GetTickCount() & 0xffff);
#else
        return lrand48();
#endif
}

