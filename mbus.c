/*
 * FILE:    mbus.c
 * AUTHORS: Colin Perkins
 * 
 * Copyright (c) 1997,1998 University College London
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
#ifndef   WIN32
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif /* WIN32 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifdef SunOS_5
#include <sys/sockio.h>		/* For SIOCGIFCONF, which according the the Sun manual pages */
#endif				/* is in <net/if.h>, but why should we believe them?         */

#include "assert.h"
#include "rat_types.h"
#include "config.h"
#include "util.h"
#include "net.h"
#include "mbus.h"

#define MBUS_ADDR 	0xe0ffdeef	/* 224.255.222.239 */
#define MBUS_PORT 	47000
#define MBUS_BUF_SIZE	1024
#define MBUS_MAX_ADDR	10
#define MBUS_MAX_PD	10
#define MBUS_MAX_QLEN	50		/* Number of messages we can queue with mbus_qmsg() */
#define MBUS_MAX_IF	16

struct mbus_ack {
	struct mbus_ack	*next;
	struct mbus_ack	*prev;
	char		*srce;
	char		*dest;
	char		*cmnd;
	char		*args;
	int		 seqn;
	struct timeval	 time;	/* Used to determine when to request retransmissions, etc... */
	int		 rtcnt;
	int		 qmsg_size;
	char		*qmsg_cmnd[MBUS_MAX_QLEN];
	char		*qmsg_args[MBUS_MAX_QLEN];
};

struct mbus {
	int		 fd;
	unsigned short	 channel;
	int		 num_addr;
	char		*addr[MBUS_MAX_ADDR];
	char		*parse_buffer[MBUS_MAX_PD];
	int		 parse_depth;
	int		 seqnum;
	struct mbus_ack	*ack_list;
	int		 ack_list_size;
	void (*cmd_handler)(char *src, char *cmd, char *arg, void *dat);
	void (*err_handler)(int seqnum);
	int		 qmsg_size;
	char		*qmsg_cmnd[MBUS_MAX_QLEN];
	char		*qmsg_args[MBUS_MAX_QLEN];
	u_int32	 	 interfaces[MBUS_MAX_IF];
	int		 num_interfaces;
};

static int mbus_addr_match(char *a, char *b)
{
	assert(a != NULL);
	assert(b != NULL);

	while ((*a != '\0') && (*b != '\0')) {
		while (isspace(*a)) a++;
		while (isspace(*b)) b++;
		if (*a == '*') {
			a++;
			if ((*a != '\0') && !isspace(*a)) {
				return FALSE;
			}
			while(!isspace(*b) && (*b != '\0')) b++;
		}
		if (*b == '*') {
			b++;
			if ((*b != '\0') && !isspace(*b)) {
				return FALSE;
			}
			while(!isspace(*a) && (*a != '\0')) a++;
		}
		if (*a != *b) {
			return FALSE;
		}
		a++;
		b++;
	}
	return TRUE;
}

static void mbus_ack_list_check(struct mbus *m) 
{
#ifndef NDEBUG
	struct mbus_ack *curr = m->ack_list;
	int		 i    = 0, j;

	assert(((m->ack_list == NULL) && (m->ack_list_size == 0)) || ((m->ack_list != NULL) && (m->ack_list_size > 0)));
	while (curr != NULL) {
		if (curr->prev != NULL) assert(curr->prev->next == curr);
		if (curr->next != NULL) assert(curr->next->prev == curr);
		if (curr->prev == NULL) assert(curr == m->ack_list);
                assert(curr->qmsg_size < MBUS_MAX_QLEN);
                for(j = 0; j < curr->qmsg_size; j++) {
                        assert(curr->qmsg_cmnd[j] != NULL);
                        assert(curr->qmsg_args[j] != NULL);
                }
		curr = curr->next;
		i++;
	}
	assert(i == m->ack_list_size);
	assert(m->ack_list_size >= 0);
#else 
	UNUSED(m);
#endif
}

