/*
 * FILE:    main.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins 
 * 
 * This is the main program for the RAT controller.  It starts the 
 * media engine and user interface, and controls them via the mbus.
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
#include "version.h"
#include "mbus_control.h"

#define UI_NAME     "rat-"##VERSION_NUM##"-ui"
#define ENGINE_NAME "rat-"##VERSION_NUM##"-media"

pid_t		 pid_ui, pid_engine;

static char *fork_process(struct mbus *m, char *proc_name, char *ctrl_addr, pid_t *pid)
{
	struct timeval	 timeout;
	char		*token = xmalloc(100);
	char		*token_e;
	char		*peer_addr;

	sprintf(token, "rat-controller-waiting-%ld", lrand48());

#ifdef DEBUG_FORK
	debug_msg("%s -ctrl %s -token %s\n", proc_name, ctrl_addr, token);
	UNUSED(pid);
#else
	*pid = fork();
	if (*pid == -1) {
		perror("Cannot fork");
		abort();
	} else if (*pid == 0) {
		execl(proc_name, proc_name, "-ctrl", ctrl_addr, "-token", token, NULL);
		perror("Cannot execute subprocess");
		/* Note: this MUST NOT be exit() or abort(), since they affect the standard */
		/* IO channels in the parent process (fork duplicates file descriptors, but */
		/* they still point to the same underlying file).                           */
		_exit(1);	
	}
#endif

	/* This is the parent: we have to wait for the child to say hello before continuing. */
	debug_msg("Waiting for token \"%s\" from sub-process\n", token);
	token_e = mbus_encode_str(token);
	mbus_control_wait_init(token);
	while ((peer_addr = mbus_control_wait_done()) == NULL) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_heartbeat(m, 1);
		mbus_qmsgf(m, "()", FALSE, "mbus.waiting", "%s", token_e);
		mbus_send(m);
		mbus_recv(m, (void *) m, &timeout);
	}
	debug_msg("forked %s\n", proc_name);
	xfree(token);
	xfree(token_e);
	return peer_addr;
}

static void kill_process(pid_t proc)
{
	kill(proc, SIGINT);
}

static void inform_addrs(struct mbus *m, char *e_addr, char *u_addr)
{
	/* Inform the media engine and user interface of each other's mbus address. */
	char		*tmp;
	struct timeval	 timeout;

	tmp = mbus_encode_str(u_addr);
	mbus_qmsgf(m, e_addr, TRUE, "tool.rat.addr.ui", "%s", tmp);
	xfree(tmp);

	tmp = mbus_encode_str(e_addr);
	mbus_qmsgf(m, u_addr, TRUE, "tool.rat.addr.engine", "%s", tmp);
	xfree(tmp);

	do {
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	} while (!mbus_sent_all(m));
}

static void parse_options(struct mbus *m, char *e_addr, char *u_addr, int argc, char *argv[])
{
	int		 i;
	int		 ttl = 16;
	char		*addr, *rx_port, *tx_port;
	struct timeval	 timeout;

	/* Parse those command line parameters which are intended for the media engine. */
	for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-ui") == 0) && (argc > i+1)) {
                } else if  (strcmp(argv[i], "-allowloopback") == 0) {
                } else if ((strcmp(argv[i], "-C")             == 0) && (argc > i+1)) {
                } else if ((strcmp(argv[i], "-t")             == 0) && (argc > i+1)) {
                } else if ((strcmp(argv[i], "-p")             == 0) && (argc > i+1)) {
                } else if  (strcmp(argv[i], "-seed")          == 0) {
                } else if  (strcmp(argv[i], "-codecs")        == 0) {
                } else if ((strcmp(argv[i], "-pt")            == 0) && (argc > i+1)) {
                } else if ((strcmp(argv[i], "-l")             == 0) && (argc > i+1)) { 
		} else if ((strcmp(argv[i], "-crypt")         == 0) && (argc > i+1)) {
		} else if  (strcmp(argv[i], "-sync")          == 0) {
		} else if ((strcmp(argv[i], "-agc")           == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-silence")       == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-repair")        == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-interleave")    == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-redundancy")    == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-f")             == 0) && (argc > i+1)) {
                }
        }
	/* Parse the list of addresses/ports at the end of the command line... */
	addr    = (char *) strtok(argv[argc-1], "/");
	rx_port = (char *) strtok(NULL, "/");
	tx_port = (char *) strtok(NULL, "/");
	if (tx_port == NULL) {
		tx_port = rx_port;
	}
	addr    = mbus_encode_str(addr);
	mbus_qmsgf(m, e_addr, TRUE, "rtp.addr", "%s %d %d %d", addr, atoi(rx_port), atoi(tx_port), ttl);
	xfree(addr);

	do {
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	} while (!mbus_sent_all(m));

	UNUSED(u_addr);
}

static void mbus_err_handler(int seqnum, int reason)
{
	/* Something has gone wrong with the mbus transmission. At this time */
	/* we don't try to recover, just kill off the media engine and user  */
	/* interface processes and exit.                                     */
	printf("FATAL ERROR: couldn't send mbus message (%d:%d)\n", seqnum, reason);
	kill_process(pid_ui);
	kill_process(pid_engine);
	abort();
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	char		 m_addr[60];
	char		*u_addr, *e_addr;
        int		 seed = (gethostid() << 8) | (getpid() & 0xff);
	struct timeval	 timeout;

	srand48(seed);

	m = mbus_init(mbus_control_rx, mbus_err_handler);
	snprintf(m_addr, 60, "(media:audio module:control app:rat instance:%lu)", (u_int32) getpid());
	mbus_addr(m, m_addr);

	u_addr = fork_process(m, UI_NAME,     m_addr, &pid_ui);
	e_addr = fork_process(m, ENGINE_NAME, m_addr, &pid_engine);

	inform_addrs(m, e_addr, u_addr);
	parse_options(m, e_addr, u_addr, argc, argv);

	while (1) {
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	}

	kill_process(pid_ui);
	kill_process(pid_engine);
	return 0;
}

