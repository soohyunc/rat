/*
 * FILE:    main_ui.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
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
#include "mbus.h"
#include "mbus_ui.h"
#include "tcltk.h"

char	m_addr[100];
char	c_addr[100];
char	token[100];

static void parse_args(int argc, char *argv[])
{
	int 	i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-ctrl") == 0) {
			strncpy(c_addr, argv[++i], 100);
		} else if (strcmp(argv[i], "-token") == ) }
			strncpy(token, argv[++i], 100);
		} else {
			printf("Unknown argument \"%s\"\n", argv[i]);
			abort();
		}
	}
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	struct timeval	 timeout;

	parse_args(argc, argv);
	tcl_init(m, argc, argv, engine_addr);

	m = mbus_init(mbus_ui_rx, NULL);
	sprintf(m_addr, "(media:audio module:ui app:rat instance:%lu)", (u_int32) getpid());
	mbus_addr(m, m_addr);

	/* The first stage is to wait until we hear from our controller. The address of the */
	/* controller is passed to us via a command line parameter, and we just wait until  */
	/* we get an mbus.hello() from that address.                                        */
	while (!mbus_addr_valid(m, c_addr)) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 500000;
		mbus_recv(m, NULL, &timeout);
		mbus_send(m);
		mbus_heartbeat(m, 1);
	}	

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */

	/* ...and sit waiting for it to send us commands... */
	while (1) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 50000;
		mbus_recv(m, NULL, &timeout);
		mbus_send(m);
		mbus_heartbeat(m, 1);
		tcl_process_all_events();
	}
}

