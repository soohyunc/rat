/*
 * FILE:    main_ui.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
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
#include "mbus_ui.h"
#include "tcltk.h"

int main(int argc, char *argv[])
{
	char		 engine_addr[100], ui_addr[100];
	struct mbus	*mbus_ui;

	sprintf(engine_addr, "(media:audio module:engine app:rat instance:%lu)", (u_int32) getpid());
	sprintf(ui_addr,     "(media:audio module:ui     app:rat instance:%lu)", (u_int32) getpid());

	mbus_ui = mbus_init(mbus_ui_rx, NULL);
	mbus_addr(mbus_ui, ui_addr);
	tcl_init(mbus_ui, argc, argv, engine_addr);

	while (1) {
		usleep(20000);
		mbus_heartbeat(mbus_ui, 1);
		mbus_send(mbus_ui);
		mbus_recv(mbus_ui, NULL);
		tcl_process_all_events();
	}
}

