/*
 * FILE:    main.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins 
 *
 * This is the main program for the RAT controller.  It starts the 
 * media engine and user interface, and controls them via the mbus.
 *
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 */
 
#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "version.h"
#include "mbus_control.h"
#include "crypt_random.h"
#include "codec_compat.h"

#include "mbus.h"
#include "mbus_parser.h"
#include "net_udp.h"

#ifdef WIN32
#define UI_NAME     "ratui.exe"
#define ENGINE_NAME "ratmedia.exe"
#else
#define UI_NAME     "rat-"##VERSION_NUM##"-ui"
#define ENGINE_NAME "rat-"##VERSION_NUM##"-media"
#endif

#define DEFAULT_RTP_PORT 5004

char	*u_addr, *e_addr;
pid_t	 pid_ui, pid_engine;
int	 should_exit;

static void 
usage(char *szOffending)
{
#ifdef WIN32
        char win_usage[] = "\
RAT is a multicast (or unicast) audio tool. It is best to start it\n\
using a multicast directory tool, like sdr or multikit. If desired RAT\n\
can be launched from the command line using:\n\n\
rat <address>/<port>\n\n\
where <address> is machine name, or a multicast IP address, and <port> is\n\
the connection identifier (an even number between 1024-65536).\n\n\
For more details see:\n\n\
http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/FAQ.html\
";
        
        if (szOffending == NULL) {
                szOffending = win_usage;
        }

        MessageBox(NULL, szOffending, "RAT Usage", MB_ICONINFORMATION | MB_OK);
#else
        printf("Usage: rat [options] -t <ttl> <addr>/<port>\n");
        if (szOffending) {
                printf(szOffending);
        }
#endif
        
}

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
		execlp(proc_name, proc_name, "-ctrl", ctrl_addr, "-token", token, NULL);
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
		debug_msg("Process already dead\n", proc);
		return;
	}
	debug_msg("Killing process %d\n", proc);
#ifdef WIN32
	/* This doesn't close down DLLs or free resources, so we have to  */
	/* hope it doesn't get called. With any luck everything is closed */
	/* down by sending it an mbus.exit() message, anyway...           */
	TerminateProcess((HANDLE) proc, 0);
#else
	kill(proc, SIGINT);
#endif
}

static int parse_options_early(int argc, const char **argv) 
{
        int i;
        char *addr;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }

        /* DOS and UNIX style command flags for usage and version */        
        for (i = 1; i < argc; i++) {
                if ((argv[i][0] == '-' || argv[i][0] == '/') &&
                    (argv[i][1] == 'v' || argv[i][1] == 'V')) {
                        printf("%s\n", RAT_VERSION);
                        return FALSE;
                } else if ((argv[i][0] == '-' || argv[i][0] == '/')&&
                           (argv[i][1] == '?' || argv[i][1] == 'h' || argv[i][1] == 'H')) {
                        usage(NULL);
                        return FALSE;
                }
        }

        /* 
         * Validate destination address.  Do it here before launching 
         * sub-processes and mbus.
         */

        addr = xstrdup(argv[argc-1]);
        addr = strtok(addr, "/");
        if (udp_addr_valid(addr) == FALSE) {
                usage("Invalid address\n");
                return FALSE;
        }
        xfree(addr);

        return TRUE;
}

