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

static void ui_info_update_sdes(session_struct *sp, char *item, char *val, u_int32 ssrc)
{
	char *arg   = mbus_encode_str(val);
	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, item, "\"%08lx\" %s", ssrc, arg);
	xfree(arg);
}

void ui_info_update_cname(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.cname", e->sentry->cname, e->sentry->ssrc);
}

void ui_info_update_name(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.name", e->sentry->name, e->sentry->ssrc);
}

void ui_info_update_email(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.email", e->sentry->email, e->sentry->ssrc);
}

void ui_info_update_phone(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.phone", e->sentry->phone, e->sentry->ssrc);
}

void ui_info_update_loc(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.loc", e->sentry->loc, e->sentry->ssrc);
}

void ui_info_update_tool(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.tool", e->sentry->tool, e->sentry->ssrc);
}

void ui_info_update_note(session_struct *sp, rtcp_dbentry *e)
{
	ui_info_update_sdes(sp, "rtp.source.note", e->sentry->note, e->sentry->ssrc);
}

void
ui_info_mute(session_struct *sp, rtcp_dbentry *e)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "rtp.source.mute", "\"%08lx\"", e->sentry->ssrc);
}

void
ui_info_remove(session_struct *sp, rtcp_dbentry *e)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "rtp.source.remove", "\"%08lx\"", e->sentry->ssrc);
}

void
ui_info_activate(session_struct *sp, rtcp_dbentry *e)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "rtp.source.active", "\"%08lx\"", e->sentry->ssrc);
}

void
ui_info_deactivate(session_struct *sp, rtcp_dbentry *e)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "rtp.source.inactive", "\"%08lx\"", e->sentry->ssrc);
}

void
ui_info_3d_settings(session_struct *sp, rtcp_dbentry *e)
{
        char *filter_name;
        int   azimuth, filter_type, filter_length;

        if (e->render_3D_data == NULL) {
                e->render_3D_data = render_3D_init(get_freq(sp->device_clock));
        }

        render_3D_get_parameters(e->render_3D_data, &azimuth, &filter_type, &filter_length);
        filter_name = mbus_encode_str(render_3D_filter_get_name(filter_type));
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.3d.user.settings", "\"%08lx\" %s %d %d", 
		  e->sentry->ssrc, filter_name, filter_length, azimuth);
        xfree(filter_name);
}

void
ui_update_stats(session_struct *sp, rtcp_dbentry *e)
{
	char			*args, *mbes;
        struct s_source      	*src;
        u_int32               	 buffered, delay;

        if (sp->db->my_dbe->sentry == NULL) {
                debug_msg("Warning sentry or name == NULL\n");
                return;
        }

        if (e->enc_fmt) {
		mbes = mbus_encode_str(e->enc_fmt);
                args = (char *) xmalloc(strlen(mbes) + 12);
                sprintf(args, "\"%08lx\" %s", e->sentry->ssrc, mbes);
                xfree(mbes);
        } else {
                args = (char *) xmalloc(19);
                sprintf(args, "\"%08lx\" unknown", e->sentry->ssrc);
        }

        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.source.codec", args, FALSE);

        xfree(args);

        src = source_get_by_rtcp_dbentry(sp->active_sources, e);
        if (src) {
                buffered = ts_to_ms(source_get_audio_buffered (src));
                delay    = ts_to_ms(source_get_playout_delay  (src, sp->cur_ts));
        } else {
                buffered = 0;
                delay    = 0;
        }

        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "tool.rat.audio.buffered", "\"%08lx\" %ld", e->sentry->ssrc, buffered);
        
        if (src == NULL || delay != 0) {
                mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "tool.rat.audio.delay", "\"%08lx\" %ld", e->sentry->ssrc, delay);
        }

	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "rtp.source.packet.loss", "\"%08lx\" \"%08lx\" %8ld", 
		  sp->db->my_dbe->sentry->ssrc, e->sentry->ssrc, (e->lost_frac * 100) >> 8);
}

void
ui_update_input_port(session_struct *sp)
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.port", mbes, TRUE);
        xfree(mbes);

	if (tx_is_sending(sp->tb)) {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.mute", "1", TRUE);
	}
        ui_update_input_gain(sp);
}

void
ui_update_output_port(session_struct *sp)
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

        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.port", mbes, TRUE);
        xfree(mbes);

	if (sp->playing_audio) {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.output.mute", "1", TRUE);
	}
        ui_update_output_gain(sp);
}

