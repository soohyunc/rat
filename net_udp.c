#include "assert.h"
#include "config_unix.h"
#include "config_win32.h"
#include "net_udp.h"
#include "util.h"

#define IPv4	4
#define IPv6	6

struct _socket_udp {
	int	 mode;	/* IPv4 or IPv6 */
	char	*addr;
	u_int16	 port;
	u_int8	 ttl;
	u_int32	 addr4;
	fd_t	 fd;
};

static void
socket_error(char *msg)
{
#ifdef WIN32
	int e = WSAGetLastError();
	printf("ERROR: %s (%d)\n", msg, e);
#else
	perror(msg);
#endif
	abort();
}

/*****************************************************************************/
/* IPv4 specific functions...                                                */
/*****************************************************************************/

static socket_udp *udp_init4(char *addr, int port, int ttl)
{
	int                 reuse = 1;
	struct sockaddr_in  s_in;
	socket_udp         *s = (socket_udp *) malloc(sizeof(socket_udp));
	s->mode  = IPv4;
	s->addr  = addr;
	s->port  = port;
	s->ttl   = ttl;
	s->addr4 = inet_addr(addr);
	if (s->addr4 == 0xffffffff) {
		struct hostent *h = gethostbyname(addr);
		if (h == NULL) {
			return NULL;
		}
		memcpy(&(s->addr4), h->h_addr_list[0], sizeof(s->addr4));
	}
	s->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (s->fd < 0) {
		socket_error("socket");
		abort();
	}
	if (setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse)) != 0) {
		socket_error("setsockopt SO_REUSEADDR");
		abort();
	}
#ifdef SO_REUSEPORT
	if (setsockopt(s->fd, SOL_SOCKET, SO_REUSEPORT, (char *) &reuse, sizeof(reuse)) != 0) {
		socket_error("setsockopt SO_REUSEPORT");
		abort();
	}
#endif
	s_in.sin_family      = AF_INET;
	s_in.sin_addr.s_addr = htonl(INADDR_ANY);
	s_in.sin_port        = htons(port);
	if (bind(s->fd, (struct sockaddr *) &s_in, sizeof(s_in)) != 0) {
		socket_error("bind");
		abort();
	}
	if (IN_MULTICAST(ntohl(s->addr4))) {
		char            loop = 1;
		struct ip_mreq  imr;

		imr.imr_multiaddr.s_addr = s->addr4;
		imr.imr_interface.s_addr = htonl(INADDR_ANY);

		if (setsockopt(s->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq)) != 0) {
			socket_error("setsockopt IP_ADD_MEMBERSHIP");
			abort();
		}
		if (setsockopt(s->fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) != 0) {
			socket_error("setsockopt IP_MULTICAST_LOOP");
			abort();
		}
		if (setsockopt(s->fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &s->ttl, sizeof(s->ttl)) != 0) {
			socket_error("setsockopt IP_MULTICAST_TTL");
			abort();
		}
	}
	return s;
}

static int udp_send4(socket_udp *s, char *buffer, int buflen)
{
	struct sockaddr_in	s_in;
	int			ret;

	assert(s != NULL);
	assert(s->mode == IPv4);
	assert(buffer != NULL);
	assert(buflen > 0);

	memcpy((char *) &s_in.sin_addr.s_addr, (char *) &(s->addr4), sizeof(s->addr4));
	s_in.sin_family = AF_INET;
	s_in.sin_port   = htons(s->port);
	if ((ret = sendto(s->fd, buffer, buflen, 0, (struct sockaddr *) &s_in, sizeof(s_in))) < 0) {
		socket_error("udp_send4");
	}
	return ret;
}

static int udp_recv4(socket_udp *s, char *buffer, int buflen)
{
	/* Reads data into the buffer, returning the number of bytes read.   */
	/* If no data is available, this returns the value zero immediately. */
	fd_set		rfd;
	struct timeval	t;
	int		len;

	assert(s != NULL);
	assert(s->mode == IPv4);
	assert(buffer != NULL);
	assert(buflen > 0);

	t.tv_sec  = 0;
	t.tv_usec = 0;
	FD_ZERO(&rfd);
	FD_SET(s->fd, &rfd);
	if (select(s->fd + 1, &rfd, NULL, NULL, &t) > 0) {
		len = recvfrom(s->fd, buffer, buflen, 0, 0, 0);
		if (len > 0) {
			return len;
		}
		socket_error("recvfrom");
	}
	return 0;
}

/*****************************************************************************/
/* IPv6 specific functions...                                                */
/*****************************************************************************/

static socket_udp *udp_init6(char *addr, int port, int ttl)
{
	return NULL;
}

static int udp_send6(socket_udp *s, char *buffer, int buflen)
{
	assert(s != NULL);
	assert(s->mode == IPv6);
	assert(buffer != NULL);
	assert(buflen > 0);
	return -1;
}

static int udp_recv6(socket_udp *s, char *buffer, int buflen)
{
	assert(s != NULL);
	assert(s->mode == IPv6);
	assert(buffer != NULL);
	assert(buflen > 0);
	return -1;
}

/*****************************************************************************/
/* Generic functions, which call the appropriate protocol specific routines. */
/*****************************************************************************/

socket_udp *udp_init(char *addr, int port, int ttl)
{
	return udp_init4(addr, port, ttl);
}

int udp_send(socket_udp *s, char *buffer, int buflen)
{
	switch (s->mode) {
		case IPv4 : return udp_send4(s, buffer, buflen);
		case IPv6 : return udp_send6(s, buffer, buflen);
		default   : abort();
	}
}

int udp_recv(socket_udp *s, char *buffer, int buflen)
{
	switch (s->mode) {
		case IPv4 : return udp_recv4(s, buffer, buflen);
		case IPv6 : return udp_recv6(s, buffer, buflen);
		default   : abort();
	}
}

