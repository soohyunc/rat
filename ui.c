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
#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "session.h"
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "convert.h"
#include "audio.h"
#include "audio_fmt.h"
#include "auddev.h"
#include "mbus.h"
#include "mbus_engine.h"
#include "channel_types.h"
#include "channel.h"
#include "mix.h"
#include "transmit.h"
#include "ui.h"
#include "timers.h"
#include "render_3D.h"
#include "source.h"
#include "net.h"

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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.name", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.exists", cname, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.email", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.phone", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.loc", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.tool", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.note", args, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.mute", args, TRUE);
	xfree(cname);
        xfree(args);
}

void
ui_info_remove(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.remove", cname, TRUE);
        xfree(cname);
}

void
ui_info_activate(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
        
        cname = mbus_encode_str(e->sentry->cname);
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.active", cname, FALSE);
        xfree(cname);
}

void
ui_info_deactivate(session_struct *sp, rtcp_dbentry *e)
{
        char *cname;

	if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.inactive", cname, FALSE);
        xfree(cname);
}

void
ui_info_3d_settings(session_struct *sp, rtcp_dbentry *e)
{
        char *cname, *filter_name, *msg;
        int   azimuth, filter_type, filter_length;

        if (e->render_3D_data == NULL) {
                e->render_3D_data = render_3D_init(get_freq(sp->device_clock));
        }

        render_3D_get_parameters(e->render_3D_data, &azimuth, &filter_type, &filter_length);
        cname       = mbus_encode_str(e->sentry->cname);
        filter_name = mbus_encode_str(render_3D_filter_get_name(filter_type));
        msg = (char*)xmalloc(strlen(cname) + strlen(filter_name) + 10);
        sprintf(msg, "%s %s %d %d", cname, filter_name, filter_length, azimuth);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.3d.user.settings", msg, TRUE);
        xfree(cname);
        xfree(filter_name);
        xfree(msg);
}

void
ui_update_stats(session_struct *sp, rtcp_dbentry *e)
{
	char	*my_cname, *their_cname, *args, *mbes;
        struct s_source      *src;
        u_int32               buffered, delay;

        if (sp->db->my_dbe->sentry == NULL || 
            sp->db->my_dbe->sentry->cname == NULL) {
                debug_msg("Warning sentry or name == NULL\n");
                return;
        }

	if (e->sentry->cname == NULL) {
		return;
	}

	my_cname    = mbus_encode_str(sp->db->my_dbe->sentry->cname);
	their_cname = mbus_encode_str(e->sentry->cname);

        if (e->enc_fmt) {
		mbes = mbus_encode_str(e->enc_fmt);
                args = (char *) xmalloc(strlen(their_cname) + strlen(mbes) + 2);
                sprintf(args, "%s %s", their_cname, mbes);
                xfree(mbes);
        } else {
                args = (char *) xmalloc(strlen(their_cname) + 7 + 2);
                sprintf(args, "%s unknown", their_cname);
        }

        mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "rtp.source.codec", 
                  args, 
                  FALSE);

        xfree(args);

        /* args size is for source.packet.loss,
         * tool.rat.audio.buffered size always less 
         */

	args = (char *) xmalloc(strlen(their_cname) + strlen(my_cname) + 11);

        src = source_get_by_rtcp_dbentry(sp->active_sources, e);
        if (src) {
                buffered = ts_to_ms(source_get_audio_buffered (src));
                delay    = ts_to_ms(source_get_playout_delay  (src));
        } else {
                buffered = 0;
                delay    = 0;
        }

        sprintf(args, 
                "%s %ld", 
                their_cname, 
                buffered);
        mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "tool.rat.audio.buffered", 
                  args, 
                  FALSE);
        
        sprintf(args, 
                "%s %ld", 
                their_cname, 
                delay);
        mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "tool.rat.audio.delay", 
                  args, 
                  FALSE);

	sprintf(args, 
                "%s %s %8ld", 
                my_cname, 
                their_cname, 
                (e->lost_frac * 100) >> 8);
	mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "rtp.source.packet.loss", 
                  args, 
                  FALSE);

	xfree(my_cname);
	xfree(their_cname);
	xfree(args);
}

