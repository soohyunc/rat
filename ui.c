/*
 * FILE:    ui_control.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * This file contains routines which update the user interface. There is no 
 * direct connection between the user interface and the media engine: all
 * updates are done via the conference bus.
 *
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_state.h"
#include "codec.h"
#include "session.h"
#include "channel_types.h"
#include "channel.h"
#include "repair.h"
#include "converter.h"
#include "audio.h"
#include "audio_fmt.h"
#include "auddev.h"
#include "mbus.h"
#include "mbus_engine.h"
#include "transmit.h"
#include "pdb.h"
#include "rtp.h"
#include "mix.h"
#include "ui.h"
#include "timers.h"
#include "render_3D.h"
#include "source.h"
#include "net.h"
#include "util.h" /* purge_chars */

static void 
ui_update_boolean(session_t *sp, const char *field, int boolval)
{
        if (boolval) {
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, field, "1", TRUE);
        } else {
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, field, "0", TRUE);
        }
}

static void ui_info_update_sdes(session_t *sp, char *item, const char *val, uint32_t ssrc)
{
	char *arg;
        if (val == NULL) {
                val = "Unknown";
        }
        arg = mbus_encode_str(val);
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, item, "\"%08lx\" %s", ssrc, arg);
	xfree(arg);
}

void ui_info_update_cname(session_t *sp, uint32_t ssrc)
{
        const char *cname = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_CNAME);
	ui_info_update_sdes(sp, "rtp.source.cname", cname, ssrc);
}

void ui_info_update_name(session_t *sp, uint32_t ssrc)
{
        const char *name = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_NAME);
	ui_info_update_sdes(sp, "rtp.source.name", name, ssrc);
}

void ui_info_update_email(session_t *sp, uint32_t ssrc)
{
        const char *email = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_EMAIL);
	ui_info_update_sdes(sp, "rtp.source.email", email, ssrc);
}

void ui_info_update_phone(session_t *sp, uint32_t ssrc)
{
        const char *phone = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_PHONE);
	ui_info_update_sdes(sp, "rtp.source.phone", phone, ssrc);
}

void ui_info_update_loc(session_t *sp, uint32_t ssrc)
{
        const char *loc = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_LOC);
	ui_info_update_sdes(sp, "rtp.source.loc", loc, ssrc);
}

void ui_info_update_tool(session_t *sp, uint32_t ssrc)
{
        const char *tool = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_TOOL);
	ui_info_update_sdes(sp, "rtp.source.tool", tool, ssrc);
}

void ui_info_update_note(session_t *sp, uint32_t ssrc)
{
        const char *note = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_NOTE);
	ui_info_update_sdes(sp, "rtp.source.note", note, ssrc);
}

void 
ui_info_gain(session_t *sp, uint32_t ssrc)
{
        pdb_entry_t *pdbe;
        if (pdb_item_get(sp->pdb, ssrc, &pdbe)) {
                mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.source.gain", "\"%08lx\" %.2f", pdbe->ssrc, pdbe->gain);
        }
}

void
ui_info_mute(session_t *sp, uint32_t ssrc)
{
        pdb_entry_t *pdbe;
        if (pdb_item_get(sp->pdb, ssrc, &pdbe)) {
                mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.source.mute", "\"%08lx\" %d", pdbe->ssrc, pdbe->mute);
        }
}

void
ui_info_remove(session_t *sp, uint32_t ssrc)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.source.remove", "\"%08lx\"", ssrc);
}

void
ui_info_activate(session_t *sp, uint32_t ssrc)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.source.active", "\"%08lx\"", ssrc);
}

void
ui_info_deactivate(session_t *sp, uint32_t ssrc)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.source.inactive", "\"%08lx\"", ssrc);
}

