/*
 * FILE:    rtp_queue.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#ifndef _rtp_queue_h_
#define _rtp_queue_h_

#include "rtp.h"

typedef struct s_rtp_queue rtp_queue_t;

int         rtp_queue_create  (rtp_queue_t **queue, u_int16 len);
int         rtp_queue_destroy (rtp_queue_t **queue);
void        rtp_queue_drain   (rtp_queue_t  *queue);
int         rtp_enqueue       (rtp_queue_t  *queue, rtp_packet *p);
int         rtp_dequeue       (rtp_queue_t  *queue, rtp_packet **p);

#endif	/* _rtp_queue_h_ */