void
ui_update_input_port(session_struct *sp)
{
        const audio_port_details_t *apd;
        audio_port_t port;
        char        *mbes; 
        int          i, n, found;
        
        port = audio_get_iport(sp->audio_device);

        found = FALSE;
        n = audio_get_iport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                if (apd->port == port) {
                        found = TRUE;
                        break;
                }
        }

        if (found == FALSE) {
                debug_msg("Port %d not found!\n", port);
                apd = audio_get_iport_details(sp->audio_device, 0);
        }

        mbes = mbus_encode_str(apd->name);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.port", mbes, TRUE);
        xfree(mbes);

	if (tx_is_sending(sp->tb)) {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.mute", "1", TRUE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
        const audio_port_details_t *apd;
        audio_port_t port;
        char        *mbes; 
        int          i, n, found;
        
        port = audio_get_oport(sp->audio_device);

        found = FALSE;
        n = audio_get_oport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                if (apd->port == port) {
                        found = TRUE;
                        break;
                }
        }

        if (found == FALSE) {
                debug_msg("Port %d not found!\n", port);
                apd = audio_get_oport_details(sp->audio_device, 0);
        }

        mbes = mbus_encode_str(apd->name);

        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.port", mbes, TRUE);
        xfree(mbes);

	if (sp->playing_audio) {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.mute", "1", TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.powermeter", args, FALSE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.powermeter", args, FALSE);
	ol = level;
}

static void
ui_repair(session_struct *sp)
{
	char	*mbes;

        mbes = mbus_encode_str(repair_get_name((u_int16)sp->repair));
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.repair", mbes, FALSE);
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
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.format.in", mbes, TRUE);
                xfree(mbes);
        } else {
                debug_msg("Could not get ifmt\n");
        }
}

void
ui_update_primary(session_struct *sp)
{
	codec_id_t            pri_id;
        const codec_format_t *pri_cf;
        char *mbes;

	pri_id = codec_get_by_payload(sp->encodings[0]);
        pri_cf = codec_get_format(pri_id);
	mbes = mbus_encode_str(pri_cf->short_name);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.codec", mbes, FALSE);
        xfree(mbes);
}

/*
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.coding", args, TRUE);        

        UNUSED(sp);
}

static void
ui_update_redundancy(session_struct *sp)
{
        int  pt;
        int  ioff;
        char buf[128]= "", *codec_name=NULL, *offset=NULL, *dummy, *args;

        pt = get_cc_pt(sp,"REDUNDANCY");
        if (pt != -1) { 
                query_channel_coder(sp, pt, buf, 128);
                if (strlen(buf)) {
                        dummy  = strtok(buf,"/");
                        dummy  = strtok(NULL,"/");
                        codec_name  = strtok(NULL,"/");
                         redundant coder returns long name convert to short
                        if (codec_name) {
                                const codec_format_t *cf;
                                codec_id_t            cid;
                                cid  = codec_get_by_name(codec_name);
                                assert(cid);
                                cf   = codec_get_format(cid);
                                codec_name = (char*)cf->short_name;
                        }
                        offset = strtok(NULL,"/");
                }
        } else {
                debug_msg("Could not find redundant channel coder!\n");
        } 

        if (codec_name != NULL && offset != NULL) {
                ioff  = atoi(offset);
        } else {
                const codec_format_t *cf;
                codec_id_t            id;
                id         = codec_get_by_payload(sp->encodings[0]);
                assert(id);
                cf         = codec_get_format(id);
                codec_name = (char*)cf->short_name;
                ioff  = 1;
        } 

	codec_name = mbus_encode_str(codec_name);

	args = (char *) xmalloc(strlen(codec_name) + 16);
        sprintf(args,"\"redundant\" %s %2d", codec_name, ioff);
        assert(strlen(args) < (strlen(codec_name) + 16));
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.coding", args, TRUE);
	xfree(codec_name);
        xfree(args);
}
*/

void 
ui_update_channel(session_struct *sp) 
{
/*
        cc_coder_t *ccp;
        */
        mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "audio.channel.coding", "\"none\"", TRUE);
/*
        ccp = get_channel_coder(sp->cc_encoding);
	if (strcmp(ccp->name, "VANILLA") == 0) {
        	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.coding", "\"none\"", TRUE);
	} else if (strcmp(ccp->name, "REDUNDANCY") == 0) {
                ui_update_redundancy(sp);
        } else if (strcmp(ccp->name, "INTERLEAVER") == 0) {
                ui_update_interleaving(sp);
        } else {
                debug_msg("Channel coding failed mapping (%s)\n", ccp->name);
                abort();
        }
        */
}

