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

/* A lot of includes here that should all probably be in files where they   */
/* are used.  If anyone ever has the time to reverse the includes into      */
/* the files where they are actually used, there would be a couple of pints */
/* in it.                                                                   */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>  
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>  
#endif 

#ifdef HAVE_SYS_SOCK_IO_H
#include <sys/sockio.h>
#endif

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>

#ifdef GETTOD_NOT_DECLARED
int gettimeofday(struct timeval *tp, void * );
#endif

#ifdef KILL_NOT_DECLARED
int kill(pid_t pid, int sig);
#endif

extern int h_errno;

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

#ifdef Solaris
#define NEED_INET_ATON
#define NEED_INET_PTON
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/audioio.h>
#include <multimedia/audio_encode.h>
#include <multimedia/audio_hdr.h>

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
#include <audio.h>
#include "usleep.h"
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
