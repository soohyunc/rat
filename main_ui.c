/*
 * FILE:    main_ui.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "mbus_ui.h"
#include "tcl.h"
#include "tk.h"
#include "tcltk.h"

char	*e_addr = NULL;
char	 m_addr[100];
char	*c_addr, *token, *token_e; 

int	 ui_active   = FALSE;
int	 should_exit = FALSE;

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

#ifdef WIN32
extern HINSTANCE hAppInstance;
extern int       TkWinXInit(HINSTANCE);
extern void      TkWinXCleanup(HINSTANCE);
#endif

static void 
mbus_error_handler(int seqnum, int reason)
{
        debug_msg("mbus message failed (%d:%d)\n", seqnum, reason);
        if (should_exit == FALSE) {
                abort();
        } 
        UNUSED(seqnum);
        UNUSED(reason);
        /* Ignore error we're closing down anyway */
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	struct timeval	 timeout;

#ifdef WIN32
        HANDLE     hWakeUpEvent;
        TkWinXInit(hAppInstance);
        hWakeUpEvent = CreateEvent(NULL, FALSE, FALSE, "Local\\RAT UI WakeUp Event");
#endif

        debug_set_core_dir(argv[0]);

	debug_msg("rat-ui started argc=%d\n", argc);
	parse_args(argc, argv);
	tcl_init1(argc, argv);

	sprintf(m_addr, "(media:audio module:ui app:rat instance:%lu)", (unsigned long) getpid());
	m = mbus_init(mbus_ui_rx, mbus_error_handler, m_addr);

	/* The first stage is to wait until we hear from our controller. The address of the */
	/* controller is passed to us via a command line parameter, and we just wait until  */
	/* we get an mbus.hello() from that address.                                        */
	debug_msg("Waiting to validate address %s\n", c_addr);
	while (!mbus_addr_valid(m, c_addr)) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(m, NULL, &timeout);
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
	}
	debug_msg("Address %s is valid\n", c_addr);

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */
	debug_msg("Waiting for mbus.waiting(%s) from controller...\n", token);
	mbus_rendezvous_go(m, token, (void *) m);
	debug_msg("...got it\n");

	/* At this point we know the mbus address of our controller, and have conducted   */
	/* a successful rendezvous with it. It will now send us configuration commands.   */
	/* We do mbus.waiting(foo) where "foo" is the original token. The controller will */
	/* eventually respond with mbus.go(foo) when it has finished sending us commands. */
	debug_msg("Waiting for mbus.go(%s) from controller...\n", token);
	mbus_rendezvous_waiting(m, c_addr, token, (void *) m);
	debug_msg("...got it\n");

	/* At this point we should know (at least) the address of the media engine. */
	/* We may also have been given other information too...                     */
	assert(e_addr != NULL);

	ui_active = TRUE;
	tcl_init2(m, e_addr);
	while (!should_exit) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, (void *)m, &timeout);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		mbus_send(m);
		while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS)) {
			/* Process Tcl/Tk events... */
		}
		if (Tk_GetNumMainWindows() == 0) {
			should_exit = TRUE;
		}
		/* Throttle CPU usage */
#ifdef WIN32
                /* Just timeout waiting for event that never happens */
                WaitForSingleObject(hWakeUpEvent, 10);
#else
		timeout.tv_sec  = 0;
		timeout.tv_usec = 10000;
                select(0, NULL, NULL, NULL, &timeout);
#endif
	}

	/* Close things down nicely... */
	mbus_qmsgf(m, "()", FALSE, "mbus.bye", "");
	do {
		mbus_send(m);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	} while (!mbus_sent_all(m));
	mbus_exit(m);
        
#ifdef WIN32
        TkWinXCleanup(hAppInstance);
#endif
        return 0;
}

