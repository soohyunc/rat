/*
 * FILE:    mbus_ui.c
 * AUTHORS: Colin Perkins
 * 
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "tcltk.h"

extern char 	*e_addr;
extern int	 ui_active;
extern int	 should_exit;

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args, struct mbus *m);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
        const char   *rxname;
        mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static void rx_tool_rat_addr_engine(char *srce, char *args, struct mbus *m)
{
	char	*addr;

	UNUSED(srce);

	mbus_parse_init(m, args);
	if (mbus_parse_str(m, &addr)) {
		e_addr = xstrdup(mbus_decode_str(addr));
	} else {
		debug_msg("mbus: usage \"tool.rat.addr.engine <addr>\"\n");
	}
	mbus_parse_done(m);
}

static void rx_mbus_hello(char *srce, char *args, struct mbus *m)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(m);
}

static void rx_mbus_waiting(char *srce, char *args, struct mbus *m)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(m);
}

static void rx_mbus_quit(char *srce, char *args, struct mbus *m)
{
	UNUSED(args);
	UNUSED(m);
	should_exit = TRUE;
	debug_msg("Got mbus.quit() from %s\n", srce);
}

static void rx_mbus_bye(char *srce, char *args, struct mbus *m)
{
	UNUSED(args);
	UNUSED(m);
	UNUSED(srce);
}

static const mbus_cmd_tuple ui_cmds[] = {
        { "tool.rat.addr.engine",	rx_tool_rat_addr_engine },
        { "mbus.hello",			rx_mbus_hello },
        { "mbus.waiting",		rx_mbus_waiting },
        { "mbus.quit",			rx_mbus_quit },
	{ "mbus.bye",			rx_mbus_bye }
};

#define NUM_UI_CMDS sizeof(ui_cmds)/sizeof(ui_cmds[0])

void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data)
{
	char        	 command[1500];
	unsigned int 	 i;
	struct mbus	*m = (struct mbus *) data;

	/* Some commands are handled in C for now... */
	for (i=0; i < NUM_UI_CMDS; i++) {
		if (strcmp(ui_cmds[i].rxname, cmnd) == 0) {
                        ui_cmds[i].rxproc(srce, args, m);
			return;
		} 
	}
	/* ...and some are in Tcl... */
	if (ui_active) {
		/* Pass it to the Tcl code to deal with... */
		sprintf(command, "mbus_recv %s %s", cmnd, args);
		for (i = 0; i < (unsigned)strlen(command); i++) {
			if (command[i] == '[') command[i] = '(';
			if (command[i] == ']') command[i] = ')';
		}
		tcl_send(command);
	} else {
		debug_msg("Got early mbus command %s (%s)\n", cmnd, args);
	}
}

