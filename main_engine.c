/*
 * FILE:    main-engine.c
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "session.h"
#include "audio.h"
#include "auddev.h"
#include "cushion.h"
#include "converter.h"
#include "tcltk.h"
#include "ui.h"
#include "pckt_queue.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "rtcp.h"
#include "net.h"
#include "timers.h"
#include "statistics.h"
#include "parameters.h"
#include "transmit.h"
#include "source.h"
#include "mix.h"
#include "sndfile.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"
#include "crypt_random.h"
#include "net_udp.h"
#include "rtcp.h"
#include "settings.h"

int should_exit = FALSE;
int thread_pri  = 2; /* Time Critical */

#ifndef WIN32
static void
signal_handler(int signal)
{
  debug_msg("Caught signal %d\n", signal);
  should_exit = TRUE;
}
#endif

char	*c_addr, *token, *token_e; 

#define MBUS_ADDR_ENGINE "(media:audio module:engine app:rat instance:%5lu)"

static void parse_args(int argc, char *argv[])
{
	int 	i;

	if (argc != 5) {
		printf("Usage: %s -ctrl <addr> -token <token>\n", argv[0]);
		exit(1);
	}
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-ctrl") == 0) {
			c_addr = xstrdup(argv[++i]);
		} else if (strcmp(argv[i], "-token") == 0) {
			token   = xstrdup(argv[++i]);
			token_e = mbus_encode_str(token);
		} else {
			printf("Unknown argument \"%s\"\n", argv[i]);
			abort();
		}
	}
}

int main(int argc, char *argv[])
{
	int            	 seed;
	char		*cname;
	session_struct 	*sp;
	struct timeval   time;
	struct timeval	 timeout;
	char	 	*mbus_addr_engine;

	gettimeofday(&time, NULL);
        seed = (gethostid() << 8) | (getpid() & 0xff);
	srand48(seed);
	lbl_srandom(seed);

	sp = (session_struct *) xmalloc(sizeof(session_struct));
	session_init(sp);
        converters_init();
        statistics_init();
        audio_init_interfaces();
        audio_device_get_safe_config(&sp->new_config);
        audio_device_reconfigure(sp);
        assert(audio_device_is_open(sp->audio_device));
	settings_load_early(sp);
	parse_args(argc, argv);

	/* Initialise our mbus interface... once this is done we can talk to our controller */
	sp->mbus_engine = mbus_init(mbus_engine_rx, NULL);
	mbus_addr_engine = (char *) xmalloc(strlen(MBUS_ADDR_ENGINE) + 3);
	sprintf(mbus_addr_engine, MBUS_ADDR_ENGINE, (u_int32) getpid());
	mbus_addr(sp->mbus_engine, mbus_addr_engine);

	/* The first stage is to wait until we hear from our controller. The address of the */
	/* controller is passed to us via a command line parameter, and we just wait until  */
	/* we get an mbus.hello() from that address.                                        */
	debug_msg("Waiting to validate address %s\n", c_addr);
	while (!mbus_addr_valid(sp->mbus_engine, c_addr)) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, NULL, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}
	debug_msg("Address %s is valid\n", c_addr);

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */
	mbus_engine_wait_handler_init(token);
	mbus_cmd_handler(sp->mbus_engine, mbus_engine_wait_handler);
	while (!mbus_engine_wait_handler_done()) {
		debug_msg("Waiting for token \"%s\" from controller\n", token);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, sp->mbus_engine, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}
	mbus_cmd_handler(sp->mbus_engine, mbus_engine_rx);
	mbus_qmsgf(sp->mbus_engine, c_addr, TRUE, "mbus.go", "%s", token_e);
	mbus_send(sp->mbus_engine);

#ifndef WIN32
 	signal(SIGINT, signal_handler); 
#endif

	/* At this point we know the mbus address of our controller, and have conducted */
	/* a successful rendezvous with it. It will now send us configuration commands. */
	while (!should_exit) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, NULL, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}

	network_init(sp);
	cname = get_cname(sp->rtp_socket[0]);
	sp->db = rtcp_init(sp->device_clock, cname, 0);
        rtcp_clock_change(sp);

	settings_load_late(sp);

	return 0;
}

