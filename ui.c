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
 *
 * Note: We must be careful, since the Mbus code uses the CNAME
 *       for communication with the UI. In a few cases we have
 *       valid state for a participant (ie: we've received an 
 *       RTCP packet for that SSRC), but do NOT know their CNAME.
 *       (For example, if the first packet we receive from a source
 *       is an RTCP BYE piggybacked with an empty RR). In those 
 *       cases, we just ignore the request and send nothing to the 
 *       UI. [csp]
 */

void
ui_info_update_name(rtcp_dbentry *e, session_struct *sp)
{
	char *cname, *arg;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->name));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_name", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_cname(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_exists", args, TRUE);
}

void
ui_info_update_email(rtcp_dbentry *e, session_struct *sp)
{
	char *cname, *arg;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->email));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_email", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_phone(rtcp_dbentry *e, session_struct *sp)
{
	char *cname, *arg;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->phone));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_phone", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_loc(rtcp_dbentry *e, session_struct *sp)
{
	char *cname, *arg;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->loc));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_loc", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_tool(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->tool));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_tool", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_remove(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_remove", args, TRUE);
}

void
ui_info_activate(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_active_now", args, FALSE);
}

void
ui_info_gray(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_active_recent", args, FALSE);
}

void
ui_info_deactivate(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_inactive", args, FALSE);
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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_encoding", args, FALSE);
	sprintf(args, "%s %ld", mbus_encode_str(e->sentry->cname), (e->lost_frac * 100) >> 8); 
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_loss_to_me", args, FALSE);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_port", "microphone", FALSE);
		break;
	case AUDIO_LINE_IN:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_port", "line_in", FALSE);
		break;
	case AUDIO_CD:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_port", "cd", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_mute", "0", FALSE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_mute", "1", FALSE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_port", "speaker", FALSE);
		break;
	case AUDIO_HEADPHONE:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_port", "headphone", FALSE);
		break;
	case AUDIO_LINE_OUT:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_port", "line_out", FALSE);
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_mute", "0", FALSE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_mute", "1", FALSE);
	}
}

void
ui_input_level(int level, session_struct *sp)
{
	static int	ol;
        assert(level>=0 && level <=100);

	if (ol == level)
		return;
	sprintf(args, "%d", level);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "powermeter_input", args, FALSE);
	ol = level;
}

void
ui_output_level(int level, session_struct *sp)
{
	static int	ol;
        assert(level>=0 && level <=100);

	if (ol == level) 
                return;
	sprintf(args, "%d", level);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "powermeter_output", args, FALSE);
	ol = level;
}

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

	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "codec_supported", arg, TRUE);
}

static void
ui_repair(session_struct *sp)
{
        switch(sp->repair) {
        case REPAIR_NONE:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "None", FALSE);
                break;
        case REPAIR_REPEAT:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "PacketRepetition", FALSE);
                break;
	case REPAIR_PATTERN_MATCH:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "PatternMatching", FALSE);
                break;
        }
}

void
ui_update_interleaving(session_struct *sp)
{
        int pt, isep;
        char buf[128], *sep=NULL, *dummy, args[80];

        pt = get_cc_pt(sp,"INTERLEAVER");
        if (pt != -1) {
                query_channel_coder(sp, pt, buf, 128);
                dummy  = strtok(buf,"/");
                dummy  = strtok(NULL,"/");
                sep    = strtok(NULL,"/");
        } else {
                dprintf("Could not find interleaving channel coder!\n");
        }
        
        if (sep != NULL) {
                isep = atoi(sep);
        } else {
                isep = 4; /* default */
        }

        sprintf(args,"%d",isep);
        mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "interleaving", args, TRUE);        
}

void
ui_update_redundancy(session_struct *sp)
{
        int  pt;
        int  ioff;
        char buf[128], *codec=NULL, *offset=NULL, *dummy, args[80];

        pt = get_cc_pt(sp,"REDUNDANCY");
        if (pt != -1) { 
                query_channel_coder(sp, pt, buf, 128);
                dummy  = strtok(buf,"/");
                dummy  = strtok(NULL,"/");
                codec  = strtok(NULL,"/");
                offset = strtok(NULL,"/");
        } else {
                dprintf("Could not find redundant channel coder!\n");
        } 

        if (codec != NULL && offset != NULL) {
                ioff  = atoi(offset);
                ioff /= get_units_per_packet(sp);
        } else {
                codec_t *pcp;
                pcp   = get_codec(sp->encodings[0]);
                codec = pcp->name;
                ioff  = 1;
        } 

        sprintf(args,"%s %d", mbus_encode_str(codec), ioff);
        mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "redundancy", args, TRUE);
}

