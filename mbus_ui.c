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

static char *wait_token = NULL;
static int   wait_done  = FALSE;

void mbus_ui_wait_handler_init(char *token)
{
	wait_token = token;
	wait_done  = FALSE;
}

int mbus_ui_wait_handler_done(void)
{
	return wait_done;
}

void mbus_ui_wait_handler(char *srce, char *cmnd, char *args, void *data)
{
	/* This routine waits for an mbus message of the form mbus.waiting(token) */
	/* where the token is provided by a call to mbus_ui_wait_handler_init().  */
	/* Other mbus commands are ignored whilst this is going on.               */
	struct mbus *m = (struct mbus *) data;

	UNUSED(srce);

	if (strcmp(cmnd, "mbus.waiting") == 0) {
		char	*t;

		mbus_parse_init(m, args);
		mbus_parse_str(m, &t);
		if (strcmp(mbus_decode_str(t), wait_token) == 0) {
			wait_done = TRUE;
		}
		mbus_parse_done(m);
	} else {
		debug_msg("Ignored command \"%s\" which arrived during rendezvous\n", cmnd);
	}
}

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

