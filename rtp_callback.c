/*
 * FILE:    rtp_callback.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "rtp.h"
#include "rtp_callback.h"

void rtp_callback(struct rtp *s, rtp_event *e)
{
	assert(s != NULL);
	assert(e != NULL);

	switch (e->type) {
	case RX_RTP:
		break;
	case RX_SR:
		break;
	case RX_RR:
		break;
	case RX_SDES:
		break;
	case RX_BYE:
		break;
	case SOURCE_DELETED:
		break;
	default:
		debug_msg("Unknown RTP event (type=%d)\n", e->type);
		abort();
	}
}

