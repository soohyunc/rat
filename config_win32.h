/*
 *  config-win32.h
 *
 *  Windows specific definitions and includes for RAT.
 *  
 *  $Revision$
 *  $Date$
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef WIN32
#ifndef _CONFIG_WIN32_H
#define _CONFIG_WIN32_H

#include <malloc.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>   /* abs() */
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmreg.h>
#include <msacm.h>
#include <mmsystem.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <time.h>
#include <tcl.h>
#include <tk.h>

typedef u_int	fd_t;
typedef unsigned char	byte;

/*
 * the definitions below are valid for 32-bit architectures and will have to
 * be adjusted for 16- or 64-bit architectures
 */
typedef u_char  u_int8;
typedef u_short u_int16;
typedef u_long  u_int32;
typedef char	int8;
typedef short	int16;
typedef long	int32;
typedef __int64   int64;

typedef short  sample;


#ifndef TRUE
#define FALSE	0
#define	TRUE	1
#endif /* TRUE */

#define USERNAMELEN	8

#define DIFF_BYTE_ORDER	1

#include <time.h>		/* For clock_t */

#define inline
#define __inline     

#define AUDIO_MICROPHONE	1
#define AUDIO_LINE_IN		2
#define AUDIO_CD            4
#define AUDIO_SPEAKER		0
#define AUDIO_HEADPHONE		1
#define AUDIO_LINE_OUT		4

#define srand48	srand
#define lrand48 rand

#define IN_CLASSD(i)	(((long)(i) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST(i)	IN_CLASSD(i)

typedef char	*caddr_t;
typedef int	ssize_t;

typedef struct iovec {
	caddr_t	iov_base;
	ssize_t	iov_len;
} iovec_t;

struct msghdr {
	caddr_t		msg_name;
	int		msg_namelen;
	struct iovec	*msg_iov;
	int		msg_iovlen;
	caddr_t		msg_accrights;
	int		msg_accrightslen;
};

#define MAXHOSTNAMELEN	256

#define _SYS_NMLN	9
struct utsname {
	char sysname[_SYS_NMLN];
	char nodename[_SYS_NMLN];
	char release[_SYS_NMLN];
	char version[_SYS_NMLN];
	char machine[_SYS_NMLN];
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
    
#if defined(__cplusplus)
extern "C" {
#endif

int uname(struct utsname *);
int getopt(int, char * const *, const char *);
int strncasecmp(const char *, const char*, int len);
int srandom(int);
int random(void);
double drand48();
int gettimeofday(struct timeval *p, struct timezone *z);
int gethostid(void);
int getuid(void);
int getgid(void);
int getpid(void);
int nice(int);
int usleep(unsigned int);
time_t time(time_t *);

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

int  RegGetValue(HKEY *, char *, char*, char*, int);
void ShowMessage(int level, char *msg);

#define bcopy(from,to,len) memcpy(to,from,len)

#if defined(__cplusplus)
}
#endif

#define ECONNREFUSED	WSAECONNREFUSED
#define ENETUNREACH	WSAENETUNREACH
#define EHOSTUNREACH	WSAEHOSTUNREACH
#define EWOULDBLOCK	WSAEWOULDBLOCK

#define M_PI		3.14159265358979323846

#endif 
#endif