void
ui_input_level(session_struct *sp, int level)
{
	static int	ol;

        assert(level>=0 && level <=100);

	if (ol == level) {
		return;
	}

	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "audio.input.powermeter", "%3d", level);
	ol = level;
}

void
ui_output_level(session_struct *sp, int level)
{
	static int	ol;
        assert(level>=0 && level <=100);

	if (ol == level) {
                return;
	}

	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "audio.output.powermeter", "%3d", level);
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
        char          		 fmt_buf[64], *mbes;
        const audio_format 	*af;

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

        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "audio.channel.coding", "\"interleaved\" %d %d",iu, isep);

        UNUSED(sp);
}
*/

static void
ui_update_redundancy(session_struct *sp)
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.coding", out, TRUE);
        xfree(out);

redundancy_update_end:
        xfree(cmd);
}

void 
ui_update_channel(session_struct *sp) 
{
        cc_details cd;
        char *mbes;

        channel_get_coder_identity(sp->channel_coder, &cd);
        switch(tolower(cd.name[0])) {
        case 'n':
                mbes = mbus_encode_str("none");
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.channel.coding", mbes, TRUE);
                xfree(mbes);
                break;
        case 'r':
                ui_update_redundancy(sp);
                break;
        }
        return;
}

void
ui_update_input_gain(session_struct *sp)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "audio.input.gain", "%3d", audio_get_igain(sp->audio_device));
}

void
ui_update_output_gain(session_struct *sp)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "audio.output.gain", "%3d", audio_get_ogain(sp->audio_device));
}

static void
ui_update_3d_enabled(session_struct *sp)
{
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.3d.enabled", "%d", (sp->render_3d ? 1 : 0));
}

static void
ui_input_ports(session_struct *sp)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;
        
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.input.ports.flush", "", TRUE);

        n = audio_get_iport_count(sp->audio_device);
        assert(n >= 1);

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
        assert(n >= 1);

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
        char *mbes;
        int i,nDev;

        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.devices.flush", "", TRUE);
        
        nDev = audio_get_device_count();
        for(i = 0; i < nDev; i++) {
                if (!audio_get_device_details(i, &ad)) continue;
                mbes = mbus_encode_str(ad.name);
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "audio.devices.add", mbes, TRUE);
                xfree(mbes);
        }
}

static void
ui_device(session_struct *sp)
{
        audio_device_details_t ad;
        char                  *mbes, *cur_dev;
        int                    i, n;

        cur_dev = NULL;

        n = audio_get_device_count();
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

	/*XXX solaris seems to give a different volume back to what we   */
	/*    actually set.  So don't even ask if it's not the first time*/
	if (done==0) {
                ui_update_input_gain(sp);
                ui_update_output_gain(sp);
		done=1;
	} 

	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.rate", "%3d", channel_encoder_get_units_per_packet(sp->channel_coder));
        
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
	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.lecture.mode", "%1d", sp->lecture);
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
ui_update_loss(session_struct *sp, u_int32 srce, u_int32 dest, int loss)
{
	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "rtp.source.packet.loss", "\"%08lx\" \"%08lx\" %3d", srce, dest, loss);
}

void
ui_update_reception(session_struct *sp, u_int32 ssrc, u_int32 recv, u_int32 lost, u_int32 misordered, u_int32 duplicates, u_int32 jitter, int jit_tog)
{
	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "rtp.source.reception", "\"%08lx\" %6ld %6ld %6ld %6ld %6ld %6d", 
		  ssrc, recv, lost, misordered, duplicates, jitter, jit_tog);
}

void
ui_update_duration(session_struct *sp, u_int32 ssrc, int duration)
{
	mbus_qmsgf(sp->mbus_engine, mbus_name_ui, FALSE, "rtp.source.packet.duration", "\"%08lx\" %3d", ssrc, duration);
}

