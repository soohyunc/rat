/*
 * FILE:    confbus_ack.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include "assert.h"
#include "util.h"
#include "confbus_addr.h"
#include "confbus_ack.h"

typedef struct s_cb_ack {
	struct s_cb_ack	*next;
	struct s_cb_ack	*prev;
	struct s_cbaddr	*srce;
	struct s_cbaddr	*dest;
	char		*mesg;
	int		 seqn;
	struct timeval	 time;	/* Used to determine when to request retransmissions, etc... */
} cb_ack;

void cb_ack_list_insert(cb_ack **al, struct s_cbaddr *srce, struct s_cbaddr *dest, char *mesg, int seqnum)
{
	cb_ack	*curr = (cb_ack *) xmalloc(sizeof(cb_ack));

	assert(srce != NULL);
	assert(dest != NULL);
	assert(mesg != NULL);

	curr->next = *al;
	curr->prev = NULL;
	curr->srce = cb_addr_dup(srce);
	curr->dest = cb_addr_dup(dest);
	curr->mesg = xstrdup(mesg);
	curr->seqn = seqnum;
	gettimeofday(&(curr->time), NULL);

	if (*al != NULL) {
		(*al)->prev = curr;
	}
	*al = curr;
}

void cb_ack_list_remove(cb_ack **al, struct s_cbaddr *srce, struct s_cbaddr *dest, int seqnum)
{
	/* This would be much more efficient if it scanned from last to first, since     */
	/* we're most likely to receive ACKs in LIFO order, and this assumes FIFO...     */
	/* We hope that the number of outstanding ACKs is small, so this doesn't matter. */
	cb_ack	*curr = *al;

	while (curr != NULL) {
		if (cb_addr_match(curr->srce, srce) && cb_addr_match(curr->dest, dest) && (curr->seqn == seqnum)) {
			cb_addr_free(curr->srce);
			cb_addr_free(curr->dest);
			xfree(curr->mesg);
			if (curr->next != NULL) curr->next->prev = curr->prev;
			if (curr->prev != NULL) curr->prev->next = curr->next;
			if (*al == curr) *al = curr->next;
			xfree(curr);
			return;
		}
		curr = curr->next;
	}
	printf("WARNING: Got a confbus ACK for something we didn't send!\n");
}

void cb_ack_retransmit(cb_ack **al)
{
	cb_ack		*curr = *al;
	struct timeval	 time;
	long		 diff;

	gettimeofday(&time, NULL);

	while (curr != NULL) {
		/* diff is time in milliseconds that the message has been awaiting an ACK */
		diff = ((time.tv_sec * 1000) + (time.tv_usec / 1000)) - ((curr->time.tv_sec * 1000) + (curr->time.tv_usec / 1000));
		printf("%ld\n", diff);
		curr = curr->next;
	}
}

