/*
* FILE:    main.c
* PROGRAM: RAT - controller
* AUTHOR:  Colin Perkins / Orion Hodson
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
#define UI_NAME     "rat-"##RAT_VERSION##"-ui"
#define ENGINE_NAME "rat-"##RAT_VERSION##"-media"
#endif

#define DEFAULT_RTP_PORT 5004

char	*u_addr, *e_addr;
pid_t	 pid_ui, pid_engine;
int	 should_exit;

static int ttl = 15;

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
                      http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/faq.html\
                      ";

	if (szOffending == NULL) {
		szOffending = win_usage;
	}
	MessageBox(NULL, szOffending, "RAT v" RAT_VERSION " Usage", MB_ICONINFORMATION | MB_OK);
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
#ifdef WIN32
        rc = _vsnprintf(s, buf_size, format, ap);
#else
        rc = sprintf(s, format, ap);
#endif   
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

static int
address_is_valid(const char *candidate)
{
	char *addr, *p;
	int   i, port, okay = TRUE;

	addr = xstrdup(candidate);

        addr = strtok(addr, "/");
        if (udp_addr_valid(addr) == FALSE) {
                char *msg = "Invalid address: %s";
		char *m   = xmalloc(strlen(candidate) + strlen(msg));
		sprintf(m, msg, candidate);
		usage(m);
		xfree(m);
                okay = FALSE;
        }
	
	/* rx_port then tx_port */
	for (i = 0; i < 2; i++) {
		p = strtok(NULL, "/");
		if (p != NULL) {
			port = atoi(p);
			if (port < 1023 || port > 65536) {
				usage("Port must be > 1023 and <= 65536\n");
				okay = FALSE;
			}
		}
	}
        xfree(addr);
        
        return okay;
}

static int parse_options_early(int argc, const char **argv) 
{
        int i;
        
        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }
        
        /* DOS and UNIX style command flags for usage and version */        
        for (i = 1; i < argc && (argv[i][0] == '-' || argv[i][0] == '/'); i++) {
                if (tolower(argv[i][1]) == 'v') {
#ifdef WIN32
			MessageBox(NULL, "RAT v" RAT_VERSION, "RAT Version", MB_OK);
#else
                        printf("%s\n", "RAT v" RAT_VERSION);
#endif
                        return FALSE;
                } else if (argv[i][1] == '?' || tolower(argv[i][1]) == 'h') {
                        usage(NULL);
                        return FALSE;
                } else if (argv[i][1] == 't') {
                        /* 
                         * Handle ttl here because it is reqd before rtp address 
                         * can be sent. 
                         */
                        ttl = atoi(argv[i+1]);
                        if (ttl < 0 || ttl > 255) {
                                usage("Usage: -t 0-255.\n");
                                return FALSE;
                        }
                }
        }
        
        /* 
         * Validate destination address.  Do it here before launching 
         * sub-processes and mbus.  This only checks the last argument
	 * for being a valid address.  In the case of layering this 
	 * will not be the only one, but we have to parse all args to
	 * find this out. 
         */     
	return address_is_valid(argv[argc-1]);
}

/* Late Command Line Argument Functions */

static int 
cmd_layers(struct mbus *m, char *addr, int argc, char *argv[])
{
        int layers;
        assert(argc == 1);
        layers = atoi(argv[0]);
        if (layers > 1) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.layers", "%d", argv[0]);
                return TRUE;
        }
        UNUSED(argc);
        return FALSE;
}

static int 
cmd_allowloop(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 0);
        mbus_qmsgf(m, addr, TRUE, "tool.rat.filter.loopback", "0");
        UNUSED(argc);
        UNUSED(argv);
        return TRUE;
}

static int 
cmd_session_name(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *enc_name;
        assert(argc == 1);
        enc_name = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "session.title", enc_name);
        xfree(enc_name);
        UNUSED(argc);
        return TRUE;
}

