/*
 * FILE:      pktbuf.h
 * AUTHOR(S): Orion Hodson 
 *      
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

/* This provides a finite length buffer for queueing packets.  It is   */
/* necessary since it process blocks it will find lots of packets when */
/* it gets around to doing a read.  However, most of these packets     */
/* won't be any use, discard them to conserve processing power.        */

/* Assumes data is allocated with xmalloc that is enqueued with        */
/* pktbuf_enqueue.  It's important since the pktbuf frees this data if */
/* the number of enqueued packets exceeds the maxpackets selected when */
/* the buffer was created.                                             */

#ifndef __PKTBUF_H__
#define __PKTBUF_H__

typedef struct s_pktbuf pktbuf_t;

int     pktbuf_create    (pktbuf_t **ppb, 
                          u_int32    maxpackets);
void    pktbuf_destroy   (pktbuf_t **ppb);
int     pktbuf_enqueue   (pktbuf_t *pb, 
                          ts_t      timestamp, 
                          u_char    payload, 
                          u_char   *data, 
                          u_int32   datalen);
int     pktbuf_dequeue   (pktbuf_t *pb, 
                          ts_t     *timestamp, 
                          u_char   *payload, 
                          u_char  **data, 
                          u_int32 *datalen);
u_int16 pktbuf_get_count (pktbuf_t *pb);

#endif /* __PKTBUF_H__ */