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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include "assert.h"
#include "config.h"
#include "util.h"
#include "mbus.h"

#define MBUS_ADDR 	0xe0ffdeef	/* 224.255.222.239 */
#define MBUS_PORT 	47000
#define MBUS_BUF_SIZE	1024
#define MBUS_MAX_ADDR	10
#define MBUS_MAX_PD	10

struct mbus_ack {
	struct mbus_ack	*next;
	struct mbus_ack	*prev;
	char		*srce;
	char		*dest;
	char		*cmnd;
	char		*args;
	int		 seqn;
	struct timeval	 time;	/* Used to determine when to request retransmissions, etc... */
};

struct mbus {
	int		 fd;
	int		 num_addr;
	char		*addr[MBUS_MAX_ADDR];
	char		*parse_buffer[MBUS_MAX_PD];
	int		 parse_depth;
	int		 seqnum;
	struct mbus_ack	*ack_list;
	void (*handler)(char *, char *, char *, void *);
};

static int mbus_addr_match(struct mbus *m, char *a, char *b)
{
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

static void mbus_ack_list_insert(struct mbus *m, char *srce, char *dest, char *cmnd, char *args, int seqnum)
{
	struct mbus_ack	*curr = (struct mbus_ack *) xmalloc(sizeof(struct mbus_ack));

	assert(srce != NULL);
	assert(dest != NULL);
	assert(cmnd != NULL);
	assert(args != NULL);

	mbus_parse_init(m, xstrdup(dest));
	mbus_parse_lst(m, &(curr->dest));
	mbus_parse_done(m);

	curr->next = m->ack_list;
	curr->prev = NULL;
	curr->srce = xstrdup(srce);
	curr->cmnd = xstrdup(cmnd);
	curr->args = xstrdup(args);
	curr->seqn = seqnum;
	gettimeofday(&(curr->time), NULL);

	if (m->ack_list != NULL) {
		m->ack_list->prev = curr;
	}
	m->ack_list = curr;
}

static void mbus_ack_list_remove(struct mbus *m, char *srce, char *dest, int seqnum)
{
	/* This would be much more efficient if it scanned from last to first, since     */
	/* we're most likely to receive ACKs in LIFO order, and this assumes FIFO...     */
	/* We hope that the number of outstanding ACKs is small, so this doesn't matter. */
	struct mbus_ack	*curr = m->ack_list;

	while (curr != NULL) {
		if (mbus_addr_match(m, curr->srce, dest) && mbus_addr_match(m, curr->dest, srce) && (curr->seqn == seqnum)) {
			xfree(curr->srce);
			xfree(curr->dest);
			xfree(curr->cmnd);
			xfree(curr->args);
			if (curr->next != NULL) curr->next->prev = curr->prev;
			if (curr->prev != NULL) curr->prev->next = curr->next;
			if (m->ack_list == curr) m->ack_list = curr->next;
			xfree(curr);
			return;
		}
		curr = curr->next;
	}
	/* If we get here, it's an ACK for something that's not in the ACK
	 * list. That's not necessarily a problem, could just be a duplicate
	 * ACK for a retransmission... We ignore it for now...
	 */
}

static void mbus_send_ack(struct mbus *m, char *dest, int seqnum)
{
	char			buffer[80];
	struct sockaddr_in	saddr;
	u_long			addr = MBUS_ADDR;

	memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(MBUS_PORT);
	sprintf(buffer, "mbus/1.0 %d U (%s) (%s) (%d)\n", ++m->seqnum, m->addr[0], dest, seqnum);
	if ((sendto(m->fd, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
		perror("mbus_send: sendto");
	}
}

void mbus_retransmit(struct mbus *m)
{
	struct mbus_ack	 	*curr = m->ack_list;
	struct timeval	 	 time;
	long		 	 diff;
	char			*b;
	struct sockaddr_in	 saddr;
	u_long			 addr = MBUS_ADDR;

	gettimeofday(&time, NULL);

	while (curr != NULL) {
		/* diff is time in milliseconds that the message has been awaiting an ACK */
		diff = ((time.tv_sec * 1000) + (time.tv_usec / 1000)) - ((curr->time.tv_sec * 1000) + (curr->time.tv_usec / 1000));
		if (diff > 10000) {
			printf("Reliable mbus message failed! (wait=%ld)\n", diff);
			printf(">>>\n");
			printf("   mbus/1.0 %d R (%s) %s ()\n   %s (%s)\n", curr->seqn, curr->srce, curr->dest, curr->cmnd, curr->args);
			printf("<<<\n");
			abort();
		}
		if (diff > 2000) {
			memcpy((char *) &saddr.sin_addr.s_addr, (char *) &addr, sizeof(addr));
			saddr.sin_family = AF_INET;
			saddr.sin_port   = htons(MBUS_PORT);
			b                = xmalloc(strlen(curr->dest)+strlen(curr->cmnd)+strlen(curr->args)+strlen(curr->srce)+80);
			sprintf(b, "mbus/1.0 %d R (%s) %s ()\n%s (%s)\n", curr->seqn, curr->srce, curr->dest, curr->cmnd, curr->args);
			if ((sendto(m->fd, b, strlen(b), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
				perror("mbus_send: sendto");
			}
			xfree(b);
		}
		curr = curr->next;
	}
}

static int mbus_socket_init(void)
{
	struct sockaddr_in sinme;
	struct ip_mreq     imr;
	char               ttl   =  0;
	int                reuse =  1;
	char               loop  =  1;
	int                fd    = -1;

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse)) < 0) {
		perror("setsockopt SO_REUSEADDR");
		return -1;
	}

	sinme.sin_family      = AF_INET;
	sinme.sin_addr.s_addr = htonl(INADDR_ANY);
	sinme.sin_port        = htons(MBUS_PORT);
	if (bind(fd, (struct sockaddr *) & sinme, sizeof(sinme)) < 0) {
		perror("bind");
		return -1;
	}

	imr.imr_multiaddr.s_addr = htonl(MBUS_ADDR);
	imr.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &imr, sizeof(struct ip_mreq)) < 0) {
		perror("setsockopt IP_ADD_MEMBERSHIP");
		return -1;
	}

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		perror("setsockopt IP_MULTICAST_LOOP");
		return -1;
	}

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		perror("setsockopt IP_MULTICAST_TTL");
		return -1;
	}
	return fd;
}

