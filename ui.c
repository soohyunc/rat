/*
 * FILE: ui.c
 * PROGRAM: Rat
 * AUTHOR: Isidor Kouvelas + Colin Perkins + Orion Hodson
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
#include "ui.h"
#include "session.h"
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "tcl.h"
#include "tk.h"
#include "confbus.h"
#include "repair.h"
#include "codec.h"
#include "audio.h"

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

static char		 comm[10000];

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

void 
ui_recv_ack(char *src, int seqnum, session_struct *sp)
{
	printf("Got an ACK in the UI %d\n", seqnum);
}

void 
ui_recv(char *srce, char *mesg, session_struct *sp)
{
	/* This function is called by cb_poll() when a conference bus message */
	/* is received, which should be processed by the UI. It should not be */
	/* called directly. It calls cb_recv() in the Tcl code, to process    */
	/* the message.                                                       */
	/* Note: The mesg received has all "[" and "]" characters stripped,   */
	/*       to avoid potential problems with command substitution in the */
	/*       Tcl scripts. This is probably not the best way to do this.   */
	char *buffer;
	int   i;

#ifdef DEBUG_CONFBUS
	printf("ConfBus: %s --> %s : %s\n", srce, sp->cb_uiaddr, mesg);
#endif

	for (i=0; i<strlen(mesg); i++) {
		if (mesg[i] == '[') mesg[i] = '(';
		if (mesg[i] == ']') mesg[i] = ')';
	}

	buffer = (char *) xmalloc(strlen(srce) + strlen(mesg) + 12);
	sprintf(buffer, "cb_recv %s \"%s\"", srce, mesg);
	ui_send(buffer);
	xfree(buffer);
}

/*
 * Update the on screen information for the given participant
 */

void
ui_info_update_name(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx name %s", e->ssrc, e->sentry->name);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_update_cname(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx cname %s", e->ssrc, e->sentry->cname);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_update_email(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx email %s", e->ssrc, e->sentry->email);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_update_phone(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx phone %s", e->ssrc, e->sentry->phone);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_update_loc(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx loc %s", e->ssrc, e->sentry->loc);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_update_tool(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx tool %s", e->ssrc, e->sentry->tool);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_remove(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx remove", e->ssrc);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_activate(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx active now", e->ssrc);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_gray(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx active recent", e->ssrc);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

void
ui_info_deactivate(rtcp_dbentry *e, session_struct *sp)
{
	sprintf(comm, "ssrc %lx inactive", e->ssrc);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
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
		for (l = 1, p = encoding; l < 10 && e->encs[l] != -1; l++) {
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

	if (sp->ui_on) {
		sprintf(comm, "ssrc %lx encoding %s", e->ssrc, encoding);                       
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
		sprintf(comm, "ssrc %lx loss_to_me   %ld", e->ssrc, (e->lost_frac * 100) >> 8); 
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	}
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "input device mic", FALSE);
		break;
	case AUDIO_LINE_IN:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "input device line_in", FALSE);
		break;
	case AUDIO_CD:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "input device cd", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "input unmute", FALSE);
	} else {
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "input mute", FALSE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "output device speaker", FALSE);
		break;
	case AUDIO_HEADPHONE:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "output device head", FALSE);
		break;
	case AUDIO_LINE_OUT:
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "output device line_out", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "output unmute", FALSE);
	} else {
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "output mute", FALSE);
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

	sprintf(comm, "powermeter input %d", level);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	ol = level;
}

void
ui_output_level(int level, session_struct *sp)
{
	static int	ol;

	if (level > 15) level = 15;
	if (ol == level) return;

	sprintf(comm, "powermeter output %d", level);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	ol = level;
}
 