static void mbus_ack_list_insert(struct mbus *m, char *srce, char *dest, const char *cmnd, const char *args, int seqnum)
{
	struct mbus_ack	*curr = (struct mbus_ack *) xmalloc(sizeof(struct mbus_ack));
	int		 i;

	assert(srce != NULL);
	assert(dest != NULL);
	assert(cmnd != NULL);
	assert(args != NULL);

	mbus_ack_list_check(m);

	mbus_parse_init(m, xstrdup(dest));
	mbus_parse_lst(m, &(curr->dest));
	mbus_parse_done(m);

	curr->next = m->ack_list;
	curr->prev = NULL;
	curr->srce = xstrdup(srce);
	curr->cmnd = xstrdup(cmnd);
	curr->args = xstrdup(args);
	curr->seqn = seqnum;
	curr->rtcnt= 0;
	gettimeofday(&(curr->time), NULL);
	curr->qmsg_size = m->qmsg_size;
	for (i = 0; i < m->qmsg_size; i++) {
		curr->qmsg_cmnd[i] = xstrdup(m->qmsg_cmnd[i]);
		curr->qmsg_args[i] = xstrdup(m->qmsg_args[i]);
	}

	if (m->ack_list != NULL) {
		m->ack_list->prev = curr;
	}
	m->ack_list = curr;
	m->ack_list_size++;
	mbus_ack_list_check(m);
}

static void mbus_ack_list_remove(struct mbus *m, char *srce, char *dest, int seqnum)
{
	/* This would be much more efficient if it scanned from last to first, since     */
	/* we're most likely to receive ACKs in LIFO order, and this assumes FIFO...     */
	/* We hope that the number of outstanding ACKs is small, so this doesn't matter. */
	struct mbus_ack	*curr = m->ack_list;
	int		 i;

	mbus_ack_list_check(m);
	while (curr != NULL) {
		if (mbus_addr_match(curr->srce, dest) && mbus_addr_match(curr->dest, srce) && (curr->seqn == seqnum)) {
			assert(m->ack_list_size > 0);
			m->ack_list_size--;
			xfree(curr->srce);
			xfree(curr->dest);
			xfree(curr->cmnd);
			xfree(curr->args);
			for (i=0; i < curr->qmsg_size; i++) {
				xfree(curr->qmsg_cmnd[i]);
				xfree(curr->qmsg_args[i]);
			}
			if (curr->next != NULL) curr->next->prev = curr->prev;
			if (curr->prev != NULL) curr->prev->next = curr->next;
			if (m->ack_list == curr) {
				assert(curr->prev == NULL);
				if (curr->next != NULL) assert(curr->next->prev == NULL);
				m->ack_list = curr->next;
			}
			xfree(curr);
			mbus_ack_list_check(m);
			return;
		}
		curr = curr->next;
	}
	mbus_ack_list_check(m);
	/* If we get here, it's an ACK for something that's not in the ACK
	 * list. That's not necessarily a problem, could just be a duplicate
	 * ACK for a retransmission... We ignore it for now...
	 */
	debug_msg("Got an ACK for something not in our ACK list...\n");
}

int mbus_waiting_acks(struct mbus *m)
{
	/* Returns TRUE if we are waiting for ACKs for any reliable
	 * messages we sent out.
	 */
	mbus_ack_list_check(m);
	if (m->ack_list != NULL) debug_msg("Waiting for ACKs on mbus 0x%p...\n", m);
	return (m->ack_list != NULL);
}

