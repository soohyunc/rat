/*
 * FILE:    mbus_engine.c
 * AUTHORS: Colin Perkins
 * MODIFICATIONS: Orion Hodson
 * 
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus_engine.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "net_udp.h"
#include "session.h"
#include "net.h"
#include "transmit.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "session.h"
#include "channel.h"
#include "converter.h"
#include "repair.h"
#include "render_3D.h"
#include "session.h"
#include "pdb.h"
#include "source.h"
#include "sndfile.h"
#include "timers.h"
#include "util.h"
#include "codec_types.h"
#include "channel_types.h"
#include "rtp.h"
#include "rtp_callback.h"
#include "ui.h"

extern int should_exit;

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args, session_t *sp);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
        const char   *rxname;
        mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static void rx_session_title(char *srce, char *args, session_t *sp)
{
	char			*title;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &title)) {
		sp->title = xstrdup(mbus_decode_str(title));
	} else {
		debug_msg("mbus: usage \"session.title <title>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_addr_ui(char *srce, char *args, session_t *sp)
{
	/* tool.rat.addr.ui ("addr") */
	char			*addr;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &addr)) {
		sp->mbus_ui_addr = xstrdup(mbus_decode_str(addr));
	} else {
		debug_msg("mbus: usage \"tool.rat.addr.ui <addr>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_powermeter(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->meter = i;
		ui_input_level(sp, 0);
		ui_output_level(sp, 0);
	} else {
		debug_msg("mbus: usage \"tool.rat.powermeter <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_silence(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->detect_silence = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.silence <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_3d_enable(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
                audio_device_register_change_render_3d(sp, i);
	} else {
		debug_msg("mbus: usage \"tool.rat.3d.enabled <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void 
rx_tool_rat_3d_user_settings(char *srce, char *args, session_t *sp)
{
        pdb_entry_t             *p;
        char 			*filter_name;
        int 			 filter_type, filter_length, azimuth, freq;
	char			*ss;
        uint32_t                 ssrc;
	struct mbus_parser	*mp;

        UNUSED(srce);

        mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss) &&
            mbus_parse_str(mp, &filter_name) &&
            mbus_parse_int(mp, &filter_length) &&
            mbus_parse_int(mp, &azimuth)) {

                mbus_decode_str(filter_name);
		ss = mbus_decode_str(ss);
                ssrc = strtoul(ss, 0, 16);

                if (pdb_item_get(sp->pdb, ssrc, &p)) {
                        filter_type = render_3D_filter_get_by_name(filter_name);
                        freq        = get_freq(sp->device_clock);
                        if (p->render_3D_data == NULL) {
                                p->render_3D_data = render_3D_init(get_freq(sp->device_clock));
                        }
                        render_3D_set_parameters(p->render_3D_data, 
                                                 freq, 
                                                 azimuth, 
                                                 filter_type, 
                                                 filter_length);
                } else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
        } else {
                debug_msg("mbus: usage \"tool.rat.3d.user.settings <cname> <filter name> <filter len> <azimuth>\"\n");
        }
	mbus_parse_done(mp);
}

static void
rx_tool_rat_3d_user_settings_req(char *srce, char *args, session_t *sp)
{
	char			*ss;
        uint32_t         	 ssrc;
	struct mbus_parser	*mp;

	UNUSED(srce);

        mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss)) {
		ss   = mbus_decode_str(ss);
                ssrc = strtoul(ss, 0, 16);
                ui_info_3d_settings(sp, ssrc);
        }
        mbus_parse_done(mp);
}

