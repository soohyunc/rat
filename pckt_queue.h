/*
 * FILE:    pckt_queue.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef _pckt_queue_h_
#define _pckt_queue_h_

#define PCKT_QUEUE_RTP_LEN  64
#define PCKT_QUEUE_RTCP_LEN 32

#include "ts.h"

struct s_pckt_queue;
struct s_rtcp_dbentry;

typedef struct {
	u_int8                 *pckt_ptr;
	int32                   len;
        u_int32                 extlen;
	ts_t                    arrival;
        ts_t                    playout;
        struct  s_rtcp_dbentry *sender;
} pckt_queue_element;

pckt_queue_element*  pckt_queue_element_create (void);
void                 pckt_queue_element_free   (pckt_queue_element **pe);

struct s_pckt_queue* pckt_queue_create  (int len);
void                 pckt_queue_destroy (struct s_pckt_queue **p);
void                 pckt_queue_drain   (struct s_pckt_queue *p);
void                 pckt_enqueue       (struct s_pckt_queue *q, pckt_queue_element *pe);
pckt_queue_element*  pckt_dequeue       (struct s_pckt_queue *q);

#endif	/* _pckt_queue_h_ */

