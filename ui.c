/*
 * FILE:    ui_control.c
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

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "session.h"
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "receive.h"
#include "codec.h"
#include "convert.h"
#include "audio.h"
#include "audio_fmt.h"
#include "mbus.h"
#include "mbus_engine.h"
#include "channel.h"
#include "mix.h"
#include "transmit.h"
#include "ui.h"
#include "timers.h"
#include "render_3D.h"

static char *mbus_name_engine = NULL;
static char *mbus_name_ui     = NULL;
static char *mbus_name_video  = NULL;

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
ui_info_update_name(session_struct *sp, rtcp_dbentry *e)
{
	char *cname, *name, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	name  = mbus_encode_str(e->sentry->name);
        args = (char*)xmalloc(strlen(cname) + strlen(name) + 2);
        
	sprintf(args, "%s %s", cname, name);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.name", args, TRUE);
	xfree(cname);
	xfree(name);
        xfree(args);
}

void
ui_info_update_cname(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
        
        cname = mbus_encode_str(e->sentry->cname);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.exists", cname, TRUE);
        xfree(cname);
}

void
ui_info_update_email(session_struct *sp, rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->email);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.email", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_phone(session_struct *sp, rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->phone);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.phone", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_loc(session_struct *sp, rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->loc);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.loc", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_tool(session_struct *sp, rtcp_dbentry *e)
{
	char *cname = mbus_encode_str(e->sentry->cname);
	char *arg   = mbus_encode_str(e->sentry->tool);
        char *args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.tool", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_note(session_struct *sp, rtcp_dbentry *e)
{
	char *cname = mbus_encode_str(e->sentry->cname);
	char *arg   = mbus_encode_str(e->sentry->note);
        char *args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.note", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_mute(session_struct *sp, rtcp_dbentry *e)
{
	char *cname = mbus_encode_str(e->sentry->cname);
        char *args = (char*)xmalloc(strlen(cname) + 4);
	sprintf(args, "%s %2d", cname, e->mute);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.mute", args, TRUE);
	xfree(cname);
        xfree(args);
}

void
ui_info_remove(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.remove", cname, TRUE);
        xfree(cname);
}

void
ui_info_activate(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
        
        cname = mbus_encode_str(e->sentry->cname);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.active", cname, FALSE);
        xfree(cname);
}

void
ui_info_deactivate(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;

	if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.inactive", cname, FALSE);
        xfree(cname);
}

void
ui_info_3d_settings(session_struct *sp, rtcp_dbentry *e)
{
        char *cname, *filter_name, *msg;
        int   azimuth, filter_type, filter_length;

        if (e->render_3D_data == NULL) {
                e->render_3D_data = render_3D_init(sp);
        }

        render_3D_get_parameters(e->render_3D_data, &azimuth, &filter_type, &filter_length);
        cname       = mbus_encode_str(e->sentry->cname);
        filter_name = mbus_encode_str(render_3D_filter_get_name(filter_type));
        msg = (char*)xmalloc(strlen(cname) + strlen(filter_name) + 10);
        sprintf(msg, "%s %s %d %d", cname, filter_name, filter_length, azimuth);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.3d.user.settings", msg, TRUE);
        xfree(cname);
        xfree(filter_name);
        xfree(msg);
}

void
ui_update_stats(session_struct *sp, rtcp_dbentry *e)
{
	char	*my_cname, *their_cname, *args, *mbes;
	codec_t	*pcp;

        if (sp->db->my_dbe->sentry == NULL || sp->db->my_dbe->sentry->cname == NULL) {
                debug_msg("Warning sentry or name == NULL\n");
                return;
        }

	if (e->sentry->cname == NULL) {
		return;
	}

	my_cname    = mbus_encode_str(sp->db->my_dbe->sentry->cname);
	their_cname = mbus_encode_str(e->sentry->cname);

        if (e->enc_fmt) {
		pcp  = get_codec_by_pt(e->enc);
		mbes = mbus_encode_str(pcp->short_name);
                args = (char *) xmalloc(strlen(their_cname) + strlen(mbes) + 2);
                sprintf(args, "%s %s", their_cname, mbes);
        } else {
                args = (char *) xmalloc(strlen(their_cname) + 7 + 2);
                sprintf(args, "%s unknown", their_cname);
        }
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.codec", args, FALSE);
        xfree(args);

        /* args size is for source.packet.loss, tool.rat.audio.buffered size always less */
	args = (char *) xmalloc(strlen(their_cname) + strlen(my_cname) + 11);
        
        sprintf(args, "%s %ld", their_cname, playout_buffer_duration(sp->playout_buf_list, e));
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.audio.buffered", args, FALSE);

	sprintf(args, "%s %s %8ld", my_cname, their_cname, (e->lost_frac * 100) >> 8);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.packet.loss", args, FALSE);

	xfree(my_cname);
	xfree(their_cname);
	xfree(args);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.port", "\"microphone\"", TRUE);
		break;
	case AUDIO_LINE_IN:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.port", "\"line_in\"", TRUE);
		break;	
	case AUDIO_CD:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.port", "\"cd\"", TRUE);
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.mute", "1", TRUE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.port", "speaker", TRUE);
		break;
	case AUDIO_HEADPHONE:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.port", "headphone", TRUE);
		break;
	case AUDIO_LINE_OUT:
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.port", "line_out", TRUE);
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.mute", "1", TRUE);
	}
}

