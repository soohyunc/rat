/*
 * FILE:    tcltk.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef _TCLTK_H
#define _TCLTK_H

struct session_tag;

void    tcl_send(char *command);
int	tcl_init(struct session_tag *session_pointer, int argc, char **argv, char *mbus_engine_addr);
void    tcl_exit(void);
int	tcl_process_event(void);
void	tcl_process_events(struct session_tag *session_pointer);
int	tcl_active(void);

#endif
