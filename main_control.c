/*
 * FILE:    main_control.c
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
#include "fatal_error.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "net_udp.h"
#include "util.h"
#include "process.h"
#include "cmd_parser.h"

#ifdef WIN32
#define UI_NAME      "ratui.exe"
#define ENGINE_NAME  "ratmedia.exe"
#define CONTROL_NAME "rat.exe"
#else
#define UI_NAME      "rat-"##RAT_VERSION##"-ui"
#define ENGINE_NAME  "rat-"##RAT_VERSION##"-media"
#endif

#define DEFAULT_RTP_PORT 5004

const char *appname;
char       *u_addr, *e_addr;
pid_t       pid_ui, pid_engine;
int         should_exit;

static int ttl = 15;

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
        
        /* Iterate over all args and pick out anything needed before
         * launching other processes.
         */
        for (i = 1; i < argc; i++) {
                if (argv[i][0] != '-' && argv[i][0] != '/') {
                        continue;
                } else if (tolower(argv[i][1]) == 'v') {
#ifdef WIN32
			MessageBox(NULL, "RAT v" RAT_VERSION, "RAT Version", MB_OK);
#else
                        printf("%s\n", "RAT v" RAT_VERSION);
#endif
                        return FALSE;
                } else if (argv[i][1] == '?' || tolower(argv[i][1]) == 'h') {
                        usage(NULL);
                        return FALSE;
                } else if (argv[i][1] == 't' && i + 1 < argc) {
                        /* 
                         * Handle ttl here because it is required before rtp 
                         * address can be sent. 
                         */
                        i++;
                        ttl = atoi(argv[i]);
                        /* Hack because 128 is max ttl in rtp library 
                         * previous versions are 0-255.
                         */
                        if (ttl > 127) {
                                ttl = 127;
                        }
                        if (ttl < 0 || ttl > 127) {
                                usage("Usage: -t 0-127.\n");
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

static int
parse_options_late(struct mbus *m, char *addr, int argc, char *argv[])
{
        const args_handler *a;
        int                 i;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }
        argc -= 1; /* Skip process name */
        argv += 1;
        
        for (i = 0; i < argc; i++) {
                a = cmd_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                if (a->cmd_proc && (argc - i) > a->argc) {
                        a->cmd_proc(m, addr, a->argc, argv + i + 1);
                }
                i += a->argc;
        }
        return (i != argc);
}

static int 
address_count(int argc, char *argv[])
{
        const args_handler *a;
        int                 i;
        
        for (i = 0; i < argc; i++) {
                a = cmd_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                i += a->argc;
        }
        return argc - i;
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
parse_addresses(struct mbus *m, char *e_addr, int argc, char *argv[])
{
        char		*addr;
        int              i, naddr, rx_port, tx_port;
         
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
        return TRUE;
}

static void mbus_err_handler(int seqnum, int reason)
{
        /* Something has gone wrong with the mbus transmission. At this time */
        /* we don't try to recover, just kill off the media engine and user  */
        /* interface processes and exit.                                     */
        
        if (should_exit == FALSE) {
                char msg[64];
                sprintf(msg, "Could not send mbus message (%d:%d)\n", seqnum, reason);
                fatal_error(appname, msg);
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
                should_exit = TRUE;
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

        appname = get_appname(argv[0]);
        
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
        if (m == NULL) {
                fatal_error(appname, "Could not initialize Mbus: Is multicast enabled?");
                return FALSE;
        }
        
        sprintf(token_ui,     "rat-token-%08lx", lrand48());
        sprintf(token_engine, "rat-token-%08lx", lrand48());
        
        u_addr = fork_process(m, UI_NAME,     c_addr, &pid_ui,     token_ui);
        e_addr = fork_process(m, ENGINE_NAME, c_addr, &pid_engine, token_engine);
        if (parse_addresses(m, e_addr, argc, argv) == TRUE) {
                mbus_rendezvous_go(m, token_engine, (void *) m);
                mbus_rendezvous_go(m, token_ui,     (void *) m); 
                parse_options_late(m, e_addr, argc, argv);
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