void
ui_update_input_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_igain(sp->audio_device)); 
        assert(strlen(args) < 4);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.gain", args, TRUE);
}

void
ui_update_output_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_ogain(sp->audio_device)); 
        assert(strlen(args) < 4);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.gain", args, TRUE);
}

static void
ui_update_3d_enabled(session_struct *sp)
{
        char args[2];
        sprintf(args, "%d", (sp->render_3d ? 1 : 0));
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.3d.enabled", args, TRUE);
}

static void
ui_input_ports(session_struct *sp)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;
        
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.ports.flush", "", TRUE);

        n = audio_get_iport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_output_ports(session_struct *sp)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;
        
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.ports.flush", "", TRUE);

        n = audio_get_oport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_devices(session_struct *sp)
{
        audio_device_details_t ad;
        int i,nDev;

        char buf[255] = "", *mbes;
        
        nDev = audio_get_device_count();
        for(i = 0; i < nDev; i++) {
                if (!audio_get_device_details(i, &ad)) continue;
                strcat(buf, ad.name);
                strcat(buf, ",");
        }
        i = strlen(buf);
        if (i != 0) buf[i-1] = '\0';

        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.devices", mbes, TRUE);
        xfree(mbes);
}

static void
ui_device(session_struct *sp)
{
        audio_device_details_t ad;
        char                  *mbes, *cur_dev;
        int                    i;

        cur_dev = NULL;

        for(i = 0; i < audio_get_device_count(); i++) {
                if (audio_get_device_details(i, &ad) && sp->audio_device == ad.descriptor) {
                        cur_dev = ad.name;
                        break;
                }
        }

        if (cur_dev) {
                mbes = mbus_encode_str(cur_dev);
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.device", mbes, TRUE);
                xfree(mbes);
                ui_input_ports(sp);
                ui_output_ports(sp);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.sampling.supported", mbes, TRUE);
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
	        sprintf(args, "%3d", sp->output_gain); mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.gain", args, TRUE);
                assert(strlen(args) < 4);
		sprintf(args, "%3d", sp->input_gain ); mbus_qmsg(sp->mbus_engine, mbus_name_ui,  "audio.input.gain", args, TRUE);
                assert(strlen(args) < 4);
	}

        sprintf(args, "%3d", channel_encoder_get_units_per_packet(sp->channel_coder));
        assert(strlen(args) < 4);
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.rate", args, TRUE);

	ui_update_output_port(sp);
	ui_update_input_port(sp);
        ui_update_3d_enabled(sp);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.disable.audio.ctls", "", TRUE);
}

void
ui_hide_audio_busy(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.enable.audio.ctls", "", TRUE);
}

void
ui_update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	char	args[2];
	sprintf(args, "%1d", sp->lecture);
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.lecture.mode", args, TRUE);
}