static void
ui_repair(int mode, session_struct *sp)
{
        switch(mode) {
        case REPAIR_NONE:
                cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "repair None", FALSE);
                break;
        case REPAIR_REPEAT:
                cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "repair {Packet Repetition}", FALSE);
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
	char	 cmd[100];
	static   int done=0;
	codec_t	*cp;

	if (done==0) {
	  /*XXX solaris seems to give a different volume back to what we   */
	  /*    actually set.  So don't even ask if it's not the first time*/
	        sprintf(cmd, "output gain %d", audio_get_volume(sp->audio_fd)); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
		sprintf(cmd, "input  gain %d", audio_get_gain(sp->audio_fd));   cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	} else {
		sprintf(cmd, "output gain %d", sp->output_gain); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
		sprintf(cmd, "input  gain %d", sp->input_gain);  cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	}

	ui_update_output_port(sp);
	if (sp->playing_audio) {
		sprintf(cmd, "output unmute"); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	} else {
		sprintf(cmd, "output mute"); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	}

	ui_update_input_port(sp);
	if (sp->sending_audio) {
		sprintf(cmd, "input unmute"); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	} else {
		sprintf(cmd, "input mute"); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	}

	if (sp->mode != TRANSCODER) {
		/* If we're using a real audio device, check if it's half duplex... */
		if (audio_duplex(sp->audio_fd) == FALSE) {
			sp->voice_switching = MIKE_MUTES_NET;
			cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "half_duplex", FALSE);
		}
	}

	cp = get_codec(sp->encodings[0]);
	sprintf(cmd, "primary {%s}", cp->name);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	if (sp->num_encodings > 1) {
		cp = get_codec(sp->encodings[1]);
		sprintf(cmd, "redundancy {%s}", cp->name);
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, cmd, FALSE);
	} else {
		cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "redundancy NONE", FALSE);
	}
	done=1;
}

void
ui_show_audio_busy(session_struct *sp)
{
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "disable_audio_ctls", FALSE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "enable_audio_ctls", FALSE);
}

static int
cb_send_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	session_struct *sp = (session_struct *) ttp;
	int		r;

	if (argc != 4) {
		i->result = "cb_send <reliable> <dest> <message>";
		return TCL_ERROR;
	}

	r = strcmp(argv[1], "R") == 0;

	cb_send(sp, sp->cb_uiaddr, argv[2], argv[3], r);
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
ui_init(session_struct *sp, int argc, char **argv)
{
	char		*args, buffer[10];

	Tcl_FindExecutable(argv[0]);
	interp = Tcl_CreateInterp();

	args = Tcl_Merge(argc - 1, argv + 1);
	Tcl_SetVar(interp, "argv", args, TCL_GLOBAL_ONLY);

#ifndef WIN32
	ckfree(args);
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

	Tcl_CreateCommand(interp, "cb_send",	cb_send_cmd,		(ClientData) sp, NULL);
#ifdef WIN32
	Tcl_SetVar(interp, "win32", "1", TCL_GLOBAL_ONLY);
	Tcl_CreateCommand(interp, "putregistry", WinPutRegistry, (ClientData)sp, NULL);
        Tcl_CreateCommand(interp, "getregistry", WinGetRegistry, (ClientData)sp, NULL);
	Tcl_CreateCommand(interp, "puts", WinPutsCmd, (ClientData)sp, NULL);
        Tcl_CreateCommand(interp, "getusername", WinGetUserName, (ClientData)sp, NULL);
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

	sprintf(comm, "set ratversion {%s}", RAT_VERSION);
	ui_send(comm);
	ui_send(sp->ui_script);

	sprintf(comm, "init %s %s", sp->cb_myaddr, sp->cb_uiaddr);                         cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	sprintf(comm, "ssrc %lx cname %s", sp->db->myssrc, sp->db->my_dbe->sentry->cname); cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	sprintf(comm, "my_ssrc %lx", sp->db->myssrc);		  			   cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	sprintf(comm, "address %s %d %d", sp->maddress, sp->rtp_port, sp->ttl); 	   cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
        sprintf(comm, "detect_silence %d", (sp->detect_silence) ? 1 : 0);              	   cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
	sprintf(comm, "agc %d", sp->agc_on);                                               cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
#ifndef NDEBUG
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, "debug", FALSE);
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
	sprintf(comm, "lecture_mode %d", sp->lecture);
	cb_send(sp, sp->cb_myaddr, sp->cb_uiaddr, comm, FALSE);
}