static void rx_tool_rat_lecture(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->lecture = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.lecture <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_agc(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->agc_on = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.agc <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_audio_loopback(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
                if (i) {
                        audio_loopback(sp->audio_device, 100);
			sp->loopback_gain = 100;
                } else {
                        audio_loopback(sp->audio_device, 0);
			sp->loopback_gain = 0;
                }
	} else {
		debug_msg("mbus: usage \"tool.rat.audio.loopback <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_echo_suppress(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->echo_suppress = i;
                if (sp->echo_suppress     == FALSE && 
                    sp->echo_tx_active    == TRUE  && 
                    tx_is_sending(sp->tb) == FALSE) {
                        /* Suppressor has just been disabled,  transmitter  */
                        /* is in suppressed state and would otherwise be    */
                        /* active.  Therefore start it up now.              */
                        tx_start(sp->tb);
                }
	} else {
		debug_msg("mbus: usage \"tool.rat.echo.suppress <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_rate(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
                assert(sp->channel_coder != NULL);
                channel_encoder_set_units_per_packet(sp->channel_coder, (uint16_t)i);
	} else {
		debug_msg("mbus: usage \"tool.rat.rate <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_input_mute(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		if (i) {
                        if (tx_is_sending(sp->tb)) {
                                tx_stop(sp->tb);
                        }
		} else {
                        if (tx_is_sending(sp->tb) == FALSE) {
                                tx_start(sp->tb);
                        }
		}
                /* Keep echo suppressor informed of change */
                sp->echo_tx_active = !i;
		ui_update_input_port(sp);
	} else {
		debug_msg("mbus: usage \"audio.input.mute <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_input_gain(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);
 
	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i) && 
            (i >= 0 && i <= 100)) {
                audio_set_igain(sp->audio_device, i);
                tx_igain_update(sp->tb);
	} else { 
		debug_msg("mbus: usage \"audio.input.gain <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_input_port(char *srce, char *args, session_t *sp)
{
        const audio_port_details_t *apd = NULL;
	char			*s;
        int      		 i, n, found;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
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
	mbus_parse_done(mp);
        ui_update_input_port(sp);
}

static void rx_audio_output_mute(char *srce, char *args, session_t *sp)
{
	struct mbus_parser	*mp;
        struct s_source 	*s;
	int 			 i, n;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
        	sp->playing_audio = !i; 
		ui_update_output_port(sp);
                n = (int)source_list_source_count(sp->active_sources);
                for (i = 0; i < n; i++) {
                        s = source_list_get_source_no(sp->active_sources, i);
                        ui_info_deactivate(sp, source_get_ssrc(s));
                        source_remove(sp->active_sources, s);
                        /* revise source no's since we removed a source */
                        i--;
                        n--;
                }
	} else {
		debug_msg("mbus: usage \"audio.output.mute <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_output_gain(char *srce, char *args, session_t *sp)
{
	struct mbus_parser	*mp;
	int			 i;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i) &&
            (i >= 0 && i <= 100)) {
		audio_set_ogain(sp->audio_device, i);
	} else {
		debug_msg("mbus: usage \"audio.output.gain <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_output_port(char *srce, char *args, session_t *sp)
{
        const audio_port_details_t *apd = NULL;
	char *s;
        int   i, n, found;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
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
	mbus_parse_done(mp);
	ui_update_output_port(sp);
}

static void rx_audio_channel_repair(char *srce, char *args, session_t *sp)
{
        const repair_details_t *r;
        uint16_t i, n;
	char	*s;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
                if (strcasecmp(s, "first") == 0) {
                        r = repair_get_details(0);
                        sp->repair = r->id;
                } else {
                        n = repair_get_count();
                        for(i = 0; i < n; i++) {
                                r = repair_get_details(i);
                                if (strcasecmp(r->name, s) == 0 || strcasecmp("first", s) == 0) {
                                        sp->repair = r->id;
                                        break;
                                }
                        }
                }
	} else {
		debug_msg("mbus: usage \"audio.channel.repair <repair>\"\n");
	}
	mbus_parse_done(mp);
        ui_update_repair(sp);
}

static void rx_security_encryption_key(char *srce, char *args, session_t *sp)
{
        int      i;
	char	*key;
	struct mbus_parser	*mp;

	UNUSED(sp);
	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &key)) {
                key = mbus_decode_str(key);
                for(i = 0; i < sp->rtp_session_count; i++) {
			if (strlen(key) == 0) {
                        	rtp_set_encryption_key(sp->rtp_session[i], NULL);
				ui_update_key(sp, "none");
				sp->encrkey = NULL;
			} else {
                        	rtp_set_encryption_key(sp->rtp_session[i], key);
				ui_update_key(sp, key);
				sp->encrkey = xstrdup(key);
			}
                }
	} else {
		debug_msg("mbus: usage \"security.encryption.key <key>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_file_play_stop(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
        UNUSED(args);

	if (sp->in_file != NULL) {
		snd_read_close(&sp->in_file);
	}
}

static void rx_audio_file_play_open(char *srce, char *args, session_t *sp)
{
	char	*file;
	struct mbus_parser	*mp;

	UNUSED(srce);
        UNUSED(sp);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &file)) {
                mbus_decode_str(file);
                if (sp->in_file) snd_read_close(&sp->in_file);
                if (snd_read_open(&sp->in_file, file, NULL)) {
                        debug_msg("Hooray opened %s\n",file);
                }
	} else {
		debug_msg("mbus: usage \"audio.file.play.open <filename>\"\n");
	}
	mbus_parse_done(mp);

        if (sp->in_file) ui_update_playback_file(sp, file);

}

static void rx_audio_file_play_pause(char *srce, char *args, session_t *sp)
{
        int pause;
	struct mbus_parser	*mp;

        UNUSED(srce);

        mp = mbus_parse_init(args);

        if (mbus_parse_int(mp, &pause)) {
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
        mbus_parse_done(mp);
}


static void rx_audio_file_play_live(char *srce, char *args, session_t *sp)
{
        /* This is a request to see if file we are playing is still valid */
        UNUSED(args);
        UNUSED(srce);
        ui_update_file_live(sp, "play", (sp->in_file) ? 1 : 0);
}

static void rx_audio_file_rec_stop(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
        UNUSED(args);

        if (sp->out_file != NULL) {
		snd_write_close(&sp->out_file);
	}
}

static void rx_audio_file_rec_open(char *srce, char *args, session_t *sp)
{
	char	*file;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &file)) {
                sndfile_fmt_t sf_fmt;
                const audio_format *ofmt;
                ofmt = audio_get_ofmt(sp->audio_device);
                mbus_decode_str(file);
                if (sp->out_file) snd_write_close(&sp->out_file);

                sf_fmt.encoding = SNDFILE_ENCODING_L16;
                sf_fmt.sample_rate = (uint16_t)get_freq(sp->device_clock);
                sf_fmt.channels = (uint16_t)ofmt->channels;
#ifdef WIN32
                if (snd_write_open(&sp->out_file, file, "wav", &sf_fmt)) {
                        debug_msg("Hooray opened %s\n",file);
                }
#else
                if (snd_write_open(&sp->out_file, file, "au", &sf_fmt)) {
                        debug_msg("Hooray opened %s\n",file);
                }
#endif /* WIN32 */
	} else {
		debug_msg("mbus: usage \"audio.file.record.open <filename>\"\n");
	}
	mbus_parse_done(mp);
        
        if (sp->out_file) ui_update_record_file(sp, file);
}

static void rx_audio_file_rec_pause(char *srce, char *args, session_t *sp)
{
        int pause;
	struct mbus_parser	*mp;

        UNUSED(srce);

        mp = mbus_parse_init(args);

        if (mbus_parse_int(mp, &pause)) {
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
        mbus_parse_done(mp);
}

static void rx_audio_file_rec_live(char *srce, char *args, session_t *sp)
{
        /* This is a request to see if file we are recording is still valid */
        UNUSED(args);
	UNUSED(srce);
        debug_msg("%d\n", sp->out_file ? 1 : 0);
        ui_update_file_live(sp, "record", (sp->out_file) ? 1 : 0);
}

static void 
rx_audio_device(char *srce, char *args, session_t *sp)
{
        char	*s, dev_name[64], first_dev_name[64];
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
                purge_chars(s, "[]()");
                if (s) {
                        const audio_device_details_t *add = NULL;
                        audio_desc_t           first_dev_desc = 0;
                        uint32_t i, n, stop_at_first_device = FALSE;
                        dev_name[0] = 0;
                        first_dev_name[0] = 0;
                        n = audio_get_device_count();

                        if (!strncasecmp("first", s, 5)) {
                                /* The ui may send first if the saved device is the null audio
                                 * device so it starts up trying to play something.
                                 */
                                stop_at_first_device = TRUE;
                        }

                        for(i = 0; i < n; i++) {
                                /* Brackets are a problem so purge them */
                                add = audio_get_device_details(i);
                                strncpy(dev_name, add->name, AUDIO_DEVICE_NAME_LENGTH);
                                purge_chars(dev_name, "[]()");
                                if (first_dev_name[0] == 0) {
                                        strncpy(first_dev_name, dev_name, AUDIO_DEVICE_NAME_LENGTH);
                                        first_dev_desc = add->descriptor;
                                }

                                if (!strcmp(s, dev_name) | stop_at_first_device) {
                                        break;
                                }
                        }
                        if (i < n) {
                                /* Found device looking for */
                                audio_device_register_change_device(sp, add->descriptor);
                        } else if (first_dev_name[0]) {
                                /* Have a fall back */
                                audio_device_register_change_device(sp, first_dev_desc);
                        }
                }
	} else {
		debug_msg("mbus: usage \"audio.device <string>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_rtp_source_sdes(char *srce, char *args, session_t *sp, uint8_t type)
{
	char	*arg, *ss;
        uint32_t ssrc;
	struct mbus_parser	*mp;
	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss) && 
            mbus_parse_str(mp, &arg)) {
		ss = mbus_decode_str(ss);
                ssrc = strtoul(ss, 0, 16);
		if (ssrc == rtp_my_ssrc(sp->rtp_session[0])) {
                        char *value;
                        int i, vlen;
                        value = mbus_decode_str(arg);
                        vlen  = strlen(value);
                        for (i = 0; i < sp->rtp_session_count; i++) {
                                rtp_set_sdes(sp->rtp_session[i], ssrc, type, value, vlen);
                        }
		} else {
			debug_msg("mbus: ssrc %s (%08lx) != %08lx\n", ss, strtoul(ss, 0, 16), rtp_my_ssrc(sp->rtp_session[0]));
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_<sdes_item> <ssrc> <name>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_rtp_addr(char *srce, char *args, session_t *sp)
{
	/* rtp.addr ("224.1.2.3" 1234 1234 16) */
	char	*addr;
	int	 rx_port, tx_port, ttl;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	mbus_parse_str(mp, &addr); addr = mbus_decode_str(addr);
	mbus_parse_int(mp, &rx_port);
	mbus_parse_int(mp, &tx_port);
	mbus_parse_int(mp, &ttl);
	mbus_parse_done(mp);

	sp->rtp_session[0] = rtp_init(addr, (uint16_t)rx_port, (uint16_t)tx_port, ttl, 64000, rtp_callback, NULL);
	sp->rtp_session_count++;
	rtp_callback_init(sp->rtp_session[0], sp);
}


static void rx_rtp_source_name(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_NAME);
}

static void rx_rtp_source_email(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_EMAIL);
}

static void rx_rtp_source_phone(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_PHONE);
}

static void rx_rtp_source_loc(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_LOC);
}

static void rx_rtp_source_note(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_NOTE);
}

static void rx_rtp_source_mute(char *srce, char *args, session_t *sp)
{
	pdb_entry_t *pdbe;
	char        *ssrc;
	int          i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ssrc) && mbus_parse_int(mp, &i)) {
		ssrc = mbus_decode_str(ssrc);
                if (pdb_item_get(sp->pdb, strtoul(ssrc, 0, 16), &pdbe)) {
                        pdbe->mute = i;
                        ui_info_mute(sp, pdbe->ssrc);
                } else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_mute <ssrc> <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_rtp_source_gain(char *srce, char *args, session_t *sp)
{
	pdb_entry_t	*pdbe;
	char		*ssrc;
        double           g;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ssrc) && mbus_parse_flt(mp, &g)) {
		ssrc = mbus_decode_str(ssrc);
                if (pdb_item_get(sp->pdb, strtoul(ssrc, 0, 16), &pdbe)) {
                        pdbe->gain = g;
                } else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_gain <ssrc> <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void 
rx_tool_rat_codec(char *srce, char *args, session_t *sp)
{
	char	*short_name, *sfreq, *schan;
        int      freq, channels;
        codec_id_t cid, next_cid;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &short_name) &&
            mbus_parse_str(mp, &schan) &&
            mbus_parse_str(mp, &sfreq)) {
                mbus_decode_str(short_name);
                mbus_decode_str(schan);
                mbus_decode_str(sfreq);
                mbus_parse_done(mp);
        } else {
		debug_msg("mbus: usage \"tool.rat.codec <codec> <channels> <freq>\"\n");
                mbus_parse_done(mp);
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
        next_cid = codec_get_matching(short_name, (uint16_t)freq, (uint16_t)channels);

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

static void rx_tool_rat_playout_limit(char *srce, char *args, session_t *sp)
{
        int i;
	struct mbus_parser	*mp;

        UNUSED(srce);
        mp = mbus_parse_init(args);
        if (mbus_parse_int(mp, &i) && (1 == i || 0 == i)) {
                sp->limit_playout = i;
        } else {
		debug_msg("mbus: usage \"tool.rat.playout.limit <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_playout_min(char *srce, char *args, session_t *sp)
{
	int	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->min_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.min <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_playout_max(char *srce, char *args, session_t *sp)
{
	int	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->max_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.max <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_payload_set(char *srce, char *args, session_t *sp)
{
        codec_id_t cid, cid_replacing;
        char *codec_long_name;
        int   i, new_pt;
	struct mbus_parser	*mp;

        UNUSED(srce);

        mp = mbus_parse_init(args);

        if (mbus_parse_str(mp, &codec_long_name) &&
            mbus_parse_int(mp, &new_pt)) {
                mbus_decode_str(codec_long_name);

                if (payload_is_valid((u_char)new_pt) == FALSE ||
                    new_pt < 0 || new_pt > 255) {
                        debug_msg("Invalid payload specified\n");
                        mbus_parse_done(mp);
                        return;
                }
                
                /* Don't allow payloads to be mapped to channel_coder payloads - it doesn't seem to work */
                if (channel_coder_exist_payload((uint8_t)new_pt)) {
                        debug_msg("Channel coder payload specified\n");
                        mbus_parse_done(mp);
                        return;
                }

                for(i = 0; i < sp->num_encodings; i++) {
                        if (new_pt == sp->encodings[i]) {
                                debug_msg("Doh! Attempting to remap encoding %d codec.\n", i);
                                mbus_parse_done(mp);
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
        mbus_parse_done(mp);
}

static void rx_tool_rat_converter(char *srce, char *args, session_t *sp)
{
        const converter_details_t *d = NULL;
        uint32_t             i, n;
        char               *name;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &name)) {
                mbus_decode_str(name);
                n = converter_get_count();
                for(i = 0; i < n; i++) {
                        d = converter_get_details(i);
                        if (0 == strcasecmp(d->name,name)) {
                                break;
                        }
                }
                if (i == n) {
                        d = converter_get_details(0);
                }
                sp->converter = d->id;
	} else {
		debug_msg("mbus: usage \"tool.rat.converter <name>\"\n");
	}
	mbus_parse_done(mp);
        ui_update_converter(sp);
}


/* set_red_parameters translates what mbus_receives into command 
 * redundancy encoder understands.
 */

static void
set_red_parameters(session_t *sp, char *sec_enc, int offset)
{
        const codec_format_t *pcf, *rcf;
        codec_id_t            pri_id, red_id;
        char *cmd;
        int   clen;
        assert(offset>0);

        pri_id = codec_get_by_payload(sp->encodings[0]);
        pcf    = codec_get_format(pri_id);
        red_id = codec_get_matching(sec_enc, (uint16_t)pcf->format.sample_rate, (uint16_t)pcf->format.channels);
        if (!codec_id_is_valid(red_id)) {
                debug_msg("Failed to get redundant codec requested (%s)\n", sec_enc);
                red_id = pri_id;  /* Use same as primary */
        }
        rcf = codec_get_format(red_id);

        clen = 2 * (CODEC_LONG_NAME_LEN + 4);
        cmd  = (char*)xmalloc(clen);
        sprintf(cmd, "%s/%d/%s/%d", pcf->long_name, 0, rcf->long_name, offset);
 
        xmemchk();
        if (channel_encoder_set_parameters(sp->channel_coder, cmd) == 0) {
                debug_msg("Red command failed: %s\n", cmd);
        }
        xmemchk();
        xfree(cmd);
        /* Now tweak session parameters */
        sp->num_encodings = 2;
        sp->encodings[1]  = codec_get_payload(red_id);
}


static void
set_layered_parameters(session_t *sp, char *sec_enc, char *schan, char *sfreq, int layerenc)
{
        const codec_format_t *pcf, *lcf;
        codec_id_t            pri_id, lay_id;
        char *cmd;
        int      freq, channels;
        int   clen, ports_ok, i;
        assert(layerenc>0);

        if (strcasecmp(schan, "mono") == 0) {
                channels = 1;
        } else if (strcasecmp(schan, "stereo") == 0) {
                channels = 2;
        } else {
                channels = 0;
        }

        freq = atoi(sfreq) * 1000;
        pri_id = codec_get_by_payload(sp->encodings[0]);
        pcf    = codec_get_format(pri_id);
        lay_id = codec_get_matching(sec_enc, (uint16_t)freq, (uint16_t)channels);
        if(lay_id == 0) {
                debug_msg("Can't find layered codec (%s) - need to change primary codec\n", sec_enc);
        }
        if (pri_id!=lay_id) {
                /* This can happen if you change codec and select layering    * 
                 * before pushing apply, so change the primary encoding here. */
                codec_id_t cid;
                if (lay_id && codec_get_payload(lay_id) != 255) {
                        cid     = codec_get_by_payload ((u_char)sp->encodings[0]);
                        if (codec_audio_formats_compatible(lay_id, cid)) {
                                sp->encodings[0] = codec_get_payload(lay_id);
                                ui_update_primary(sp);
                        } else {
                                /* just register we want to make a change */
                                audio_device_register_change_primary(sp, lay_id);
                        }
                }
        }                    
        lcf = codec_get_format(lay_id);
        
        if(layerenc<=MAX_LAYERS) {
                ports_ok = PORT_UNINIT;
                for(i=0;i<layerenc;i++) {
                        if(sp->rx_rtp_port[i]==PORT_UNINIT) ports_ok = i;
                }
                if(ports_ok!=PORT_UNINIT) {
                        layerenc = ports_ok;
                        debug_msg("Too many layers - ports not inited - forcing %d layers\n", ports_ok);
                }
        }
        
        clen = CODEC_LONG_NAME_LEN + 4;
        cmd  = (char*)xmalloc(clen);
        sprintf(cmd, "%s/%d", lcf->long_name, layerenc);
 
        xmemchk();
        if (channel_encoder_set_parameters(sp->channel_coder, cmd) == 0) {
                debug_msg("Layered command failed: %s\n", cmd);
        }
        xmemchk();
        xfree(cmd);
        /* Now tweak session parameters */
        sp->layers = layerenc;
        sp->num_encodings = 1;
}

/* This function is a bit nasty because it has to coerce what the
 * mbus gives us into something the channel coders understand.  In addition,
 * we assume we know what channel coders are which kind of defies point
 * 'generic' structure but that's probably because it's not generic enough.
 */
static void rx_audio_channel_coding(char *srce, char *args, session_t *sp)
{
        const cc_details_t *ccd;
        char        *coding, *sec_enc, *schan, *sfreq;
        int          offset, layerenc;
        uint32_t      i, n;
        uint16_t      upp;
	struct mbus_parser	*mp;

	UNUSED(srce);

        mp = mbus_parse_init(args);
        if (mbus_parse_str(mp, &coding)) {
                mbus_decode_str(coding);
                upp = channel_encoder_get_units_per_packet(sp->channel_coder);
                n = channel_get_coder_count();
                for(i = 0; i < n; i++) {
                        ccd = channel_get_coder_details(i);
                        if (strncasecmp(ccd->name, coding, 3) == 0) {
                                debug_msg("rx_audio_channel_coding: 0x%08x, %s\n", ccd->descriptor, &ccd->name);
                                switch(tolower(ccd->name[0])) {
                                case 'n':   /* No channel coding */
                                        sp->num_encodings = 1;
                                        sp->layers = 1;
                                        channel_encoder_destroy(&sp->channel_coder);
                                        channel_encoder_create(ccd->descriptor, &sp->channel_coder);
                                        channel_encoder_set_units_per_packet(sp->channel_coder, upp);
                                        break;
                                case 'r':   /* Redundancy -> extra parameters */
                                        if (mbus_parse_str(mp, &sec_enc) &&
                                                mbus_parse_int(mp, &offset)) {
                                                mbus_decode_str(sec_enc);
                                                sp->layers = 1;
                                                channel_encoder_destroy(&sp->channel_coder);
                                                channel_encoder_create(ccd->descriptor, &sp->channel_coder);
                                                channel_encoder_set_units_per_packet(sp->channel_coder, upp);
                                                set_red_parameters(sp, sec_enc, offset);
                                        }
                                        break;
                                case 'l':       /*Layering */
                                        if (mbus_parse_str(mp, &sec_enc) &&
                                                mbus_parse_str(mp, &schan) &&
                                                mbus_parse_str(mp, &sfreq) &&
                                                mbus_parse_int(mp, &layerenc)) {
                                                mbus_decode_str(sec_enc);
                                                mbus_decode_str(schan);
                                                mbus_decode_str(sfreq);
                                                channel_encoder_destroy(&sp->channel_coder);
                                                channel_encoder_create(ccd->descriptor, &sp->channel_coder);
                                                channel_encoder_set_units_per_packet(sp->channel_coder, upp);
                                                set_layered_parameters(sp, sec_enc, schan, sfreq, layerenc);
                                        }
                                        break;
                                }
                                break;
                        }
                }
        }
        mbus_parse_done(mp);
#ifdef DEBUG
        ccd = channel_get_coder_identity(sp->channel_coder);
        debug_msg("***** %s\n", ccd->name);
#endif /* DEBUG */
	ui_update_channel(sp);
}

static void rx_tool_rat_settings(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	UNUSED(srce);
        ui_update(sp);
}

static void rx_mbus_quit(char *srce, char *args, session_t *sp)
{
	/* mbus.quit() means that we should quit */
	UNUSED(args);
	UNUSED(srce);
	UNUSED(sp);
        should_exit = TRUE;
	debug_msg("Media engine got mbus.quit()\n");
}

static void rx_mbus_bye(char *srce, char *args, session_t *sp)
{
	/* mbus.bye() means that the sender of the message is about to quit */
	UNUSED(args);
	if (strstr(srce, "media:video") != NULL) {
		sp->sync_on = FALSE;
	}
}

static void rx_mbus_waiting(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(sp);
}

static void rx_mbus_go(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(sp);
}

static void rx_mbus_hello(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	if (strstr(srce, "media:video") != NULL) {
		sp->sync_on = TRUE;
	}
}

static const mbus_cmd_tuple engine_cmds[] = {
        { "session.title",                         rx_session_title },
        { "tool.rat.addr.ui",                      rx_tool_rat_addr_ui },
        { "tool.rat.silence",                      rx_tool_rat_silence },
        { "tool.rat.lecture",                      rx_tool_rat_lecture },
        { "tool.rat.3d.enabled",                   rx_tool_rat_3d_enable },
        { "tool.rat.3d.user.settings",             rx_tool_rat_3d_user_settings },
        { "tool.rat.3d.user.settings.request",     rx_tool_rat_3d_user_settings_req },
        { "tool.rat.agc",                          rx_tool_rat_agc },
        { "tool.rat.loopback",                     rx_tool_rat_audio_loopback },
        { "tool.rat.echo.suppress",                rx_tool_rat_echo_suppress },
        { "tool.rat.rate",                         rx_tool_rat_rate },
        { "tool.rat.powermeter",                   rx_tool_rat_powermeter },
        { "tool.rat.converter",                    rx_tool_rat_converter },
        { "tool.rat.settings",                     rx_tool_rat_settings },
        { "tool.rat.codec",                        rx_tool_rat_codec },
        { "tool.rat.playout.limit",                rx_tool_rat_playout_limit },
        { "tool.rat.playout.min",                  rx_tool_rat_playout_min },
        { "tool.rat.playout.max",                  rx_tool_rat_playout_max },
        { "tool.rat.payload.set",                  rx_tool_rat_payload_set },
        { "audio.input.mute",                      rx_audio_input_mute },
        { "audio.input.gain",                      rx_audio_input_gain },
        { "audio.input.port",                      rx_audio_input_port },
        { "audio.output.mute",                     rx_audio_output_mute },
        { "audio.output.gain",                     rx_audio_output_gain },
        { "audio.output.port",                     rx_audio_output_port },
        { "audio.channel.coding",                  rx_audio_channel_coding },
        { "audio.channel.repair",                  rx_audio_channel_repair },
        { "audio.file.play.open",                  rx_audio_file_play_open },
        { "audio.file.play.pause",                 rx_audio_file_play_pause },
        { "audio.file.play.stop",                  rx_audio_file_play_stop },
        { "audio.file.play.live",                  rx_audio_file_play_live },
        { "audio.file.record.open",                rx_audio_file_rec_open },
        { "audio.file.record.pause",               rx_audio_file_rec_pause },
        { "audio.file.record.stop",                rx_audio_file_rec_stop },
        { "audio.file.record.live",                rx_audio_file_rec_live },
        { "audio.device",                          rx_audio_device },
        { "security.encryption.key",               rx_security_encryption_key },
        { "rtp.addr",                              rx_rtp_addr },
        { "rtp.source.name",                       rx_rtp_source_name },
        { "rtp.source.email",                      rx_rtp_source_email },
        { "rtp.source.phone",                      rx_rtp_source_phone },
        { "rtp.source.loc",                        rx_rtp_source_loc },
        { "rtp.source.note",                       rx_rtp_source_note },
        { "rtp.source.mute",                       rx_rtp_source_mute },
        { "rtp.source.gain",                       rx_rtp_source_gain },
        { "mbus.quit",                             rx_mbus_quit },
	{ "mbus.bye",                              rx_mbus_bye },
        { "mbus.waiting",                          rx_mbus_waiting },
        { "mbus.go",                               rx_mbus_go },
        { "mbus.hello",                            rx_mbus_hello },
};

#define NUM_ENGINE_CMDS sizeof(engine_cmds)/sizeof(engine_cmds[0])

void mbus_engine_rx(char *srce, char *cmnd, char *args, void *data)
{
        uint32_t i;

        for (i = 0; i < NUM_ENGINE_CMDS; i++) {
		if (strcmp(engine_cmds[i].rxname, cmnd) == 0) {
                        engine_cmds[i].rxproc(srce, args, (session_t *) data);
			return;
		} 
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
#ifndef NDEBUG
	abort();
#endif
}

