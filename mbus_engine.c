/*
 * FILE:    mbus_engine.c
 * AUTHORS: Colin Perkins
 * MODIFICATIONS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus_engine.h"
#include "mbus_ui.h"
#include "mbus.h"
#include "ui.h"
#include "net_udp.h"
#include "net.h"
#include "transmit.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "session.h"
#include "channel.h"
#include "convert.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "render_3D.h"
#include "crypt.h"
#include "session.h"
#include "source.h"
#include "sndfile.h"
#include "timers.h"
#include "util.h"

extern int should_exit;

static void rx_tool_rat_powermeter(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->meter = i;
		ui_input_level(sp, 0);
		ui_output_level(sp, 0);
	} else {
		debug_msg("mbus: usage \"tool.rat.powermeter <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_silence(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->detect_silence = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.silence <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_3d_enable(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
                audio_device_register_change_render_3d(sp, i);
	} else {
		debug_msg("mbus: usage \"tool.rat.3d.enabled <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_3d_user_settings(char *srce, char *args, session_struct *sp)
{
        struct s_rtcp_dbentry 	*e;
        char 			*filter_name;
        int 			 filter_type, filter_length, azimuth, freq;
	u_int32			 ssrc;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) &&
            mbus_parse_str(sp->mbus_engine, &filter_name) &&
            mbus_parse_int(sp->mbus_engine, &filter_length) &&
            mbus_parse_int(sp->mbus_engine, &azimuth)) {

                mbus_decode_str(filter_name);

                e = rtcp_get_dbentry(sp, ssrc);
                if (e != NULL) {
                        filter_type = render_3D_filter_get_by_name(filter_name);
                        freq        = get_freq(sp->device_clock);
                        if (e->render_3D_data == NULL) {
                                e->render_3D_data = render_3D_init(get_freq(sp->device_clock));
                        }
                        render_3D_set_parameters(e->render_3D_data, freq, azimuth, filter_type, filter_length);
                } else {
			debug_msg("Unknown source 0x%08ld\n", ssrc);
		}
        } else {
                debug_msg("mbus: usage \"tool.rat.3d.user.settings <cname> <filter name> <filter len> <azimuth>\"\n");
        }
	mbus_parse_done(sp->mbus_engine);
}

static void
rx_tool_rat_3d_user_settings_req(char *srce, char *args, session_struct *sp)
{
	u_int32		 ssrc;
        rtcp_dbentry 	*e = NULL;

	UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc)) {
                e = rtcp_get_dbentry(sp, ssrc);
		if (e != NULL) {
			ui_info_3d_settings(sp, e);
		} else {
			debug_msg("Source 0x%08lx not found\n", ssrc);
		}
        }
        mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_lecture(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->lecture = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.lecture <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_sync(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->sync_on = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.sync <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_agc(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->agc_on = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_audio_loopback(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
                if (i) {
                        audio_loopback(sp->audio_device, 100);
                } else {
                        audio_loopback(sp->audio_device, 0);
                }
	} else {
		debug_msg("mbus: usage \"tool.rat.audio.loopback <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_echo_suppress(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->echo_suppress = i;
                if (sp->echo_suppress) {
                        source_list_clear(sp->active_sources);
                }
	} else {
		debug_msg("mbus: usage \"tool.rat.echo.suppress <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_rate(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
                assert(sp->channel_coder != NULL);
                channel_encoder_set_units_per_packet(sp->channel_coder, (u_int16)i);
	} else {
		debug_msg("mbus: usage \"tool.rat.rate <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_input_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		if (i) {
                        if (tx_is_sending(sp->tb)) {
                                tx_stop(sp->tb);
                        }
		} else {
                        if (tx_is_sending(sp->tb) == FALSE) {
                                tx_start(sp->tb);
                        }
		}
                sp->echo_was_sending = i;
		ui_update_input_port(sp);
	} else {
		debug_msg("mbus: usage \"audio.input.mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_input_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);
 
	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i) && 
            (i >= 0 && i <= 100)) {
                audio_set_igain(sp->audio_device, i);
                tx_igain_update(sp->tb);
	} else { 
		debug_msg("mbus: usage \"audio.input.gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_input_port(char *srce, char *args, session_struct *sp)
{
        const audio_port_details_t *apd;
	char	*s;
        int      i, n, found;
	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
		s = mbus_decode_str(s);
                n = audio_get_iport_count(sp->audio_device);
                found = FALSE;
                for(i = 0; i < n; i++) {
                        apd = audio_get_iport_details(sp->audio_device, i);
                        if (!strcasecmp(s, apd->name)) {
                                found = TRUE;
                                break;
                        }
                }
                if (found == FALSE) {
                        debug_msg("%s does not match any port names\n", s);
                        apd = audio_get_iport_details(sp->audio_device, 0);
                }
                audio_set_iport(sp->audio_device, apd->port);
	} else {
		debug_msg("mbus: usage \"audio.input.port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
        ui_update_input_port(sp);
}

static void rx_audio_output_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
        	sp->playing_audio = !i; 
		ui_update_output_port(sp);
	} else {
		debug_msg("mbus: usage \"audio.output.mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_output_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i) &&
            (i >= 0 && i <= 100)) {
		audio_set_ogain(sp->audio_device, i);
	} else {
		debug_msg("mbus: usage \"audio.output.gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_output_port(char *srce, char *args, session_struct *sp)
{
        const audio_port_details_t *apd;
	char *s;
        int   i, n, found;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
		s = mbus_decode_str(s);
                n     = audio_get_oport_count(sp->audio_device);
                found = FALSE;                

                for(i = 0; i < n; i++) {
                        apd = audio_get_oport_details(sp->audio_device, i);
                        if (!strcasecmp(s, apd->name)) {
                                found = TRUE;
                                break;
                        }
                }
                if (found == FALSE) {
                        debug_msg("%s does not match any port names\n", s);
                        apd = audio_get_oport_details(sp->audio_device, 0);
                }
                audio_set_oport(sp->audio_device, apd->port);
	} else {
		debug_msg("mbus: usage \"audio.output.port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
	ui_update_output_port(sp);
}

static void rx_audio_channel_repair(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
		s = mbus_decode_str(s);
                if (!strcasecmp(s, "first")) {
                        sp->repair = repair_get_by_name(repair_get_name(0));
                } else {
                        sp->repair = repair_get_by_name(s);
                }
	} else {
		debug_msg("mbus: usage \"audio.channel.repair <repair>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
        ui_update_repair(sp);
}

static void rx_security_encryption_key(char *srce, char *args, session_struct *sp)
{
	char	*key;

	UNUSED(sp);
	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &key)) {
		Set_Key(mbus_decode_str(key));
	} else {
		debug_msg("mbus: usage \"security.encryption.key <key>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_file_play_stop(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);
        UNUSED(args);

	if (sp->in_file != NULL) {
		snd_read_close(&sp->in_file);
	}
}

static void rx_audio_file_play_open(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);
        UNUSED(sp);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &file)) {
                mbus_decode_str(file);
                if (sp->in_file) snd_read_close(&sp->in_file);
                if (snd_read_open(&sp->in_file, file)) {
                        debug_msg("Hooray opened %s\n",file);
                }
	} else {
		debug_msg("mbus: usage \"audio.file.play.open <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);

        if (sp->in_file) ui_update_playback_file(sp, file);

}

static void rx_audio_file_play_pause(char *srce, char *args, session_struct *sp)
{
        int pause;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);

        if (mbus_parse_int(sp->mbus_engine, &pause)) {
                if (sp->in_file) {
                        if (pause) {
                                snd_pause(sp->in_file);
                        } else {
                                snd_resume(sp->in_file);
                        }
                }
        } else {
                debug_msg("mbus: usage \"audio.file.play.pause <bool>\"\n");        
        }
        mbus_parse_done(sp->mbus_engine);
}


static void rx_audio_file_play_live(char *srce, char *args, session_struct *sp)
{
        /* This is a request to see if file we are playing is still valid */
        UNUSED(args);
        UNUSED(srce);
        ui_update_file_live(sp, "play", (sp->in_file) ? 1 : 0);
}

