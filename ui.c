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
#include "assert.h"
#include "version.h"
#include "session.h"
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "repair.h"
#include "receive.h"
#include "codec.h"
#include "convert.h"
#include "audio.h"
#include "mbus.h"
#include "mbus_engine.h"
#include "channel.h"
#include "mix.h"
#include "transmit.h"
#include "ui.h"
#include "timers.h"

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
ui_info_update_name(rtcp_dbentry *e)
{
	char *cname, *name, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	name  = mbus_encode_str(e->sentry->name);
        args = (char*)xmalloc(strlen(cname) + strlen(name) + 2);
        
	sprintf(args, "%s %s", cname, name);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.name", args, TRUE);
	xfree(cname);
	xfree(name);
        xfree(args);
}

void
ui_info_update_cname(rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
        
        cname = mbus_encode_str(e->sentry->cname);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.exists", cname, TRUE);
        xfree(cname);
}

void
ui_info_update_email(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->email);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.email", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_phone(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->phone);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.phone", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_loc(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = mbus_encode_str(e->sentry->cname);
	arg   = mbus_encode_str(e->sentry->loc);
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.loc", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_tool(rtcp_dbentry *e)
{
	char *cname = mbus_encode_str(e->sentry->cname);
	char *arg   = mbus_encode_str(e->sentry->tool);
        char *args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.tool", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_note(rtcp_dbentry *e)
{
	char *cname = mbus_encode_str(e->sentry->cname);
	char *arg   = mbus_encode_str(e->sentry->note);
        char *args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.note", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_remove(rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.remove", cname, TRUE);
        xfree(cname);
}

void
ui_info_activate(rtcp_dbentry *e)
{
        char *cname;
	
        if (e->sentry->cname == NULL) return;
        
        cname = mbus_encode_str(e->sentry->cname);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.active", cname, FALSE);
        xfree(cname);
}

void
ui_info_deactivate(rtcp_dbentry *e)
{
        char *cname;

	if (e->sentry->cname == NULL) return;
	
        cname = mbus_encode_str(e->sentry->cname);
        mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.inactive", cname, FALSE);
        xfree(cname);
}

void
ui_update_stats(rtcp_dbentry *e, session_struct *sp)
{
	char	*my_cname, *their_cname, *args;

	assert(sp->db->my_dbe->sentry->cname != NULL);

	if (e->sentry->cname == NULL) {
		return;
	}

	my_cname    = mbus_encode_str(sp->db->my_dbe->sentry->cname);
	their_cname = mbus_encode_str(e->sentry->cname);

        if (e->enc_fmt) {
                args = (char *) xmalloc(strlen(their_cname) + strlen(e->enc_fmt) + 2);
                sprintf(args, "%s %s", their_cname, e->enc_fmt);
        } else {
                args = (char *) xmalloc(strlen(their_cname) + 7 + 2);
                sprintf(args, "%s unknown", their_cname);
        }
        mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.codec", args, FALSE);
        xfree(args);

        /* args size is for source.packet.loss, source.audio.buffered size always less */
	args = (char *) xmalloc(strlen(their_cname) + strlen(my_cname) + 11);
        
        sprintf(args, "%s %ld", their_cname, playout_buffer_duration(sp->playout_buf_list, e));
        mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.audio.buffered", args, FALSE);

	sprintf(args, "%s %s %8ld", my_cname, their_cname, (e->lost_frac * 100) >> 8);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.packet.loss", args, FALSE);

	xfree(my_cname);
	xfree(their_cname);
	xfree(args);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.port", "microphone", TRUE);
		break;
	case AUDIO_LINE_IN:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.port", "line_in", TRUE);
		break;	
	case AUDIO_CD:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.port", "cd", TRUE);
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.mute", "0", TRUE);
	} else {
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.mute", "1", TRUE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.port", "speaker", TRUE);
		break;
	case AUDIO_HEADPHONE:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.port", "headphone", TRUE);
		break;
	case AUDIO_LINE_OUT:
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.port", "line_out", TRUE);
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.mute", "0", TRUE);
	} else {
		mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.mute", "1", TRUE);
	}
}

void
ui_input_level(int level)
{
	static int	ol;
	char		args[4];

        assert(level>=0 && level <=100);

	if (ol == level) {
		return;
	}

	sprintf(args, "%3d", level);
	mbus_engine_tx(TRUE, mbus_name_ui, "audio.powermeter.input", args, FALSE);
	ol = level;
}

void
ui_output_level(int level)
{
	static int	ol;
	char		args[4];
        assert(level>=0 && level <=100);

	if (ol == level) {
                return;
	}

	sprintf(args, "%3d", level);
	mbus_engine_tx(TRUE, mbus_name_ui, "audio.powermeter.output", args, FALSE);
	ol = level;
}

