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

#include <pwd.h>
#include "config.h"
#include "util.h"
#include "rtcp.h"
#include "session.h"
#include "interfaces.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "rat_time.h"

void 
service_rtcp(session_struct    *sp,
             session_struct    *sp2,
	     pckt_queue_struct *rtcp_pckt_queue_ptr,
	     u_int32            cur_time)
{
	double				 RTCP_SIZE_GAIN = (1.0/16.0);	/* Copied from RFC1889 [csp] */
	pckt_queue_element_struct 	*pckt;

	while (rtcp_pckt_queue_ptr->queue_empty == FALSE) {
		pckt = get_pckt_off_queue(rtcp_pckt_queue_ptr);
		if (rtcp_check_rtcp_pkt(pckt->pckt_ptr, pckt->len)) {
			rtcp_decode_rtcp_pkt(sp, sp2, pckt->pckt_ptr, pckt->len, pckt->addr, cur_time);
    			sp->db->avg_size += (pckt->len - sp->db->avg_size)*RTCP_SIZE_GAIN;    /* Update the average RTCP packet size... */
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
		free_pckt_queue_element(&pckt);
	}
	rtcp_update(sp, sp->rtcp_fd, sp->net_maddress, sp->rtcp_port);
}

char *get_cname(void)
{
	char           		*uname;
	char	       		*hname;
	char			*cname;
	struct passwd  		*pwent;
	struct hostent 		*hent;
	struct in_addr  	 iaddr;

	cname = (char *) xmalloc(MAXHOSTNAMELEN + 10);
	/* Set the CNAME. This is "user@hostname" or just "hostname" if the username cannot be found. */
	/* First, fill in the username.....                                                           */
#ifdef WIN32
	if ((uname = getenv("USER")) == NULL) {
		uname = "unknown";
	}
#else 		/* ...it's a unix machine... */
	pwent = getpwuid(getuid());
	uname = pwent->pw_name;
#endif
	sprintf(cname, "%s@", uname);

	/* Now the hostname. Must be FQDN or dotted-quad IP address (RFC1889) */
	hname = cname + strlen(cname);
	if (gethostname(hname, MAXHOSTNAMELEN) != 0) {
		perror("Cannot get hostname!");
		return NULL;
	}
	hent = gethostbyname(hname);
	memcpy(&iaddr.s_addr, hent->h_addr, sizeof(iaddr.s_addr));
	strcpy(hname, inet_ntoa(iaddr));
	return cname;
}

u_int32 get_ssrc(void)
{
	return lrand48();
}