void 
ui_update_video_playout(session_struct *sp, u_int32 ssrc, int playout)
{
	mbus_qmsgf(sp->mbus_engine, mbus_name_video, FALSE, "rtp.source.playout", "\"%08lx\" %12d", ssrc, playout);
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
        char 			*caps, *long_name_e, *short_name_e, *pay_e, *descr_e;
        int 			 can_enc, can_dec;
        char 			 pay[4];
        u_char 			 pt;
        const codec_format_t	*cf;
        
        cf  = codec_get_format(cid);
        assert(cf != NULL);

        can_enc = codec_can_encode(cid);
        can_dec = codec_can_decode(cid);

        assert(can_enc || can_dec);
        caps = NULL;
        if (can_enc && can_dec) {
                caps = mbus_encode_str("Encode and decode");
        } else if (can_enc) {
                caps = mbus_encode_str("Encode only");
        } else if (can_dec) {
                caps = mbus_encode_str("Decode only");
        }

        pt = codec_get_payload(cid);
        if (payload_is_valid(pt)) {
                sprintf(pay, "%3d", pt);
        } else {
                sprintf(pay, "-");
        }
	pay_e        = mbus_encode_str(pay);
	long_name_e  = mbus_encode_str(cf->long_name);
	short_name_e = mbus_encode_str(cf->short_name);
	descr_e      = mbus_encode_str(cf->description);

        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, 
                   "tool.rat.codec.details",
                   "%s %s %s %d %d %d %d %d %s %s",
                   pay_e,
                   long_name_e,
                   short_name_e,
                   cf->format.channels,
                   cf->format.sample_rate,
                   cf->format.bytes_per_block,
                   cf->mean_per_packet_state_size,
                   cf->mean_coded_frame_size,
                   descr_e,
                   caps);
	xfree(caps);
	xfree(pay_e);
	xfree(long_name_e);
	xfree(short_name_e);
	xfree(descr_e);
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
        char *mbes;
        int i, cnt;

        cnt = converter_get_count();

        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.converters.flush", "", TRUE);

        for (i = 0; i < cnt; i++) {
                converter_get_details(i, &details);
                mbes = mbus_encode_str(details.name);
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.converters.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_update_converter(session_struct *sp)
{
        converter_details_t details;
        char *mbes;
        int i, cnt;

        cnt = converter_get_count();

        for(i = 0; i < cnt; i++) {
                converter_get_details(i, &details);
                if (sp->converter == details.id) {
                        mbes = mbus_encode_str(details.name);
                        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.converter", mbes, TRUE);
                        xfree(mbes);
                        return;
                }
        }
}

static void
ui_repair_schemes(session_struct *sp)
{
        char *mbes;
        u_int16 i, cnt;
        
        cnt = repair_get_count();
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.repairs.flush", "", TRUE);

        for(i = 0; i < cnt; i++) {
                mbes = mbus_encode_str(repair_get_name(i));
                mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.repairs.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_update_repair(session_struct *sp)
{
        char *mbes;

        assert(sp->repair < repair_get_count());
        mbes = mbus_encode_str(repair_get_name((u_int16)sp->repair));
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.repair", mbes, TRUE);
        xfree(mbes);
}

void
ui_controller_init(session_struct *sp, u_int32 ssrc, char *name_engine, char *name_ui, char *name_video)
{
	char	my_ssrc[11];

	mbus_name_engine = name_engine;
	mbus_name_ui     = name_ui;
	mbus_name_video  = name_video;

	sprintf(my_ssrc, "\"%08lx\"", ssrc);
	mbus_qmsg(sp->mbus_engine, mbus_name_ui, "rtp.ssrc", my_ssrc, TRUE);
}

static void
ui_title(session_struct *sp) 
{
	char	*addr, *title;

        title = mbus_encode_str(sp->title);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "session.title", title, TRUE);
        xfree(title);

	addr = mbus_encode_str(sp->asc_address);
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "session.address", "%s %5d %3d", addr, sp->rtp_port, sp->ttl);
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
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.3d.filter.types", mbes, TRUE);
        xfree(mbes);

        args[0] = '\0';
        cnt = render_3D_filter_get_lengths_count();
        for(i = 0; i < cnt; i++) {
                sprintf(tmp, "%d", render_3D_filter_get_length(i));
                strcat(args, tmp);
                if (i != cnt - 1) strcat(args, ",");
        }
        
        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine, mbus_name_ui, "tool.rat.3d.filter.lengths", mbes, TRUE);
        xfree(mbes);

        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.3d.azimuth.min", "%d", render_3D_filter_get_lower_azimuth());
        mbus_qmsgf(sp->mbus_engine, mbus_name_ui, TRUE, "tool.rat.3d.azimuth.max", "%d", render_3D_filter_get_upper_azimuth());
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