static void
ui_repair(session_struct *sp)
{
	char	*repair = NULL;

        switch(sp->repair) {
        case REPAIR_NONE:
		repair = mbus_encode_str("None");
                break;
        case REPAIR_REPEAT:
		repair = mbus_encode_str("Packet Repetition");
                break;
	case REPAIR_PATTERN_MATCH:
		repair = mbus_encode_str("Pattern Matching");
                break;
        }
	mbus_engine_tx(TRUE, mbus_name_ui, "repair", repair, FALSE);
	xfree(repair);
}

void
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

        sprintf(args,"%d %d",iu, isep);
        mbus_engine_tx(TRUE, mbus_name_ui, "interleaving", args, TRUE);        
}

void
ui_update_frequency(session_struct *sp)
{
	codec_t *pcp;
	char	 args[7], *mbes;

	pcp = get_codec_by_pt(sp->encodings[0]);
	sprintf(args, "%d-kHz", pcp->freq/1000);
        assert(strlen(args) < 7);
        mbes = mbus_encode_str(args);
	mbus_engine_tx(TRUE, mbus_name_ui, "frequency", mbes, FALSE);
        xfree(mbes);
}

void
ui_update_channels(session_struct *sp)
{
	codec_t *pcp;
	char	*mbes;
        
	pcp = get_codec_by_pt(sp->encodings[0]);
        switch(pcp->channels) {
        case 1:
                mbes = mbus_encode_str("Mono");
                break;
        case 2:
                mbes = mbus_encode_str("Stereo");
                break;
        default:
                debug_msg("UI not ready for %d channels\n", pcp->channels);
                return;
        }
	mbus_engine_tx(TRUE, mbus_name_ui, "channels", mbes, FALSE);
        xfree(mbes);
}       

void
ui_update_primary(session_struct *sp)
{
	codec_t *pcp;
        char *mbes;

	pcp = get_codec_by_pt(sp->encodings[0]);
	mbes = mbus_encode_str(pcp->short_name);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.codec", mbes, FALSE);
        xfree(mbes);
}

void
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

	args = (char *) xmalloc(strlen(codec_name) + 4);
        sprintf(args,"%s %2d", codec_name, ioff);
        assert(strlen(args) < (strlen(codec_name) + 4));
        mbus_engine_tx(TRUE, mbus_name_ui, "redundancy", args, TRUE);
	xfree(codec_name);
        xfree(args);
}

void 
ui_update_channel(session_struct *sp) 
{
        cc_coder_t *ccp;
        char       *mbes = NULL;

        ccp = get_channel_coder(sp->cc_encoding);
        assert(ccp != NULL);
        switch(ccp->name[0]) {
        case 'V':
                mbes = mbus_encode_str("No Loss Protection");
                break;
        case 'R':
                mbes = mbus_encode_str("Redundancy");
                break;
        case 'I':
                mbes = mbus_encode_str("Interleaving");
                break;
        default:
                debug_msg("Channel coding failed mapping.\n");
                return;
        }

        mbus_engine_tx(TRUE, mbus_name_ui, "audio.channel.coding", mbes, TRUE);
        if (mbes) xfree(mbes);
}

void
ui_update_input_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_gain(sp->audio_fd)); 
        assert(strlen(args) < 4);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.input.gain", args, TRUE);
}

void
ui_update_output_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_volume(sp->audio_fd)); 
        assert(strlen(args) < 4);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.gain", args, TRUE);
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
	        sprintf(args, "%3d", sp->output_gain); mbus_engine_tx(TRUE, mbus_name_ui, "audio.output.gain", args, TRUE);
                assert(strlen(args) < 4);
		sprintf(args, "%3d", sp->input_gain ); mbus_engine_tx(TRUE, mbus_name_ui,  "audio.input.gain", args, TRUE);
                assert(strlen(args) < 4);
	}

        sprintf(args, "%3d", collator_get_units(sp->collator));
        assert(strlen(args) < 4);
	mbus_engine_tx(TRUE, mbus_name_ui, "rate", args, TRUE);

	ui_update_output_port(sp);
	ui_update_input_port(sp);
        ui_codecs(sp->encodings[0]);
        ui_converters();
        ui_update_frequency(sp);
        ui_update_channels(sp);
	ui_update_primary(sp);
        ui_update_redundancy(sp);
        ui_update_interleaving(sp);
        ui_update_channel(sp);
        ui_repair(sp);
}

void
ui_show_audio_busy(void)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "disable.audio.ctls", "", TRUE);
}

void
ui_hide_audio_busy(void)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "enable.audio.ctls", "", TRUE);
}


void
ui_update_lecture_mode(session_struct *sp)
{
	/* Update the UI to reflect the lecture mode setting...*/
	char	args[2];
	sprintf(args, "%1d", sp->lecture);
	mbus_engine_tx(TRUE, mbus_name_ui, "lecture.mode", args, TRUE);
}