void
ui_input_level(session_struct *sp, int level)
{
	static int	ol;
	char		args[4];

        assert(level>=0 && level <=100);

	if (ol == level) {
		return;
	}

	sprintf(args, "%3d", level);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.powermeter", args, FALSE);
	ol = level;
}

void
ui_output_level(session_struct *sp, int level)
{
	static int	ol;
	char		args[4];
        assert(level>=0 && level <=100);

	if (ol == level) {
                return;
	}

	sprintf(args, "%3d", level);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.powermeter", args, FALSE);
	ol = level;
}

static void
ui_repair(session_struct *sp)
{
	char	*mbes;

        mbes = mbus_encode_str(repair_get_name(sp->repair));
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.channel.repair", mbes, FALSE);
	xfree(mbes);
}

void
ui_update_device_config(session_struct *sp)
{
        char          fmt_buf[64], *mbes;
        const audio_format *af;

        af = audio_get_ifmt(sp->audio_device);
        if (af && audio_format_name(af, fmt_buf, 64)) {
                mbes = mbus_encode_str(fmt_buf);
                mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.format.in", mbes, TRUE);
                xfree(mbes);
        } else {
                debug_msg("Could not get ifmt\n");
        }
        
        af = audio_get_ofmt(sp->audio_device);
        if (af && audio_format_name(af, fmt_buf, 64)) {
                mbes = mbus_encode_str(fmt_buf);
                mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.format.out", mbes, TRUE);
                xfree(mbes);
        } else {
                debug_msg("Could not get ofmt\n");
        }
}

void
ui_update_primary(session_struct *sp)
{
	codec_t *pcp;
        char *mbes;

	pcp = get_codec_by_pt(sp->encodings[0]);
	mbes = mbus_encode_str(pcp->short_name);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.codec", mbes, FALSE);
        xfree(mbes);
}

static void
ui_update_interleaving(session_struct *sp)
{
        int pt, isep, iu;
        char buf[128], *sep=NULL, *units = NULL, *dummy, args[80];

        pt = get_cc_pt(sp,"INTERLEAVER");
        if (pt != -1) {
                query_channel_coder(sp, pt, buf, 128);
                dummy  = strtok(buf,"/");
                units  = strtok(NULL,"/");
                sep    = strtok(NULL,"/");
        } else {
                debug_msg("Could not find interleaving channel coder!\n");
        }
        
        if (units != NULL && sep != NULL) {
                iu   = atoi(units);
                isep = atoi(sep);
        } else {
                iu   = 4;
                isep = 4;
        }

        sprintf(args,"\"interleaved\" %d %d",iu, isep);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.channel.coding", args, TRUE);        
}

static void
ui_update_redundancy(session_struct *sp)
{
        int  pt;
        int  ioff;
        char buf[128]= "", *codec_name=NULL, *offset=NULL, *dummy, *args;

        pt = get_cc_pt(sp,"REDUNDANCY");
        if (pt != -1) { 
                codec_t *cp;
                query_channel_coder(sp, pt, buf, 128);
                if (strlen(buf)) {
                        dummy  = strtok(buf,"/");
                        dummy  = strtok(NULL,"/");
                        codec_name  = strtok(NULL,"/");
                        /* redundant coder returns long name convert to short*/
                        if (codec_name) {
                                cp         = get_codec_by_name(codec_name);
                                assert(cp);
                                codec_name = cp->short_name;
                        }
                        offset = strtok(NULL,"/");
                }
        } else {
                debug_msg("Could not find redundant channel coder!\n");
        } 

        if (codec_name != NULL && offset != NULL) {
                ioff  = atoi(offset);
        } else {
                codec_t *pcp;
                pcp   = get_codec_by_pt(sp->encodings[0]);
                codec_name = pcp->short_name;
                ioff  = 1;
        } 

	codec_name = mbus_encode_str(codec_name);

	args = (char *) xmalloc(strlen(codec_name) + 16);
        sprintf(args,"\"redundant\" %s %2d", codec_name, ioff);
        assert(strlen(args) < (strlen(codec_name) + 16));
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.channel.coding", args, TRUE);
	xfree(codec_name);
        xfree(args);
}