static void mbus_send_ack(struct mbus *m, char *dest, int seqnum)
{
	char			buffer[96];
	struct sockaddr_in	saddr;
	u_long			addr = MBUS_ADDR;

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((short)(MBUS_PORT+m->channel));
	sprintf(buffer, "mbus/1.0 %d U (%s) (%s) (%d)\n", ++m->seqnum, m->addr[0], dest, seqnum);
        assert(strlen(buffer) < 96);
        if ((sendto(m->fd, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("mbus_send: sendto");
	}
}

static void resend(struct mbus *m, struct mbus_ack *curr) 
{
	struct sockaddr_in	 saddr;
	u_long			 addr = MBUS_ADDR;
	char			*b, *bp;
	int			 i;

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((short)(MBUS_PORT+m->channel));
	b                = (char *) xmalloc(MBUS_BUF_SIZE);
	bp		 = b;
	sprintf(bp, "mbus/1.0 %6d R (%s) (%s) ()\n", curr->seqn, curr->srce, curr->dest);
	bp += strlen(curr->srce) + strlen(curr->dest) + 28;
	for (i = 0; i < curr->qmsg_size; i++) {
		sprintf(bp, "%s (%s)\n", curr->qmsg_cmnd[i], curr->qmsg_args[i]);
		bp += strlen(curr->qmsg_cmnd[i]) + strlen(curr->qmsg_args[i]) + 4;
		xfree(curr->qmsg_cmnd[i]); curr->qmsg_cmnd[i] = NULL;
		xfree(curr->qmsg_args[i]); curr->qmsg_args[i] = NULL;
	}
	sprintf(bp, "%s (%s)\n", curr->cmnd, curr->args);
	bp += strlen(curr->cmnd) + strlen(curr->args) + 4;

	if ((sendto(m->fd, b, strlen(b), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("mbus_send: sendto");
	}
	curr->rtcnt++;
	xfree(b);
}

void mbus_retransmit(struct mbus *m)
{
	struct mbus_ack	 	*curr = m->ack_list;
	struct timeval	 	 time;
	long		 	 diff;

	mbus_ack_list_check(m);

	gettimeofday(&time, NULL);

	while (curr != NULL) {
		/* diff is time in milliseconds that the message has been awaiting an ACK */
		diff = ((time.tv_sec * 1000) + (time.tv_usec / 1000)) - ((curr->time.tv_sec * 1000) + (curr->time.tv_usec / 1000));
		if (diff > 1000) {
			debug_msg("Reliable mbus message failed!\n");
			debug_msg("   mbus/1.0 %d R (%s) %s ()\n", curr->seqn, curr->srce, curr->dest);
			debug_msg("   %s (%s)\n", curr->cmnd, curr->args);
			if (m->err_handler == NULL) {
				abort();
			}
			m->err_handler(curr->seqn);
			return;
		} 
		/* Note: We only send one retransmission each time, to avoid
		 * overflowing the receiver with a burst of requests...
		 */
		if ((diff > 750) && (curr->rtcnt == 2)) {
			resend(m, curr);
			return;
		} 
		if ((diff > 500) && (curr->rtcnt == 1)) {
			resend(m, curr);
			return;
		} 
		if ((diff > 250) && (curr->rtcnt == 0)) {
			resend(m, curr);
			return;
		}
		curr = curr->next;
	}
}

static int mbus_socket_init(unsigned short channel)
{
	struct sockaddr_in sinme;
	struct ip_mreq     imr;
	char               ttl   =  0;
	int                reuse =  1;
	char               loop  =  1;
	int                fd    = -1;
#ifndef SunOS_4
	int		   rbuf  = 65535;
#endif

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("mbus: socket");
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse)) < 0) {
		perror("mbus: setsockopt SO_REUSEADDR");
		return -1;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char *) &reuse, sizeof(reuse)) < 0) {
		perror("mbus: setsockopt SO_REUSEPORT");
		return -1;
	}
#endif
#ifndef SunOS_4
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &rbuf, sizeof(rbuf)) < 0) {
		perror("mbus: setsockopt SO_RCVBUF");
		return -1;
	}
#endif

	sinme.sin_family      = AF_INET;
	sinme.sin_addr.s_addr = htonl(INADDR_ANY);
	sinme.sin_port        = htons((short)(MBUS_PORT+channel));
	if (bind(fd, (struct sockaddr *) & sinme, sizeof(sinme)) < 0) {
		perror("mbus: bind");
		return -1;
	}

	imr.imr_multiaddr.s_addr = MBUS_ADDR;
	imr.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq)) < 0) {
		perror("mbus: setsockopt IP_ADD_MEMBERSHIP");
		return -1;
	}
#ifndef WIN32
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		perror("mbus: setsockopt IP_MULTICAST_LOOP");
		return -1;
	}

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		perror("mbus: setsockopt IP_MULTICAST_TTL");
		return -1;
	}
#endif

	return fd;
}

