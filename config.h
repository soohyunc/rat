/*
 *  config.h
 *
 *  Machine/operating-system specific definitions and includes for RAT.
 *  
 *  $Revision$
 *  $Date$
 *
 * Copyright (c) 1995,1996 University College London
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

#ifndef _RAT_CONFIG_H_
#define _RAT_CONFIG_H_

#include "assert.h"

#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>   /* abs() */
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else /* WIN32 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
extern int h_errno;
#if !defined(HPUX) && !defined(Linux) && !defined(__FreeBSD__)
#include <stropts.h>
#include <sys/filio.h>  
#endif /* HPUX */
#endif /* WIN32 */

#ifndef TRUE
#define FALSE	0
#define	TRUE	1
#endif /* TRUE */

#define USERNAMELEN	8

#ifndef WIN32
#define max(a, b)	(((a) > (b))? (a): (b))
#define min(a, b)	(((a) < (b))? (a): (b))
#endif

#ifdef FreeBSD
#define OSNAME "FreeBSD"
#include <unistd.h>
#include <stdlib.h>
#define DIFF_BYTE_ORDER  1
#define AUDIO_SPEAKER    0
#define AUDIO_HEADPHONE  1
#define AUDIO_LINE_OUT   4
#define AUDIO_MICROPHONE 1
#define AUDIO_LINE_IN    2
#define AUDIO_CD         4
#endif /* FreeBSD */

#ifdef SunOS_5
#define OSNAME "Solaris"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/audioio.h>
#include <multimedia/audio_encode.h>
#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tp, void * );
int gethostname(char *name, int namelen);
#ifdef __cplusplus
}
#endif
#endif

#ifdef SunOS_4
#define AUDIO_CD         4
#define OSNAME "SunOS"
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 7
#include <ctype.h>
#include <sun/audioio.h>
#include <multimedia/ulaw2linear.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <memory.h>
int 	gethostname(char *name, int namelen);
int 	gettimeofday(struct timeval *tp, struct timezone *tzp);
double	drand48();
void 	srand48(long seedval);
long	lrand48();
int	setsockopt(int s, int level, int optname, const char *optval, int optlen);
void	perror();
int	printf(char *format, ...);
int	fprintf(FILE *stream, char *format, ...);
int	fclose(FILE *stream);
int	fread(void *ptr, int size, int nitems, FILE *stream);
int	fwrite(void *ptr, int size, int nitems, FILE *stream);
int	fflush(FILE *stream);
void	bzero(char *b, int length);
void	bcopy(char *b1, char *b2, int length);
int	connect(int s, struct sockaddr *name, int namelen);
int	select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int	bind(int s, struct sockaddr *name, int namelen);
int	socket(int domain, int type, int protocol);
int	sendto(int s, char *msg, int len, int flags, struct sockaddr *to, int tolen);
int	writev(int fd, struct iovec *iov, int iovcnt);
int	recvfrom(int s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
int	close(int fd);
int	ioctl(int fd, int request, caddr_t arg);
int 	sscanf(char *s, char *format, ...);
time_t	time(time_t *tloc);
int	strcasecmp(char *s1, char *s2);
long	strtol(char *str, char **ptr, int base);
int	toupper(int c);
#define	memmove(dst, src, len)	bcopy((char *) src, (char *) dst, len)
#endif

#ifdef IRIX
#define OSNAME "IRIX"
#include <bstring.h>     /* Needed for FDZERO on IRIX only */
#include <audio.h>
#define AUDIO_SPEAKER    0
#define AUDIO_HEADPHONE  1
#define AUDIO_LINE_OUT   4
#define AUDIO_MICROPHONE 1
#define AUDIO_LINE_IN    2
#define AUDIO_CD         4
int gethostname(char *name, int namelen);
#endif

#ifdef HPUX
#define OSNAME "HPUX"
#include <unistd.h>
#include <sys/audio.h>
#define AUDIO_SPEAKER    AUDIO_OUT_SPEAKER
#define AUDIO_HEADPHONE  AUDIO_OUT_HEADPHONE
#define AUDIO_LINE_OUT   AUDIO_OUT_LINE
#define AUDIO_MICROPHONE AUDIO_IN_MIKE
#define AUDIO_LINE_IN    AUDIO_IN_LINE
int gethostname(char *hostname, size_t size);
#endif

#ifdef Linux
#define OSNAME           "Linux"
#define DIFF_BYTE_ORDER  1
#define AUDIO_SPEAKER    0
#define AUDIO_HEADPHONE  1
#define AUDIO_LINE_OUT   4
#define AUDIO_MICROPHONE 1
#define AUDIO_LINE_IN    2
#define AUDIO_CD         4
#include <sys/stat.h>
#include <fcntl.h>
void *memcpy(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);
#endif /* Linux */

#ifdef WIN32
#define OSNAME "WIN32"
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
int strcasecmp(const char *, const char *);
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
time_t time(time_t *);

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

#endif /* WIN32 */

#endif /* _RAT_CONFIG_H_ */