void 
ui_update_channel(session_struct *sp) 
{
        cc_coder_t *ccp;

        ccp = get_channel_coder(sp->cc_encoding);
	if (strcmp(ccp->name, "VANILLA") == 0) {
        	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.channel.coding", "\"none\"", TRUE);
	} else if (strcmp(ccp->name, "REDUNDANCY") == 0) {
                ui_update_redundancy(sp);
        } else if (strcmp(ccp->name, "INTERLEAVER") == 0) {
                ui_update_interleaving(sp);
        } else {
                debug_msg("Channel coding failed mapping (%s)\n", ccp->name);
                abort();
        }
}

void
ui_update_input_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_gain(sp->audio_device)); 
        assert(strlen(args) < 4);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.input.gain", args, TRUE);
}

void
ui_update_output_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_volume(sp->audio_device)); 
        assert(strlen(args) < 4);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.gain", args, TRUE);
}

static void
ui_update_3d_enabled(session_struct *sp)
{
        char args[2];
        sprintf(args, "%d", (sp->render_3d ? 1 : 0));
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.3d.enabled", args, TRUE);
}



static void
ui_devices(session_struct *sp)
{
        int i,nDev;
        char buf[255] = "", *this_dev, *mbes;
        
        nDev = audio_get_number_of_interfaces();
        for(i = 0; i < nDev; i++) {
                this_dev = audio_get_interface_name(i);
                if (this_dev) {
                        strcat(buf, this_dev);
                        strcat(buf, ",");
                }
        }
        i = strlen(buf);
        if (i != 0) buf[i-1] = '\0';

        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.devices", mbes, TRUE);
        xfree(mbes);
}

static void
ui_device(session_struct *sp)
{
        char *mbes, *cur_dev;
        cur_dev = audio_get_interface_name(audio_get_interface());

        if (cur_dev) {
                mbes = mbus_encode_str(cur_dev);
                mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.device", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_sampling_modes(session_struct *sp)
{
	char	*mbes;
        char    modes[255]="";
        char    tmp[22];
        u_int16 rate, channels, support, zap;
        
        for(rate = 8000; rate <=48000; rate += 8000) {
                support = 0;
                if (rate == 24000 || rate == 40000) continue;
                for(channels = 1; channels <= 2; channels++) {
                        if (audio_device_supports(sp->audio_device, rate, channels)) support += channels;
                }
                switch(support) {
                case 3: sprintf(tmp, "%d-kHz,Mono,Stereo ", rate/1000); break; 
                case 2: sprintf(tmp, "%d-kHz,Stereo ", rate/1000);      break;
                case 1: sprintf(tmp, "%d-kHz,Mono ", rate/1000);        break;
                case 0: continue;
                }
                strcat(modes, tmp);
        }

        debug_msg("Sampling modes: %s\n", modes);

        /* Remove trailing space */
        zap = strlen(modes);
        if (zap) {
                zap -= 1;
                modes[zap] = '\0';
        }

	mbes = mbus_encode_str(modes);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.sampling.supported", mbes, TRUE);
	xfree(mbes);
}

void
ui_update(session_struct *sp)
{
	static   int 	done=0;
	char	 	args[4];

	/*XXX solaris seems to give a different volume back to what we   */
	/*    actually set.  So don't even ask if it's not the first time*/
	if (done==0) {
                ui_update_input_gain(sp);
                ui_update_output_gain(sp);
		done=1;
	} else {
	        sprintf(args, "%3d", sp->output_gain); mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.output.gain", args, TRUE);
                assert(strlen(args) < 4);
		sprintf(args, "%3d", sp->input_gain ); mbus_qmsg(sp->mbus_engine_base, mbus_name_ui,  "audio.input.gain", args, TRUE);
                assert(strlen(args) < 4);
	}

        sprintf(args, "%3d", collator_get_units(sp->collator));
        assert(strlen(args) < 4);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.rate", args, TRUE);

	ui_update_output_port(sp);
	ui_update_input_port(sp);
        ui_update_3d_enabled(sp);
        ui_codecs(sp, sp->encodings[0]);
        ui_devices(sp);
        ui_device(sp);
        ui_sampling_modes(sp);
        ui_update_device_config(sp);
	ui_update_primary(sp);
        ui_update_channel(sp);
        ui_repair(sp);
}

void
ui_show_audio_busy(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.disable.audio.ctls", "", TRUE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.enable.audio.ctls", "", TRUE);
}

void
ui_update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	char	args[2];
	sprintf(args, "%1d", sp->lecture);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.lecture.mode", args, TRUE);
}

