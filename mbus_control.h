/*
 * FILE:    mbus_control.h
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _MBUS_CONTROL_H
#define _MBUS_CONTROL_H

void  mbus_control_wait_init(char *token);
char *mbus_control_wait_done(void);
void  mbus_control_rx(char *srce, char *cmnd, char *args, void *data);

#endif
