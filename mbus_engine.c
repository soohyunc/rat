/*
 * FILE:    mbus_engine.c
 * AUTHORS: Colin Perkins
 * MODIFICATIONS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
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
#include "assert.h"
#include "debug.h"
#include "mbus_engine.h"
#include "mbus_ui.h"
#include "mbus.h"
#include "ui.h"
#include "net_udp.h"
#include "net.h"
#include "transmit.h"
#include "audio.h"
#include "codec.h"
#include "channel.h"
#include "receive.h"
#include "convert.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "crypt.h"
#include "session.h"
#include "sndfile.h"
#include "timers.h"
#include "util.h"

extern int should_exit;

static void rx_tool_rat_toggle_input_port(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: tool.rat.toggle.input.port does not require parameters\n");
		return;
	}

	audio_next_iport(sp->audio_device);
	sp->input_mode = audio_get_iport(sp->audio_device);
	ui_update_input_port(sp);
}

static void rx_tool_rat_toggle_output_port(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: tool.rat.toggle.output.port does not require parameters\n");
		return;
	}

	audio_next_oport(sp->audio_device);
	sp->output_mode = audio_get_oport(sp->audio_device);
	ui_update_output_port(sp);
}

static void rx_tool_rat_get_audio(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: get_audio does not require parameters\n");
		return;
	}

	if (sp->have_device) {
		/* We already have the device! */
		return;
	}

	if (audio_device_take(sp) == FALSE) {
		/* Request device using the mbus... */
	}
}

