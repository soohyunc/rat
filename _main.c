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

int should_exit = FALSE;
int thread_pri  = 2; /* Time Critical */

#define UI_NAME     "rat-"##VERSION_NUM##"-ui"
#define ENGINE_NAME "rat-"##VERSION_NUM##"-media"

static pid_t fork_process(struct mbus *m, char *proc_name, char *ctrl_name)
{
	pid_t		pid;
	struct timeval	timeout;

	pid = fork();
	if (pid == -1) {
		perror("Cannot fork");
		abort();
	} else if (pid == 0) {
		execl(proc_name, proc_name, "-ctrl", ctrl_name, "-token", "foo");
		perror("Cannot execute subprocess");
		exit(1);
	}
	while (1) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 500000;
		mbus_recv(m, NULL, &timeout);
		mbus_send(m);
		mbus_heartbeat(m, 1);
	}	
	return pid;
}

static void kill_process(pid_t proc)
{
	kill(proc, SIGINT);
}

static void mbus_cmd_handler(char *src, char *cmd, char *arg, void *dat)
{
	debug_msg("%s %s %s %p\n", src, cmd, arg, dat);
}

static void mbus_err_handler(int seqnum, int reason)
{
	debug_msg("mbus command %s failed (reason %d)\n", seqnum, reason);
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	char		 m_addr[60];
	pid_t		 pid_ui, pid_engine;

	UNUSED(argc);
	UNUSED(argv);

	m = mbus_init(mbus_cmd_handler, mbus_err_handler);
	snprintf(m_addr, 60, "(media:audio module:control app:rat instance:%lu)", (u_int32) getpid());
	mbus_addr(m, m_addr);

	pid_ui     = fork_process(m, UI_NAME,     m_addr);
	pid_engine = fork_process(m, ENGINE_NAME, m_addr);

	while (!should_exit) {
		sleep(1);
	}

	kill_process(pid_ui);
	kill_process(pid_engine);
	return 0;
}

