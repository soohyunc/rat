/*
 * Copyright (c) 1996 The Regents of the University of California.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the Network Research
 * 	Group at Lawrence Berkeley National Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
#include "tcl.h"
#include "tk.h"

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
	(void)strcpy(ub->machine, "ix86");
	break;
    case PROCESSOR_ARCHITECTURE_MIPS :
	(void)strcpy(ub->machine, "mips");
	break;
    case PROCESSOR_ARCHITECTURE_ALPHA:
	(void)strcpy(ub->machine, "alpha");
	break;
    case PROCESSOR_ARCHITECTURE_PPC:
	(void)strcpy(ub->machine, "ppc");
	break;
    default:
	(void)strcpy(ub->machine, "unknown");
	break;
    }
    
    if (version < 0x80000000) {
	(void)strcpy(ub->version, "NT");
    }
    else if (LOBYTE(LOWORD(version))<4) {
	(void)strcpy(ub->version, "Win32s");
    }
    else				/* Win95 */ {
	(void)strcpy(ub->version, "Win95");
    }
    (void)sprintf(ub->release, "%u.%u",
		  (DWORD)(LOBYTE(LOWORD(version))),
		  (DWORD)(HIBYTE(LOWORD(version))));
    (void)strcpy(ub->sysname, "Windows");
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        ptr = strchr(hostname, '.');
            if (ptr)
	    *ptr = '\0';
    }
    else {
	perror("uname: gethostname failed");
	strcpy(hostname, "FAILURE");
    }
    strncpy(ub->nodename, hostname, sizeof(ub->nodename));
    ub->nodename[_SYS_NMLN - 1] = '\0';
    return 0;
}

int gettimeofday(struct timeval *p, struct timezone *z)
{
    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);
	if (p) {
		extern void TclpGetTime(Tcl_Time*);
		Tcl_Time t;

		TclpGetTime(&t);
		p->tv_sec = t.sec;
		p->tv_usec = t.usec;
    }
    if (z) {
	z->tz_minuteswest = tz.Bias ;
	z->tz_dsttime = tz.StandardBias != tz.Bias;
    }
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

int
gethostid(void)
{
    /*XXX*/
    return 0;
}

int
nice(int pri)
{
    return 0;
}

extern void TkWinXInit(HINSTANCE hInstance);
extern int main(int argc, const char *argv[]);
extern int __argc;
extern char **__argv;

static char argv0[255];		/* Buffer used to hold argv0. */

char *__progname = "main";

int APIENTRY
WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpszCmdLine,
    int nCmdShow)
{
    char *p;
    WSADATA WSAdata;

    if (WSAStartup(MAKEWORD (1, 1), &WSAdata)) {
    	perror("Windows Sockets init failed");
	abort();
    }

    TkWinXInit(hInstance);

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
    
    return main(__argc, (const char**)__argv);
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
    va_start (ap, fmt);
    retval = vsprintf(szTemp, fmt, ap);
    OutputDebugString(szTemp);
    va_end (ap);

    return(retval);
}