static void rx_audio_file_rec_stop(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);
        UNUSED(args);

        if (sp->out_file != NULL) {
		snd_write_close(&sp->out_file);
	}
}

static void rx_audio_file_rec_open(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &file)) {
                const audio_format *ofmt;
                ofmt = audio_get_ofmt(sp->audio_device);
                mbus_decode_str(file);
                if (sp->out_file) snd_write_close(&sp->out_file);

                if (snd_write_open(&sp->out_file, file, (u_int16)get_freq(sp->device_clock), (u_int16)ofmt->channels)) {
                        debug_msg("Hooray opened %s\n",file);
                }
	} else {
		debug_msg("mbus: usage \"audio.file.record.open <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
        
        if (sp->out_file) ui_update_record_file(sp, file);
}

static void rx_audio_file_rec_pause(char *srce, char *args, session_struct *sp)
{
        int pause;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);

        if (mbus_parse_int(sp->mbus_engine, &pause)) {
                if (sp->out_file) {
                        if (pause) {
                                snd_pause(sp->out_file);
                        } else {
                                snd_resume(sp->out_file);
                        }
                }
        } else {
                debug_msg("mbus: usage \"audio.file.record.pause <bool>\"\n");        
        }
        mbus_parse_done(sp->mbus_engine);
}