void
ui_info_3d_settings(session_t *sp, uint32_t ssrc)
{
        char *filter_name;
        int   azimuth, filter_type, filter_length;
        pdb_entry_t *p;

        if (pdb_item_get(sp->pdb, ssrc, &p) == FALSE) {
                return;
        }

        if (p->render_3D_data == NULL) {
                p->render_3D_data = render_3D_init(get_freq(sp->device_clock));
        }

        render_3D_get_parameters(p->render_3D_data, &azimuth, &filter_type, &filter_length);
        filter_name = mbus_encode_str(render_3D_filter_get_name(filter_type));
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.3d.user.settings", "\"%08lx\" %s %d %d", ssrc, filter_name, filter_length, azimuth);
        xfree(filter_name);
}

void
ui_update_stats(session_t *sp, uint32_t ssrc)
{
        const rtcp_rr           *rr;
        uint32_t                  fract_lost, my_ssrc, total_lost;
        double                   skew_rate;
	char			*args, *mbes;
        struct s_source      	*src;
        uint32_t               	 buffered, delay;
        pdb_entry_t             *pdbe;

        if (pdb_item_get(sp->pdb, ssrc, &pdbe) == FALSE) {
                debug_msg("ui_update_stats: pdb entry does not exist (0x%08x)\n", ssrc);
                return;
        }
        pdbe->last_ui_update = sp->cur_ts;

        if (pdbe->enc_fmt) {
		mbes = mbus_encode_str(pdbe->enc_fmt);
                args = (char *) xmalloc(strlen(mbes) + 12);
                sprintf(args, "\"%08x\" %s", pdbe->ssrc, mbes);
                xfree(mbes);
        } else {
                args = (char *) xmalloc(19);
                sprintf(args, "\"%08x\" unknown", pdbe->ssrc);
        }

        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "rtp.source.codec", args, FALSE);
        xfree(args);

        src = source_get_by_ssrc(sp->active_sources, pdbe->ssrc);
        if (src) {
                buffered = ts_to_ms(source_get_audio_buffered(src));
                delay    = ts_to_ms(source_get_playout_delay(src));
                skew_rate = source_get_skew_rate(src);
        } else {
                buffered  = 0;
                delay     = 0;
                skew_rate = 1.0;
        }

        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.audio.buffered", "\"%08lx\" %ld", pdbe->ssrc, buffered);
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.audio.delay", "\"%08lx\" %ld", pdbe->ssrc, delay);
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.audio.skew", "\"%08lx\" %.5f", pdbe->ssrc, skew_rate);
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.spike.events", "\"%08lx\" %ld", pdbe->ssrc, pdbe->spike_events);
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.spike.toged", "\"%08lx\" %ld",  pdbe->ssrc, pdbe->spike_toged);

        my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
        rr = rtp_get_rr(sp->rtp_session[0], my_ssrc, pdbe->ssrc);
        if (rr != NULL) {
                fract_lost = (rr->fract_lost * 100) >> 8;
                total_lost = rr->total_lost;
        } else {
                debug_msg("No rr\n");
                fract_lost = 0;
                total_lost = 0;
        }
        ui_update_loss(sp, my_ssrc, pdbe->ssrc, fract_lost);
        ui_update_reception(sp, pdbe->ssrc, pdbe->received, total_lost,
                            pdbe->misordered, pdbe->duplicates, 
                            ts_to_ms(pdbe->jitter), pdbe->jit_toged);
        ui_update_duration(sp, pdbe->ssrc, pdbe->inter_pkt_gap * 1000 / get_freq(pdbe->clock));
}

void
ui_update_input_port(session_t *sp)
{
        const audio_port_details_t 	*apd = NULL;
        audio_port_t 			 port;
        char        			*mbes; 
        int          			 i, n, found;
        
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
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.input.port", mbes, TRUE);
        xfree(mbes);

	if (tx_is_sending(sp->tb)) {
		mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.input.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.input.mute", "1", TRUE);
	}
        ui_update_input_gain(sp);
}

void
ui_update_output_port(session_t *sp)
{
        const audio_port_details_t 	*apd = NULL;
        audio_port_t 			 port;
        char        			*mbes; 
        int          			 i, n, found;
        
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

        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.output.port", mbes, TRUE);
        xfree(mbes);

        ui_update_boolean(sp, "audio.output.mute", !sp->playing_audio);
        ui_update_output_gain(sp);
}

