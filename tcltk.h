/*
 * FILE:    tcltk.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 * $Revision$
 * $Date$
 */

#ifndef _TCLTK_H
#define _TCLTK_H

void    tcl_send(char *command);
int 	tcl_init1(int argc, char **argv);
int	tcl_init2(struct mbus *mbus_ui, char *mbus_engine_addr);
void    tcl_exit(void);

#endif