void
ui_update_powermeters(session_struct *sp, struct s_mix_info *ms, int elapsed_time)
{
	static u_int32 power_time = 0;

	if (power_time > sp->meter_period) {
		if (sp->meter && (ms != NULL)) {
			mix_update_ui(sp, ms);
		}
                if (sp->sending_audio) {
			tx_update_ui(sp);
		}
		power_time = 0;
	} else {
		power_time += elapsed_time;
	}
}

void
ui_update_loss(session_struct *sp, char *srce, char *dest, int loss)
{
	char	*srce_e, *dest_e, *args;

	if ((srce == NULL) || (dest == NULL)) {
		return;
	}

 	srce_e = mbus_encode_str(srce);
	dest_e = mbus_encode_str(dest);
	args   = (char *) xmalloc(strlen(srce_e) + strlen(dest_e) + 6);
	sprintf(args, "%s %s %3d", srce_e, dest_e, loss);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.packet.loss", args, FALSE);
	xfree(args);
	xfree(srce_e);
	xfree(dest_e);
}

void
ui_update_reception(session_struct *sp, char *cname, u_int32 recv, u_int32 lost, u_int32 misordered, u_int32 duplicates, u_int32 jitter, int jit_tog)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);

	/* I hate this function! */
	args = (char *) xmalloc(strlen(cname_e) + 88);
	sprintf(args, "%s %6ld %6ld %6ld %6ld %6ld %6d", cname_e, recv, lost, misordered, duplicates, jitter, jit_tog);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.reception", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void
ui_update_duration(session_struct *sp, char *cname, int duration)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(5 + strlen(cname_e));

	sprintf(args, "%s %3d", cname_e, duration);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.source.packet.duration", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void 
ui_update_video_playout(session_struct *sp, char *cname, int playout)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(14 + strlen(cname_e));

	sprintf(args, "%s %12d", cname_e, playout);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_video, "source_playout", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void	
ui_update_sync(session_struct *sp, int sync)
{
	if (sync) {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.sync", "1", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.sync", "0", TRUE);
	}
}

void
ui_update_key(session_struct *sp, char *key)
{
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "security.encryption.key", key, TRUE);
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

static char *
ui_get_codecs(int pt, char *buf, unsigned int buf_len, int loose) 
{
	codec_t	*codec[10],*sel;
	u_int32	 i,nc, cnt;
        char *bp = buf;
        
        cnt = get_codec_count();
        sel = get_codec_by_pt(pt);
        
        for (nc = i = 0; i< cnt ; i++) {
                codec[nc] = get_codec_by_index(i);
                if (codec[nc] != NULL && codec[nc]->encode != NULL) {
                        if (loose == TRUE && codec_loosely_compatible(sel,codec[nc])) {
                                /* Picking out primary codecs, i.e. not bothered 
                                 * about sample size and block sizes matching.
                                 */
                                nc++;
                                assert(nc<10); 
                        } else if (codec_compatible(sel, codec[nc])) {
                                /* Picking out redundant codecs where we are 
                                 * fussed about sample and block size matching.
                                 */
                                nc++;
                                assert(nc<10);
                        }
                }
        }
        
        /* sort by bw as this makes handling of acceptable redundant codecs easier in ui */
        qsort(codec,nc,sizeof(codec_t*),codec_bw_cmp);
        for(i=0;i<nc && strlen(buf) + strlen(codec[i]->short_name) < buf_len;i++) {
                sprintf(bp, "%s ", codec[i]->short_name);
                bp += strlen(codec[i]->short_name) + 1;
        }

        if (i != nc) {
                debug_msg("Ran out of buffer space.\n");
        }
        
        if (bp != buf) *(bp-1) = 0;
        return buf;
}

