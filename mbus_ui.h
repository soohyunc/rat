/*
 * FILE:    mbus_ui.h
 * AUTHORS: Colin Perkins
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef _MBUS_UI_H
#define _MBUS_UI_H

void mbus_ui_wait_handler_init(char *token);
int  mbus_ui_wait_handler_done(void);
void mbus_ui_wait_handler(char *srce, char *cmnd, char *args, void *data);
void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data);

#endif