static int
cmd_payload_map(struct mbus *m, char *addr, int argc, char *argv[])
{
        const char *compat;
        char       *codec;
        int         codec_pt;

        assert(argc == 1);
        /* Dynamic payload type mapping. Format: "-pt pt/codec" */
        /* Codec is of the form "pcmu-8k-mono"                  */
        codec_pt = atoi((char*)strtok(argv[0], "/"));
        compat   = codec_get_compatible_name(strtok(NULL, "/"));
        if (compat == NULL) {
                usage("Usage: -pt <pt>/<codec>");
                return FALSE;
        }
        codec = mbus_encode_str(compat);
        mbus_qmsgf(m, addr, TRUE, "tool.rat.payload.set", "%s %d", codec, codec_pt);
        xfree(codec);

        UNUSED(argc);
        return TRUE;
}

static int
cmd_crypt(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *key;

        assert(argc == 1);
        key = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "security.encryption.key", key);
        xfree(key);

        UNUSED(argc);
        return TRUE;
}

static int
cmd_agc(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 1);
        if (strcmp(argv[0], "on") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.agc", "1");
                return TRUE;
        } else if (strcmp(argv[0], "off") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.agc", "0");
                return TRUE;
        }
        UNUSED(argc);
        usage("Usage: -agc on|off\n");
        return FALSE;
}

static int
cmd_silence(struct mbus *m, char *addr, int argc, char *argv[])
{
        assert(argc == 1);
        if (strcmp(argv[0], "on") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.silence", "1");
                return TRUE;
        } else if (strcmp(argv[0], "off") == 0) {
                mbus_qmsgf(m, addr, TRUE, "tool.rat.silence", "0");
                return TRUE;
        } 
        UNUSED(argc);
        usage("Usage: -silence on|off\n");
        return FALSE;
}

static int
cmd_repair(struct mbus *m, char *addr, int argc, char *argv[])
{
        char *repair;
        assert(argc == 1);
        repair = mbus_encode_str(argv[0]);
        mbus_qmsgf(m, addr, TRUE, "audio.channel.repair", repair);
        xfree(repair);
        UNUSED(argc);
        return TRUE;
}

static int
cmd_primary(struct mbus *m, char *addr, int argc, char *argv[])
{
        /* Set primary codec: "-f codec". You cannot set the   */
        /* redundant codec with this option, use "-r" instead. */
        char *firstname, *realname, *name, *freq, *chan;
        
        assert(argc == 1);
        /* Break at trailing / in case user attempting old syntax */
        firstname = (char*)strtok(argv[0], "/");
        
        /* The codec should be of the form "pcmu-8k-mono".     */
        realname = xstrdup(codec_get_compatible_name(firstname));
        name     = (char*)strtok(realname, "-");
        freq     = (char*)strtok(NULL, "-");
        chan     = (char*)strtok(NULL, "");
        if (freq != NULL && chan != NULL) {
                debug_msg("codec: %s %s %s\n", name, freq, chan);
                name    = mbus_encode_str(name);
                freq    = mbus_encode_str(freq);
                chan    = mbus_encode_str(chan);
                mbus_qmsgf(m, addr, TRUE, "tool.rat.codec", "%s %s %s", name, freq, chan);
                xfree(name);
                xfree(freq);
                xfree(chan);
        }
        xfree(realname);
        return TRUE;
}

static int
cmd_redundancy(struct mbus *m, char *addr, int argc, char *argv[])
{
        const char *compat;
        char       *redundancy, *codec;
        int         offset;

        assert(argc == 1);
        /* Set channel coding to redundancy: "-r codec/offset" */
        compat = codec_get_compatible_name((const char*)strtok(argv[0], "/"));
        offset = atoi((char*)strtok(NULL, ""));

        if (offset > 0) {
                redundancy = mbus_encode_str("redundancy");
                codec      = mbus_encode_str(compat);
                mbus_qmsgf(m, addr, TRUE, "audio.channel.coding", "%s %s %d", redundancy, codec, offset);
                xfree(redundancy);
                xfree(codec);
                return TRUE;
        }
        UNUSED(argc);
        usage("Usage: -r <codec>/<offset>");
        return FALSE;
}

typedef struct {
        const char *cmdname;                               /* Command line flag */
        int       (*cmd_proc)(struct mbus *m, char *addr, int argc, char *argv[]); /* TRUE = success, FALSE otherwise */
        int        argc;                                   /* No. of args       */
} args_handler;

