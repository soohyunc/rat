/*
 * FILE:    ui.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996,1997 University College London
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
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "repair.h"
#include "codec.h"
#include "audio.h"
#include "mbus.h"
#include "ui.h"
#include "channel.h"
/* The tcl/tk includes have to go after config.h, else we get warnings on
 * solaris 2.5.1, due to buggy system header files included by config.h [csp]
 */
#include <tcl.h>
#include <tk.h>

extern Tcl_Interp	*interp;
extern char		init_ui_script[];
extern char		init_ui_small_script[];
extern char		TCL_LIBS[];

static char args[1000];

#ifdef WIN32
int WinPutRegistry(ClientData, Tcl_Interp*, int ac, char** av);
int WinGetRegistry(ClientData, Tcl_Interp*, int ac, char** av);
int WinPutsCmd(ClientData, Tcl_Interp*, int ac, char** av);
int WinGetUserName(ClientData, Tcl_Interp*, int ac, char** av);
#endif

Tcl_Interp *interp;	/* Interpreter for application. */

void
ui_send(char *command)
{
	/* This is called to send a message to the user interface...  */
	/* If the UI is not enabled, the message is silently ignored. */
	assert(command != NULL);

	if (Tk_GetNumMainWindows() <= 0) {
		return;
	}

	if (Tcl_Eval(interp, command) != TCL_OK) {
#ifdef DEBUG
		printf("TCL error: %s\n", Tcl_GetVar(interp, "errorInfo", 0));
#endif
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

	mbus_send(sp->mbus_ui_chan, sp->mbus_engine_addr, argv[2], argv[3], strcmp(argv[1], "R") == 0);
	return TCL_OK;
}

static int
mbus_qmsg_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	session_struct *sp = (session_struct *) ttp;

	if (argc != 3) {
		i->result = "mbus_qmsg <cmnd> <args>";
		return TCL_ERROR;
	}

	mbus_qmsg(sp->mbus_ui_chan, argv[1], argv[2]);
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
	i->result = mbus_encode_str(argv[1]);
	return TCL_OK;
}

#include "xbm/ucl.xbm"
#include "xbm/mic.xbm"
#include "xbm/cd.xbm"
#include "xbm/speaker.xbm"
#include "xbm/head.xbm"
#include "xbm/line_out.xbm"
#include "xbm/line_in.xbm"
#include "xbm/rat_small.xbm"
#include "xbm/rat_med.xbm"
#include "xbm/rat2.xbm"

static int
codec_bw_cmp(const void *a, const void *b)
{
        int bwa, bwb;
        bwa = (*((codec_t**)a))->max_unit_sz;
        bwb = (*((codec_t**)b))->max_unit_sz;
        if (bwa<bwb) {
                return 1;
        } else if (bwa>bwb) {
                return -1;
        } 
        return 0;
}
 
static void 
ui_codecs(session_struct *sp)
{
	char	 arg[1000], *a;
	codec_t	*codec[10],*sel;
	int 	 i, nc;

	a = &arg[0];
        sel = get_codec(sp->encodings[0]);
        
	for (nc=i=0; i<MAX_CODEC; i++) {
		codec[nc] = get_codec(i);
		if (codec[nc] != NULL && codec_compatible(sel,codec[nc])) {
                        nc++;
                        assert(nc<10); 
		}
	}

        /* sort by bw as this makes handling of acceptable redundant codecs easier in ui */
        qsort(codec,nc,sizeof(codec_t*),codec_bw_cmp);
        for(i=0;i<nc;i++) {
                sprintf(a, " %s", codec[i]->name);
                a += strlen(codec[i]->name) + 1;
        }

	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "codec.supported", arg, TRUE);
}

int
ui_init(session_struct *sp, char *cname, int argc, char **argv)
{
	char		*cmd_line_args, buffer[10];
	char		*ecname;

	Tcl_FindExecutable(argv[0]);
	interp = Tcl_CreateInterp();

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

	if (Tcl_VarEval(interp, TCL_LIBS, NULL) != TCL_OK) {
		fprintf(stderr, "TCL_LIBS error: %s\n", interp->result);
	}

	Tcl_CreateCommand(interp, "mbus_send",	     mbus_send_cmd,   (ClientData) sp, NULL);
	Tcl_CreateCommand(interp, "mbus_qmsg",	     mbus_qmsg_cmd,   (ClientData) sp, NULL);
	Tcl_CreateCommand(interp, "mbus_encode_str", mbus_encode_cmd, (ClientData) sp, NULL);
#ifdef WIN32
	Tcl_SetVar(interp, "win32", "1", TCL_GLOBAL_ONLY);
	Tcl_CreateCommand(interp, "putregistry", WinPutRegistry,  (ClientData)sp, NULL);
        Tcl_CreateCommand(interp, "getregistry", WinGetRegistry,  (ClientData)sp, NULL);
	Tcl_CreateCommand(interp, "puts",        WinPutsCmd,      (ClientData)sp, NULL);
        Tcl_CreateCommand(interp, "getusername", WinGetUserName,  (ClientData)sp, NULL);
#else
	Tcl_SetVar(interp, "win32", "0", TCL_GLOBAL_ONLY);
#endif
	Tk_DefineBitmap(interp, Tk_GetUid("ucl"), ucl_bits, ucl_width, ucl_height);
	Tk_DefineBitmap(interp, Tk_GetUid("microphone"), mic_bits, mic_width, mic_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd"), cd_bits, cd_width, cd_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker"), speaker_bits, speaker_width, speaker_height);
	Tk_DefineBitmap(interp, Tk_GetUid("headphone"), head_bits, head_width, head_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out"), line_out_bits, line_out_width, line_out_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in"), line_in_bits, line_in_width, line_in_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_small"), rat_small_bits, rat_small_width, rat_small_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_med"),   rat_med_bits, rat_med_width, rat_med_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat2"), rat2_bits, rat2_width, rat2_height);

	ui_send(sp->ui_script);
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT)) {
		/* Processing Tcl events, to allow the UI to initialize... */
	};

	ecname = xstrdup(mbus_encode_str(cname));
	sprintf(args, "%s", ecname); 					mbus_qmsg(sp->mbus_engine_chan, "my.cname",    args);
	sprintf(args, "%s %d %d", sp->maddress, sp->rtp_port, sp->ttl); mbus_qmsg(sp->mbus_engine_chan, "address",     args);
	sprintf(args, "%s %s", ecname, mbus_encode_str(RAT_VERSION));	mbus_qmsg(sp->mbus_engine_chan, "source.tool", args);
#ifndef NDEBUG
	mbus_qmsg(sp->mbus_engine_chan, "debug", "");
#endif
	xfree(ecname);

	ui_codecs(sp);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "load.settings", "", TRUE);

	Tcl_ResetResult(interp);
	return TRUE;
}

