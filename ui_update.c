/*
 * FILE:    ui_update.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * This file contains routines which update the user interface. There is no 
 * direct connection between the user interface and the media engine now, all
 * updates are done via the conference bus.
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
#include "ui_update.h"
#include "mix.h"
#include "transmit.h"
#include "speaker_table.h"

static char args[1000];

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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.name", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_cname(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.exists", args, TRUE);
}

void
ui_info_update_email(rtcp_dbentry *e, session_struct *sp)
{
	char *cname, *arg;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->email));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.email", args, TRUE);
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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.phone", args, TRUE);
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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.loc", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_update_tool(rtcp_dbentry *e, session_struct *sp)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->tool));

	sprintf(args, "%s %s", cname, arg);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.tool", args, TRUE);
	xfree(cname);
	xfree(arg);
}

void
ui_info_remove(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.remove", args, TRUE);
}

void
ui_info_activate(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.active.now", args, FALSE);
}

void
ui_info_gray(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.active.recent", args, FALSE);
}

void
ui_info_deactivate(rtcp_dbentry *e, session_struct *sp)
{
	if (e->sentry->cname == NULL) return;
	sprintf(args, "%s", mbus_encode_str(e->sentry->cname));
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.inactive", args, FALSE);
}

void
update_stats(rtcp_dbentry *e, session_struct *sp)
{
	char	 encoding[100], *p, *my_cname, *their_cname;
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

	sprintf(args, "%s %s", mbus_encode_str(e->sentry->cname), encoding);                       
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.codec", args, FALSE);

	my_cname    = strdup(mbus_encode_str(sp->db->my_dbe->sentry->cname));
	their_cname = strdup(mbus_encode_str(e->sentry->cname));
	sprintf(args, "%s %s %ld", my_cname, their_cname, (e->lost_frac * 100) >> 8);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.packet.loss", args, FALSE);
	free(my_cname);
	free(their_cname);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_qmsg(sp->mbus_engine_chan, "input.port", "microphone");
		break;
	case AUDIO_LINE_IN:
		mbus_qmsg(sp->mbus_engine_chan, "input.port", "line_in");
		break;
	case AUDIO_CD:
		mbus_qmsg(sp->mbus_engine_chan, "input.port", "cd");
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input.mute", "0", FALSE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "input.mute", "1", FALSE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_qmsg(sp->mbus_engine_chan, "output.port", "speaker");
		break;
	case AUDIO_HEADPHONE:
		mbus_qmsg(sp->mbus_engine_chan, "output.port", "headphone");
		break;
	case AUDIO_LINE_OUT:
		mbus_qmsg(sp->mbus_engine_chan, "output.port", "line_out");
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output.mute", "0", FALSE);
	} else {
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "output.mute", "1", FALSE);
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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "powermeter.input", args, FALSE);
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
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "powermeter.output", args, FALSE);
	ol = level;
}

static void
ui_repair(session_struct *sp)
{
        switch(sp->repair) {
        case REPAIR_NONE:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "None", FALSE);
                break;
        case REPAIR_REPEAT:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "Packet Repetition", FALSE);
                break;
	case REPAIR_PATTERN_MATCH:
		mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "repair", "Pattern Matching", FALSE);
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
        mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "channel.code", args, TRUE);
}


void
ui_update(session_struct *sp)
{
	static   int done=0;
	codec_t	*cp;

	if (!sp->ui_on) {
		/* UI is disabled, do nothing... */
		return;
	}

        /* we want to dump ALL settings here to ui */
        /* Device settings */

	/*XXX solaris seems to give a different volume back to what we   */
	/*    actually set.  So don't even ask if it's not the first time*/
	if (done==0) {
	        sprintf(args, "%d", audio_get_volume(sp->audio_fd)); mbus_qmsg(sp->mbus_engine_chan, "output.gain", args);
		sprintf(args, "%d", audio_get_gain(sp->audio_fd));   mbus_qmsg(sp->mbus_engine_chan,  "input.gain", args);
	} else {
	        sprintf(args, "%d", sp->output_gain); mbus_qmsg(sp->mbus_engine_chan, "output.gain", args);
		sprintf(args, "%d", sp->input_gain ); mbus_qmsg(sp->mbus_engine_chan,  "input.gain", args);
	}

	ui_update_output_port(sp);
	if (sp->playing_audio) {
		mbus_qmsg(sp->mbus_engine_chan, "output.mute", "0");
	} else {
		mbus_qmsg(sp->mbus_engine_chan, "output.mute", "1");
	}

	ui_update_input_port(sp);
	if (sp->sending_audio) {
		mbus_qmsg(sp->mbus_engine_chan, "input.mute", "0");
	} else {
		mbus_qmsg(sp->mbus_engine_chan, "input.mute", "1");
	}

        /* Transmission Options */
	if (sp->mode != TRANSCODER) {
		/* If we're using a real audio device, check if it's half duplex... */
		if (audio_duplex(sp->audio_fd) == FALSE) {
			sp->voice_switching = MIKE_MUTES_NET;
			mbus_qmsg(sp->mbus_engine_chan, "half.duplex", "");
		}
	}
        
	cp = get_codec(sp->encodings[0]);
	sprintf(args, "%s", mbus_encode_str(cp->name));
	mbus_qmsg(sp->mbus_engine_chan, "primary", args);
        sprintf(args, "%d", get_units_per_packet(sp));
	mbus_qmsg(sp->mbus_engine_chan, "rate", args);

	/* The following all call mbus_send() with the correct arguments to send 
	 * the messages we have queued here...
	 */
        ui_update_redundancy(sp);
        ui_update_interleaving(sp);
        ui_update_channel(sp);
        ui_repair(sp);

	done=1;
}

