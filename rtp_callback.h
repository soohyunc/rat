/*
 * FILE:    rtp_callback.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins / Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

struct s_session;

void rtp_callback_init (struct rtp *s, struct s_session *sp);
void rtp_callback      (struct rtp *s, rtp_event *e);
void rtp_callback_exit (struct rtp *s);
