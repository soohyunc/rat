/*
 * FILE:    tcltk.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "version.h"
#include "session.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "util.h"
#include "tcltk.h"
/* The tcl/tk includes have to go after config.h, else we get warnings on
 * solaris 2.5.1, due to buggy system header files included by config.h [csp]
 */
#include <tcl.h>
#include <tk.h>

#ifdef TclX
#include "tclExtend.h"
extern char	profrep[];
#endif

extern char 	ui_audiotool[];
extern char	ui_transcoder[];
extern char	TCL_LIBS[];

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
	UNUSED(ttp);

	if (argc != 4) {
		i->result = "mbus_send <reliable> <cmnd> <args>";
		return TCL_ERROR;
	}

	mbus_ui_tx(TRUE, engine_addr, argv[2], argv[3], strcmp(argv[1], "R") == 0);
	return TCL_OK;
}

static int
mbus_qmsg_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	UNUSED(ttp);

	if (argc != 3) {
		i->result = "mbus_qmsg <cmnd> <args>";
		return TCL_ERROR;
	}

	mbus_ui_tx_queue(TRUE, argv[1], argv[2]);
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

int
tcl_process_event(void)
{
	return Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS);
}

void
tcl_process_events(void)
{
	int i;
	for (i=0; i<16 && tcl_process_event(); i++) ;
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

        /* There's a nasty in the pre-compiled library code that goes
         * looking here and sourcing tcl files, but we have all the ones
         * we need already precompiled in.
         */
        putenv("TCL_LIBRARY=");
        putenv("TK_LIBRARY=");
        
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

	Tk_MainWindow(interp);
	/*
	 * There is no easy way of preventing the Init functions from
	 * loading the library files. Ignore error returns and load
	 * built in versions.
	 */
	Tcl_Init(interp);
	Tk_Init(interp);
	if (Tcl_EvalObj(interp, Tcl_NewStringObj(TCL_LIBS, strlen(TCL_LIBS))) != TCL_OK) {
		fprintf(stderr, "TCL_LIBS error: %s\n", interp->result);
	}

#ifdef TclX
	Tclx_Init(interp);
	Tkx_Init(interp);
	if (Tcl_EvalObj(interp, Tcl_NewStringObj(profrep, strlen(profrep))) != TCL_OK) {
		fprintf(stderr, "profrep error: %s\n", interp->result);
	}
#endif

	Tcl_CreateCommand(interp, "mbus_send",	     mbus_send_cmd,   (ClientData) sp, NULL);
	Tcl_CreateCommand(interp, "mbus_qmsg",	     mbus_qmsg_cmd,   (ClientData) sp, NULL);
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

	if (sp->mode == AUDIO_TOOL) {
		if (Tcl_EvalObj(interp, Tcl_NewStringObj(ui_audiotool, strlen(ui_audiotool))) != TCL_OK) {
			fprintf(stderr, "ui_audiotool error: %s\n", interp->result);
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