static args_handler late_args[] = {
        { "-l",              cmd_layers,       1 },
        { "-allowloopback",  cmd_allowloop,    0 },
        { "-allow_loopback", cmd_allowloop,    0 },
        { "-C",              cmd_session_name, 1 },
        { "-pt",             cmd_payload_map,  1 },
        { "-crypt",          cmd_crypt,        1 },
        { "-K",              cmd_crypt,        1 },
        { "-agc",            cmd_agc,          1 },
        { "-silence",        cmd_silence,      1 },
        { "-repair",         cmd_repair,       1 },
        { "-f",              cmd_primary,      1 },
        { "-r",              cmd_redundancy,   1 },
        { "-t",            NULL,             1 }, /* handled in parse early args */
};

static uint32_t late_args_cnt = sizeof(late_args)/sizeof(late_args[0]);

static const args_handler *
get_late_args_handler(char *cmdname)
{
        uint32_t j;
        for (j = 0; j < late_args_cnt; j++) {
                if (strcmp(cmdname, late_args[j].cmdname) == 0) {
                        return late_args + j;
                }
        }
        return NULL;
}

static int address_count(int argc, char *argv[])
{
        const args_handler *a;
        int                 i;
        
        for (i = 0; i < argc; i++) {
                a = get_late_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                i += a->argc;
        }
        
        return argc - i;
}

static void
parse_non_addr(struct mbus *m, char *addr, int argc, char *argv[])
{
        const args_handler *a;
        int                 i;
        
        for (i = 0; i < argc; i++) {
                a = get_late_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                if (a->cmd_proc && (argc - i) > a->argc) {
                        a->cmd_proc(m, addr, a->argc, argv + i + 1);
                }
                i += a->argc;
        }
        return;
}

static int 
parse_addr(char *arg, char **addr, int *rx_port, int *tx_port)
{
        char *token;
        int   port;
	
	if (address_is_valid(arg) == FALSE) {
		*addr    = NULL;
		*rx_port = 0;
		*tx_port = 0;
		return FALSE;
	}

	/* Address is definitely valid... */

        *addr = (char *)strtok(arg, "/");
     
        *rx_port = DEFAULT_RTP_PORT;
        token = strtok(NULL, "/");
        if (token) {
                port     = atoi(token);
                port    &= ~1; 
		*rx_port = port;
        }

        *tx_port = *rx_port;
        token = strtok(NULL, "/");
        if (token) {
                port     = atoi(token);
                port    &= ~1; 
                *tx_port = port;
        }
        return TRUE;
}

static int 
parse_options(struct mbus *m, char *e_addr, char *u_addr, int argc, char *argv[])
{
        char		*addr;
        int              i, naddr, rx_port, tx_port;
        struct timeval	 timeout;
        
        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }
        argc -= 1; /* Skip process name */
        argv += 1;

        naddr = address_count(argc, argv);
        if (naddr < 1) {
                usage(NULL);
                return FALSE;
        }

        /* Send address to engine before parsing other args.  They need to context to be relevent */
        for(i = 0; i < naddr; i++) {
                if (parse_addr(argv[argc - naddr + i], &addr, &rx_port, &tx_port) == FALSE) {
                        usage(NULL);
                        return FALSE;
                }
		addr    = mbus_encode_str(addr);
		mbus_qmsgf(m, e_addr, TRUE, "rtp.addr", "%s %d %d %d", addr, rx_port, tx_port, ttl);
		xfree(addr);
        }

        parse_non_addr(m, e_addr, argc, argv);
        
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

#ifndef WIN32

/* Signal Handlers */
static void
sigint_handler(int signal)
{
        UNUSED(signal);
        debug_msg("Caught signal %d\n", signal);
        kill_process(pid_ui);
        kill_process(pid_engine);
        exit(-1);
}

static void
sigchld_handler(int signal)
{
        UNUSED(signal);
        /* Child has terminated or stopped.  Try to shutdown nicely... */
        debug_msg("Caught signal SIGCHLD: %d\n", signal);
        should_exit = TRUE;
	wait(NULL);	/* Buffy, the zombie slayer... */
}

#else

/* Can use waitForMultipleObject to check liveness of child processes */
#define NUM_CHILD_PROCS 2

