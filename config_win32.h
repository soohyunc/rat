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
#ifndef MUSICA_IPV6
#include <winsock2.h>
#endif

#ifdef HAVE_IPv6
#ifdef MUSICA_IPV6
#include <winsock6.h>
#else
#include <ws2ip6.h>
#endif

#endif
#ifndef MUSICA_IPV6
#include <ws2tcpip.h>
#endif

#include <mmreg.h>
#include <msacm.h>
#include <mmsystem.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <time.h>

typedef int		ttl_t;
typedef u_int		fd_t;
typedef unsigned char	byte;

/*
 * the definitions below are valid for 32-bit architectures and will have to
 * be adjusted for 16- or 64-bit architectures
 */
typedef u_char		u_int8;
typedef u_short		u_int16;
typedef u_long		u_int32;
typedef char		int8;
typedef short		int16;
typedef long		int32;
typedef __int64		int64;
typedef unsigned long	in_addr_t;

#ifndef TRUE
#define FALSE	0
#define	TRUE	1
#endif /* TRUE */

#define USERNAMELEN	8

#define DIFF_BYTE_ORDER	1
#define NEED_INET_ATON

#include <time.h>		/* For clock_t */
#include "audio_types.h"
#include "usleep.h"

#define srand48	lbl_srandom
#define lrand48 lbl_random

#ifdef NDEBUG
#define assert(x) if ((x) == 0) fprintf(stderr, "%s:%u: failed assertion\n", __FILE__, __LINE__)
#else
#include <assert.h>
#endif

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

typedef DWORD pid_t;
typedef DWORD uid_t;
typedef DWORD gid_t;
    
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
unsigned long gethostid(void);
int getuid(void);
int getgid(void);
int getpid(void);
int nice(int);
int usleep(unsigned int);
time_t time(time_t *);

const char * w32_make_version_info(char * rat_verion);

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
