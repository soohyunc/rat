/*
 * FILE:    mbus_engine.c
 * AUTHORS: Colin Perkins
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


static void func_toggle_send(char *srce, char *args, session_struct *sp)
{
	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: toggle_send does not require parameters\n");
		return;
	}

        if (sp->sending_audio) {
		stop_sending(sp);
	} else {
		start_sending(sp);
	}
	ui_update_input_port(sp);
}

static void func_toggle_play(char *srce, char *args, session_struct *sp)
{
	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: toggle_play does not require parameters\n");
		return;
	}

	if (sp->playing_audio) {
        	sp->playing_audio = FALSE;
	} else {
        	sp->playing_audio = TRUE;
	}
        sp->receive_audit_required = TRUE;
	ui_update_output_port(sp);
}

static void func_get_audio(char *srce, char *args, session_struct *sp)
{
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

static void func_toggle_input_port(char *srce, char *args, session_struct *sp)
{
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
	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: toggle_output_port does not require parameters\n");
		return;
	}

	audio_next_oport(sp->audio_fd);
	sp->output_mode = audio_get_oport(sp->audio_fd);
	ui_update_output_port(sp);
}

static void func_powermeter(char *srce, char *args, session_struct *sp)
{
	int i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->meter = i;
		ui_input_level(0, sp);
		ui_output_level(0, sp);
	} else {
		printf("mbus: usage \"powermeter <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_silence(char *srce, char *args, session_struct *sp)
{
	int i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->detect_silence = i;
	} else {
		printf("mbus: usage \"silence <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_lecture(char *srce, char *args, session_struct *sp)
{
	int i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->lecture = i;
	} else {
		printf("mbus: usage \"lecture <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_sync(char *srce, char *args, session_struct *sp)
{
	int i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->sync_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_agc(char *srce, char *args, session_struct *sp)
{
	int i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->agc_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_rate(char *srce, char *args, session_struct *sp)
{
	int	 i;
	codec_t	*cp;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		cp = get_codec(sp->encodings[0]);
		set_units_per_packet(sp, (i * cp->freq) / (1000 * cp->unit_len));
	} else {
		printf("mbus: usage \"rate <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_input_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->input_gain = i;
		audio_set_gain(sp->audio_fd, sp->input_gain);
	} else {
		printf("mbus: usage \"input_gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_output_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_int(sp->mbus_engine, &i)) {
		sp->output_gain = i;
		audio_set_volume(sp->audio_fd, sp->output_gain);
	} else {
		printf("mbus: usage \"output_gain <integer>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_output_mode(char *srce, char *args, session_struct *sp)
{
	char	*s;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
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
	mbus_parse_done(sp->mbus_engine);
}

static void func_repair(char *srce, char *args, session_struct *sp)
{
	char	*s;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s,             "None") == 0) sp->repair = REPAIR_NONE;
		if (strcmp(s, "PacketRepetition") == 0) sp->repair = REPAIR_REPEAT;
        	if (strcmp(s,  "PatternMatching") == 0) sp->repair = REPAIR_PATTERN_MATCH;
	} else {
		printf("mbus: usage \"repair None|Repetition\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_update_key(char *srce, char *args, session_struct *sp)
{
	char	*key;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &key)) {
		Set_Key(mbus_decode_str(key));
	} else {
		printf("mbus: usage \"update_key <key>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_play_stop(char *srce, char *args, session_struct *sp)
{
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

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &file)) {
		file = mbus_decode_str(file);
		sp->in_file = fopen(file, "r");
	} else {
		printf("mbus: usage \"play_file <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_rec_stop(char *srce, char *args, session_struct *sp)
{
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

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &file)) {
		file = mbus_decode_str(file);
		sp->out_file = fopen(file, "w");
	} else {
		printf("mbus: usage \"rec_file <filename>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_name(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_NAME,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_name <cname> <name>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_email(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_EMAIL,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_email <cname> <email>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_phone(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_PHONE,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_phone <cname> <phone>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_loc(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(sp->mbus_engine, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_LOC,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_loc <cname> <loc>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_mute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
		e->mute = TRUE;
	} else {
		printf("mbus: usage \"source_mute <cname>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}


static void func_source_unmute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
		e->mute = FALSE;
	} else {
		printf("mbus: usage \"source_unmute <cname>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_source_playout(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int	 	 playout;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &cname) && mbus_parse_int(sp->mbus_engine, &playout)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
		e->video_playout = (playout * get_freq(e->clock)) / 1000;
	} else {
		printf("mbus: usage \"source_loc <cname> <loc>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_redundancy(char *srce, char *args, session_struct *sp)
{
	char	*codec;
	char	 arg[80];
	codec_t *cp, *pcp;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &codec)) {
		cp = get_codec_byname(mbus_decode_str(codec), sp);
		if (cp != NULL) {
			pcp = get_codec(sp->encodings[0]);
			if (pcp->value < cp->value) {
				/* Current primary scheme and requested redundancy */
				/* do not match so do not change redundancy scheme */
				cp = get_codec(sp->encodings[1]);
				if (sp->num_encodings > 1) {
					sprintf(arg, "%s", mbus_encode_str(cp->name));
				} else {
					sprintf(arg, "\"NONE\"");
				}
				mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "redundancy", arg, FALSE);
			} else {
				sp->encodings[1]  = cp->pt;
				sp->num_encodings = 2;
			}
			sprintf(arg, "%s", mbus_encode_str(cp->name));
			mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "redundancy", arg, FALSE);
		} else {
			sp->num_encodings = 1;
		}
	} else {
		printf("mbus: usage \"redundancy <codec>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_primary(char *srce, char *args, session_struct *sp)
{
	char	*codec;
	char	 arg[80];
	codec_t *cp, *scp;

	mbus_parse_init(sp->mbus_engine, args);
	if (mbus_parse_str(sp->mbus_engine, &codec)) {
		cp = get_codec_byname(mbus_decode_str(codec), sp);
		if (cp != NULL) {
			sp->encodings[0] = cp->pt;
			if (sp->num_encodings > 1) {
				scp = get_codec(sp->encodings[1]);
				if (cp->value < scp->value) {
					/* Current redundancy scheme and requested primary do not match so change redundancy scheme */
					sp->encodings[1] = sp->encodings[0];
					sprintf(arg, "%s", mbus_encode_str(cp->name));
					mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "redundancy", arg, FALSE);
				}
			}
			sprintf(arg, "%s", mbus_encode_str(cp->name));
			mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "primary", arg, FALSE);
		} else {
			sp->num_encodings = 1;
		}
	} else {
		printf("mbus: usage \"primary <codec>\"\n");
	}
	mbus_parse_done(sp->mbus_engine);
}