struct mbus *mbus_init(unsigned short channel, 
                       void  (*cmd_handler)(char *src, char *cmd, char *arg, void *dat), 
		       void  (*err_handler)(int seqnum))
{
	struct mbus	*m;
	int		 i;
#if !defined(WIN32) && !defined(FreeBSD)
	struct ifreq	 ifbuf[32];
	struct ifreq	*ifp;
	struct ifconf	 ifc;
#endif
	m = (struct mbus *) xmalloc(sizeof(struct mbus));
	m->fd           = mbus_socket_init(channel);
	assert(m->fd != -1);
	m->channel	    = channel;
	m->seqnum       = 0;
	m->ack_list     = NULL;
	m->ack_list_size= 0;
	m->cmd_handler  = cmd_handler;
	m->err_handler	= err_handler;
	m->num_addr     = 0;
	m->parse_depth  = 0;
	m->qmsg_size    = 0;
	for (i = 0; i < MBUS_MAX_ADDR; i++) m->addr[i]         = NULL;
	for (i = 0; i < MBUS_MAX_PD;   i++) m->parse_buffer[i] = NULL;
	for (i = 0; i < MBUS_MAX_QLEN; i++) m->qmsg_cmnd[i]    = NULL;
	for (i = 0; i < MBUS_MAX_QLEN; i++) m->qmsg_args[i]    = NULL;

	/* Determine the network interfaces on this host... */
#if !defined(WIN32) && !defined(FreeBSD)
	ifc.ifc_buf = (char *)ifbuf;
	ifc.ifc_len = sizeof(ifbuf);
	if (ioctl(m->fd, SIOCGIFCONF, (char *) &ifc) < 0) {
		debug_msg("Can't find interface configuration...\n");
		return m;
	}

	ifp = ifc.ifc_req;
	m->num_interfaces = 0;
	while (ifp < (struct ifreq *) ((char *) ifbuf + ifc.ifc_len)) {
		m->interfaces[m->num_interfaces++] = ((struct sockaddr_in *) &((ifp++)->ifr_addr))->sin_addr.s_addr;
	}
#else
	m->num_interfaces = 0;
#endif
	return m;
}

void mbus_addr(struct mbus *m, char *addr)
{
	assert(m->num_addr < MBUS_MAX_ADDR);
	mbus_parse_init(m, strdup(addr));
	if (mbus_parse_lst(m, &(m->addr[m->num_addr]))) {
		m->num_addr++;
	}
	mbus_parse_done(m);
}

int mbus_fd(struct mbus *m)
{
	return m->fd;
}

void mbus_qmsg(struct mbus *m, const char *cmnd, const char *args)
{
	/* Queue a message for sending. The next call to mbus_send() sends this message
	 * piggybacked in that packet. The destination address and reliability for the
	 * messages queued in this way is specified by the call to mbus_send().
	 */
	assert(cmnd != NULL);
	assert(args != NULL);
	assert((m->qmsg_size < MBUS_MAX_QLEN-1) && (m->qmsg_size >= 0));
	m->qmsg_cmnd[m->qmsg_size] = xstrdup(cmnd);
	m->qmsg_args[m->qmsg_size] = xstrdup(args);
	m->qmsg_size++;
}

