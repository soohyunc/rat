/*
 * FILE:    mbus_engine.h
 * AUTHORS: Colin Perkins
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef _MBUS_ENGINE_H
#define _MBUS_ENGINE_H

void mbus_engine_wait_handler_init(char *token);
int  mbus_engine_wait_handler_done(void);
void mbus_engine_wait_handler(char *srce, char *cmnd, char *args, void *data);
void mbus_engine_rx(char *srce, char *cmnd, char *args, void *data);

#endif
