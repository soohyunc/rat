/*
 * FILE:     rtp_queue.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 * MODIFIED: Colin Perkins
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#include <assert.h>
#include "config_unix.h"
#include "config_win32.h"
#include "rtp_queue.h"

struct s_rtp_queue {
        rtp_packet **buf;
        u_int16      len;
        u_int16      head;
        u_int16      tail;
        u_int16      used;
};

int 
rtp_queue_create(rtp_queue_t **ppr, u_int16 len)
{
        rtp_queue_t *r;
	int         i;

        r =  (rtp_queue_t*)xmalloc(sizeof(rtp_queue_t));
        if (r == NULL) {
                *ppr = NULL;
                return FALSE;
        } 
        
        r->buf      = (rtp_packet **)xmalloc(len * sizeof(rtp_packet*));
        if (r->buf == NULL) {
                xfree(r);
                *ppr = NULL;
                return FALSE;
        }

	for (i = 0; i < len; i++) {
		r->buf[i] = NULL;
	}
        r->len  = len;
        r->head = r->tail = r->used = 0;

        *ppr = r;
        return TRUE;
}

int 
rtp_queue_destroy(rtp_queue_t **r)
{
        rtp_queue_drain(*r);
        xfree((*r)->buf);
        xfree(*r);
        *r = NULL;
        return TRUE;
}

void
rtp_queue_drain(rtp_queue_t *r)
{
        rtp_packet *p;

        while(r->buf[r->head] != NULL) {
                p = r->buf[r->head];
                xfree(p);
                r->buf[r->head] = NULL;
                r->used--;
                r->head = (r->head + 1) % r->len;
        }
        assert(r->used == 0);
        r->head = r->tail = 0;
}

int 
rtp_enqueue(rtp_queue_t *r, rtp_packet *p)
{
        int overflow = 0;
        if (r->used == r->len) {
                assert(((r->tail + 1) % r->len) == r->head);
                xfree(r->buf[r->head]);
                r->buf[r->head] = NULL;
                r->head = (r->head + 1) % r->len;
                r->used--;
                overflow = 1;
        }
        if (r->used != 0) {
                r->tail = (r->tail + 1) % r->len;
        }
        assert(r->buf[r->tail] == NULL);
        r->buf[r->tail] = p;
        r->used++;
        assert(r->used <= r->len);
        return overflow;
}

int
rtp_dequeue (rtp_queue_t *r, rtp_packet **p)
{
        if (r->buf[r->head] == NULL) {
                return FALSE;
        }
        *p = r->buf[r->head];
        r->buf[r->head] = NULL;
        r->head++;
        r->used--;
        assert(r->used <= r->len);
        return TRUE;
}