int mbus_send(struct mbus *m, char *dest, const char *cmnd, const char *args, int reliable)
{
	char			*buffer, *bufp;
	struct sockaddr_in	 saddr;
	u_long			 addr = MBUS_ADDR;
	int			 i;

	assert(dest != NULL);
	assert(cmnd != NULL);
	assert(args != NULL);
	assert(strlen(cmnd) != 0);

	m->seqnum++;

	if (reliable) {
		mbus_ack_list_insert(m, m->addr[0], dest, cmnd, args, m->seqnum);
	}

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons((short)(MBUS_PORT+m->channel));
	buffer           = (char *) xmalloc(MBUS_BUF_SIZE);
	bufp		 = buffer;

	sprintf(bufp, "mbus/1.0 %6d %c (%s) %s ()\n", m->seqnum, reliable?'R':'U', m->addr[0], dest);
	bufp += strlen(m->addr[0]) + strlen(dest) + 25;
	for (i = 0; i < m->qmsg_size; i++) {
		sprintf(bufp, "%s (%s)\n", m->qmsg_cmnd[i], m->qmsg_args[i]);
		bufp += strlen(m->qmsg_cmnd[i]) + strlen(m->qmsg_args[i]) + 4;
		xfree(m->qmsg_cmnd[i]); m->qmsg_cmnd[i] = NULL;
		xfree(m->qmsg_args[i]); m->qmsg_args[i] = NULL;
	}
	m->qmsg_size -= i;
	assert(m->qmsg_size == 0);
	sprintf(bufp, "%s (%s)\n", cmnd, args);
	bufp += strlen(cmnd) + strlen(args) + 4;

	assert((int) strlen(buffer) == (bufp - buffer));
	if ((sendto(m->fd, buffer, bufp - buffer, 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("mbus_send: sendto");
	}
	xfree(buffer);
	return m->seqnum;
}

void mbus_parse_init(struct mbus *m, char *str)
{
	assert(m->parse_depth < (MBUS_MAX_PD - 1));
	m->parse_buffer[++m->parse_depth] = str;
}

void mbus_parse_done(struct mbus *m)
{
	m->parse_depth--;
	assert(m->parse_depth >= 0);
}

int mbus_parse_lst(struct mbus *m, char **l)
{
	int instr = FALSE;
	int inlst = FALSE;

	*l = m->parse_buffer[m->parse_depth];
        while (isspace(*m->parse_buffer[m->parse_depth])) {
                m->parse_buffer[m->parse_depth]++;
        }
	if (*m->parse_buffer[m->parse_depth] != '(') {
		return FALSE;
	}
	*(m->parse_buffer[m->parse_depth]) = ' ';
	while (*m->parse_buffer[m->parse_depth] != '\0') {
		if ((*m->parse_buffer[m->parse_depth] == '"') && (*(m->parse_buffer[m->parse_depth]-1) != '\\')) {
			instr = !instr;
		}
		if ((*m->parse_buffer[m->parse_depth] == '(') && (*(m->parse_buffer[m->parse_depth]-1) != '\\') && !instr) {
			inlst = !inlst;
		}
		if ((*m->parse_buffer[m->parse_depth] == ')') && (*(m->parse_buffer[m->parse_depth]-1) != '\\') && !instr) {
			if (inlst) {
				inlst = !inlst;
			} else {
				*m->parse_buffer[m->parse_depth] = '\0';
				m->parse_buffer[m->parse_depth]++;
				return TRUE;
			}
		}
		m->parse_buffer[m->parse_depth]++;
	}
	return FALSE;
}

int mbus_parse_str(struct mbus *m, char **s)
{
        while (isspace(*m->parse_buffer[m->parse_depth])) {
                m->parse_buffer[m->parse_depth]++;
        }
	if (*m->parse_buffer[m->parse_depth] != '"') {
		return FALSE;
	}
	*s = m->parse_buffer[m->parse_depth]++;
	while (*m->parse_buffer[m->parse_depth] != '\0') {
		if ((*m->parse_buffer[m->parse_depth] == '"') && (*(m->parse_buffer[m->parse_depth]-1) != '\\')) {
			m->parse_buffer[m->parse_depth]++;
			*m->parse_buffer[m->parse_depth] = '\0';
			m->parse_buffer[m->parse_depth]++;
			return TRUE;
		}
		m->parse_buffer[m->parse_depth]++;
	}
	return FALSE;
}

static int mbus_parse_sym(struct mbus *m, char **s)
{
        while (isspace(*m->parse_buffer[m->parse_depth])) {
                m->parse_buffer[m->parse_depth]++;
        }
	if (!isalpha(*m->parse_buffer[m->parse_depth])) {
		return FALSE;
	}
	*s = m->parse_buffer[m->parse_depth]++;
	while (!isspace(*m->parse_buffer[m->parse_depth]) && (*m->parse_buffer[m->parse_depth] != '\0')) {
		m->parse_buffer[m->parse_depth]++;
	}
	*m->parse_buffer[m->parse_depth] = '\0';
	m->parse_buffer[m->parse_depth]++;
	return TRUE;
}

int mbus_parse_int(struct mbus *m, int *i)
{
	char	*p;
	*i = strtol(m->parse_buffer[m->parse_depth], &p, 10);

	if (p == m->parse_buffer[m->parse_depth]) {
		return FALSE;
	}
	if (!isspace(*p) && (*p != '\0')) {
		return FALSE;
	}
	m->parse_buffer[m->parse_depth] = p;
	return TRUE;
}

int mbus_parse_flt(struct mbus *m, double *d)
{
	char	*p;
	*d = strtod(m->parse_buffer[m->parse_depth], &p);

	if (p == m->parse_buffer[m->parse_depth]) {
		return FALSE;
	}
	if (!isspace(*p) && (*p != '\0')) {
		return FALSE;
	}
	m->parse_buffer[m->parse_depth] = p;
	return TRUE;
}

char *mbus_decode_str(char *s)
{
	int	l = strlen(s);
	int	i, j;

	/* Check that this an encoded string... */
	assert(s[0]   == '\"');
	assert(s[l-1] == '\"');

	for (i=1,j=0; i < l - 1; i++,j++) {
		if (s[i] == '\\') {
			i++;
		}
		s[j] = s[i];
	}
	s[j] = '\0';
	return s;
}

char *mbus_encode_str(const char *s)
{
	int 	 i, j;
	int	 len = strlen(s);
	char	*buf = (char *) xmalloc((len * 2) + 3);

	for (i = 0, j = 1; i < len; i++,j++) {
		if (s[i] == ' ') {
			buf[j] = '\\';
			buf[j+1] = ' ';
			j++;
		} else if (s[i] == '\"') {
			buf[j] = '\\';
			buf[j+1] = '\"';
			j++;
		} else {
			buf[j] = s[i];
		}
	}
	buf[0]   = '\"';
	buf[j]   = '\"';
	buf[j+1] = '\0';
	return buf;
}

void mbus_recv(struct mbus *m, void *data)
{
	char			*ver, *src, *dst, *ack, *r, *cmd, *param;
	char			 buffer[MBUS_BUF_SIZE];
	int			 buffer_len, seq, i, a;
	struct sockaddr_in	 from;
	int			 fromlen = sizeof(from);
	int			 p;
	int			 match_addr = FALSE;

	memset(buffer, 0, MBUS_BUF_SIZE);
	if ((buffer_len = recvfrom(m->fd, buffer, MBUS_BUF_SIZE, 0, (struct sockaddr *) &from, &fromlen)) <= 0) {
		return;
	}

	/* Security: check that the source address of the packet we just
	 * received belongs to the localhost, else someone can multicast
	 * mbus commands with ttl 127 and cause chaos. Does this prevent
	 * people faking the source address and attacking a single host
	 * though? Probably, because of the rpf check in the mrouters...
	 */

	if (m->num_interfaces > 0) {
		for (p = 0; p < m->num_interfaces; p++) {
			if (m->interfaces[p] == from.sin_addr.s_addr) {
				match_addr = TRUE;
			}
		}
		if (!match_addr) {
			debug_msg("Packet source address does not match local host address!\n");
				return;
		}
	}

	mbus_parse_init(m, buffer);
	/* Parse the header */
	if (!mbus_parse_sym(m, &ver) || (strcmp(ver, "mbus/1.0") != 0)) {
		mbus_parse_done(m);
                debug_msg("Parser failed version: %s\n",ver);
		return;
	}
	if (!mbus_parse_int(m, &seq)) {
		mbus_parse_done(m);
                debug_msg("Parser failed seq: %s\n", seq);
		return;
	}
	if (!mbus_parse_sym(m, &r)) {
		mbus_parse_done(m);
                debug_msg("Parser failed reliable: %s\n", seq);
		return;
	}
	if (!mbus_parse_lst(m, &src)) {
		mbus_parse_done(m);
                debug_msg("Parser failed seq: %s\n", src);
		return;
	}
	if (!mbus_parse_lst(m, &dst)) {
		mbus_parse_done(m);
                debug_msg("Parser failed dst: %s\n", dst);
		return;
	}
	if (!mbus_parse_lst(m, &ack)) {
		mbus_parse_done(m);
                debug_msg("Parser failed ack: %s\n", ack);
		return;
	}
	/* Check if the message was addressed to us... */
	for (i = 0; i < m->num_addr; i++) {
		if (mbus_addr_match(m->addr[i], dst)) {
			/* ...if so, process any ACKs received... */
			mbus_parse_init(m, ack);
			while (mbus_parse_int(m, &a)) {
				mbus_ack_list_remove(m, src, dst, a);
			}
			mbus_parse_done(m);
			/* ...if an ACK was requested, send one... */
			if (strcmp(r, "R") == 0) {
				mbus_send_ack(m, src, seq);
			}
			/* ...and process the commands contained in the message */
			while (mbus_parse_sym(m, &cmd)) {
				if (mbus_parse_lst(m, &param) == FALSE) {
					debug_msg("Unable to parse mbus command paramaters...\n");
					debug_msg("cmd = %s\n", cmd);
					debug_msg("arg = %s\n", param);
					break;
				}
				m->cmd_handler(src, cmd, param, data);
			}
		}
	}
	mbus_parse_done(m);
}

