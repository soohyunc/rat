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

extern char 	*e_addr;
extern int	 ui_active;
extern int	 should_exit;

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

/* Note: These next two arrays MUST be in the same order! */

const char *rx_cmnd[] = {
	"tool.rat.addr.engine",
	"mbus.hello",
	"mbus.waiting",
	"mbus.quit",
	""
};

static void (*rx_func[])(char *srce, char *args, struct mbus *m) = {
	rx_tool_rat_addr_engine,
	rx_mbus_hello,
	rx_mbus_waiting,
	rx_mbus_quit,
        NULL
};

void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data)
{
	char        	 command[1500];
	unsigned int 	 i;
	struct mbus	*m = (struct mbus *) data;

	/* Some commands are handled in C for now... */
	for (i=0; strlen(rx_cmnd[i]) != 0; i++) {
		if (strcmp(rx_cmnd[i], cmnd) == 0) {
                        rx_func[i](srce, args, m);
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

