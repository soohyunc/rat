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
#include "ts.h"
#include "pktbuf.h"

typedef struct {
        ts_t    timestamp;
        u_char  payload;
        u_char *data;
        u_int32 datalen;
} pkt_t;

struct s_pktbuf {
        u_int32  insert; /* Last insertion point (FIFO circular buffer) */
        u_int32  buflen; /* Number of packets */
        pkt_t   *buf;    /* The packets */
};

int
pktbuf_create(struct s_pktbuf **ppb, u_int32 size)
{
        struct s_pktbuf *pb;
        u_int32          i;
        
        pb = (struct s_pktbuf*)xmalloc(sizeof(struct s_pktbuf));
        if (pb == NULL) {
                return FALSE;
        }
        
        pb->buf = (pkt_t*)xmalloc(sizeof(pkt_t) * size);
        if (pb->buf == NULL) {
                xfree(pb);
                return FALSE;
        }
        
        pb->buflen = size;
        pb->insert = 0;
        for(i = 0; i < size; i++) {
                pb->buf[i].data = NULL;
        }
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
                if (pb->buf[i].data) {
                        xfree(pb->buf[i].data);
                }
        }
        xfree(pb);
        *ppb = NULL;
}

int 
pktbuf_enqueue(struct s_pktbuf *pb, ts_t timestamp, u_char payload, u_char *data, u_int32 datalen)
{
        assert(data != NULL);
        assert(datalen > 0);

        pb->insert++;
        if (pb->insert == pb->buflen) {
                pb->insert = 0;
        }

        if (pb->buf[pb->insert].data != NULL) {
                /* A packet already sits in this space */
                xfree(pb->buf[pb->insert].data);
                debug_msg("Buffer overflow.  Process was blocked?\n");
        }

        pb->buf[pb->insert].timestamp = timestamp;
        pb->buf[pb->insert].payload   = payload;
        pb->buf[pb->insert].data      = data;
        pb->buf[pb->insert].datalen   = datalen;

        return TRUE;
}

int 
pktbuf_dequeue(struct s_pktbuf *pb, ts_t *timestamp, u_char *payload, u_char **data, u_int32 *datalen)
{
        u_int32 idx = pb->insert;

        /* Not vaguely efficient since no tail info is maintained, but */
        /* not a big deal on the scale of things.                      */
        do {
                idx = (idx + 1) % pb->buflen;
                if (pb->buf[idx].data) {
                        *timestamp           = pb->buf[idx].timestamp;
                        *payload             = pb->buf[idx].payload;
                        *data                = pb->buf[idx].data;
                        *datalen             = pb->buf[idx].datalen;
                        pb->buf[idx].data    = NULL;                        
                        pb->buf[idx].datalen = 0;

                        return TRUE;
                }
        } while (idx != pb->insert);

        return FALSE;
}
