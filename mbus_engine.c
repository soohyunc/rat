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
#include <ctype.h>
#include "rat_types.h"
#include "mbus_engine.h"
#include "mbus.h"
#include "ui_control.h"
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

extern int should_exit;

static struct mbus *mbus_base = NULL;
static struct mbus *mbus_chan = NULL;

/*****************************************************************************/

static void rx_toggle_input_port(char *srce, char *args, session_struct *sp)
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

static void rx_toggle_output_port(char *srce, char *args, session_struct *sp)
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

static void rx_get_audio(char *srce, char *args, session_struct *sp)
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

static void rx_powermeter(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->meter = i;
		ui_input_level(0);
		ui_output_level(0);
	} else {
		printf("mbus: usage \"powermeter <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_silence(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->detect_silence = i;
	} else {
		printf("mbus: usage \"silence <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_lecture(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->lecture = i;
	} else {
		printf("mbus: usage \"lecture <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_sync(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->sync_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_agc(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->agc_on = i;
	} else {
		printf("mbus: usage \"agc <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_rate(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		set_units_per_packet(sp, i);
	} else {
		printf("mbus: usage \"rate <integer>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_input_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		if (i) {
			tx_stop(sp);
		} else {
			tx_start(sp);
		}
		ui_update_input_port(sp);
	} else {
		printf("mbus: usage \"input_mute <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_input_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->input_gain = i;
		audio_set_gain(sp->audio_fd, sp->input_gain);
                tx_igain_update(sp);
	} else {
		printf("mbus: usage \"input_gain <integer>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_input_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &s)) {
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
	mbus_parse_done(mbus_chan);
	ui_update_input_port(sp);
}

static void rx_output_mute(char *srce, char *args, session_struct *sp)
{
	int i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
        	sp->playing_audio = !i; 
		ui_update_output_port(sp);
	} else {
		printf("mbus: usage \"output_mute <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_output_gain(char *srce, char *args, session_struct *sp)
{
	int   i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->output_gain = i;
		audio_set_volume(sp->audio_fd, sp->output_gain);
	} else {
		printf("mbus: usage \"output_gain <integer>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_output_port(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &s)) {
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
	mbus_parse_done(mbus_chan);
	ui_update_output_port(sp);
}

static void rx_repair(char *srce, char *args, session_struct *sp)
{
	char	*s;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &s)) {
		s = mbus_decode_str(s);
		if (strcmp(s,              "None") == 0) sp->repair = REPAIR_NONE;
		if (strcmp(s, "Packet Repetition") == 0) sp->repair = REPAIR_REPEAT;
        	if (strcmp(s,  "Pattern Matching") == 0) sp->repair = REPAIR_PATTERN_MATCH;
	} else {
		printf("mbus: usage \"repair None|Repetition\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_update_key(char *srce, char *args, session_struct *sp)
{
	char	*key;

	UNUSED(sp);
	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &key)) {
		Set_Key(mbus_decode_str(key));
	} else {
		printf("mbus: usage \"update_key <key>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_play_stop(char *srce, char *args, session_struct *sp)
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

static void rx_play_file(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &file)) {
		file = mbus_decode_str(file);
		sp->in_file = fopen(file, "r");
	} else {
		printf("mbus: usage \"play_file <filename>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_rec_stop(char *srce, char *args, session_struct *sp)
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

static void rx_rec_file(char *srce, char *args, session_struct *sp)
{
	char	*file;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &file)) {
		file = mbus_decode_str(file);
		sp->out_file = fopen(file, "w");
	} else {
		printf("mbus: usage \"rec_file <filename>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_source_name(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(mbus_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_NAME,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_name <cname> <name>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_source_email(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(mbus_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_EMAIL,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_email <cname> <email>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_source_phone(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(mbus_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_PHONE,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_phone <cname> <phone>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_source_loc(char *srce, char *args, session_struct *sp)
{
	char	*arg, *cname;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && (strcmp(mbus_decode_str(cname), sp->db->my_dbe->sentry->cname) == 0) && mbus_parse_str(mbus_chan, &arg)) {
		rtcp_set_attribute(sp, RTCP_SDES_LOC,  mbus_decode_str(arg));
	} else {
		printf("mbus: usage \"source_loc <cname> <loc>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_source_mute(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int		 i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && mbus_parse_int(mbus_chan, &i)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) {	
				e->mute = i;
			}
		}
	} else {
		printf("mbus: usage \"source_mute <cname> <bool>\"\n");
	}
	mbus_parse_done(mbus_chan);
}


static void rx_source_playout(char *srce, char *args, session_struct *sp)
{
	rtcp_dbentry	*e;
	char		*cname;
	int	 	 playout;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &cname) && mbus_parse_int(mbus_chan, &playout)) {
		for (e = sp->db->ssrc_db; e != NULL; e = e->next) {
			if (strcmp(e->sentry->cname, mbus_decode_str(cname)) == 0) break;
		}
                e->video_playout_received = TRUE;
		e->video_playout = (playout * get_freq(e->clock)) / 1000;
	} else {
		printf("mbus: usage \"source_playout <cname> <playout>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void
rx_interleaving(char *srce, char *args, session_struct *sp)
{
        int separation, cc_pt;
        codec_t *pcp;
        char config[80];

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &separation)) {
                cc_pt        = get_cc_pt(sp,"INTERLEAVER");
                pcp          = get_codec(sp->encodings[0]);
                sprintf(config, "%s/%d/%d", pcp->name, get_units_per_packet(sp), separation);
                config_channel_coder(sp, cc_pt, config);
        } else {
                printf("mbus: usage \"interleaving <codec> <separation in units>\"\n");
        }
        mbus_parse_done(mbus_chan);
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
rx_redundancy(char *srce, char *args, session_struct *sp)
{
	char	*codec;
        int      offset, cc_pt;
	char	 config[80];
	codec_t *rcp, *pcp;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &codec) && 
            mbus_parse_int(mbus_chan, &offset)) {
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
	mbus_parse_done(mbus_chan);
        ui_update_redundancy(sp);
}

static void 
rx_primary(char *srce, char *args, session_struct *sp)
{
	char	*short_name, *sfreq, *schan;
        int      pt, freq, channels;
	codec_t *next_cp, *cp;

	UNUSED(srce);

        pt = -1;
        next_cp = NULL;

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &short_name) &&
            mbus_parse_str(mbus_chan, &schan) &&
            mbus_parse_str(mbus_chan, &sfreq)) {
                mbus_decode_str(short_name);
                mbus_decode_str(schan);
                mbus_decode_str(sfreq);

                if (strcasecmp(schan, "mono") == 0) {
                        channels = 1;
                } else if (strcasecmp(schan, "stereo") == 0) {
                        channels = 2;
                } else {
                        channels = 0;
                }
                freq = atoi(sfreq) * 1000;
		if (-1 != (pt = codec_matching(short_name, freq, channels))) {
                        next_cp = get_codec(pt);
                        cp      = get_codec(sp->encodings[0]);
                        assert(next_cp != NULL);
                        assert(cp      != NULL);
                        if (codec_compatible(next_cp, cp)) {
                                sp->encodings[0] = pt;
                        } else {
                                /* reconfigure device */
                        }
                }
        } else {
		printf("mbus: usage \"primary <codec> <freq> <channels>\"\n");
        }
        ui_update_frequency(sp);
        ui_update_channels(sp);
        ui_update_primary(sp);
        ui_update_redundancy(sp);
	mbus_parse_done(mbus_chan);
}

static void 
rx_sampling(char *srce, char *args, session_struct *sp)
{
        int channels, freq, pt;
        char *sfreq, *schan;

        UNUSED(srce);
        UNUSED(sp);

        freq = channels = 0;
        mbus_parse_init(mbus_chan, args);
        if (mbus_parse_str(mbus_chan, &sfreq) && 
            mbus_parse_str(mbus_chan, &schan)) {
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
                ui_codecs(pt);
        } else {
                printf("mbus: usage \"sampling <freq> <channels>\"\n");
        }
        
        mbus_parse_done(mbus_chan);
}

static void rx_min_playout(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->min_playout = i;
	} else {
		printf("mbus: usage \"min_playout <integer>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_max_playout(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
		sp->max_playout = i;
	} else {
		printf("mbus: usage \"max_playout <integer>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_auto_convert(char *srce, char *args, session_struct *sp)
{
	int	 i;

	UNUSED(srce);

	mbus_parse_init(mbus_chan, args);
	if (mbus_parse_int(mbus_chan, &i)) {
                assert(i==0||i==1);
		sp->auto_convert = i;
	} else {
		printf("mbus: usage \"auto_convert <boolean>\"\n");
	}
	mbus_parse_done(mbus_chan);
}

static void rx_channel_code(char *srce, char *args, session_struct *sp)
{
        char *channel;

	UNUSED(srce);

        mbus_parse_init(mbus_chan, args);
	if (mbus_parse_str(mbus_chan, &channel)) {
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
        mbus_parse_done(mbus_chan);
}

static void rx_settings(char *srce, char *args, session_struct *sp)
{
	UNUSED(args);
	UNUSED(srce);
        ui_update(sp);
}

static void rx_quit(char *srce, char *args, session_struct *sp)
{
	UNUSED(args);
	UNUSED(srce);
	UNUSED(sp);
	ui_quit();
        should_exit = TRUE;
}

const char *rx_cmnd[] = {
	"get_audio",
	"toggle.input.port",
	"toggle.output.port",
	"powermeter",
	"silence",
	"lecture",
	"agc",
	"sync",
	"rate",
	"input.mute",
	"input.gain",
	"input.port",
	"output.mute",
	"output.gain",
	"output.port",
	"repair",
	"update.key",
	"play.stop",
	"play.file",
	"rec.stop",
	"rec.file",
	"source.name",
	"source.email",
	"source.phone",
	"source.loc",
	"source.mute",
	"source.playout",
        "interleaving",
	"redundancy",
	"primary",
        "sampling",
        "min.playout",
        "max.playout",
        "auto.convert",
        "channel.code",
        "settings",
	"quit",
	""
};

static void (*rx_func[])(char *srce, char *args, session_struct *sp) = {
	rx_get_audio,
	rx_toggle_input_port,
	rx_toggle_output_port,
	rx_powermeter,
	rx_silence,
	rx_lecture,
	rx_agc,
	rx_sync,
	rx_rate,
	rx_input_mute,
	rx_input_gain,
	rx_input_port,
	rx_output_mute,
	rx_output_gain,
	rx_output_port,
	rx_repair,
	rx_update_key,
	rx_play_stop,
	rx_play_file,
	rx_rec_stop,
	rx_rec_file,
	rx_source_name,
	rx_source_email,
	rx_source_phone,
	rx_source_loc,
	rx_source_mute,
	rx_source_playout,
        rx_interleaving,
	rx_redundancy,
	rx_primary,
        rx_sampling,
        rx_min_playout,
        rx_max_playout,
        rx_auto_convert,
        rx_channel_code,
        rx_settings,
	rx_quit
};

void mbus_engine_rx(char *srce, char *cmnd, char *args, void *data)
{
	int i;

	for (i=0; strlen(rx_cmnd[i]) != 0; i++) {
		if (strcmp(rx_cmnd[i], cmnd) == 0) {
			rx_func[i](srce, args, (session_struct *) data);
			return;
		}
	}
	dprintf("Unknown mbus command: %s %s\n", cmnd, args);
}

void mbus_engine_tx(int channel, char *dest, char *cmnd, char *args, int reliable)
{
	if (channel == 0) {
		mbus_send(mbus_base, dest, cmnd, args, reliable);
	} else {
		mbus_send(mbus_chan, dest, cmnd, args, reliable);
	}
}

void mbus_engine_tx_queue(int channel, char *cmnd, char *args)
{
	if (channel == 0) {
		mbus_qmsg(mbus_base, cmnd, args);
	} else {
		mbus_qmsg(mbus_chan, cmnd, args);
	}
}

void mbus_engine_init(char *name_engine, int channel)
{
	mbus_base = mbus_init(0, mbus_engine_rx, NULL); mbus_addr(mbus_base, name_engine);
	if (channel == 0) {
		mbus_chan = mbus_base;
	} else {
		mbus_chan = mbus_init(channel, mbus_engine_rx, NULL); mbus_addr(mbus_chan, name_engine);
	}
}

int  mbus_engine_fd(int channel)
{
	if (channel == 0) {
		return mbus_fd(mbus_base);
	} else {
		return mbus_fd(mbus_chan);
	}
}

struct mbus *mbus_engine(int channel)
{
	if (channel == 0) {
		return mbus_base;
	} else {
		return mbus_chan;
	}
}

void mbus_engine_retransmit(void)
{
	mbus_retransmit(mbus_base);
	mbus_retransmit(mbus_chan);
}

