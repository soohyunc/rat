/*
 * FILE:    mbus_control.c
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

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_control.h"

extern int should_exit;

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args, void *data);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
        const char   *rxname;
        mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static char *wait_token;
static char *wait_addr;

void mbus_control_wait_init(char *token)
{
	wait_token = token;
	wait_addr  = NULL;
}

char *mbus_control_wait_done(void)
{
	return wait_addr;
}

static void rx_mbus_quit(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);

	/* We mark ourselves as ready to exit. The main() will */
	/* cleanup and terminate all our subprocesses.         */
	should_exit = TRUE;
}

static void rx_mbus_waiting(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);
}

static void rx_mbus_go(char *srce, char *args, void *data)
{
	struct mbus *m = (struct mbus *) data;
	char	*t;

	mbus_parse_init(m, args);
	mbus_parse_str(m, &t);
	if (strcmp(mbus_decode_str(t), wait_token) == 0) {
		wait_addr = xstrdup(srce);
	}
	mbus_parse_done(m);
}

static void rx_mbus_hello(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);
}

static const mbus_cmd_tuple control_cmds[] = {
        { "mbus.quit",                             rx_mbus_quit },
        { "mbus.bye",                              rx_mbus_quit },
        { "mbus.waiting",                          rx_mbus_waiting },
        { "mbus.go",                               rx_mbus_go },
        { "mbus.hello",                            rx_mbus_hello },
};

#define NUM_CONTROL_CMDS sizeof(control_cmds)/sizeof(control_cmds[0])

void mbus_control_rx(char *srce, char *cmnd, char *args, void *data)
{
	uint32_t i;

	for (i=0; i < NUM_CONTROL_CMDS; i++) {
		if (strcmp(control_cmds[i].rxname, cmnd) == 0) {
                        control_cmds[i].rxproc(srce, args, data);
			return;
		} 
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
}