static void
win32_check_children_running(void)
{
        HANDLE hProc[NUM_CHILD_PROCS];
        DWORD  dwResult;
        
        hProc[0] = (HANDLE)pid_ui;
        hProc[1] = (HANDLE)pid_engine;
        
        dwResult = WaitForMultipleObjects(NUM_CHILD_PROCS, hProc, FALSE, 0);
        if (dwResult >= WAIT_OBJECT_0 && dwResult < WAIT_OBJECT_0 + NUM_CHILD_PROCS) {
                debug_msg("Process %lu is no longer running\n", 
                        (uint32_t)hProc[dwResult - WAIT_OBJECT_0]);
                kill_process(pid_ui);
                kill_process(pid_engine);
                exit(-1);
                return;
        }
        
        if (dwResult >= WAIT_ABANDONED_0 && dwResult < WAIT_ABANDONED_0 + NUM_CHILD_PROCS) {
                debug_msg("Process %lu wait abandoned\n",
                        (uint32_t)hProc[dwResult - WAIT_ABANDONED_0]);
                return; /* Nothing to do, process quit already */
        }
        
        if (dwResult == WAIT_FAILED) {
                debug_msg("Wait failed\n");
                return;
        }
        
        assert(dwResult == WAIT_TIMEOUT);
        return;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
        switch(msg) {
        case WM_DESTROY:
                should_exit =TRUE;
                break;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void win32_process_messages(void)
{
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, 0)) {
                if (GetMessage(&msg, NULL, 0, 0) == 0) {
                        OutputDebugString("\nGot WM_QUIT\n");
                        should_exit = TRUE;
                        return;
                }
                TranslateMessage( &msg );
                DispatchMessage( &msg );
        }
}

extern HINSTANCE hAppPrevInstance, hAppInstance;

void
win32_create_null_window()
{
        WNDCLASS wc;
        HWND     hWnd;

        /* Create a non-visible window so we can process messages */

        if( !hAppPrevInstance )
        {
                wc.lpszClassName = "RatControllerClass";
                wc.lpfnWndProc = MainWndProc;
                wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
                wc.hInstance = hAppInstance;
                wc.hIcon = NULL;
                wc.hCursor = NULL;
                wc.hbrBackground = (HBRUSH)( COLOR_WINDOW+1 );
                wc.lpszMenuName = NULL;
                wc.cbClsExtra = 0;
                wc.cbWndExtra = 0;
                
                RegisterClass( &wc );
        }
        
        hWnd = CreateWindow( "RatControllerClass",
                "RAT",
                WS_OVERLAPPEDWINDOW|WS_HSCROLL|WS_VSCROLL,
                0,
                0,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                NULL,
                NULL,
                hAppInstance,
                NULL
                );
}

#endif /* WIN32 */

int main(int argc, char *argv[])
{
        struct mbus	*m;
        char		 c_addr[60], token_ui[20], token_engine[20];
        int		 seed = (gethostid() << 8) | (getpid() & 0xff), final_iters;
        struct timeval	 timeout;
        
#ifdef WIN32
        win32_create_null_window(); /* Needed to listen to messages */
#else
        signal(SIGCHLD, sigchld_handler); 
        signal(SIGINT, sigint_handler);
#endif

        if (parse_options_early(argc, (const char**)argv) == FALSE) {
                return FALSE;
        }
        
        srand48(seed);
        snprintf(c_addr, 60, "(media:audio module:control app:rat id:%lu)", (unsigned long) getpid());
        m = mbus_init(mbus_control_rx, mbus_err_handler, c_addr);
        
        sprintf(token_ui,     "rat-token-%08lx", lrand48());
        sprintf(token_engine, "rat-token-%08lx", lrand48());
        
        u_addr = fork_process(m, UI_NAME,     c_addr, &pid_ui,     token_ui);
        e_addr = fork_process(m, ENGINE_NAME, c_addr, &pid_engine, token_engine);
        
        if (parse_options(m, e_addr, u_addr, argc, argv) == TRUE) {
                mbus_rendezvous_go(m, token_engine, (void *) m);
                mbus_rendezvous_go(m, token_ui,     (void *) m); 
                
		final_iters = 25;
                should_exit = FALSE;
                while (final_iters > 0) {
                        mbus_send(m);
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 20000;
#ifdef WIN32
                        win32_check_children_running();
                        win32_process_messages();
#endif
                        mbus_recv(m, NULL, &timeout);
			if (should_exit) {
				final_iters--;
			}
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
