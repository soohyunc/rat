/*
 * Copyright (c) 1996 The Regents of the University of California.
 * All rights reserved.
 * 
 *
 * This module contributed by John Brezak <brezak@apollo.hp.com>.
 * January 31, 1996
 *
 * Additional contributions Orion Hodson (UCL) 1996-98.
 *
 */

#ifdef WIN32

#ifndef lint
static char rcsid[] =
    "@(#) $Header$ (LBL)";
#endif

#include "config_win32.h"
#include "debug.h"
#include "tcl.h"
#include "tk.h"
#include "util.h"

int
uname(struct utsname *ub)
{
    char *ptr;
    DWORD version;
    SYSTEM_INFO sysinfo;
    char hostname[MAXGETHOSTSTRUCT];
    
    version = GetVersion();
    GetSystemInfo(&sysinfo);
    
    switch (sysinfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
	(void)strncpy(ub->machine, "ix86", _SYS_NMLN);
	break;
    case PROCESSOR_ARCHITECTURE_MIPS :
	(void)strncpy(ub->machine, "mips", _SYS_NMLN);
	break;
    case PROCESSOR_ARCHITECTURE_ALPHA:
	(void)strncpy(ub->machine, "alpha", _SYS_NMLN);
	break;
    case PROCESSOR_ARCHITECTURE_PPC:
	(void)strncpy(ub->machine, "ppc", _SYS_NMLN);
	break;
    default:
	(void)strncpy(ub->machine, "unknown", _SYS_NMLN);
	break;
    }
    
    if (version < 0x80000000) {
	(void)strncpy(ub->version, "NT", _SYS_NMLN);
    }
    else if (LOBYTE(LOWORD(version))<4) {
	(void)strncpy(ub->version, "Win32s", _SYS_NMLN);
    }
    else				/* Win95 */ {
	(void)strncpy(ub->version, "Win95", _SYS_NMLN);
    }
    (void)sprintf(ub->release, "%u.%u",
		  (DWORD)(LOBYTE(LOWORD(version))),
		  (DWORD)(HIBYTE(LOWORD(version))));
    (void)strncpy(ub->sysname, "Windows", _SYS_NMLN);
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        ptr = strchr(hostname, '.');
            if (ptr)
	    *ptr = '\0';
    }
    else {
	perror("uname: gethostname failed");
	strncpy(hostname, "FAILURE", _SYS_NMLN);
    }
    strncpy(ub->nodename, hostname, sizeof(ub->nodename));
    ub->nodename[_SYS_NMLN - 1] = '\0';
    return 0;
}

uid_t
getuid(void) 
{ 
    return 0;
    
}

gid_t
getgid(void)
{
    return 0;
}

unsigned long
gethostid(void)
{
    char   hostname[WSADESCRIPTION_LEN];
    struct hostent *he;

    if ((gethostname(hostname, WSADESCRIPTION_LEN) == 0) &&
        (he = gethostbyname(hostname)) != NULL) {
            return *((unsigned long*)he->h_addr_list[0]);        
    }

    /*XXX*/
    return 0;
}

int
nice(int pri)
{
    return 0;
}

extern int main(int argc, const char *argv[]);
extern int __argc;
extern char **__argv;

static char argv0[255];		/* Buffer used to hold argv0. */

HINSTANCE hAppInstance;

char *__progname = "main";

#define WS_VERSION_ONE MAKEWORD(1,1)
#define WS_VERSION_TWO MAKEWORD(2,2)

int APIENTRY
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    char *p;
    WSADATA WSAdata;
    int r;
    if (WSAStartup(WS_VERSION_TWO, &WSAdata) != 0 &&
        WSAStartup(WS_VERSION_ONE, &WSAdata) != 0) {
    	MessageBox(NULL, "Windows Socket initialization failed. TCP/IP stack\nis not installed or is damaged.", "Network Error", MB_OK | MB_ICONERROR);
        exit(-1);
    }

    debug_msg("WSAStartup OK: %sz\nStatus:%s\n", WSAdata.szDescription, WSAdata.szSystemStatus);

    hAppInstance = hInstance;

    /*
     * Increase the application queue size from default value of 8.
     * At the default value, cross application SendMessage of WM_KILLFOCUS
     * will fail because the handler will not be able to do a PostMessage!
     * This is only needed for Windows 3.x, since NT dynamically expands
     * the queue.
     */
    SetMessageQueue(64);

    GetModuleFileName(NULL, argv0, 255);
    p = argv0;
    __progname = strrchr(p, '/');
    if (__progname != NULL) {
	__progname++;
    }
    else {
	__progname = strrchr(p, '\\');
	if (__progname != NULL) {
	    __progname++;
	} else {
	    __progname = p;
	}
    }
    
    r = main(__argc, (const char**)__argv);

    WSACleanup();
    return r;
}

void
ShowMessage(int level, char *msg)
{
    MessageBeep(level);
    MessageBox(NULL, msg, __progname,
	       level | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);
}

static char szTemp[256];

int
printf(const char *fmt, ...)
{
	int retval;
	
	va_list ap;
	va_start(ap, fmt);
	retval = _vsnprintf(szTemp, 256, fmt, ap);
	OutputDebugString(szTemp);
	va_end(ap);

	return retval;
}

int
fprintf(FILE *f, const char *fmt, ...)
{
	int 	retval;
	va_list	ap;

	va_start(ap, fmt);
	if (f == stderr) {
		retval = _vsnprintf(szTemp, 256, fmt, ap);
		OutputDebugString(szTemp);
	} else {
		retval = vfprintf(f, fmt, ap);
	}
	va_end(ap);
	return retval;
}

void
perror(const char *msg)
{
    DWORD cMsgLen;
    CHAR *msgBuf;
    DWORD dwError = GetLastError();
    
    cMsgLen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
			    FORMAT_MESSAGE_ALLOCATE_BUFFER | 40, NULL,
			    dwError,
			    MAKELANGID(0, SUBLANG_ENGLISH_US),
			    (LPTSTR) &msgBuf, 512,
			    NULL);
    if (!cMsgLen)
	fprintf(stderr, "%s%sError code %lu\n",
		msg?msg:"", msg?": ":"", dwError);
    else {
	fprintf(stderr, "%s%s%s\n", msg?msg:"", msg?": ":"", msgBuf);
	LocalFree((HLOCAL)msgBuf);
    }
}


#define MAX_VERSION_STRING_LEN 256
const char*
w32_make_version_info(char *szRatVer) 
{
	char		platform[64];
        static char	szVer[MAX_VERSION_STRING_LEN];
        OSVERSIONINFO	oi;

        oi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&oi);

        switch(oi.dwPlatformId) {
	        case VER_PLATFORM_WIN32_NT:
			sprintf(platform, "Windows NT");
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			if (oi.dwMinorVersion > 0) {
				sprintf(platform, "Windows 98");
			} else {
				sprintf(platform, "Windows 95");
			}
			break;
		case  VER_PLATFORM_WIN32s:
			sprintf(platform, "Win32s");
		        break;
		default:
			sprintf(platform, "Windows (unknown)");
        }

	sprintf(szVer, "%s %s %d.%d %s", 
		szRatVer, 
		platform, 
		oi.dwMajorVersion, 
		oi.dwMinorVersion, 
		oi.szCSDVersion);
        return szVer;
}

#endif /* WIN32 */
