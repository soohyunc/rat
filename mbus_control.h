/*
 * FILE:    mbus_control.h
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#ifndef _MBUS_CONTROL_H
#define _MBUS_CONTROL_H

void mbus_control_wait_init(char *token);
int  mbus_control_wait_done(void);
void mbus_control_rx(char *srce, char *cmnd, char *args, void *data);

#endif