void
ui_input_level(session_t *sp, int level)
{
	static int	ol;

        assert(level>=0 && level <=100);

	if (ol == level) {
		return;
	}

	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "audio.input.powermeter", "%3d", level);
	ol = level;
}

void
ui_output_level(session_t *sp, int level)
{
	static int	ol;
        assert(level>=0 && level <=100);

	if (ol == level) {
                return;
	}

	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "audio.output.powermeter", "%3d", level);
	ol = level;
}

void
ui_update_device_config(session_t *sp)
{
        char          		 fmt_buf[64], *mbes;
        const audio_format 	*af;

        af = audio_get_ifmt(sp->audio_device);
        if (af && audio_format_name(af, fmt_buf, 64)) {
                mbes = mbus_encode_str(fmt_buf);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.format.in", mbes, TRUE);
                xfree(mbes);
        } else {
                debug_msg("Could not get ifmt\n");
        }
}

void
ui_update_primary(session_t *sp)
{
	codec_id_t            pri_id;
        const codec_format_t *pri_cf;
        char *mbes;

	pri_id = codec_get_by_payload(sp->encodings[0]);
        pri_cf = codec_get_format(pri_id);
	mbes = mbus_encode_str(pri_cf->short_name);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.codec", mbes, FALSE);
        xfree(mbes);
}

/*
static void
ui_update_interleaving(session_t *sp)
{

        int pt, isep, iu;
        char buf[128], *sep=NULL, *units = NULL, *dummy;

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

        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "audio.channel.coding", "\"interleaved\" %d %d",iu, isep);

        UNUSED(sp);
}
*/

static void
ui_update_redundancy(session_t *sp)
{
        const codec_format_t *scf;
        codec_id_t            scid;
        char *cmd, *out, *sec_enc, *sec_off, *mbes;
        
        int clen;

        clen = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cmd  = (char*)xmalloc(clen);

        channel_encoder_get_parameters(sp->channel_coder, cmd, clen);
        
        sec_enc = (char *) strtok(cmd, "/");  /* ignore primary encoding   */
        sec_enc = (char *) strtok(NULL, "/"); /* ignore primary offset     */
        sec_enc = (char *) strtok(NULL, "/"); /* get secondary encoding    */
        sec_off = (char *) strtok(NULL, "/"); /* get secondary offset      */

        if (sec_enc == NULL || sec_off == NULL) {
                goto redundancy_update_end;
        }

        scid    = codec_get_by_name(sec_enc);
        if (!codec_id_is_valid(scid)) {
                   goto redundancy_update_end;
        }
        
        scf = codec_get_format(scid);
        out = (char*)xmalloc(clen);

        mbes = mbus_encode_str("redundancy");
        sprintf(out, "%s ", mbes);
        xfree(mbes);

        mbes = mbus_encode_str(scf->short_name);
        strcat(out, mbes);
        xfree(mbes);

        strcat(out, " ");
        strcat(out, sec_off);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.channel.coding", out, TRUE);
        xfree(out);

redundancy_update_end:
        xfree(cmd);
}

static void
ui_update_layering(session_t *sp)
{
        const codec_format_t *lcf;
        codec_id_t            lcid;
        char *cmd, *out, *sec_enc, *layerenc, *mbes;
        
        int clen;

        clen = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cmd  = (char*)xmalloc(clen);

        channel_encoder_get_parameters(sp->channel_coder, cmd, clen);
        
        sec_enc = (char *) strtok(cmd, "/");
        layerenc = (char *) strtok(NULL, "/");

        if (sec_enc == NULL || layerenc == NULL) {
                goto layering_update_end;
        }

        lcid    = codec_get_by_name(sec_enc);
        if (!codec_id_is_valid(lcid)) {
                   goto layering_update_end;
        }
        
        lcf = codec_get_format(lcid);
        out = (char*)xmalloc(clen);

        mbes = mbus_encode_str("layering");
        sprintf(out, "%s ", mbes);
        xfree(mbes);

        mbes = mbus_encode_str(lcf->short_name);
        strcat(out, mbes);
        xfree(mbes);

        strcat(out, " ");
        strcat(out, layerenc);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.channel.coding", out, TRUE);
        xfree(out);

layering_update_end:
        xfree(cmd);
}