int
fprintf(FILE *f, const char *fmt, ...)
{
    int retval;
    
    va_list ap;
    va_start (ap, fmt);
    if (f == stderr) {
	retval = vsprintf(szTemp, fmt, ap);
	OutputDebugString(szTemp);
	va_end (ap);
    }
    else
	retval = vfprintf(stderr, fmt, ap);
    
    return(retval);
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

int
WinPutsCmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *f;
    int i, newline;
    char *fileId;

    i = 1;
    newline = 1;
    if ((argc >= 2) && (strcmp(argv[1], "-nonewline") == 0)) {
	newline = 0;
	i++;
    }
    if ((i < (argc-3)) || (i >= argc)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-nonewline? ?fileId? string\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below provides backwards compatibility with an old
     * form of the command that is no longer recommended or documented.
     */

    if (i == (argc-3)) {
	if (strncmp(argv[i+2], "nonewline", strlen(argv[i+2])) != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[i+2],
		    "\": should be \"nonewline\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newline = 0;
    }
    if (i == (argc-1)) {
	fileId = "stdout";
    } else {
	fileId = argv[i];
	i++;
    }

    if (strcmp(fileId, "stdout") == 0 || strcmp(fileId, "stderr") == 0) {
	char *result;

	if (newline) {
	    int len = strlen(argv[i]);
	    result = ckalloc(len+2);
	    memcpy(result, argv[i], len);
	    result[len] = '\n';
	    result[len+1] = 0;
	} else {
	    result = argv[i];
	}
	OutputDebugString(result);
	if (newline)
	    ckfree(result);
    } else {
	return TCL_OK;
	clearerr(f);
	fputs(argv[i], f);
	if (newline) {
	    fputc('\n', f);
	}
	if (ferror(f)) {
	    Tcl_AppendResult(interp, "error writing \"", fileId,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

int
WinGetUserName(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    char user[256];
    int size = sizeof(user);
    
    if (!GetUserName(user, &size)) {
	Tcl_AppendResult(interp, "GetUserName failed", NULL);
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, user, NULL);
    return TCL_OK;
}

double
drand48(void)
{
	return ((double)rand() / 2147483648.0);
}

static HKEY
regroot(root)
    char *root;
{
    if (strcasecmp(root, "HKEY_LOCAL_MACHINE") == 0)
	return HKEY_LOCAL_MACHINE;
    else if (strcasecmp(root, "HKEY_CURRENT_USER") == 0)
	return HKEY_CURRENT_USER;
    else if (strcasecmp(root, "HKEY_USERS") == 0)
	return HKEY_USERS;
    else if (strcasecmp(root, "HKEY_CLASSES_ROOT") == 0)
	return HKEY_CLASSES_ROOT;
    else
	return (HKEY)-1;
}

int 
WinReg(ClientData clientdata, Tcl_Interp *interp, int argc, char **argv)
{
	static char szBuf[255], szOutBuf[255];
        char *szRegRoot = NULL, *szRegPath = NULL, *szValueName;
        int cbOutBuf = 255;
        HKEY hKey, hKeyResult;
        DWORD dwDisp;

        if (argc < 4 || argc > 5) {
                Tcl_AppendResult(interp, "wrong number of args\n", szBuf, NULL);
                return TCL_ERROR;
        }
	
        strcpy(szBuf, argv[2]);
        szValueName = argv[3];
        szRegRoot   = szBuf;
        szRegPath   = strchr(szBuf, '\\');

        if (szRegPath == NULL || szValueName == NULL) {
                Tcl_AppendResult(interp, "registry path is wrongly written\n", szBuf, NULL);
                return TCL_ERROR;
        }
        
        *szRegPath = '\x0';
        szRegPath++;

        hKey = regroot(szRegRoot);
        
        if (hKey == (HKEY)-1) {
                Tcl_AppendResult(interp, "root not found %s", szRegRoot, NULL);
                return TCL_ERROR;
        }

        if (ERROR_SUCCESS != RegCreateKeyEx(hKey, szRegPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyResult, &dwDisp)) {
                Tcl_AppendResult(interp, "Could not open key", szRegRoot, szRegPath, NULL);
                return TCL_ERROR;
        }

	if (argc == 4 && !strcmp(argv[1],"get")) {
                DWORD dwType = REG_SZ;
                if (ERROR_SUCCESS != RegQueryValueEx(hKeyResult, szValueName, 0, &dwType, szOutBuf, &cbOutBuf)) {
                        RegCloseKey(hKeyResult);
                        Tcl_AppendResult(interp, "Could not set value", szValueName, NULL);
                        return TCL_ERROR;       
                }
                Tcl_SetResult(interp, szOutBuf, TCL_STATIC);	
        } else if (argc == 5 && !strcmp(argv[1], "set")) {
                if (ERROR_SUCCESS != RegSetValueEx(hKeyResult, szValueName, 0, REG_SZ, argv[4], strlen(argv[4]))) {
                        RegCloseKey(hKeyResult);
                        Tcl_AppendResult(interp, "Could not set value", szValueName, argv[4], NULL);
                        return TCL_ERROR;
                }
	}
        RegCloseKey(hKeyResult);
        return TCL_OK;
}

int
RegGetValue(HKEY* key, char *subkey, char *value, char *dst, int dlen)
{
        HKEY lkey;      
        LONG r;
        LONG len;
        DWORD type;
 
        r = RegOpenKeyEx(*key, subkey, 0, KEY_READ, &lkey);
 
        if (ERROR_SUCCESS == r) {
                r = RegQueryValueEx(lkey, value, 0, &type, NULL, &len);
                if (ERROR_SUCCESS == r && len <= dlen && type == REG_SZ) {
                        type = REG_SZ;
                        r = RegQueryValueEx(lkey, value, 0, &type, dst, &len);
                } else {
                        SetLastError(r);
                        perror("");
                }
        } else {
                SetLastError(r);
                perror("");
                return FALSE;
        }
        RegCloseKey(lkey);
        return TRUE;
}

int 
usleep(unsigned int usec)
{
        DWORD dur = usec/1000;
        if (dur) Sleep(dur);
        return 0;
}

#define MAX_VERSION_STRING_LEN 64
const char*
w32_make_version_info(char *szRatVer) 
{
        static char szVer[MAX_VERSION_STRING_LEN];
        OSVERSIONINFO oi;

        /* This could be better */

        oi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&oi);

        switch(oi.dwPlatformId) {
        case VER_PLATFORM_WIN32_NT:
                sprintf(szVer, "%s Windows NT (%d.%d)", szRatVer, oi.dwMajorVersion, oi.dwMinorVersion);
                break;
        case VER_PLATFORM_WIN32_WINDOWS:
                if (oi.dwMajorVersion > 4 ||
                   (oi.dwMajorVersion == 4 && oi.dwMinorVersion > 0)) {
                        sprintf(szVer, "%s Windows 98 (%d.%d)", szRatVer, oi.dwMajorVersion, oi.dwMinorVersion);
                } else {
                        sprintf(szVer, "%s Windows 95 (%d.%d)", szRatVer, oi.dwMajorVersion, oi.dwMinorVersion);
                }
                break;
        default:
                /* No idea what this could be :-)*/
                sprintf(szVer, "%s Windows (Unknown... beta tester, huh?)", szRatVer);
        }

        return szVer;
}

#endif /* WIN32 */