void 
ui_codecs(session_struct *sp, int pt)
{
	char	args[256], *mbes;	/* Hope that's big enough... :-) */

        ui_get_codecs(pt, args, 256, TRUE);
        mbes = mbus_encode_str(args);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.codec.supported", mbes, TRUE);
        xfree(mbes);
        ui_get_codecs(pt, args, 256, FALSE);
        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.redundancy.supported", mbes, TRUE);
        xfree(mbes);
}

void
ui_update_playback_file(session_struct *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.file.play.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_record_file(session_struct *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "audio.file.record.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_file_live(session_struct *sp, char *mode, int valid)
{
        char cmd[32], arg[2];
        
        assert(!strcmp(mode, "play") || !strcmp(mode, "record"));
        
        sprintf(cmd, "audio.file.%s.alive", mode);
        sprintf(arg, "%1d", valid); 
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, cmd, arg, TRUE);
}

static void 
ui_converters(session_struct *sp)
{
        char buf[255], *mbes;
        int i, cnt;

        cnt = converter_get_count() - 1;

        buf[0] = '\0';
        
        for (i = 0; i <= cnt; i++) {
                strcat(buf, converter_get_name(i));
                if (i != cnt) strcat(buf, ",");
        }
        
        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.converter.supported", mbes, TRUE);
        xfree(mbes);
}

static void
ui_repair_schemes(session_struct *sp)
{
        char buf[255], *mbes;
        int  i, cnt;
        
        cnt = repair_get_count();
        
        buf[0] = '\0';
        for(i = cnt - 1; i >= 0; i--) {
                strcat(buf, repair_get_name(i));
                if (i) strcat(buf, ",");
        }
        
        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.repair.supported", mbes, TRUE);
        xfree(mbes);
}

void
ui_controller_init(session_struct *sp, char *cname, char *name_engine, char *name_ui, char *name_video)
{
	char	*my_cname;

	mbus_name_engine = name_engine;
	mbus_name_ui     = name_ui;
	mbus_name_video  = name_video;

	my_cname = mbus_encode_str(cname);
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "rtp.cname", my_cname, TRUE);
        xfree(my_cname);
}

static void
ui_title(session_struct *sp) 
{
	char	*addr, *args, *title;

        title = mbus_encode_str(sp->title);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "session.title", title, TRUE);
        xfree(title);

	addr = mbus_encode_str(sp->asc_address);
	args = (char *) xmalloc(strlen(addr) + 11);
        sprintf(args, "%s %5d %3d", addr, sp->rtp_port, sp->ttl);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "session.address", args, TRUE);
	xfree(args);
        xfree(addr);
}

static void
ui_load_settings(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "tool.rat.load.settings", "", TRUE);
}

static void
ui_3d_options(session_struct *sp)
{
        char args[256], tmp[5];
        char *mbes;
        int i, cnt;

        args[0] = '\0';
        cnt = render_3D_filter_get_count();
        for(i = 0; i < cnt; i++) {
                strcat(args, render_3D_filter_get_name(i));
                if (i != cnt - 1) strcat(args, ",");
        }

        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, 
                  "tool.rat.3d.filter.types", mbes, TRUE);
        xfree(mbes);

        args[0] = '\0';
        cnt = render_3D_filter_get_lengths_count();
        for(i = 0; i < cnt; i++) {
                sprintf(tmp, "%d", render_3D_filter_get_length(i));
                strcat(args, tmp);
                if (i != cnt - 1) strcat(args, ",");
        }
        
        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, 
                  "tool.rat.3d.filter.lengths", 
                  mbes, TRUE);
        xfree(mbes);

        sprintf(args, "%d", render_3D_filter_get_lower_azimuth());
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, 
                  "tool.rat.3d.azimuth.min", args, TRUE);

        sprintf(args, "%d", render_3D_filter_get_upper_azimuth());
        mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, 
                  "tool.rat.3d.azimuth.max", args, TRUE);
}

void
ui_initial_settings(session_struct *sp)
{
        /* One off setting transfers / initialization */
        ui_sampling_modes(sp);
        ui_converters(sp);
        ui_repair_schemes(sp);
	ui_codecs(sp, sp->encodings[0]);
        ui_3d_options(sp);
	ui_info_update_cname(sp, sp->db->my_dbe);
	ui_info_update_tool(sp, sp->db->my_dbe);
        ui_title(sp);
	ui_load_settings(sp);
}

void 
ui_quit(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine_base, mbus_name_ui, "mbus.quit", "", TRUE);
}