static void rx_audio_file_rec_live(char *srce, char *args, session_struct *sp)
{
        /* This is a request to see if file we are recording is still valid */
        UNUSED(args);
	UNUSED(srce);
        debug_msg("%d\n", sp->out_file ? 1 : 0);
        ui_update_file_live(sp, "record", (sp->out_file) ? 1 : 0);
}

static void 
rx_audio_device(char *srce, char *args, session_struct *sp)
{
        char	*s, dev_name[64];

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
		s = mbus_decode_str(s);
                purge_chars(s, "[]()");
                if (s) {
                        audio_device_details_t details;
                        int i, n;
                        n = audio_get_device_count();
                        for(i = 0; i < n; i++) {
                                /* Brackets are a problem so purge them */
                                if (!audio_get_device_details(i, &details)) continue;
                                strncpy(dev_name, details.name, AUDIO_DEVICE_NAME_LENGTH);
                                purge_chars(dev_name, "[]()");
                                if (!strcmp(s, dev_name)) {
                                        audio_device_register_change_device(sp, details.descriptor);
                                        break;
                                }
                        }
                }
	} else {
		debug_msg("mbus: usage \"audio.device <string>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_rtp_source_name(char *srce, char *args, session_struct *sp)
{
	char	*arg;
	u_int32	 ssrc;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && (ssrc == sp->db->myssrc) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_NAME,  mbus_decode_str(arg));
	} else {
		debug_msg("mbus: usage \"rtp_source_name <ssrc> <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_rtp_source_email(char *srce, char *args, session_struct *sp)
{
	char	*arg;
	u_int32	 ssrc;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && (ssrc == sp->db->myssrc) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_EMAIL,  mbus_decode_str(arg));
	} else {
		debug_msg("mbus: usage \"rtp_source_email <ssrc> <email>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_rtp_source_phone(char *srce, char *args, session_struct *sp)
{
	char	*arg;
	u_int32	 ssrc;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && (ssrc == sp->db->myssrc) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_PHONE,  mbus_decode_str(arg));
	} else {
		debug_msg("mbus: usage \"rtp_source_phone <ssrc> <phone>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_rtp_source_loc(char *srce, char *args, session_struct *sp)
{
	char	*arg;
	u_int32	 ssrc;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && (ssrc == sp->db->myssrc) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_LOC,  mbus_decode_str(arg));
	} else {
		debug_msg("mbus: usage \"rtp_source_loc <ssrc> <loc>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_rtp_source_mute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	u_int32		 ssrc;
	int		 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && mbus_parse_int(sp->mbus_engine, &i)) {
                e = rtcp_get_dbentry(sp, ssrc);
		if (e != NULL) {
                        e->mute = i;
                        ui_info_mute(sp, e);
                } else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_mute <ssrc> <bool>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}


static void rx_rtp_source_playout(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	u_int32		 ssrc;
	int	 	 playout;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, (int *) &ssrc) && mbus_parse_int(sp->mbus_engine, &playout)) {
                e = rtcp_get_dbentry(sp, ssrc);
		if (e != NULL) {
                	e->video_playout_received = TRUE;
			e->video_playout          = playout;
		} else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_playout <ssrc> <playout>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void 
rx_tool_rat_codec(char *srce, char *args, session_struct *sp)
{
	char	*short_name, *sfreq, *schan;
        int      freq, channels;
        codec_id_t cid, next_cid;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &short_name) &&
            mbus_parse_str(sp->mbus_engine, &schan) &&
            mbus_parse_str(sp->mbus_engine, &sfreq)) {
                mbus_decode_str(short_name);
                mbus_decode_str(schan);
                mbus_decode_str(sfreq);
                mbus_parse_done(sp->mbus_engine);
        } else {
		debug_msg("mbus: usage \"audio.codec <codec> <freq> <channels>\"\n");
                mbus_parse_done(sp->mbus_engine);
                return;
        }

        if (strcasecmp(schan, "mono") == 0) {
                channels = 1;
        } else if (strcasecmp(schan, "stereo") == 0) {
                channels = 2;
        } else {
                channels = 0;
        }

        freq = atoi(sfreq) * 1000;
        next_cid = codec_get_matching(short_name, (u_int16)freq, (u_int16)channels);

        if (next_cid && codec_get_payload(next_cid) != 255) {
                cid     = codec_get_by_payload ((u_char)sp->encodings[0]);
                if (codec_audio_formats_compatible(next_cid, cid)) {
                        sp->encodings[0] = codec_get_payload(next_cid);
                        ui_update_primary(sp);
                } else {
                        /* just register we want to make a change */
                        audio_device_register_change_primary(sp, next_cid);
                }
        }
}

static void rx_tool_rat_playout_limit(char *srce, char *args, session_struct *sp)
{
        int i;

        UNUSED(srce);
        mbus_parse_init(sp->mbus_engine, args);
        if (mbus_parse_int(sp->mbus_engine, &i) && (1 == i || 0 == i)) {
                sp->limit_playout = i;
        } else {
		debug_msg("mbus: usage \"tool.rat.playout.limit <bool>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_playout_min(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->min_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.min <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_playout_max(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->max_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.max <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_payload_set(char *srce, char *args, session_struct *sp)
{
        codec_id_t cid, cid_replacing;
        char *codec_long_name;
        int   i, new_pt;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);

        if (mbus_parse_str(sp->mbus_engine, &codec_long_name) &&
            mbus_parse_int(sp->mbus_engine, &new_pt)) {
                mbus_decode_str(codec_long_name);

                if (payload_is_valid((u_char)new_pt) == FALSE ||
                    new_pt < 0 || new_pt > 255) {
                        debug_msg("Invalid payload specified\n");
                        mbus_parse_done(sp->mbus_engine);
                        return;
                }

                for(i = 0; i < sp->num_encodings; i++) {
                        if (new_pt == sp->encodings[i]) {
                                debug_msg("Doh! Attempting to remap encoding %d codec.\n", i);
                                mbus_parse_done(sp->mbus_engine);
                                return;
                        }
                }

                cid_replacing = codec_get_by_payload((u_char)new_pt);
                if (cid_replacing) {
                        const codec_format_t *cf;
                        cf = codec_get_format(cid_replacing);
                        assert(cf);
                        debug_msg("Codec map replacing %s\n", cf->long_name);
                        codec_unmap_payload(cid_replacing, (u_char)new_pt);
                        ui_update_codec(sp, cid_replacing);
                }

                cid = codec_get_by_name(codec_long_name);
                if (cid && codec_map_payload(cid, (u_char)new_pt)) {
                        ui_update_codec(sp, cid);
                        debug_msg("map %s %d succeeded.\n", codec_long_name, new_pt);
                } else {
                        debug_msg("map %s %d failed.\n", codec_long_name, new_pt);
                }
        }
        mbus_parse_done(sp->mbus_engine);
}

static void rx_tool_rat_converter(char *srce, char *args, session_struct *sp)
{
        converter_details_t d;
        u_int32             i, n;
        char               *name;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &name)) {
                mbus_decode_str(name);
                n = converter_get_count();
                for(i = 0; i < n; i++) {
                        converter_get_details(i, &d);
                        if (0 == strcasecmp(d.name,name)) {
                                break;
                        }
                }
                if (i == n) {
                        converter_get_details(0, &d);
                }
                sp->converter = d.id;

	} else {
		debug_msg("mbus: usage \"tool.rat.converter <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
        ui_update_converter(sp);
}

static void rx_audio_channel_coding(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

        mbus_parse_init(sp->mbus_engine, args);
        mbus_parse_done(sp->mbus_engine);
	ui_update_channel(sp);
}

static void rx_tool_rat_settings(char *srce, char *args, session_struct *sp)
{
	UNUSED(args);
	UNUSED(srce);
        ui_update(sp);
}

static void rx_mbus_quit(char *srce, char *args, session_struct *sp)
{
	UNUSED(args);
	UNUSED(srce);
	UNUSED(sp);
	ui_quit(sp);
        should_exit = TRUE;
}

static void rx_mbus_waiting(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(sp);
}

static void rx_mbus_go(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);
	UNUSED(args);
	sp->wait_on_startup = FALSE;
}

static void rx_mbus_hello(char *srce, char *args, session_struct *sp)
{
	/* Ignore "hello" messages... */
	UNUSED(args);
	UNUSED(srce);
	UNUSED(sp);
}

/* Note: These next two arrays MUST be in the same order! */

const char *rx_cmnd[] = {
	"tool.rat.silence",
	"tool.rat.lecture",
	"tool.rat.3d.enabled",
        "tool.rat.3d.user.settings",
        "tool.rat.3d.user.settings.request",
	"tool.rat.agc",
        "tool.rat.loopback",
        "tool.rat.echo.suppress",
	"tool.rat.sync",       
	"tool.rat.rate",          
	"tool.rat.powermeter",
        "tool.rat.converter",
        "tool.rat.settings",
	"tool.rat.codec",
        "tool.rat.playout.limit",
        "tool.rat.playout.min",            
        "tool.rat.playout.max",            
        "tool.rat.payload.set",
	"audio.input.mute",
	"audio.input.gain",
	"audio.input.port",
	"audio.output.mute",
	"audio.output.gain",
	"audio.output.port",
        "audio.channel.coding",
	"audio.channel.repair",
        "audio.file.play.open",   
        "audio.file.play.pause",
        "audio.file.play.stop",
        "audio.file.play.live",
	"audio.file.record.open",
        "audio.file.record.pause",
	"audio.file.record.stop",
        "audio.file.record.live",
        "audio.device",
        "security.encryption.key",             
        "rtp.source.name",
	"rtp.source.email",
	"rtp.source.phone",       
	"rtp.source.loc",
	"rtp.source.mute",
	"rtp.source.playout",
	"mbus.quit",
	"mbus.waiting",
	"mbus.go",
	"mbus.hello",             
	""
};

static void (*rx_func[])(char *srce, char *args, session_struct *sp) = {
	rx_tool_rat_silence,
	rx_tool_rat_lecture,
	rx_tool_rat_3d_enable,
        rx_tool_rat_3d_user_settings,
        rx_tool_rat_3d_user_settings_req,
	rx_tool_rat_agc,
        rx_tool_rat_audio_loopback,
        rx_tool_rat_echo_suppress,
	rx_tool_rat_sync,
	rx_tool_rat_rate,              
	rx_tool_rat_powermeter,
        rx_tool_rat_converter,
        rx_tool_rat_settings,
	rx_tool_rat_codec,
        rx_tool_rat_playout_limit,
        rx_tool_rat_playout_min,
        rx_tool_rat_playout_max,                
        rx_tool_rat_payload_set,
	rx_audio_input_mute,
	rx_audio_input_gain,
	rx_audio_input_port,
	rx_audio_output_mute,
	rx_audio_output_gain,
	rx_audio_output_port,
        rx_audio_channel_coding,
	rx_audio_channel_repair,
        rx_audio_file_play_open,       
        rx_audio_file_play_pause,
        rx_audio_file_play_stop,                
        rx_audio_file_play_live,
        rx_audio_file_rec_open,
	rx_audio_file_rec_pause,
        rx_audio_file_rec_stop,
        rx_audio_file_rec_live,
	rx_audio_device,
        rx_security_encryption_key,
	rx_rtp_source_name,
	rx_rtp_source_email,
	rx_rtp_source_phone,            
	rx_rtp_source_loc,
	rx_rtp_source_mute,
	rx_rtp_source_playout,
	rx_mbus_quit,
	rx_mbus_waiting,
	rx_mbus_go,
	rx_mbus_hello,
        NULL
};

void mbus_engine_rx(char *srce, char *cmnd, char *args, void *data)
{
	int i;

	debug_msg("%s (%s)\n", cmnd, args);
	for (i=0; strlen(rx_cmnd[i]) != 0; i++) {
		if (strcmp(rx_cmnd[i], cmnd) == 0) {
                        rx_func[i](srce, args, (session_struct *) data);
			return;
		} 
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
#ifndef NDEBUG
	abort();
#endif
}