void
ui_update_powermeters(session_struct *sp, struct s_mix_info *ms, int elapsed_time)
{
	static u_int32 power_time = 0;

	if (power_time > sp->meter_period) {
		if (sp->meter && (ms != NULL)) {
			mix_update_ui(sp, ms);
		}
                if (tx_is_sending(sp->tb)) {
			tx_update_ui(sp->tb);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.packet.loss", args, FALSE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.reception", args, FALSE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.packet.duration", args, FALSE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_video, "source_playout", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void	
ui_update_sync(session_struct *sp, int sync)
{
	if (sync) {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.sync", "1", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.sync", "0", TRUE);
	}
}

void
ui_update_key(session_struct *sp, char *key)
{
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "security.encryption.key", key, TRUE);
}

void
ui_update_codec(session_struct *sp, codec_id_t cid)
{
        char entry[255], *mbes[3];

        u_char pt;
        const codec_format_t *cf;
        
        cf  = codec_get_format(cid);
        assert(cf != NULL);

        mbes[0] = mbus_encode_str(cf->long_name);
        mbes[1] = mbus_encode_str(cf->short_name);
        mbes[2] = mbus_encode_str(cf->description);
        pt = codec_get_payload(cid);
        if (payload_is_valid(pt)) {
                sprintf(entry, 
                        "%u %s %s %d %d %d %d %d %s",
                        pt,
                        mbes[0],
                        mbes[1],
                        cf->format.channels,
                        cf->format.sample_rate,
                        cf->format.bytes_per_block,
                        cf->mean_per_packet_state_size,
                        cf->mean_coded_frame_size,
                        mbes[2]);
        } else {
                sprintf(entry, 
                        "- %s %s %d %d %d %d %d %s",
                        mbes[0],
                        mbes[1],
                        cf->format.channels,
                        cf->format.sample_rate,
                        cf->format.bytes_per_block,
                        cf->mean_per_packet_state_size,
                        cf->mean_coded_frame_size,
                        mbes[2]);
        }
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.codec.details", entry, TRUE);
        xfree(mbes[2]); xfree(mbes[1]); xfree(mbes[0]);
}

static void
ui_codecs(session_struct *sp)
{
        u_int32 nCodecs, iCodec;
        codec_id_t cid;

        nCodecs = codec_get_number_of_codecs();

        for(iCodec = 0; iCodec < nCodecs; iCodec++) {
                cid = codec_get_codec_number(iCodec);
                if (cid) ui_update_codec(sp, cid);
        }
}

void
ui_update_playback_file(session_struct *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.file.play.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_record_file(session_struct *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.file.record.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_file_live(session_struct *sp, char *mode, int valid)
{
        char cmd[32], arg[2];
        
        assert(!strcmp(mode, "play") || !strcmp(mode, "record"));
        
        sprintf(cmd, "audio.file.%s.alive", mode);
        sprintf(arg, "%1d", valid); 
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, cmd, arg, TRUE);
}

static void 
ui_converters(session_struct *sp)
{
        converter_details_t details;
        char buf[255], *mbes;
        int i, cnt;

        cnt = converter_get_count() - 1;

        buf[0] = '\0';
        
        for (i = 0; i <= cnt; i++) {
                converter_get_details(i, &details);
                strcat(buf, details.name);
                if (i != cnt) strcat(buf, ",");
        }
        
        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine, 
                  mbus_name_ui, 
                  "tool.rat.converter.supported", 
                  mbes, TRUE);
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
                strcat(buf, repair_get_name((u_char)i));
                if (i) strcat(buf, ",");
        }
        
        mbes = mbus_encode_str(buf);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.repair.supported", mbes, TRUE);
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
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.cname", my_cname, TRUE);
        xfree(my_cname);
}

static void
ui_title(session_struct *sp) 
{
	char	*addr, *args, *title;

        title = mbus_encode_str(sp->title);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "session.title", title, TRUE);
        xfree(title);

	addr = mbus_encode_str(sp->asc_address);
	args = (char *) xmalloc(strlen(addr) + 11);
        sprintf(args, "%s %5d %3d", addr, sp->rtp_port, sp->ttl);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "session.address", args, TRUE);
	xfree(args);
        xfree(addr);
}

static void
ui_load_settings(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.load.settings", "", TRUE);
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, 
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, 
                  "tool.rat.3d.filter.lengths", 
                  mbes, TRUE);
        xfree(mbes);

        sprintf(args, "%d", render_3D_filter_get_lower_azimuth());
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, 
                  "tool.rat.3d.azimuth.min", args, TRUE);

        sprintf(args, "%d", render_3D_filter_get_upper_azimuth());
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, 
                  "tool.rat.3d.azimuth.max", args, TRUE);
}

void
ui_initial_settings(session_struct *sp)
{
        /* One off setting transfers / initialization */
        ui_sampling_modes(sp); 				network_process_mbus(sp);
        ui_converters(sp); 				network_process_mbus(sp);
        ui_repair_schemes(sp); 				network_process_mbus(sp);
        ui_codecs(sp); 					network_process_mbus(sp);
        ui_3d_options(sp); 				network_process_mbus(sp);
	ui_info_update_cname(sp, sp->db->my_dbe); 	network_process_mbus(sp);
	ui_info_update_tool(sp, sp->db->my_dbe); 	network_process_mbus(sp);
        ui_title(sp); 					network_process_mbus(sp);
	ui_load_settings(sp); 				network_process_mbus(sp);
}

void 
ui_quit(session_struct *sp)
{
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "mbus.quit", "", TRUE);
}