static int parse_options(struct mbus *m, char *e_addr, char *u_addr, int argc, char *argv[])
{
	int		 i;
	int		 ttl = 15;
	char		*addr, *port, *tmp;
        int              tx_port, rx_port;
	struct timeval	 timeout;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }

	/* Parse the list of addresses/ports at the end of the command line. */
	addr    = (char *) strtok(argv[argc-1], "/");
        rx_port = DEFAULT_RTP_PORT;
	port    = (char *) strtok(NULL, "/");
        if (port != NULL) {
                rx_port = atoi(port);
        }
	port    = (char *) strtok(NULL, "/");
	if (port != NULL) {
		tx_port = atoi(port);
	} else {
                tx_port = rx_port;
        }
	/* Fix odd numbered ports to the next lower port... */
	if (rx_port % 2) rx_port--;
	if (tx_port % 2) tx_port--;

	/* Parse early command line parameters. These are things which we can */
	/* do before the media engine knows its RTP session information.      */
	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-t") == 0) && (argc > i+1)) {
                        ttl = atoi(argv[i+1]);
                        if (ttl < 0 || ttl > 255) {
                                usage("Usage: -t 0-255.\n");
                                return FALSE;
                        }
                        i++;
		}
	}

	/* Send the RTP address to the media engine... */
	addr    = mbus_encode_str(addr);
	mbus_qmsgf(m, e_addr, TRUE, "rtp.addr", "%s %d %d %d", addr, rx_port, tx_port, ttl);
	xfree(addr);


	/* Parse late command line parameters... */
	for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-allowloopback") == 0) || (strcmp(argv[i], "-allow_loopback") == 0)) {
			mbus_qmsgf(m, e_addr, TRUE, "tool.rat.filter.loopback", "0");
                } else if ((strcmp(argv[i], "-C") == 0) && (argc > i+1)) {
			tmp = mbus_encode_str(argv[i+1]);
			mbus_qmsgf(m, e_addr, TRUE, "session.title", tmp);
			xfree(tmp);
                } else if ((strcmp(argv[i], "-pt") == 0) && (argc > i+1)) {
			/* Dynamic payload type mapping. Format: "-pt pt/codec" */
			/* Codec is of the form "pcmu-8k-mono"                  */
			int         pt    = atoi((char*)strtok(argv[i+1], "/"));
			const char *codec = codec_get_compatible_name((const char*)strtok(NULL, "/"));
                        if (codec != NULL) {
                                mbus_qmsgf(m, e_addr, TRUE, "tool.rat.payload.set", "\"%s\" %d", codec, pt);
                        } else {
                                usage("Usage: -pt <pt>/<codec>");
                        }
		} else if (((strcmp(argv[i], "-K") == 0) || strcmp(argv[i], "-crypt") == 0) && (argc > i+1)) {
			tmp = mbus_encode_str(argv[i+1]);
			mbus_qmsgf(m, e_addr, TRUE, "security.encryption.key", tmp);
			xfree(tmp);
		} else if ((strcmp(argv[i], "-agc") == 0) && (argc > i+1)) {
			if (strcmp(argv[i+1], "on") == 0) {
				mbus_qmsgf(m, e_addr, TRUE, "tool.rat.agc", "1");
			} else if (strcmp(argv[i+1], "off") == 0) {
				mbus_qmsgf(m, e_addr, TRUE, "tool.rat.agc", "0");
			} else {
				usage("Usage: -agc on|off\n");
                                return FALSE;
			}
		} else if ((strcmp(argv[i], "-silence") == 0) && (argc > i+1)) {
			if (strcmp(argv[i+1], "on") == 0) {
				mbus_qmsgf(m, e_addr, TRUE, "tool.rat.silence", "1");
			} else if (strcmp(argv[i+1], "off") == 0) {
				mbus_qmsgf(m, e_addr, TRUE, "tool.rat.silence", "0");
			} else {
				usage("Usage: -silence on|off\n");
                                return FALSE;
			}
		} else if ((strcmp(argv[i], "-repair") == 0) && (argc > i+1)) {
			tmp = mbus_encode_str(argv[i+1]);
			mbus_qmsgf(m, e_addr, TRUE, "audio.channel.repair", tmp);
			xfree(tmp);
		} else if ((strcmp(argv[i], "-f") == 0) && (argc > i+1)) {
			/* Set primary codec: "-f codec". You cannot set the   */
			/* redundant codec with this option, use "-r" instead. */

                        /* Strip trailing / in case user attempting old syntax */
                        char *firstname = (char*)strtok(argv[i+1], "/");

			/* The codec should be of the form "pcmu-8k-mono".     */
                        char *realname  = xstrdup(codec_get_compatible_name(firstname));
			char *name = (char*)strtok(realname, "-");
			char *freq = (char*)strtok(NULL, "-");
			char *chan = (char*)strtok(NULL, "");
                        if (freq != NULL && chan != NULL) {
                                debug_msg("codec: %s %s %s\n", name, freq, chan);
                                name = mbus_encode_str(name);
                                freq = mbus_encode_str(freq);
                                chan = mbus_encode_str(chan);
                                mbus_qmsgf(m, e_addr, TRUE, "tool.rat.codec", "%s %s %s", name, chan, freq);
                                xfree(name);
                                xfree(freq);
                                xfree(chan);
                        }
                        xfree(realname);
		} else if ((strcmp(argv[i], "-r") == 0) && (argc > i+1)) {
			/* Set channel coding to redundancy: "-r codec/offset" */
			const char *codec  = codec_get_compatible_name((const char*)strtok(argv[i+1], "/"));
			int   offset = atoi((char*)strtok(NULL, ""));
                        if (offset != 0) {
                                codec  = mbus_encode_str(codec);
                                mbus_qmsgf(m, e_addr, TRUE, "audio.channel.coding", "\"redundancy\" %s %d", codec, offset);
                        } else {
                                usage("Usage: -r <codec>/<offset>");
                        }
			xfree((char*)codec);
                } else if ((strcmp(argv[i], "-l") == 0) && (argc > i+1)) { 
			/* Set channel coding to layered */
		} else if ((strcmp(argv[i], "-i") == 0) && (argc > i+1)) {
			/* Set channel coding to interleaved */
                }
        }

	/* Synchronize with the sub-processes... */
	do {
		mbus_send(m);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	} while (!mbus_sent_all(m));

	UNUSED(u_addr);
        return TRUE;
}

