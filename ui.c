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
/* The tcl/tk includes have to go after config.h, else we get warnings on
 * solaris 2.5.1, due to buggy system header files included by config.h [csp]
 */
#include <tcl.h>
#include <tk.h>

extern Tcl_Interp	*interp;
extern char		init_ui_script[];
extern char		init_ui_small_script[];
extern char		TCL_LIBS[];

#ifdef WIN32
int WinPutRegistry(ClientData, Tcl_Interp*, int ac, char** av);
int WinGetRegistry(ClientData, Tcl_Interp*, int ac, char** av);
int WinPutsCmd(ClientData, Tcl_Interp*, int ac, char** av);
int WinGetUserName(ClientData, Tcl_Interp*, int ac, char** av);
#endif

Tcl_Interp *interp;	/* Interpreter for application. */

static char		 args[1000];

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

/*
 * Update the on screen information for the given participant
 */

void
ui_info_update_name(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->name));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_name", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_cname(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_exists", args, TRUE);
}

void
ui_info_update_email(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->email));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_email", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_phone(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->phone));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_phone", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_loc(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->loc));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_loc", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_tool(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->tool));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_tool", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_remove(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_remove", args, TRUE);
}

void
ui_info_activate(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_active_now", args, FALSE);
}

void
ui_info_gray(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_active_recent", args, FALSE);
}

void
ui_info_deactivate(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_inactive", args, FALSE);
}

void
update_stats(rtcp_dbentry *e, session_struct *sp)
{
	char	 encoding[100], *p;
	int	 l;
	codec_t	*cp;

	if (e->encs[0] != -1) {
		cp = get_codec(e->encs[0]);
		strcpy(encoding, cp->name);
		for (l = 1, p = encoding; l < 2 && e->encs[l] != -1; l++) {
			p += strlen(p);
			*p++ = '+';
			cp = get_codec(e->encs[l]);
			if (cp != NULL) {
				strcpy(p, cp->name);
			} else {
				*p++ = '?';
			}
		}
	} else {
		*encoding = 0;
	}

	sprintf(args, "%s %s", mbus_encode_str(e->sentry->cname), encoding);                       
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_encoding", args, FALSE);
	sprintf(args, "%s %ld", mbus_encode_str(e->sentry->cname), (e->lost_frac * 100) >> 8); 
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "source_loss_to_me", args, FALSE);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_device", "mic", FALSE);
		break;
	case AUDIO_LINE_IN:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_device", "line_in", FALSE);
		break;
	case AUDIO_CD:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_device", "cd", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_unmute", "", FALSE);
	} else {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_mute", "", FALSE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_device", "speaker", FALSE);
		break;
	case AUDIO_HEADPHONE:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_device", "head", FALSE);
		break;
	case AUDIO_LINE_OUT:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_device", "line_out", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_unmute", "", FALSE);
	} else {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_mute", "", FALSE);
	}
}

void
ui_input_level(int level, session_struct *sp)
{
	static int	ol;

	if (level > 15)
		level = 15;
	if (ol == level)
		return;

	sprintf(args, "%d", level);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "powermeter_input", args, FALSE);
	ol = level;
}

void
ui_output_level(int level, session_struct *sp)
{
	static int	ol;

	if (level > 15) level = 15;
	if (ol == level) return;

	sprintf(args, "%d", level);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "powermeter_output", args, FALSE);
	ol = level;
}
 
static void
ui_repair(int mode, session_struct *sp)
{
        switch(mode) {
        case REPAIR_NONE:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "repair", "None", FALSE);
                break;
        case REPAIR_REPEAT:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "repair", "PacketRepetition", FALSE);
                break;
	case REPAIR_PATTERN_MATCH:
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "repair", "PatternMatching", FALSE);
                break;
        }
}

/*
 * This updates the look of the user interface when we get control
 * of the audio device. It is called only once...
 */
void
ui_update(session_struct *sp)
{
	static   int done=0;
	codec_t	*cp;

	/*XXX solaris seems to give a different volume back to what we   */
	/*    actually set.  So don't even ask if it's not the first time*/
	if (done==0) {
	        sprintf(args, "%d", audio_get_volume(sp->audio_fd)); mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_gain", args, TRUE);
		sprintf(args, "%d", audio_get_gain(sp->audio_fd));   mbus_send(sp->mbus_engine, sp->mbus_ui_addr,  "input_gain", args, TRUE);
	} else {
	        sprintf(args, "%d", sp->output_gain); mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_gain", args, TRUE);
		sprintf(args, "%d", sp->input_gain ); mbus_send(sp->mbus_engine, sp->mbus_ui_addr,  "input_gain", args, TRUE);
	}

	ui_update_output_port(sp);
	if (sp->playing_audio) {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_unmute", "", TRUE);
	} else {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "output_mute", "", TRUE);
	}

	ui_update_input_port(sp);
	if (sp->sending_audio) {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_unmute", "", TRUE);
	} else {
		mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "input_mute", "", TRUE);
	}

	if (sp->mode != TRANSCODER) {
		/* If we're using a real audio device, check if it's half duplex... */
		if (audio_duplex(sp->audio_fd) == FALSE) {
			sp->voice_switching = MIKE_MUTES_NET;
			mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "half_duplex", "", TRUE);
		}
	}

	cp = get_codec(sp->encodings[0]);
	sprintf(args, "%s", mbus_encode_str(cp->name));
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "primary", args, TRUE);
	if (sp->num_encodings > 1) {
		cp = get_codec(sp->encodings[1]);
		sprintf(args, "%s", mbus_encode_str(cp->name));
	} else {
		sprintf(args, "\"NONE\"");
	}
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "redundancy", args, TRUE);
	done=1;
}

