/*
 * FILE:     pckt_queue.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 * MODIFIED: COlin Perkins
 * 
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "ts.h"
#include "pckt_queue.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

/* For PACKET_LENGTH */
#include "session.h"

typedef struct s_pckt_queue {
        pckt_queue_element **buf;
        u_int16 buf_head;
        u_int16 buf_tail;
        u_int16 buf_used;
        u_int16 buf_len;
} pckt_queue;

pckt_queue * 
pckt_queue_create(int len)
{
        pckt_queue *p = (pckt_queue*)xmalloc(sizeof(pckt_queue));
	int         i;

	assert(len > 0);

        assert(p != NULL);
        memset(p, 0, sizeof(pckt_queue));
        
        p->buf  = (pckt_queue_element **)xmalloc(len * sizeof(pckt_queue_element*));
        p->buf_len  = len;
        p->buf_head = p->buf_tail = p->buf_used = 0;
        assert(p->buf != NULL);

	for (i=0; i<len; i++) {
		p->buf[i] = NULL;
	}

        return p;
}

void
pckt_queue_destroy(pckt_queue **p)
{
        assert(*p);
        pckt_queue_drain(*p);
        xfree((*p)->buf);
        xfree(*p);
        *p = NULL;
}

void
pckt_queue_drain(pckt_queue *q) 
{
        pckt_queue_element *pe;

        while((pe = pckt_dequeue(q)) != NULL) {
                pckt_queue_element_free(&pe);
        }
}

void
pckt_enqueue(pckt_queue *q,
             pckt_queue_element * pe)
{
        u_int16 insert;

        assert(q);
        assert(pe);
        
        if (q->buf_head != q->buf_tail || q->buf_used == 1) {
                insert = (q->buf_head + 1) % q->buf_len;
        } else {
                assert(q->buf[q->buf_head] == NULL);
                insert = q->buf_head;
        }

        if (insert == q->buf_tail) {
                if (q->buf[q->buf_tail]) {
                        pckt_queue_element_free(q->buf + q->buf_tail);
                        q->buf_tail = (q->buf_tail + 1) % q->buf_len;
                        q->buf_used--;
                        debug_msg("Packet queue overrun\n");
                }
        }

        q->buf[insert] = pe;
        q->buf_head    = insert;
        q->buf_used++;
        assert(q->buf_used <= q->buf_len);
}

pckt_queue_element *
pckt_dequeue(pckt_queue * q)
{
        pckt_queue_element *p;
        p = NULL;
        if (q->buf[q->buf_tail] != NULL) {
                assert(q->buf_used >= 1);
                p = q->buf[q->buf_tail];
                q->buf[q->buf_tail] = NULL;
                q->buf_tail = (q->buf_tail + 1) % q->buf_len;
                q->buf_used--;
        }

        assert(p != NULL || (p == NULL && q->buf_used == 0));
        assert(q->buf_used < q->buf_len);
        return p;
}

pckt_queue_element*
pckt_queue_element_create()
{
        pckt_queue_element *pe = (pckt_queue_element*)block_alloc(sizeof(pckt_queue_element));
        assert(pe != NULL);
        memset(pe, 0, sizeof(pckt_queue_element));
        return pe;
}

void
pckt_queue_element_free(pckt_queue_element ** ppe)
{
        pckt_queue_element *pe = *ppe;
        if (pe->pckt_ptr) {
                block_free(pe->pckt_ptr, pe->len);
        }
        block_free(pe, sizeof(pckt_queue_element));
        (*ppe) = NULL;
}