void
ui_show_audio_busy(session_struct *sp)
{
	if (sp->ui_on) mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "disable.audio.ctls", "", TRUE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	if (sp->ui_on) mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "enable.audio.ctls", "", TRUE);
}


void
update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	sprintf(args, "%d", sp->lecture);
	if (sp->ui_on) mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "lecture.mode", args, TRUE);
}

void
ui_update_powermeters(session_struct *sp, struct s_mix_info *ms, int elapsed_time)
{
	static u_int32 power_time = 0;

	if (power_time == 0) {
		if (sp->meter) {
			mix_update_ui(ms, sp);
		}
		clear_active_senders(sp);
	}
	if (power_time > 400) {
		if (sp->sending_audio) {
			transmitter_update_ui(sp);
		}
		power_time = 0;
	} else {
		power_time += elapsed_time;
	}
}

void
ui_update_loss(session_struct *sp, char *srce, char *dest, int loss)
{
	char	*srce_e = xstrdup(mbus_encode_str(srce));
	char	*dest_e = xstrdup(mbus_encode_str(dest));

	sprintf(args, "%s %s %d", srce_e, dest_e, loss);
	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "source.packet.loss", args, FALSE);

	xfree(srce_e);
	xfree(dest_e);
}

void
ui_update_reception(session_struct *sp, char *cname, u_int32 recv, u_int32 lost, u_int32 misordered, double jitter)
{
	char	*cname_e = mbus_encode_str(cname);
	char	*args    = (char *) xmalloc(29 + strlen(cname_e));

	sprintf(args, "%s %6ld %6ld %6ld %6f", cname_e, recv, lost, misordered, jitter);
	mbus_qmsg(sp->mbus_engine_chan, "source.reception", args);
	xfree(args);
}

void
ui_update_duration(session_struct *sp, char *cname, int duration)
{
	char	*cname_e = mbus_encode_str(cname);
	char	*args    = (char *) xmalloc(5 + strlen(cname_e));

	sprintf(args, "%s %3d", cname_e, duration);
	mbus_qmsg(sp->mbus_engine_chan, "source.packet.duration", args);
	xfree(args);
}

