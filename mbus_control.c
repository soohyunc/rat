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
#include "mbus_control.h"

static void rx_mbus_quit(char *srce, char *args, void *data)
{
	debug_msg("%s %s %p\n", srce, args, data);
}

static void rx_mbus_waiting(char *srce, char *args, void *data)
{
	debug_msg("%s %s %p\n", srce, args, data);
}

static void rx_mbus_go(char *srce, char *args, void *data)
{
	debug_msg("%s %s %p\n", srce, args, data);
}

static void rx_mbus_hello(char *srce, char *args, void *data)
{
	debug_msg("%s %s %p\n", srce, args, data);
}

static const char *rx_cmnd[] = {
	"mbus.quit",
	"mbus.waiting",
	"mbus.go",
	"mbus.hello",             
	""
};

static void (*rx_func[])(char *srce, char *args, void *data) = {
	rx_mbus_quit,
	rx_mbus_waiting,
	rx_mbus_go,
	rx_mbus_hello,
        NULL
};

void mbus_control_rx(char *srce, char *cmnd, char *args, void *data)
{
	int i;

	for (i=0; strlen(rx_cmnd[i]) != 0; i++) {
		if (strcmp(rx_cmnd[i], cmnd) == 0) {
                        rx_func[i](srce, args, data);
			return;
		} 
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
}