void
ui_update_powermeters(session_struct *sp, struct s_mix_info *ms, int elapsed_time)
{
	static u_int32 power_time = 0;

	if (power_time > sp->meter_period) {
		if (sp->meter && (ms != NULL)) {
			mix_update_ui(ms);
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
ui_update_loss(char *srce, char *dest, int loss)
{
	char	*srce_e, *dest_e, *args;

	if ((srce == NULL) || (dest == NULL)) {
		return;
	}

 	srce_e = mbus_encode_str(srce);
	dest_e = mbus_encode_str(dest);
	args   = (char *) xmalloc(strlen(srce_e) + strlen(dest_e) + 6);
	sprintf(args, "%s %s %3d", srce_e, dest_e, loss);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.packet.loss", args, FALSE);
	xfree(args);
	xfree(srce_e);
	xfree(dest_e);
}

void
ui_update_reception(char *cname, u_int32 recv, u_int32 lost, u_int32 misordered, u_int32 duplicates, u_int32 jitter, int jit_tog)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);

	/* I hate this function! */
	args = (char *) xmalloc(strlen(cname_e) + 88);
	sprintf(args, "%s %6ld %6ld %6ld %6ld %6ld %6d", cname_e, recv, lost, misordered, duplicates, jitter, jit_tog);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.reception", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void
ui_update_duration(char *cname, int duration)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(5 + strlen(cname_e));

	sprintf(args, "%s %3d", cname_e, duration);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.source.packet.duration", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void 
ui_update_video_playout(char *cname, int playout)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(14 + strlen(cname_e));

	sprintf(args, "%s %12d", cname_e, playout);
	mbus_engine_tx(TRUE, mbus_name_video, "source_playout", args, FALSE);
	xfree(args);
        xfree(cname_e);
}

void	
ui_update_sync(int sync)
{
	if (sync) {
		mbus_engine_tx(TRUE, mbus_name_ui, "sync", "1", TRUE);
	} else {
		mbus_engine_tx(TRUE, mbus_name_ui, "sync", "0", TRUE);
	}
}

void
ui_update_key(char *key)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "update_key", key, TRUE);
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
ui_update_playback_file(char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.file.play.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_record_file(char *name)
{
        char *mbes;
        mbes = mbus_encode_str(name);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.file.record.ready", mbes, TRUE); 
        xfree(mbes);
}

void
ui_update_file_live(char *mode, int valid)
{
        char cmd[32], arg[2];
        
        assert(!strcmp(mode, "play") || !strcmp(mode, "record"));
        
        sprintf(cmd, "audio.file.%s.alive", mode);
        sprintf(arg, "%1d", valid); 
        mbus_engine_tx(TRUE, mbus_name_ui, cmd, arg, TRUE);
}

void 
ui_codecs(int pt)
{
	char	args[256], *mbes;	/* Hope that's big enough... :-) */

        ui_get_codecs(pt, args, 256, TRUE);
        mbes = mbus_encode_str(args);
	mbus_engine_tx(TRUE, mbus_name_ui, "audio.codec.supported", mbes, TRUE);
        xfree(mbes);
        ui_get_codecs(pt, args, 256, FALSE);
        mbes = mbus_encode_str(args);
        mbus_engine_tx(TRUE, mbus_name_ui, "audio.redundancy.supported", mbes, TRUE);
        xfree(mbes);
}

void 
ui_converters()
{
        char buf[255], *mbes;
        if (converter_get_names(buf, 255)) {
                mbes = mbus_encode_str(buf);
                mbus_engine_tx(TRUE, mbus_name_ui, "converter.supported", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_sampling_modes(session_struct *sp)
{
	char	*freqs;

        UNUSED(sp);
        /* this is just a quick con job for the moment */
	freqs = mbus_encode_str("8-kHz 16-kHz 32-kHz 48-kHz");
	mbus_engine_tx(TRUE, mbus_name_ui, "frequencies.supported", freqs, TRUE);
	xfree(freqs);
}

void
ui_controller_init(char *cname, char *name_engine, char *name_ui, char *name_video)
{
	char	*my_cname;

	mbus_name_engine = name_engine;
	mbus_name_ui     = name_ui;
	mbus_name_video  = name_video;

	my_cname = mbus_encode_str(cname);
	mbus_engine_tx(TRUE, mbus_name_ui, "rtp.cname", my_cname, TRUE);
        xfree(my_cname);
}

void
ui_title(session_struct *sp) 
{
	char	*addr, *args, *title;

        title = mbus_encode_str(sp->title);
        mbus_engine_tx(TRUE, mbus_name_ui, "session.title", title, TRUE);
        xfree(title);

	addr = mbus_encode_str(sp->asc_address);
	args = (char *) xmalloc(strlen(addr) + 11);
        sprintf(args, "%s %5d %3d", addr, sp->rtp_port, sp->ttl);
        mbus_engine_tx(TRUE, mbus_name_ui, "session.address", args, TRUE);
	xfree(args);
        xfree(addr);
}

void
ui_load_settings()
{
	mbus_engine_tx(TRUE, mbus_name_ui, "load.settings", "", TRUE);
}

void 
ui_quit(void)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "quit", "", TRUE);
}




