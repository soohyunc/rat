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
#include "crypt_random.h"

#ifdef WIN32
#define UI_NAME     "ratui.exe"
#define ENGINE_NAME "ratmedia.exe"
#else
#define UI_NAME     "rat-"##VERSION_NUM##"-ui"
#define ENGINE_NAME "rat-"##VERSION_NUM##"-media"
#endif

pid_t	 pid_ui, pid_engine;
int	 should_exit;

#ifdef FreeBSD
int kill(pid_t pid, int sig);
#endif

#ifdef NEED_SNPRINTF
static int snprintf(char *s, int buf_size, const char *format, ...)
{
	/* Quick hack replacement for snprintf... note that this */
	/* doesn't check for buffer overflows, and so is open to */
	/* many really nasty attacks!                            */
	va_list	ap;
	int	rc;

	UNUSED(buf_size);
	va_start(ap, format);
        rc = sprintf(s, format, ap);
	va_end(ap);
        return rc;
}
#endif

static char *fork_process(struct mbus *m, char *proc_name, char *ctrl_addr, pid_t *pid, char *token)
{
#ifdef WIN32
	char			 args[1024];
	LPSTARTUPINFO		 startup_info;
	LPPROCESS_INFORMATION	 proc_info;

	startup_info = (LPSTARTUPINFO) xmalloc(sizeof(STARTUPINFO));
	startup_info->cb              = sizeof(STARTUPINFO);
	startup_info->lpReserved      = 0;
	startup_info->lpDesktop       = 0;
	startup_info->lpTitle         = 0;
	startup_info->dwX             = 0;
	startup_info->dwY             = 0;
	startup_info->dwXSize         = 0;
	startup_info->dwYSize         = 0;
	startup_info->dwXCountChars   = 0;
	startup_info->dwYCountChars   = 0;
	startup_info->dwFillAttribute = 0;
	startup_info->dwFlags         = 0;
	startup_info->wShowWindow     = 0;
	startup_info->cbReserved2     = 0;
	startup_info->lpReserved2     = 0;
	startup_info->hStdInput       = 0;
	startup_info->hStdOutput      = 0;
	startup_info->hStdError       = 0;

	proc_info = (LPPROCESS_INFORMATION) xmalloc(sizeof(PROCESS_INFORMATION));

	sprintf(args, "%s -ctrl \"%s\" -token %s", proc_name, ctrl_addr, token);

	if (!CreateProcess(NULL, args, NULL, NULL, TRUE, 0, NULL, NULL, startup_info, proc_info)) {
		perror("Couldn't create process");
		abort();
	}
	*pid = (pid_t) proc_info->hProcess;	/* Sigh, hope a HANDLE fits into 32 bits... */
	debug_msg("Forked %s\n", proc_name);
#else
#ifdef DEBUG_FORK
	debug_msg("%s -ctrl '%s' -token %s\n", proc_name, ctrl_addr, token);
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
#endif
	/* This is the parent: we have to wait for the child to say hello before continuing. */
	return mbus_rendezvous_waiting(m, "()", token, m);
}

static void kill_process(pid_t proc)
{
	if (proc == 0) {
		debug_msg("Process %d already marked as dead\n", proc);
		return;
	}
#ifdef WIN32
	/* This doesn't close down DLLs or free resources, so we have to  */
	/* hope it doesn't get called. With any luck everything is closed */
	/* down by sending it an mbus.exit() message, anyway...           */
	TerminateProcess((HANDLE) proc, 0);
#else
	kill(proc, SIGINT);
#endif
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
	int		 ttl = 15;
	char		*addr, *rx_port, *tx_port;
	struct timeval	 timeout;

	/* Parse those command line parameters which are intended for the media engine. */
	for (i = 1; i < argc; i++) {
		debug_msg("argv[%d]=%s\n", i, argv[i]);
                if ((strcmp(argv[i], "-ui") == 0) && (argc > i+1)) {
                } else if  (strcmp(argv[i], "-allowloopback") == 0) {
                } else if ((strcmp(argv[i], "-C") == 0) && (argc > i+1)) {
                } else if ((strcmp(argv[i], "-t") == 0) && (argc > i+1)) {
                        ttl = atoi(argv[i+1]);
                        if (ttl > 255) {
                                fprintf(stderr, "TTL must be in the range 0 to 255.\n");
				ttl = 255;
                        }
                        i++;
                } else if ((strcmp(argv[i], "-p") == 0) && (argc > i+1)) {
                } else if  (strcmp(argv[i], "-seed") == 0) {
                } else if  (strcmp(argv[i], "-codecs") == 0) {
                } else if ((strcmp(argv[i], "-pt") == 0) && (argc > i+1)) {
                } else if ((strcmp(argv[i], "-l") == 0) && (argc > i+1)) { 
		} else if ((strcmp(argv[i], "-crypt") == 0) && (argc > i+1)) {
		} else if  (strcmp(argv[i], "-sync") == 0) {
		} else if ((strcmp(argv[i], "-agc") == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-silence") == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-repair") == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-interleave") == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-redundancy") == 0) && (argc > i+1)) {
		} else if ((strcmp(argv[i], "-f") == 0) && (argc > i+1)) {
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

static void terminate(struct mbus *m, char *addr)
{
	if (mbus_addr_valid(m, addr)) {
		/* This is a valid address, ask that process to quit. */
		mbus_qmsgf(m, addr, TRUE, "mbus.quit", "");
		do {
			struct timeval	 timeout;
			mbus_send(m);
			mbus_heartbeat(m, 1);
			mbus_retransmit(m);
			timeout.tv_sec  = 0;
			timeout.tv_usec = 20000;
			mbus_recv(m, NULL, &timeout);
		} while (!mbus_sent_all(m));
	} else {
		/* That process has already terminated, do nothing. */
	}
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	char		 c_addr[60], token_ui[20], token_engine[20];
	char		*u_addr, *e_addr;
        int		 seed = (gethostid() << 8) | (getpid() & 0xff);
	struct timeval	 timeout;

	srand48(seed);

	m = mbus_init(mbus_control_rx, mbus_err_handler);
	snprintf(c_addr, 60, "(media:audio module:control app:rat instance:%lu)", (u_int32) getpid());
	mbus_addr(m, c_addr);

	sprintf(token_ui,     "rat-token-%08lx", lrand48());
	sprintf(token_engine, "rat-token-%08lx", lrand48());

	u_addr = fork_process(m, UI_NAME,     c_addr, &pid_ui,     token_ui);
	e_addr = fork_process(m, ENGINE_NAME, c_addr, &pid_engine, token_engine);

	inform_addrs(m, e_addr, u_addr);
	parse_options(m, e_addr, u_addr, argc, argv);

	mbus_rendezvous_go(m, token_ui, (void *) m);
	mbus_rendezvous_go(m, token_engine, (void *) m);

	should_exit = FALSE;
	while (!should_exit) {
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	}

	terminate(m, u_addr);
	terminate(m, e_addr);

	kill_process(pid_ui);
	kill_process(pid_engine);
	return 0;
}

