/*
 * FILE:    statistics.h
 * PROGRAM: RAT
 * AUTHOR:  V.J.Hardman
 * CREATED: 23/03/95
 *
 * $Revision$
 * $Date$
 *
 * This module houses the routine from the main execution loop that monitors
 * lost packets, duplicated packets etc.
 *
 * This routine fixes duplications - by throwing away the duplicates
 *
 * This module also contains irregularly executed stats analysis routines
 *
 * Input queue: netrx_queue Output Queue: receive_queue Stats Queue: Stats queue
 *
 * Copyright (c) 1995,1996 University College London
 * All rights reserved.
 *
 */


#ifndef _rat_statistics_h_
#define _rat_statistics_h_

struct session_tag;
struct pckt_queue_tag;
struct rx_queue_tag;
struct s_cushion_struct;

void statistics_init(void);

void statistics_process(struct session_tag      *session_pointer, 
                        struct s_pckt_queue     *rtp_pckt_queue,
                        struct s_cushion_struct *cushion,
                        u_int32                  ntp_time,
                        ts_t                     curr_ts);

#endif /* _rat_statistics_h_ */

