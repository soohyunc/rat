/*
 * FILE:      pktbuf.c
 * AUTHOR(S): Orion Hodson 
 *      
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "rtp.h"
#include "pktbuf.h"

struct s_pktbuf {
        rtp_packet **buf;    /* Pointer to rtp packets                      */
        u_int16      insert; /* Next insertion point (FIFO circular buffer) */
        u_int16      buflen; /* Max number of packets                       */
        u_int16      used;   /* Actual number of packets buffered           */
};

int
pktbuf_create(struct s_pktbuf **ppb, u_int16 size)
{
        struct s_pktbuf *pb;
        u_int32          i;
        
        pb = (struct s_pktbuf*)xmalloc(sizeof(struct s_pktbuf));
        if (pb == NULL) {
                return FALSE;
        }
        
        pb->buf = (rtp_packet**)xmalloc(sizeof(rtp_packet) * size);
        if (pb->buf == NULL) {
                xfree(pb);
                return FALSE;
        }

        for(i = 0; i < size; i++) {
                pb->buf[i] = NULL;
        }

        pb->buflen = size;
        pb->used   = 0;
        pb->insert = 0;

        *ppb = pb;
        return TRUE;
}

void
pktbuf_destroy(struct s_pktbuf **ppb)
{
        struct s_pktbuf *pb;
        u_int32 i;

        pb = *ppb;
        for(i = 0; i < pb->buflen; i++) {
                if (pb->buf[i]) {
                        xfree(pb->buf[i]);
                }
        }
        xfree(pb);
        *ppb = NULL;
}

int 
pktbuf_enqueue(struct s_pktbuf *pb, rtp_packet *p)
{
        assert(p != NULL);

        if (pb->buf[pb->insert] != NULL) {
                /* A packet already sits in this space */
                xfree(pb->buf[pb->insert]);
                debug_msg("Buffer overflow.  Process was blocked or network burst.\n");
        } else {
                pb->used++;
                assert(pb->used <= pb->buflen);
        }

        pb->buf[pb->insert] = p;

        pb->insert++;
        if (pb->insert == pb->buflen) {
                pb->insert = 0;
        }

        return TRUE;
}

int 
pktbuf_dequeue(struct s_pktbuf *pb, rtp_packet **pp)
{
        u_int32 idx = (pb->insert + pb->buflen - pb->used) % pb->buflen;

        *pp = pb->buf[idx];
        if (*pp) {
                pb->buf[idx] = NULL;
                pb->used--;
                return TRUE;
        }
        return FALSE;
}

static int 
timestamp_greater(u_int32 t1, u_int32 t2)
{
        u_int32 delta = t1 - t2;
        
        if (delta < 0x7fffffff && delta != 0) {
                return TRUE;
        }
        return FALSE;
}

int
pktbuf_peak_last(pktbuf_t   *pb,
                 rtp_packet **pp)
{
        u_int32     idx, max_idx;

        max_idx = idx = (pb->insert + pb->buflen - pb->used) % pb->buflen;
        if (pb->buf[idx] == NULL) {
                assert(pb->used == 0);
                *pp = NULL;
                return FALSE;
        }

        idx = (idx + 1) % pb->buflen;
        while (pb->buf[idx] != NULL) {
                if (timestamp_greater(pb->buf[idx]->ts, 
                                      pb->buf[max_idx]->ts)) {
                        max_idx = idx;
                }
                idx = (idx + 1) % pb->buflen;
        }
        
        *pp = pb->buf[max_idx];
        return TRUE;
}

u_int16 
pktbuf_get_count(pktbuf_t *pb)
{
        return pb->used;
}
