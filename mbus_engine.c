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

#include <stdio.h>
#include "mbus_engine.h"
#include "mbus.h"
#include "ui.h"
#include "net.h"
#include "util.h"
#include "transmit.h"
#include "audio.h"
#include "codec.h"
#include "channel.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "repair.h"
#include "crypt.h"
#include "session.h"
#include "rat_time.h"

static void func_toggle_input_port(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: toggle_input_port does not require parameters\n");
		return;
	}

	audio_next_iport(sp->audio_fd);
	sp->input_mode = audio_get_iport(sp->audio_fd);
	ui_update_input_port(sp);
}

static void func_toggle_output_port(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: toggle_output_port does not require parameters\n");
		return;
	}

	audio_next_oport(sp->audio_fd);
	sp->output_mode = audio_get_oport(sp->audio_fd);
	ui_update_output_port(sp);
}

static void func_get_audio(char *srce, char *args, session_struct *sp)
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

static void func_powermeter(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->meter = i;
		ui_input_level(0, sp);
		ui_output_level(0, sp);
	} else {
		printf("mbus: usage \"powermeter <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_silence(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->detect_silence = i;
	} else {
		printf("mbus: usage \"silence <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_lecture(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->lecture = i;
	} else {
		printf("mbus: usage \"lecture <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_sync(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->sync_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_agc(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->agc_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_rate(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		set_units_per_packet(sp, i);
	} else {
		printf("mbus: usage \"rate <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_input_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		if (i) {
			stop_sending(sp);
		} else {
			start_sending(sp);
		}
		ui_update_input_port(sp);
	} else {
		printf("mbus: usage \"input_mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_input_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->input_gain = i;
		audio_set_gain(sp->audio_fd, sp->input_gain);
	} else {
		printf("mbus: usage \"input_gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_input_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s, "microphone") == 0) {
			audio_set_iport(sp->audio_fd, AUDIO_MICROPHONE);
		}
		if (strcmp(s, "cd") == 0) {
			audio_set_iport(sp->audio_fd, AUDIO_CD);
		}
		if (strcmp(s, "line_in") == 0) {
			audio_set_iport(sp->audio_fd, AUDIO_LINE_IN);
		}
	} else {
		printf("mbus: usage \"input_port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
	ui_update_input_port(sp);
}

static void func_output_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
        	sp->playing_audio = !i; 
		ui_update_output_port(sp);
	} else {
		printf("mbus: usage \"output_mute <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_output_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->output_gain = i;
		audio_set_volume(sp->audio_fd, sp->output_gain);
	} else {
		printf("mbus: usage \"output_gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_output_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s, "speaker") == 0) {
			audio_set_oport(sp->audio_fd, AUDIO_SPEAKER);
		}
		if (strcmp(s, "headphone") == 0) {
			audio_set_oport(sp->audio_fd, AUDIO_HEADPHONE);
		}
		if (strcmp(s, "line_out") == 0) {
			audio_set_oport(sp->audio_fd, AUDIO_LINE_OUT);
		}
	} else {
		printf("mbus: usage \"output_port <port>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
	ui_update_output_port(sp);
}

static void func_output_mode(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &s)) {
		s = mbus_decode_str(s);
		switch(s[0]) {
		case 'N': sp->voice_switching = NET_MUTES_MIKE;
			  break;
		case 'M': sp->voice_switching = MIKE_MUTES_NET;
			  break;
		case 'F': sp->voice_switching = FULL_DUPLEX;
			  break;
		}
	} else {
		printf("mbus: usage \"output_mode <N|M|F>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_repair(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s,              "None") == 0) sp->repair = REPAIR_NONE;
		if (strcmp(s, "Packet Repetition") == 0) sp->repair = REPAIR_REPEAT;
        	if (strcmp(s,  "Pattern Matching") == 0) sp->repair = REPAIR_PATTERN_MATCH;
	} else {
		printf("mbus: usage \"repair None|Repetition\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_update_key(char *srce, char *args, session_struct *sp)
{
	char	*key;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &key)) {
		Set_Key(mbus_decode_str(key));
	} else {
		printf("mbus: usage \"update_key <key>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_play_stop(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: play-stop does not require parameters\n");
		return;
	}
	if (sp->in_file != NULL) {
		fclose(sp->in_file);
	}
	sp->in_file = NULL;
}

static void func_play_file(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &file)) {
		file = mbus_decode_str(file);
		sp->in_file = fopen(file, "r");
	} else {
		printf("mbus: usage \"play_file <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_rec_stop(char *srce, char *args, session_struct *sp)
{
	UNUSED(srce);

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: rec-stop does not require parameters\n");
		return;
	}
	if (sp->out_file != NULL) {
		fclose(sp->out_file);
	}
	sp->out_file = NULL;
}

static void func_rec_file(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &file)) {
		file = mbus_decode_str(file);
		sp->out_file = fopen(file, "w");
	} else {
		printf("mbus: usage \"rec_file <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_source_name(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_NAME,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_name <cname> <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_source_email(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_EMAIL,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_email <cname> <email>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_source_phone(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_PHONE,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_phone <cname> <phone>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_source_loc(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_LOC,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_loc <cname> <loc>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_source_mute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int		 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && mbus_parse_int(sp->mbus_engine_chan, &i)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) {	
				e->mute = i;
			}
		}
	} else {
		printf("mbus: usage \"source_mute <cname> <bool>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}


static void func_source_playout(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int	 	 playout;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &cname) && mbus_parse_int(sp->mbus_engine_chan, &playout)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
                e->video_playout_received = TRUE;
		e->video_playout = (playout * get_freq(e->clock)) / 1000;
	} else {
		printf("mbus: usage \"source_playout <cname> <playout>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void
func_interleaving(char *srce, char *args, session_struct *sp)
{
        int separation, cc_pt;
        codec_t *pcp;
        char config[80];

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &separation)) {
                cc_pt        = get_cc_pt(sp,"INTERLEAVER");
                pcp          = get_codec(sp->encodings[0]);
                sprintf(config, "%s/%d/%d", pcp->name, get_units_per_packet(sp), separation);
                config_channel_coder(sp, cc_pt, config);
        } else {
                printf("mbus: usage \"interleaving <codec> <separation in units>\"\n");
        }
        mbus_parse_done(sp->mbus_engine_chan);
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
func_redundancy(char *srce, char *args, session_struct *sp)
{
	char	*codec;
        int      offset, cc_pt;
	char	 config[80];
	codec_t *rcp, *pcp;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &codec) && 
            mbus_parse_int(sp->mbus_engine_chan, &offset)) {
                assert(offset>0);
                pcp    = get_codec(sp->encodings[0]);
		rcp    = get_codec_byname(mbus_decode_str(codec), sp);
                /* Check redundancy makes sense... */
                rcp    = validate_redundant_codec(pcp,rcp);
                offset = offset*get_units_per_packet(sp); /* units-to-packets */
                sprintf(config,"%s/0/%s/%d", pcp->name, rcp->name, offset);
                cc_pt = get_cc_pt(sp,"REDUNDANCY");
                config_channel_coder(sp, cc_pt, config);
        } else {
                printf("mbus: usage \"redundancy <codec> <offset in units>\"\n");
        }                
	mbus_parse_done(sp->mbus_engine_chan);
        ui_update_redundancy(sp);
}

static void func_primary(char *srce, char *args, session_struct *sp)
{
	char	*codec;
	char	 arg[80];
	codec_t *pcp;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &codec)) {
		pcp = get_codec_byname(mbus_decode_str(codec), sp);
                if (pcp != NULL) {
			sp->encodings[0] = pcp->pt;
                } else {
                        pcp = get_codec(sp->encodings[0]);
                        sprintf(arg, "%s", mbus_encode_str(pcp->name));
                        mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "primary", arg, FALSE);
                }
        } else {
		printf("mbus: usage \"primary <codec>\"\n");
	}
        ui_update_redundancy(sp);
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_min_playout(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->min_playout = i;
	} else {
		printf("mbus: usage \"min_playout <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_max_playout(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
		sp->max_playout = i;
	} else {
		printf("mbus: usage \"max_playout <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_auto_convert(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_int(sp->mbus_engine_chan, &i)) {
                assert(i==0||i==1);
		sp->auto_convert = i;
	} else {
		printf("mbus: usage \"auto_convert <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine_chan);
}

static void func_channel_code(char *srce, char *args, session_struct *sp)
{
        char *channel;

	UNUSED(srce);

        mbus_parse_init(sp->mbus_engine_chan, args);
	if (mbus_parse_str(sp->mbus_engine_chan, &channel)) {
                channel = mbus_decode_str(channel);
                switch(channel[0]) {
                case 'N':
                        sp->cc_encoding = get_cc_pt(sp,"VANILLA");
                        break;
                case 'R':
                        sp->cc_encoding = get_cc_pt(sp,"REDUNDANCY");
                        break;
                case 'I':
                        sp->cc_encoding = get_cc_pt(sp,"INTERLEAVER");
                        break;
                default:
                        printf("%s %d: scheme %s not recognized.\n",__FILE__,__LINE__,channel);
                }
        } else {
                printf("mbus: usage \"channel_code <scheme>\"\n");
        }
        mbus_parse_done(sp->mbus_engine_chan);
}

static void func_settings(char *srce, char *args, session_struct *sp)
{
	UNUSED(args);
	UNUSED(srce);
        ui_update(sp);
}

const char *mbus_cmnd[] = {
	"get_audio",
	"toggle_input_port",
	"toggle_output_port",
	"powermeter",
	"silence",
	"lecture",
	"agc",
	"sync",
	"rate",
	"input_mute",
	"input_gain",
	"input_port",
	"output_mute",
	"output_gain",
	"output_port",
	"output_mode",
	"repair",
	"update_key",
	"play_stop",
	"play_file",
	"rec_stop",
	"rec_file",
	"source_name",
	"source_email",
	"source_phone",
	"source_loc",
	"source_mute",
	"source_playout",
        "interleaving",
	"redundancy",
	"primary",
        "min_playout",
        "max_playout",
        "auto_convert",
        "channel_code",
        "settings",
	""
};

void (*mbus_func[])(char *srce, char *args, session_struct *sp) = {
	func_get_audio,
	func_toggle_input_port,
	func_toggle_output_port,
	func_powermeter,
	func_silence,
	func_lecture,
	func_agc,
	func_sync,
	func_rate,
	func_input_mute,
	func_input_gain,
	func_input_port,
	func_output_mute,
	func_output_gain,
	func_output_port,
	func_output_mode,
	func_repair,
	func_update_key,
	func_play_stop,
	func_play_file,
	func_rec_stop,
	func_rec_file,
	func_source_name,
	func_source_email,
	func_source_phone,
	func_source_loc,
	func_source_mute,
	func_source_playout,
        func_interleaving,
	func_redundancy,
	func_primary,
        func_min_playout,
        func_max_playout,
        func_auto_convert,
        func_channel_code,
        func_settings
};

void mbus_handler_engine(char *srce, char *cmnd, char *args, void *data)
{
	int i;

	dprintf("%s %s\n", cmnd, args);
	for (i=0; strlen(mbus_cmnd[i]) != 0; i++) {
		if (strcmp(mbus_cmnd[i], cmnd) == 0) {
			mbus_func[i](srce, args, (session_struct *) data);
			return;
		}
	}
#ifdef DEBUG_MBUS
	printf("Unknown mbus command: %s %s\n", cmnd, args);
#endif
}

