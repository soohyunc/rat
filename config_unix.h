/*
 *  config-unix.h
 *
 *  Unix specific definitions and includes for RAT.
 *  
 *  $Revision$
 *  $Date$
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef WIN32
#ifndef _CONFIG_UNIX_H
#define _CONFIG_UNIX_H

/* This is horrible */
#include "../common/config.h"

#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <pwd.h>
#include <signal.h>
#include <ctype.h>

#ifndef __FreeBSD__
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>   /* abs() */
#include <string.h>

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
#include <net/if.h>

typedef unsigned char	byte;

typedef char	ttl_t;
typedef int	fd_t;

#ifndef TRUE
#define FALSE	0
#define	TRUE	1
#endif /* TRUE */

#define USERNAMELEN	8

#define max(a, b)	(((a) > (b))? (a): (b))
#define min(a, b)	(((a) < (b))? (a): (b))

#ifdef NDEBUG
#define assert(x) if ((x) == 0) fprintf(stderr, "%s:%u: failed assertion\n", __FILE__, __LINE__)
#else
#include <assert.h>
#endif

#ifdef __NetBSD__
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/audioio.h>
#endif

#ifdef FreeBSD
#include <unistd.h>
#include <stdlib.h>
#define DIFF_BYTE_ORDER  1
#include <machine/pcaudioio.h>
#include <machine/soundcard.h>
#endif /* FreeBSD */

#ifdef Solaris
#define NEED_INET_ATON
#define NEED_INET_PTON
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/audioio.h>
#include <multimedia/audio_encode.h>
#include <multimedia/audio_hdr.h>
#include <sys/sockio.h>
#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tp, void * );
#ifdef __cplusplus
}
#endif
#endif

#ifdef SunOS
#define NEED_INET_PTON
#define AUDIO_CD         4
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
#define NEED_INET_PTON
#include <bstring.h>     /* Needed for FDZERO on IRIX only */
#include <audio.h>
#include "usleep.h"
#endif

#ifdef HPUX
#define NEED_INET_PTON
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
#define DIFF_BYTE_ORDER  1
#include <sys/soundcard.h>
#include <sys/stat.h>
#include <fcntl.h>
void *memcpy(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);
#endif /* Linux */

#endif 

#include "audio_types.h"

#endif
