/*
 * FILE:    mbus_ui.c
 * AUTHORS: Colin Perkins
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "tcltk.h"

void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data)
{
	char        command[1500];
	unsigned int i;

	UNUSED(srce);
	UNUSED(data);

	sprintf(command, "mbus_recv %s %s", cmnd, args);

	for (i = 0; i < (unsigned)strlen(command); i++) {
		if (command[i] == '[') command[i] = '(';
		if (command[i] == ']') command[i] = ')';
	}

	tcl_send(command);
}