struct mbus *mbus_init(char *addr, void (*handler)(char *, char *, char *, void *))
{
	struct mbus	*m;
	int		 i;

	m = (struct mbus *) xmalloc(sizeof(struct mbus));
	m->fd           = mbus_socket_init();
	m->seqnum       = 0;
	m->ack_list     = NULL;
	m->handler      = handler;
	m->num_addr     = 0;
	m->parse_depth  = 0;
	for (i = 0; i < MBUS_MAX_ADDR; i++) m->addr[i]         = NULL;
	for (i = 0; i <   MBUS_MAX_PD; i++) m->parse_buffer[i] = NULL;
	mbus_addr(m, addr);
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

int mbus_send(struct mbus *m, char *dest, char *cmnd, char *args, int reliable)
{
	char			*buffer;
	struct sockaddr_in	 saddr;
	u_long			 addr = MBUS_ADDR;

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
	saddr.sin_port   = htons(MBUS_PORT);
	buffer           = (char *) xmalloc(strlen(dest) + strlen(cmnd) + strlen(args) + strlen(m->addr[0]) + 80);
	sprintf(buffer, "mbus/1.0 %d %c (%s) %s ()\n%s (%s)\n", m->seqnum, reliable?'R':'U', m->addr[0], dest, cmnd, args);
	if ((sendto(m->fd, buffer, strlen(buffer), 0, (struct sockaddr *) &saddr, sizeof(saddr))) < 0) {
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
		if ((*m->parse_buffer[m->parse_depth] == ')') && (*(m->parse_buffer[m->parse_depth]-1) != '\\') && !instr) {
			*m->parse_buffer[m->parse_depth] = '\0';
			m->parse_buffer[m->parse_depth]++;
			return TRUE;
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

int mbus_parse_sym(struct mbus *m, char **s)
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
	int   l = strlen(s);

	/* Check that this an encoded string... */
	assert(s[0]   == '\"');
	assert(s[l-1] == '\"');

	memmove(s, s+1, l-2);
	s[l-2] = '\0';
	return s;
}

char *mbus_encode_str(char *s)
{
	static char	*encode_buffer = NULL;
	static int	 encode_buflen = 0;

	int l = strlen(s);
	if (encode_buflen < l) {
		if (encode_buffer != NULL) {
			xfree(encode_buffer);
		}
		encode_buflen = l+3;
		encode_buffer = (char *) xmalloc(encode_buflen);
	}
	strcpy(encode_buffer+1, s);
	encode_buffer[0]   = '\"';
	encode_buffer[l+1] = '\"';
	encode_buffer[l+2] = '\0';
	return encode_buffer;
}

void mbus_recv(struct mbus *m, void *data)
{
	char	*ver, *src, *dst, *ack, *r, *cmd, *param;
	char	 buffer[MBUS_BUF_SIZE];
	int	 buffer_len, seq, i;

	memset(buffer, 0, MBUS_BUF_SIZE);
	if ((buffer_len = recvfrom(m->fd, buffer, MBUS_BUF_SIZE, 0, NULL, NULL)) <= 0) {
		return;
	}

	mbus_parse_init(m, buffer);
	/* Parse the header */
	if (!mbus_parse_sym(m, &ver) || (strcmp(ver, "mbus/1.0") != 0)) {
		mbus_parse_done(m);
		return;
	}
	if (!mbus_parse_int(m, &seq)) {
		mbus_parse_done(m);
		return;
	}
	if (!mbus_parse_sym(m, &r)) {
		mbus_parse_done(m);
		return;
	}
	if (!mbus_parse_lst(m, &src)) {
		mbus_parse_done(m);
		return;
	}
	if (!mbus_parse_lst(m, &dst)) {
		mbus_parse_done(m);
		return;
	}
	if (!mbus_parse_lst(m, &ack)) {
		mbus_parse_done(m);
		return;
	}
	/* Process any ACKs received */
	mbus_parse_init(m, ack);
	while (mbus_parse_int(m, &i)) {
		mbus_ack_list_remove(m, src, dst, i);
	}
	mbus_parse_done(m);
	/* Check if the message was addressed to us... */
	for (i = 0; i < m->num_addr; i++) {
		if (mbus_addr_match(m, m->addr[i], dst)) {
			/* ...if it was, and an ACK was requested, send one... */
			if (strcmp(r, "R") == 0) {
				mbus_send_ack(m, src, seq);
			}
			/* ...and process the commands contained in the message */
			while (mbus_parse_sym(m, &cmd) && mbus_parse_lst(m, &param)) {
				m->handler(src, cmd, param, data);
			}
		}
	}
	mbus_parse_done(m);
}