void 
ui_update_channel(session_t *sp) 
{
        const cc_details_t *ccd;
        char *mbes;

        ccd = channel_get_coder_identity(sp->channel_coder);
        switch(tolower(ccd->name[0])) {
        case 'n':
                mbes = mbus_encode_str("none");
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.channel.coding", mbes, TRUE);
                xfree(mbes);
                debug_msg("ui_update_channel: n\n");
                break;
        case 'r':
                ui_update_redundancy(sp);
                debug_msg("ui_update_channel: r\n");
                break;
        case 'l':
                ui_update_layering(sp);
                debug_msg("ui_update_channel: l\n");
                break;
        }
        return;
}

void
ui_update_input_gain(session_t *sp)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "audio.input.gain", "%3d", audio_get_igain(sp->audio_device));
}

void
ui_update_output_gain(session_t *sp)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "audio.output.gain", "%3d", audio_get_ogain(sp->audio_device));
}

static void
ui_update_3d_enabled(session_t *sp)
{
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.3d.enabled", "%d", (sp->render_3d ? 1 : 0));
}

static void
ui_input_ports(session_t *sp)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;
        
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.input.ports.flush", "", TRUE);

        n = audio_get_iport_count(sp->audio_device);
        assert(n >= 1);

        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.input.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_output_ports(session_t *sp)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;
        
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.output.ports.flush", "", TRUE);

        n = audio_get_oport_count(sp->audio_device);
        assert(n >= 1);

        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.output.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_devices(session_t *sp)
{
        const audio_device_details_t *add;
        char *mbes, dev_name[AUDIO_DEVICE_NAME_LENGTH];
        int i,nDev;

        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.devices.flush", "", TRUE);
        nDev = audio_get_device_count();

        for(i = 0; i < nDev; i++) {
                add  = audio_get_device_details(i);
                strcpy(dev_name, add->name);
                purge_chars(dev_name, "[]()");
                mbes = mbus_encode_str(dev_name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.devices.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_device(session_t *sp)
{
        const audio_device_details_t *add = NULL;
        char                         *mbes;
        uint32_t                       i, n;

        n = audio_get_device_count();
        for(i = 0; i < n; i++) {
                add = audio_get_device_details(i);
                if (sp->audio_device == add->descriptor) {
                        break;
                }
        }

        if (i != n) {
                char dev_name[AUDIO_DEVICE_NAME_LENGTH];
                strcpy(dev_name, add->name);
                purge_chars(dev_name, "()[]");
                mbes = mbus_encode_str(dev_name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.device", mbes, TRUE);
                xfree(mbes);
                ui_input_ports(sp);
                ui_output_ports(sp);
        }
}

static void
ui_sampling_modes(session_t *sp)
{
	char	*mbes;
        char    modes[255]="";
        char    tmp[22];
        uint16_t rate, channels, support, zap;
        
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

        /* Remove trailing space */
        zap = strlen(modes);
        if (zap) {
                zap -= 1;
                modes[zap] = '\0';
        }

	mbes = mbus_encode_str(modes);
	mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.sampling.supported", mbes, TRUE);
	xfree(mbes);
}

static void
ui_update_powermeter(session_t *sp)
{
        ui_update_boolean(sp, "tool.rat.powermeter", sp->meter);
}

static void
ui_update_lipsync(session_t *sp) 
{
        ui_update_boolean(sp, "tool.rat.sync", sp->sync_on);
}

static void
ui_update_silence(session_t *sp)
{
        ui_update_boolean(sp, "audio.suppress.silence", sp->detect_silence);
}

static void
ui_update_playout_bounds(session_t *sp)
{
        char tmp[6];
        ui_update_boolean(sp, "tool.rat.playout.limit", sp->limit_playout);
        sprintf(tmp, "%4d", (int)sp->min_playout);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.playout.min", tmp, TRUE);
        sprintf(tmp, "%4d", (int)sp->max_playout);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.playout.max", tmp, TRUE);
}

static void
ui_update_agc(session_t *sp)
{
        ui_update_boolean(sp, "tool.rat.agc", sp->agc_on);
}

static void
ui_update_audio_loopback(session_t *sp)
{
        ui_update_boolean(sp, "tool.rat.loopback", sp->loopback_gain);
}

static void
ui_update_echo_suppression(session_t *sp)
{
        ui_update_boolean(sp, "tool.rat.echo.suppress", sp->echo_suppress);
}

void
ui_update(session_t *sp)
{
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.rate", "%3d", channel_encoder_get_units_per_packet(sp->channel_coder));
        
        ui_devices(sp);
        ui_device(sp);
        ui_update_input_gain(sp);
        ui_update_output_gain(sp);
        ui_input_ports(sp);
        ui_output_ports(sp);
	ui_update_output_port(sp);
	ui_update_input_port(sp);
        ui_update_3d_enabled(sp);
        ui_sampling_modes(sp);
        ui_update_device_config(sp);
	ui_update_primary(sp);
        ui_update_channel(sp);
        ui_update_repair(sp);
        ui_update_converter(sp);
        ui_update_playout_bounds(sp);
        ui_update_lecture_mode(sp);
        ui_update_powermeter(sp);
        ui_update_lipsync(sp);
        ui_update_silence(sp);
        ui_update_agc(sp);
        ui_update_audio_loopback(sp);
        ui_update_echo_suppression(sp);
}

void
ui_update_lecture_mode(session_t *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.lecture.mode", "%1d", sp->lecture);
}

static void
ui_update_powermeters(session_t *sp, struct s_mix_info *ms)
{
        if (sp->meter && (ms != NULL)) {
                mix_update_ui(sp, ms);
        }
        if (tx_is_sending(sp->tb)) {
                tx_update_ui(sp->tb);
        }
}

static void
ui_update_bps(session_t *sp)
{
        double inbps = 0.0, outbps = 0.0;
        uint32_t scnt, sidx;
        struct s_source *s;
        
        scnt = source_list_source_count(sp->active_sources);
        for(sidx = 0; sidx < scnt; sidx++) {
                s = source_list_get_source_no(sp->active_sources, sidx);
                inbps += source_get_bps(s);
        }
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.bps.in", "%.0f", inbps);        

        outbps = tx_get_bps(sp->tb);
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "tool.rat.bps.out", "%.0f", outbps);        
}

void
ui_periodic_updates(session_t *sp, int elapsed_time) 
{
        static uint32_t power_time = 0;
        static uint32_t bps_time   = 0;

        bps_time   += elapsed_time;
        if (bps_time > 10 * sp->meter_period) {
                ui_update_bps(sp);
                bps_time = 0;
        }

        power_time += elapsed_time;
        if (power_time > sp->meter_period) {
                ui_update_powermeters(sp, sp->ms);
                power_time = 0;
        }
}

void
ui_update_loss(session_t *sp, uint32_t srce, uint32_t dest, int loss)
{
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "rtp.source.packet.loss", "\"%08lx\" \"%08lx\" %3d", srce, dest, loss);
}

void
ui_update_reception(session_t *sp, uint32_t ssrc, uint32_t recv, uint32_t lost, uint32_t misordered, uint32_t duplicates, uint32_t jitter, int jit_tog)
{
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "rtp.source.reception", "\"%08lx\" %6ld %6ld %6ld %6ld %6ld %6d", 
		  ssrc, recv, lost, misordered, duplicates, jitter, jit_tog);
}

void
ui_update_duration(session_t *sp, uint32_t ssrc, int duration)
{
	mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, FALSE, "rtp.source.packet.duration", "\"%08lx\" %3d", ssrc, duration);
}