static void 
ui_update_channel(session_struct *sp) 
{
        cc_coder_t *ccp;
        char args[80];

        ccp = get_channel_coder(sp->cc_encoding);
        assert(ccp != NULL);
        switch(ccp->name[0]) {
        case 'V':
                sprintf(args, mbus_encode_str("No Loss Protection"));
                break;
        case 'R':
                sprintf(args, mbus_encode_str("Redundancy"));
                break;
        case 'I':
                sprintf(args, mbus_encode_str("Interleaving"));
                break;
        default:
                dprintf("Channel coding failed mapping.\n");
                return;
        }
        mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "channel_code", args, TRUE);
}


void
ui_update(session_struct *sp)
{
	static   int done=0;
	codec_t	*cp;

        /* we want to dump ALL settings here to ui */
        /* Device settings */


	/*XXX solaris seems to give a different volume back to what we   */
	/*    actually set.  So don't even ask if it's not the first time*/
	if (done==0) {
	        sprintf(args, "%d", audio_get_volume(sp->audio_fd)); mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_gain", args, TRUE);
		sprintf(args, "%d", audio_get_gain(sp->audio_fd));   mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr,  "input_gain", args, TRUE);
	} else {
	        sprintf(args, "%d", sp->output_gain); mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_gain", args, TRUE);
		sprintf(args, "%d", sp->input_gain ); mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr,  "input_gain", args, TRUE);
	}

	ui_update_output_port(sp);
	if (sp->playing_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_mute", "0", TRUE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output_mute", "1", TRUE);
	}

	ui_update_input_port(sp);
	if (sp->sending_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_mute", "0", TRUE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input_mute", "1", TRUE);
	}

        /* Transmission Options */
	if (sp->mode != TRANSCODER) {
		/* If we're using a real audio device, check if it's half duplex... */
		if (audio_duplex(sp->audio_fd) == FALSE) {
			sp->voice_switching = MIKE_MUTES_NET;
			mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "half_duplex", "", TRUE);
		}
	}
        
	cp = get_codec(sp->encodings[0]);
	sprintf(args, "%s", mbus_encode_str(cp->name));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "primary", args, TRUE);
        sprintf(args, "%d", get_units_per_packet(sp));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "rate", args, TRUE);

        ui_update_redundancy(sp);
        ui_update_interleaving(sp);
        ui_update_channel(sp);
        ui_repair(sp);

	done=1;

}

void
ui_show_audio_busy(session_struct *sp)
{
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "disable_audio_ctls", "", TRUE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "enable_audio_ctls", "", TRUE);
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

void
usage(void)
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
	Tk_DefineBitmap(interp, Tk_GetUid("microphone"), mic_bits, mic_width, mic_height);
	Tk_DefineBitmap(interp, Tk_GetUid("microphone_mute"), mic_mute_bits, mic_mute_width, mic_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd"), cd_bits, cd_width, cd_height);
	Tk_DefineBitmap(interp, Tk_GetUid("cd_mute"), cd_mute_bits, cd_mute_width, cd_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker"), speaker_bits, speaker_width, speaker_height);
	Tk_DefineBitmap(interp, Tk_GetUid("speaker_mute"), speaker_mute_bits, speaker_mute_width, speaker_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("headphone"), head_bits, head_width, head_height);
	Tk_DefineBitmap(interp, Tk_GetUid("headphone_mute"), head_mute_bits, head_mute_width, head_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out"), line_out_bits, line_out_width, line_out_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_out_mute"), line_out_mute_bits, line_out_mute_width, line_out_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in"), line_in_bits, line_in_width, line_in_height);
	Tk_DefineBitmap(interp, Tk_GetUid("line_in_mute"), line_in_mute_bits, line_in_mute_width, line_in_mute_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_small"), rat_small_bits, rat_small_width, rat_small_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat_med"),   rat_med_bits, rat_med_width, rat_med_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rat2"), rat2_bits, rat2_width, rat2_height);

	ui_send(sp->ui_script);
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT)) {
		/* Processing Tcl events, to allow the UI to initialize... */
	};

	ecname = xstrdup(mbus_encode_str(cname));
	sprintf(args, "%s", ecname); 					mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "my_cname",       args, TRUE);
	sprintf(args, "%s %d %d", sp->maddress, sp->rtp_port, sp->ttl); mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "address",        args, TRUE);
	sprintf(args, "%s %s", ecname, mbus_encode_str(RAT_VERSION));	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source_tool",    args, TRUE);
#ifndef NDEBUG
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "debug", "", TRUE);
#endif
	xfree(ecname);

	ui_codecs(sp);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "load_settings", "", TRUE);

	Tcl_ResetResult(interp);
	return TRUE;
}

void
update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	sprintf(args, "%d", sp->lecture);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "lecture_mode", args, TRUE);
}