static void mbus_err_handler(int seqnum, int reason)
{
	/* Something has gone wrong with the mbus transmission. At this time */
	/* we don't try to recover, just kill off the media engine and user  */
	/* interface processes and exit.                                     */
	printf("FATAL ERROR: couldn't send mbus message (%d:%d)\n", seqnum, reason);
        if (should_exit == FALSE) {
                kill_process(pid_ui);
                kill_process(pid_engine);
                abort();
        }
}

static void terminate(struct mbus *m, char *addr, pid_t *pid)
{
	if (mbus_addr_valid(m, addr)) {
		/* This is a valid address, ask that process to quit. */
		debug_msg("Sending mbus.quit() to %s...\n", addr);
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
		debug_msg("...done\n");
	} else {
		/* That process has already terminated, do nothing. */
	}
	*pid = 0;
}


/* Signal Handlers */
#ifndef WIN32
static void
sigchld_handler(int signal)
{
        UNUSED(signal);
        /* Child has terminated or stopped.  Try to shutdown nicely... */
        debug_msg("Caught signal SIGCHLD: %d\n", signal);
        should_exit = TRUE;
}

static void
sigint_handler(int signal)
{
        UNUSED(signal);
        debug_msg("Caught signal %d\n", signal);
	kill_process(pid_ui);
	kill_process(pid_engine);
        exit(-1);
}
#endif

int main(int argc, char *argv[])
{
	struct mbus	*m;
	char		 c_addr[60], token_ui[20], token_engine[20];
        int		 seed = (gethostid() << 8) | (getpid() & 0xff);
	struct timeval	 timeout;

#ifndef WIN32
 	signal(SIGINT, sigint_handler); 
 	signal(SIGCHLD, sigchld_handler); 
#endif

        if (parse_options_early(argc, (const char**)argv) == FALSE) {
                return FALSE;
        }

	srand48(seed);
	snprintf(c_addr, 60, "(media:audio module:control app:rat instance:%lu)", (unsigned long) getpid());
	m = mbus_init(mbus_control_rx, mbus_err_handler, c_addr);

	sprintf(token_ui,     "rat-token-%08lx", lrand48());
	sprintf(token_engine, "rat-token-%08lx", lrand48());

	u_addr = fork_process(m, UI_NAME,     c_addr, &pid_ui,     token_ui);
	e_addr = fork_process(m, ENGINE_NAME, c_addr, &pid_engine, token_engine);

        if (parse_options(m, e_addr, u_addr, argc, argv) == TRUE) {
                mbus_rendezvous_go(m, token_engine, (void *) m);
                mbus_rendezvous_go(m, token_ui,     (void *) m); 

                should_exit = FALSE;
                while (!should_exit) {
                        mbus_send(m);
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 20000;
                        mbus_recv(m, NULL, &timeout);
                }        
                terminate(m, u_addr, &pid_ui);
                terminate(m, e_addr, &pid_engine);
        }

	kill_process(pid_ui);
	kill_process(pid_engine);

#ifdef WIN32
        WSACleanup();
#endif
	debug_msg("Controller exit\n");
	return 0;
}