void 
ui_update_video_playout(session_t *sp, uint32_t ssrc, int playout)
{
        const char *cname = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_CNAME);
	char *arg = mbus_encode_str(cname);
	mbus_qmsgf(sp->mbus_engine, sp->mbus_video_addr, FALSE, "rtp.source.cname",   "\"%08lx\" %s",   ssrc, arg);
	mbus_qmsgf(sp->mbus_engine, sp->mbus_video_addr, FALSE, "rtp.source.playout", "\"%08lx\" %12d", ssrc, playout);
	xfree(arg);
}

void
ui_update_key(session_t *sp, char *key)
{
	char	*key_e;

	key_e = mbus_encode_str(key);
	mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "security.encryption.key", key_e, TRUE);
	xfree(key_e);
}

void
ui_update_codec(session_t *sp, codec_id_t cid)
{
        char 			*caps, *long_name_e, *short_name_e, *pay_e, *descr_e;
        int 			 can_enc, can_dec, layers;
        char 			 pay[4];
        u_char 			 pt;
        const codec_format_t	*cf;
        
        cf  = codec_get_format(cid);
        assert(cf != NULL);

        can_enc = codec_can_encode(cid);
        can_dec = codec_can_decode(cid);

        caps = NULL;
        if (can_enc && can_dec) {
                caps = mbus_encode_str("Encode and decode");
        } else if (can_enc) {
                caps = mbus_encode_str("Encode only");
        } else if (can_dec) {
                caps = mbus_encode_str("Decode only");
        } else {
                caps = mbus_encode_str("Not available");
        }

        pt = codec_get_payload(cid);
        if (payload_is_valid(pt)) {
                sprintf(pay, "%d", pt);
        } else {
                sprintf(pay, "-");
        }
	pay_e        = mbus_encode_str(pay);
	long_name_e  = mbus_encode_str(cf->long_name);
	short_name_e = mbus_encode_str(cf->short_name);
	descr_e      = mbus_encode_str(cf->description);
        layers       = codec_can_layer(cid);

        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, 
                   "tool.rat.codec.details",
                   "%s %s %s %d %d %d %d %d %s %s %d",
                   pay_e,
                   long_name_e,
                   short_name_e,
                   cf->format.channels,
                   cf->format.sample_rate,
                   cf->format.bytes_per_block,
                   cf->mean_per_packet_state_size,
                   cf->mean_coded_frame_size,
                   descr_e,
                   caps,
                   layers);
	xfree(caps);
	xfree(pay_e);
	xfree(long_name_e);
	xfree(short_name_e);
	xfree(descr_e);
}

