/*
 * FILE:    tcltk.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "tcl.h"
#include "tk.h"
#include "debug.h"
#include "auddev.h"
#include "memory.h"
#include "version.h"
#include "session.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "tcltk.h"

extern char 	ui_audiotool[];
extern char	ui_transcoder[];

/* Should probably have these functions inline here, rather than in win32.c??? [csp] */
#ifdef WIN32		
int WinPutsCmd(ClientData, Tcl_Interp*, int ac, char** av);
int WinGetUserName(ClientData, Tcl_Interp*, int ac, char** av);
int WinReg(ClientData clientdata, Tcl_Interp *interp, int ac, char **av);
#endif

Tcl_Interp *interp;	/* Interpreter for application. */
char       *engine_addr;

void
tcl_send(char *command)
{
	/* This is called to send a message to the user interface...  */
	/* If the UI is not enabled, the message is silently ignored. */
	assert(command != NULL);

	if (Tk_GetNumMainWindows() <= 0) {
		return;
	}

	if (Tcl_Eval(interp, command) != TCL_OK) {
		debug_msg("TCL error: %s\n", Tcl_GetVar(interp, "errorInfo", 0));
	}
}

static int
mbus_send_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	session_struct *sp = (session_struct *) ttp;

	if (argc != 4) {
		i->result = "mbus_send <reliable> <cmnd> <args>";
		return TCL_ERROR;
	}

	mbus_qmsg(sp->mbus_ui, engine_addr, argv[2], argv[3], strcmp(argv[1], "R") == 0);
	return TCL_OK;
}

static int
mbus_encode_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	UNUSED(ttp);
	if (argc != 2) {
		i->result = "mbus_encode_str <str>";
		return TCL_ERROR;
	}
        Tcl_SetResult(i, mbus_encode_str(argv[1]), (Tcl_FreeProc *) xfree);
	return TCL_OK;
}

#include "xbm/mic.xbm"
#include "xbm/cd.xbm"
#include "xbm/speaker.xbm"
#include "xbm/head.xbm"
#include "xbm/line_out.xbm"
#include "xbm/line_in.xbm"
#include "xbm/rat_small.xbm"
#include "xbm/disk.xbm"
#include "xbm/play.xbm"
#include "xbm/rec.xbm"
#include "xbm/pause.xbm"
#include "xbm/stop.xbm"

int
tcl_process_event(void)
{
	return Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS);
}

void
tcl_process_events(session_struct *sp)
{
	int i;
	for (i=0; i<16 && tcl_process_event(); i++) {
                /* User Interface event processing has lower priority than 
                 * audio processing.
                 */
                if (audio_is_ready(sp->audio_device)) break;
        }
}

int
tcl_active(void)
{
	return (Tk_GetNumMainWindows() > 0);
}

int
tcl_init(session_struct *sp, int argc, char **argv, char *mbus_engine_addr)
{
	char		*cmd_line_args, buffer[10];

	Tcl_FindExecutable(argv[0]);
	interp        = Tcl_CreateInterp();
	engine_addr   = xstrdup(mbus_engine_addr);
	cmd_line_args = Tcl_Merge(argc - 1, argv + 1);
	Tcl_SetVar(interp, "argv", cmd_line_args, TCL_GLOBAL_ONLY);

#ifndef WIN32
	ckfree(cmd_line_args); 
#endif
	sprintf(buffer, "%d", argc - 1);
	Tcl_SetVar(interp, "argc", buffer, TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "argv0", argv[0], TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);

	if (Tk_MainWindow(interp) != TCL_OK) {
                fprintf(stderr, "%s\n", Tcl_GetStringResult(interp));
                exit(-1);
        }
	/*
	 * There is no easy way of preventing the Init functions from
	 * loading the library files. Ignore error returns and load
	 * built in versions.
	 */
	if (Tcl_Init(interp) != TCL_OK) {
                fprintf(stderr, "%s\n", Tcl_GetStringResult(interp));
                exit(-1);
        }
        if (Tk_Init(interp) != TCL_OK) {
                fprintf(stderr, "%s\n", Tcl_GetStringResult(interp));
                exit(-1);
        }

	Tcl_CreateCommand(interp, "mbus_send",	     mbus_send_cmd,   (ClientData) sp, NULL);
	Tcl_CreateCommand(interp, "mbus_encode_str", mbus_encode_cmd, (ClientData) sp, NULL);
#ifdef WIN32
        Tcl_SetVar(interp, "win32", "1", TCL_GLOBAL_ONLY);
        Tcl_CreateCommand(interp, "puts",        WinPutsCmd,     (ClientData)sp, NULL);
        Tcl_CreateCommand(interp, "getusername", WinGetUserName, (ClientData)sp, NULL);
	Tcl_CreateCommand(interp, "registry",    WinReg,         (ClientData)sp, NULL);
#else
	Tcl_SetVar(interp, "win32", "0", TCL_GLOBAL_ONLY);
#endif
	Tk_DefineBitmap(interp, Tk_GetUid("microphone"), mic_bits, mic_width, mic_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd"), cd_bits, cd_width, cd_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker"), speaker_bits, speaker_width, speaker_height);
	Tk_DefineBitmap(interp, Tk_GetUid("headphone"), head_bits, head_width, head_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out"), line_out_bits, line_out_width, line_out_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in"), line_in_bits, line_in_width, line_in_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_small"), rat_small_bits, rat_small_width, rat_small_height);
	Tk_DefineBitmap(interp, Tk_GetUid("disk"), disk_bits, disk_width, disk_height);
	Tk_DefineBitmap(interp, Tk_GetUid("play"), play_bits, play_width, play_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rec"),  rec_bits,  rec_width,  rec_height);
	Tk_DefineBitmap(interp, Tk_GetUid("pause"), pause_bits, pause_width, pause_height);
	Tk_DefineBitmap(interp, Tk_GetUid("stop"),  stop_bits,  stop_width,  stop_height);

	if (sp->mode == AUDIO_TOOL) {
		int      len           = strlen(ui_audiotool);
		Tcl_Obj *audiotool_obj = Tcl_NewStringObj(ui_audiotool, strlen(ui_audiotool));
		debug_msg("ui_script len %d bytes\n", len);
		if (Tcl_EvalObj(interp, audiotool_obj) != TCL_OK) {
			fprintf(stderr, "ui_audiotool error: %s\n", Tcl_GetStringResult(interp));
		}
	} else if (sp->mode == TRANSCODER) {
		if (Tcl_EvalObj(interp, Tcl_NewStringObj(ui_transcoder, strlen(ui_transcoder))) != TCL_OK) {
			fprintf(stderr, "ui_transcoder error: %s\n", interp->result);
		}
	} else {
		debug_msg("Unknown mode: huh?");
		abort();
	}
	while (tcl_process_event()) {
		/* Processing Tcl events, to allow the UI to initialize... */
	};

	Tcl_ResetResult(interp);
	return TRUE;
}

void 
tcl_exit()
{
        xfree(engine_addr);
}