void
ui_show_audio_busy(session_struct *sp)
{
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "disable_audio_ctls", "", TRUE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "enable_audio_ctls", "", TRUE);
}

static int
mbus_send_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	session_struct *sp = (session_struct *) ttp;

	if (argc != 4) {
		i->result = "cb_send <reliable> <cmnd> <args>";
		return TCL_ERROR;
	}

	mbus_send(sp->mbus_ui, sp->mbus_engine_addr, argv[2], argv[3], strcmp(argv[1], "R") == 0);
	return TCL_OK;
}

static int
mbus_encode_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	if (argc != 2) {
		i->result = "mbus_encode_str <str>";
		return TCL_ERROR;
	}
	i->result = mbus_encode_str(argv[1]);
	return TCL_OK;
}

#include "xbm/ucl.xbm"
#include "xbm/mic.xbm"
#include "xbm/mic_mute.xbm"
#include "xbm/cd.xbm"
#include "xbm/cd_mute.xbm"
#include "xbm/speaker.xbm"
#include "xbm/speaker_mute.xbm"
#include "xbm/head.xbm"
#include "xbm/head_mute.xbm"
#include "xbm/line_out.xbm"
#include "xbm/line_out_mute.xbm"
#include "xbm/line_in.xbm"
#include "xbm/line_in_mute.xbm"
#include "xbm/rat_small.xbm"
#include "xbm/rat_med.xbm"
#include "xbm/rat2.xbm"

static int synchronize = 0;
static char *name      = NULL;
static char *display   = NULL;
static char *geometry  = NULL;

static Tk_ArgvInfo argTable[] = {
    {"-geometry",   TK_ARGV_STRING,   (char *) NULL, (char *) &geometry,    "Initial geometry for window"},
    {"-display",    TK_ARGV_STRING,   (char *) NULL, (char *) &display,     "Display to use"},
    {"-name",       TK_ARGV_STRING,   (char *) NULL, (char *) &name,        "Name to use for application"},
    {"-Xsync",      TK_ARGV_CONSTANT, (char *) 1,    (char *) &synchronize, "Use synchronous mode for display server"},
    {(char *) NULL, TK_ARGV_END,      (char *) NULL, (char *) NULL,         (char *) NULL}
};

extern void TkCreateXEventSource(void);

int
TkPlatformInit(Tcl_Interp *interp)
{
        Tcl_SetVar(interp, "tk_library", ".", TCL_GLOBAL_ONLY);
#ifndef WIN32
        TkCreateXEventSource();
#endif
        return (TCL_OK);
}

void
usage()
{
	int	argc = 2;
	char	*argv[3];
	
	argv[0] = "rat";
	argv[1] = "-help";
	argv[2] = 0;
	interp = Tcl_CreateInterp();
	if (Tk_ParseArgv(interp, (Tk_Window) NULL, &argc, argv, argTable, 0) != TCL_OK) {
		fprintf(stderr, "%s\n", interp->result);
		exit(1);
	}
	exit(1);
}

int
ui_init(session_struct *sp, char *cname, int argc, char **argv)
{
	char		*cmd_line_args, buffer[10];

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

	if (geometry != NULL)
		Tcl_SetVar(interp, "geometry", geometry, TCL_GLOBAL_ONLY);

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
	Tk_DefineBitmap(interp, Tk_GetUid("mic"), mic_bits, mic_width, mic_height);
	Tk_DefineBitmap(interp, Tk_GetUid("mic_mute"), mic_mute_bits, mic_mute_width, mic_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd"), cd_bits, cd_width, cd_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd_mute"), cd_mute_bits, cd_mute_width, cd_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker"), speaker_bits, speaker_width, speaker_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker_mute"), speaker_mute_bits, speaker_mute_width, speaker_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("head"), head_bits, head_width, head_height);
	Tk_DefineBitmap(interp, Tk_GetUid("head_mute"), head_mute_bits, head_mute_width, head_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out"), line_out_bits, line_out_width, line_out_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out_mute"), line_out_mute_bits, line_out_mute_width, line_out_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in"), line_in_bits, line_in_width, line_in_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in_mute"), line_in_mute_bits, line_in_mute_width, line_in_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_small"), rat_small_bits, rat_small_width, rat_small_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_med"),   rat_med_bits, rat_med_width, rat_med_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat2"), rat2_bits, rat2_width, rat2_height);

	sprintf(args, "set ratversion {%s}", RAT_VERSION);
	ui_send(args);
	ui_send(sp->ui_script);

	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "init", "", TRUE);
	sprintf(args, "%s", cname); 					mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "my_cname",       args, TRUE);
	sprintf(args, "%s %d %d", sp->maddress, sp->rtp_port, sp->ttl); mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "address",        args, TRUE);
        sprintf(args, "%d", sp->detect_silence); 			mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "detect_silence", args, TRUE);
	sprintf(args, "%d", sp->agc_on); 				mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "agc",            args, TRUE);
#ifndef NDEBUG
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "debug", "", TRUE);
#endif

	ui_repair(sp->repair, sp);
	update_lecture_mode(sp);

	Tcl_ResetResult(interp);
	return TRUE;
}

void
update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	sprintf(args, "%d", sp->lecture);
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "lecture_mode", args, TRUE);
}