static void
ui_codecs(session_t *sp)
{
        uint32_t nCodecs, iCodec;
        codec_id_t cid;

        nCodecs = codec_get_number_of_codecs();

        for(iCodec = 0; iCodec < nCodecs; iCodec++) {
                cid = codec_get_codec_number(iCodec);
                if (cid) ui_update_codec(sp, cid);
        }
}

void
ui_update_playback_file(session_t *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.file.play.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_record_file(session_t *sp, char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.file.record.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_file_live(session_t *sp, char *mode, int valid)
{
        char cmd[32], arg[2];
        
        assert(!strcmp(mode, "play") || !strcmp(mode, "record"));
        
        sprintf(cmd, "audio.file.%s.alive", mode);
        sprintf(arg, "%1d", valid); 
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, cmd, arg, TRUE);
}

static void 
ui_converters(session_t *sp)
{
        const converter_details_t *details;
        char *mbes;
        int i, cnt;

        cnt = converter_get_count();

        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.converters.flush", "", TRUE);

        for (i = 0; i < cnt; i++) {
                details = converter_get_details(i);
                mbes = mbus_encode_str(details->name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.converters.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_update_converter(session_t *sp)
{
        const converter_details_t *details;
        char *mbes;
        int i, cnt;

        cnt = converter_get_count();

        for(i = 0; i < cnt; i++) {
                details = converter_get_details(i);
                if (sp->converter == details->id) {
                        mbes = mbus_encode_str(details->name);
                        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.converter", mbes, TRUE);
                        xfree(mbes);
                        return;
                }
        }
        debug_msg("Converter not found: %d\n", sp->converter);
}

static void
ui_repair_schemes(session_t *sp)
{
        const repair_details_t *r;
        char *mbes;
        uint16_t i, n;
        
        n = repair_get_count();
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.repairs.flush", "", TRUE);

        for(i = 0; i < n; i++) {
                r = repair_get_details(i);
                mbes = mbus_encode_str(r->name);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.repairs.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_update_repair(session_t *sp)
{
        const repair_details_t *r;
        uint16_t i, n;
        char *mbes;

        n = repair_get_count();
        for (i = 0; i < n; i++) {
                r = repair_get_details(i);
                if (sp->repair == r->id) {
                        mbes = mbus_encode_str(r->name);
                        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "audio.channel.repair", mbes, TRUE);
                        xfree(mbes);
                        return;
                }
        }
        debug_msg("Repair not found: %d\n", sp->repair);
}

static void
ui_title(session_t *sp) 
{
	char	*addr, *title;

        title = mbus_encode_str(sp->title);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "session.title", title, TRUE);
        xfree(title);

	addr = mbus_encode_str(rtp_get_addr(sp->rtp_session[0]));
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "rtp.addr", "%s %5d %5d %3d", 
		addr, rtp_get_rx_port(sp->rtp_session[0]), rtp_get_tx_port(sp->rtp_session[0]), rtp_get_ttl(sp->rtp_session[0]));
        xfree(addr);
}

/* ui_final_settings: so we here we load things that get broken
 * because we open null audio device first, and it has limited no of
 * input and output ports.  
 */

void
ui_final_settings(session_t *sp)
{
        int i, n;
        char *mbes;
        const char *settings[] = {
                "audio.input.mute",
                "audio.input.gain",
                "audio.input.port",
                "audio.output.mute",
                "audio.output.gain",
                "audio.output.port"
        };
        
        n = sizeof(settings)/sizeof(settings[0]);
        for(i = 0; i < n; i++) {
                mbes = mbus_encode_str(settings[i]);
                mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.load.setting", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_3d_options(session_t *sp)
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
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.3d.filter.types", mbes, TRUE);
        xfree(mbes);

        args[0] = '\0';
        cnt = render_3D_filter_get_lengths_count();
        for(i = 0; i < cnt; i++) {
                sprintf(tmp, "%d", render_3D_filter_get_length(i));
                strcat(args, tmp);
                if (i != cnt - 1) strcat(args, ",");
        }
        
        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "tool.rat.3d.filter.lengths", mbes, TRUE);
        xfree(mbes);

        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.3d.azimuth.min", "%d", render_3D_filter_get_lower_azimuth());
        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "tool.rat.3d.azimuth.max", "%d", render_3D_filter_get_upper_azimuth());
}

void
ui_initial_settings(session_t *sp)
{
        uint32_t my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
        /* One off setting transfers / initialization */
        ui_sampling_modes(sp); 			network_process_mbus(sp);
        ui_converters(sp); 			network_process_mbus(sp);
        ui_repair_schemes(sp); 			network_process_mbus(sp);
        ui_codecs(sp); 				network_process_mbus(sp);
        ui_3d_options(sp); 	 		network_process_mbus(sp);
	ui_info_update_cname(sp, my_ssrc); 	network_process_mbus(sp);
	ui_info_update_name(sp,  my_ssrc); 	network_process_mbus(sp);
	ui_info_update_email(sp, my_ssrc); 	network_process_mbus(sp);
	ui_info_update_phone(sp, my_ssrc); 	network_process_mbus(sp);
	ui_info_update_loc(sp,   my_ssrc); 	network_process_mbus(sp);
	ui_info_update_tool(sp,  my_ssrc); 	network_process_mbus(sp);
        ui_title(sp); 				network_process_mbus(sp);
#ifdef NDEF /* This is done by load_settings() now... */
	ui_load_settings(sp); 			network_process_mbus(sp);
#endif
}

void 
ui_quit(session_t *sp)
{
	mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "mbus.quit", "", TRUE);
}

void
ui_controller_init(session_t *sp)
{
	char	my_ssrc[11];

	sprintf(my_ssrc, "\"%08x\"", rtp_my_ssrc(sp->rtp_session[0]));
	mbus_qmsg(sp->mbus_engine, sp->mbus_ui_addr, "rtp.ssrc", my_ssrc, TRUE);
}