static void func_codec_query(char *srce, char *args, session_struct *sp)
{
	char	 arg[1000], *a;
	codec_t	*codec;
	int 	 i;

	if ((strlen(args) != 1) || (args[0] != ' ')) {
		printf("mbus: play-stop does not require parameters\n");
		return;
	}

	a = &arg[0];
	for (i=0; i<MAX_CODEC; i++) {
		codec = get_codec(i);
		if (codec != NULL) {
			sprintf(a, " %s", codec->name);
			a += strlen(codec->name) + 1;
		}
	}
	mbus_send(sp->mbus_engine, sp->mbus_ui_addr, "codec_supported", arg, TRUE);
}

char *mbus_cmnd[] = {
	"toggle_send",
	"toggle_play",
	"get_audio",
	"toggle_input_port",
	"toggle_output_port",
	"powermeter",
	"silence",
	"lecture",
	"agc",
	"sync",
	"rate",
	"input_gain",
	"output_gain",
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
	"source_unmute",
	"source_playout",
	"redundancy",
	"primary",
	"codec_query",
	""
};

void (*mbus_func[])(char *srce, char *args, session_struct *sp) = {
	func_toggle_send,
	func_toggle_play,
	func_get_audio,
	func_toggle_input_port,
	func_toggle_output_port,
	func_powermeter,
	func_silence,
	func_lecture,
	func_agc,
	func_sync,
	func_rate,
	func_input_gain,
	func_output_gain,
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
	func_source_unmute,
	func_source_playout,
	func_redundancy,
	func_primary,
	func_codec_query,
};

void mbus_handler_engine(char *srce, char *cmnd, char *args, void *data)
{
	int i;

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