static void rx_tool_rat_powermeter(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->meter = i;
		ui_input_level(sp, 0);
		ui_output_level(sp, 0);
	} else {
		printf("mbus: usage \"tool.rat.powermeter <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_silence(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->detect_silence = i;
	} else {
		printf("mbus: usage \"tool.rat.silence <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_externalise(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->externalise_audio = i;
	} else {
		printf("mbus: usage \"tool.rat.externalise <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_lecture(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->lecture = i;
	} else {
		printf("mbus: usage \"tool.rat.lecture <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_sync(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		if (sp->mbus_channel != 0) {
			sp->sync_on = i;
		} else {
			ui_update_sync(sp, FALSE);
		}
	} else {
		printf("mbus: usage \"tool.rat.sync <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_agc(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->agc_on = i;
	} else {
		printf("mbus: usage \"tool.rat.agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_audio_loopback(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
                if (i) {
                        audio_loopback(sp->audio_device, 100);
                } else {
                        audio_loopback(sp->audio_device, 0);
                }
	} else {
		printf("mbus: usage \"tool.rat.audio.loopback <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_echo_suppress(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->echo_suppress = i;
                if (sp->echo_suppress) {
                        playout_buffers_destroy(sp, &sp->playout_buf_list);
                }
	} else {
		printf("mbus: usage \"tool.rat.echo.suppress <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_rate(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		collator_set_units(sp->collator, i);
	} else {
		printf("mbus: usage \"tool.rat.rate <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_input_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		if (i) {
			tx_stop(sp);
		} else {
			tx_start(sp);
		}
                sp->echo_was_sending = i;
		ui_update_input_port(sp);
	} else {
		printf("mbus: usage \"audio.input.mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_input_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->input_gain = i;
		if (sp->have_device) {
			audio_set_gain(sp->audio_device, sp->input_gain);
			tx_igain_update(sp);
		}
	} else {
		printf("mbus: usage \"audio.input.gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_input_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s, "microphone") == 0) {
			audio_set_iport(sp->audio_device, AUDIO_MICROPHONE);
		}
		if (strcmp(s, "cd") == 0) {
			audio_set_iport(sp->audio_device, AUDIO_CD);
		}
		if (strcmp(s, "line_in") == 0) {
			audio_set_iport(sp->audio_device, AUDIO_LINE_IN);
		}
	} else {
		printf("mbus: usage \"audio.input.port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
        sp->input_mode = audio_get_iport(sp->audio_device);
	ui_update_input_port(sp);
}

static void rx_audio_output_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
        	sp->playing_audio = !i; 
		ui_update_output_port(sp);
	} else {
		printf("mbus: usage \"audio.output.mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_output_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->output_gain = i;
		audio_set_volume(sp->audio_device, sp->output_gain);
	} else {
		printf("mbus: usage \"audio.output.gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_output_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s, "speaker") == 0) {
			audio_set_oport(sp->audio_device, AUDIO_SPEAKER);
		}
		if (strcmp(s, "headphone") == 0) {
			audio_set_oport(sp->audio_device, AUDIO_HEADPHONE);
		}
		if (strcmp(s, "line_out") == 0) {
			audio_set_oport(sp->audio_device, AUDIO_LINE_OUT);
		}
	} else {
		printf("mbus: usage \"audio.output.port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
        sp->output_mode = audio_get_oport(sp->audio_device);
	ui_update_output_port(sp);
}

static void rx_tool_rat_repair(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s,              "None") == 0) sp->repair = REPAIR_NONE;
		if (strcmp(s, "Packet Repetition") == 0) sp->repair = REPAIR_REPEAT;
        	if (strcmp(s,  "Pattern Matching") == 0) sp->repair = REPAIR_PATTERN_MATCH;
	} else {
		printf("mbus: usage \"tool.rat.repair None|Repetition\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_security_encryption_key(char *srce, char *args, session_struct *sp)
{
	char	*key;

	UNUSED(sp);
	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &key)) {
		Set_Key(mbus_decode_str(key));
	} else {
		printf("mbus: usage \"security.encryption.key <key>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
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

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &file)) {
                mbus_decode_str(file);
                if (sp->in_file) snd_read_close(&sp->in_file);
                if (snd_read_open(&sp->in_file, file)) {
                        debug_msg("Hooray opened %s\n",file);
                }
	} else {
		printf("mbus: usage \"audio.file.play.open <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);

        if (sp->in_file) ui_update_playback_file(sp, file);

}

static void rx_audio_file_play_pause(char *srce, char *args, session_struct *sp)
{
        int pause;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine_conf, args);

        if (mbus_parse_int(sp->mbus_engine_conf, &pause)) {
                if (sp->in_file) {
                        if (pause) {
                                snd_pause(sp->in_file);
                        } else {
                                snd_resume(sp->in_file);
                        }
                }
        } else {
                printf("mbus: usage \"audio.file.play.pause <bool>\"\n");        
        }
        mbus_parse_done(sp->mbus_engine_conf);
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

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &file)) {
                mbus_decode_str(file);
                if (sp->out_file) snd_write_close(&sp->out_file);
                if (snd_write_open(&sp->out_file, file, (u_int16)get_freq(sp->device_clock), (u_int16)audio_get_channels(sp->audio_device))) {
                        debug_msg("Hooray opened %s\n",file);
                }
	} else {
		printf("mbus: usage \"audio.file.record.open <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
        
        if (sp->out_file) ui_update_record_file(sp, file);
}

static void rx_audio_file_rec_pause(char *srce, char *args, session_struct *sp)
{
        int pause;

        UNUSED(srce);

        mbus_parse_init(sp->mbus_engine_conf, args);

        if (mbus_parse_int(sp->mbus_engine_conf, &pause)) {
                if (sp->out_file) {
                        if (pause) {
                                snd_pause(sp->out_file);
                        } else {
                                snd_resume(sp->out_file);
                        }
                }
        } else {
                printf("mbus: usage \"audio.file.record.pause <bool>\"\n");        
        }
        mbus_parse_done(sp->mbus_engine_conf);
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
        char	*s, dev_name[32];
        int next_device;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &s)) {
		s = mbus_decode_str(s);
                purge_chars(s, "[]()");
                next_device = -1;
                if (s) {
                        int i, n;
                        n = audio_get_number_of_interfaces();
                        for(i = 0; i < n; i++) {
                                /* Brackets are a problem so purge them */
                                strncpy(dev_name, audio_get_interface_name(i), 31);
                                purge_chars(dev_name, "[]()");
                                if (!strcmp(s, dev_name)) {
                                        next_device = i;
                                        break;
                                }
                        }
                }
                        
		debug_msg("Next audio device: %s (%d).\n", s, next_device);
                /* We should close audio device, transmitter, etc
                 * Then find corresponding audio interface, select it,
                 * and re-open the device.
                 */
	} else {
		printf("mbus: usage \"audio.device <string>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
        
        sp->next_selected_device = next_device;
}

static void rx_rtp_source_name(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_conf, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_NAME,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"rtp_source_name <cname> <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_rtp_source_email(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_conf, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_EMAIL,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"rtp_source_email <cname> <email>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_rtp_source_phone(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_conf, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_PHONE,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"rtp_source_phone <cname> <phone>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_rtp_source_loc(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_conf, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_LOC,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"rtp_source_loc <cname> <loc>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_rtp_source_mute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int		 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && mbus_parse_int(sp->mbus_engine_conf, &i)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) {	
				e->mute = i;
			}
		}
	} else {
		printf("mbus: usage \"rtp_source_mute <cname> <bool>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}


static void rx_rtp_source_playout(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int	 	 playout;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &cname) && mbus_parse_int(sp->mbus_engine_conf, &playout)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
                e->video_playout_received = TRUE;
		e->video_playout = playout;
	} else {
		printf("mbus: usage \"rtp_source_playout <cname> <playout>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void
rx_tool_rat_interleaving(char *srce, char *args, session_struct *sp)
{
        int units, separation, cc_pt;
        char config[80];

	UNUSED(srce);
        
	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &units) &&
            mbus_parse_int(sp->mbus_engine_conf, &separation)) {
                cc_pt        = get_cc_pt(sp,"INTERLEAVER");
                sprintf(config, "%d/%d", units, separation);
                debug_msg("config %s\n", config);
                config_channel_coder(sp, cc_pt, config);
        } else {
                printf("mbus: usage \"tool.rat.interleaving <codec> <separation in units>\"\n");
        }
        mbus_parse_done(sp->mbus_engine_conf);
        ui_update_interleaving(sp);
}

static codec_t*
validate_redundant_codec(codec_t *primary, codec_t *redundant) 
{
        assert(primary != NULL);
        
        if ((redundant == NULL) ||                       /* passed junk */
            (!codec_compatible(primary, redundant)) ||   /* passed incompatible codec */
            (redundant->unit_len > primary->unit_len)) { /* passed higher bandwidth codec */
                return primary;
        }
        return redundant;
}

static void 
rx_tool_rat_redundancy(char *srce, char *args, session_struct *sp)
{
	char	*codec;
        int      offset, cc_pt, rpt;
	char	 config[80];
	codec_t *rcp, *pcp;

	UNUSED(srce);

        mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &codec) && 
            mbus_parse_int(sp->mbus_engine_conf, &offset)) {
                if (offset<=0) offset = 0;;
                pcp = get_codec_by_pt(sp->encodings[0]);
		rpt = codec_matching(mbus_decode_str(codec), pcp->freq, pcp->channels);
		if (rpt != -1) {
			rcp = get_codec_by_pt(rpt);
		} else {
			/* Specified secondary codec doesn't exist. Make it the same */
			/* as the primary, and hope that's a sensible choice.        */
			rcp = pcp;
		}
                assert(rcp != NULL);
                /* Check redundancy makes sense... */
                rcp = validate_redundant_codec(pcp,rcp);
                sprintf(config,"%s/0/%s/%d", pcp->name, rcp->name, offset);
                debug_msg("Configuring redundancy %s\n", config);
                cc_pt = get_cc_pt(sp,"REDUNDANCY");
                config_channel_coder(sp, cc_pt, config);
        } else {
                printf("mbus: usage \"tool.rat.redundancy <codec> <offset in units>\"\n");
        }                
	mbus_parse_done(sp->mbus_engine_conf);
        ui_update_redundancy(sp);
}

static void 
rx_tool_rat_codec(char *srce, char *args, session_struct *sp)
{
        static int virgin = 1;
	char	*short_name, *sfreq, *schan;
        int      pt, freq, channels;
	codec_t *next_cp, *cp;

	UNUSED(srce);

        pt = -1;
        next_cp = NULL;

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &short_name) &&
            mbus_parse_str(sp->mbus_engine_conf, &schan) &&
            mbus_parse_str(sp->mbus_engine_conf, &sfreq)) {
                mbus_decode_str(short_name);
                mbus_decode_str(schan);
                mbus_decode_str(sfreq);
                mbus_parse_done(sp->mbus_engine_conf);
        } else {
		printf("mbus: usage \"audio.codec <codec> <freq> <channels>\"\n");
                mbus_parse_done(sp->mbus_engine_conf);
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

        if (-1 != (pt = codec_matching(short_name, freq, channels))) {
                next_cp = get_codec_by_pt(pt);
                cp      = get_codec_by_pt(sp->encodings[0]);
                assert(next_cp != NULL);
                assert(cp      != NULL);
                if (codec_compatible(next_cp, cp)) {
                        sp->encodings[0] = pt;
                        ui_update_primary(sp);
                        ui_update_redundancy(sp);
                } else {
                        /* just register we want to make a change */
                        sp->next_encoding = pt;
                        if (virgin) {
                                audio_device_reconfigure(sp);
                                virgin = 0;
                        }
                }
        }
}

static void 
rx_tool_rat_sampling(char *srce, char *args, session_struct *sp)
{
        int channels, freq, pt;
        char *sfreq, *schan;

        UNUSED(srce);
        UNUSED(sp);

        freq = channels = 0;
        mbus_parse_init(sp->mbus_engine_conf, args);
        if (mbus_parse_str(sp->mbus_engine_conf, &sfreq) && 
            mbus_parse_str(sp->mbus_engine_conf, &schan)) {
                mbus_decode_str(sfreq);
                mbus_decode_str(schan);
                if (strcasecmp(schan, "mono") == 0) {
                        channels = 1;
                } else if (strcasecmp(schan, "stereo") == 0) {
                        channels = 2;
                } 
                freq = atoi(sfreq) * 1000;
        }

        pt = codec_first_with(freq, channels);
        if (pt != -1) {
                ui_codecs(sp, pt);
        } else {
                printf("mbus: usage \"tool.rat.sampling <freq> <channels>\"\n");
        }
        
        mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_playout_limit(char *srce, char *args, session_struct *sp)
{
        int i;

        UNUSED(srce);
        mbus_parse_init(sp->mbus_engine_conf, args);
        if (mbus_parse_int(sp->mbus_engine_conf, &i) && (1 == i || 0 == i)) {
                sp->limit_playout = i;
        } else {
		printf("mbus: usage \"tool.rat.playout.limit <bool>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_playout_min(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->min_playout = i;
	} else {
		printf("mbus: usage \"tool.rat.playout.min <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_playout_max(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_int(sp->mbus_engine_conf, &i)) {
		sp->max_playout = i;
	} else {
		printf("mbus: usage \"tool.rat.playout.max <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_tool_rat_converter(char *srce, char *args, session_struct *sp)
{
        char *name;
        
	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &name)) {
                mbus_decode_str(name);
                sp->converter = converter_get_byname(name);
                assert(sp->converter);
	} else {
		printf("mbus: usage \"tool.rat.converter <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_conf);
}

static void rx_audio_channel_coding(char *srce, char *args, session_struct *sp)
{
        char *channel;

	UNUSED(srce);

        mbus_parse_init(sp->mbus_engine_conf, args);
	if (mbus_parse_str(sp->mbus_engine_conf, &channel)) {
                channel = mbus_decode_str(channel);
                switch(channel[0]) {
                case 'N':
                        channel_set_coder(sp, get_cc_pt(sp, "VANILLA"));
                        break;
                case 'R':
                        channel_set_coder(sp, get_cc_pt(sp, "REDUNDANCY"));
                        break;
                case 'I':
                        channel_set_coder(sp, get_cc_pt(sp, "INTERLEAVER"));
                        break;
                default:
                        printf("%s %d: scheme %s not recognized.\n",__FILE__,__LINE__,channel);
                }
        } else {
                printf("mbus: usage \"audio.channel.coding <scheme>\"\n");
        }
        mbus_parse_done(sp->mbus_engine_conf);
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
	"tool.rat.get_audio",              
	"tool.rat.toggle.input.port",
	"tool.rat.toggle.output.port",
	"tool.rat.silence",
	"tool.rat.lecture",
	"tool.rat.externalise",
	"tool.rat.agc",
        "tool.rat.loopback",
        "tool.rat.echo.suppress",
	"tool.rat.sync",       
	"tool.rat.rate",          
	"tool.rat.powermeter",
        "tool.rat.converter",
        "tool.rat.settings",
	"tool.rat.repair",
        "tool.rat.interleaving",
	"tool.rat.redundancy",
	"tool.rat.codec",
        "tool.rat.sampling",
        "tool.rat.playout.limit",
        "tool.rat.playout.min",            
        "tool.rat.playout.max",            
	"audio.input.mute",
	"audio.input.gain",
	"audio.input.port",
	"audio.output.mute",
	"audio.output.gain",
	"audio.output.port",
        "audio.channel.coding",
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
	rx_tool_rat_get_audio,                  
	rx_tool_rat_toggle_input_port,
	rx_tool_rat_toggle_output_port,
	rx_tool_rat_silence,
	rx_tool_rat_lecture,
	rx_tool_rat_externalise,
	rx_tool_rat_agc,
        rx_tool_rat_audio_loopback,
        rx_tool_rat_echo_suppress,
	rx_tool_rat_sync,
	rx_tool_rat_rate,              
	rx_tool_rat_powermeter,
        rx_tool_rat_converter,
        rx_tool_rat_settings,
	rx_tool_rat_repair,
        rx_tool_rat_interleaving,
	rx_tool_rat_redundancy,
	rx_tool_rat_codec,
        rx_tool_rat_sampling,
        rx_tool_rat_playout_limit,
        rx_tool_rat_playout_min,
        rx_tool_rat_playout_max,                
	rx_audio_input_mute,
	rx_audio_input_gain,
	rx_audio_input_port,
	rx_audio_output_mute,
	rx_audio_output_gain,
	rx_audio_output_port,
        rx_audio_channel_coding,
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

