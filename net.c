/* FILE:    net.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins / Orion Hodson / Dimitris Miras
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-1999 University College London
 * All rights reserved.
 */

#include "config_unix.h"
#include "config_win32.h"
#include "session.h"
#include "mbus.h"
#include "net.h"

#define SECS_BETWEEN_1900_1970 2208988800u

void network_process_mbus(session_t *sp)
{
	/* Process outstanding Mbus messages. */
	int		rc, c;
	struct timeval	timeout;

	c = 0;
	do {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		rc  = mbus_recv(sp->mbus_engine, (void *) sp, &timeout); 
		mbus_send(sp->mbus_engine); 
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
		if (rc) {
			c = 0;
		} else {
			c++;
		}
	} while (c < 3);
}

u_int32 
ntp_time32(void)
{
        struct timeval  tv;
        u_int32 sec, usec, frac;

        gettimeofday(&tv, 0);
        sec  = tv.tv_sec + SECS_BETWEEN_1900_1970;
        usec = tv.tv_usec;
        frac = (usec << 12) + (usec << 8) - ((usec * 3650) >> 6);
        return (sec & 0xffff) << 16 | frac >> 16;
}
