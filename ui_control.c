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

#include "config.h"
#include "version.h"
#include "session.h"
#include "crypt.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "util.h"
#include "repair.h"
#include "codec.h"
#include "audio.h"
#include "mbus.h"
#include "mbus_engine.h"
#include "channel.h"
#include "mix.h"
#include "transmit.h"
#include "speaker_table.h"
#include "ui_control.h"
#include "rat_time.h"

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

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	name  = xstrdup(mbus_encode_str(e->sentry->name));
        args = (char*)xmalloc(strlen(cname) + strlen(name) + 2);
        
	sprintf(args, "%s %s", cname, name);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.name", args, TRUE);
	xfree(cname);
	xfree(name);
        xfree(args);
}

void
ui_info_update_cname(rtcp_dbentry *e)
{
	if (e->sentry->cname == NULL) return;
	mbus_engine_tx(TRUE, mbus_name_ui, "source.exists", xstrdup(mbus_encode_str(e->sentry->cname)), TRUE);
}

void
ui_info_update_email(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->email));
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.email", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_phone(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->phone));
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.phone", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_loc(rtcp_dbentry *e)
{
	char *cname, *arg, *args;

	if (e->sentry->cname == NULL) return;

	cname = xstrdup(mbus_encode_str(e->sentry->cname));
	arg   = xstrdup(mbus_encode_str(e->sentry->loc));
        args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.loc", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_update_tool(rtcp_dbentry *e)
{
	char *cname = xstrdup(mbus_encode_str(e->sentry->cname));
	char *arg   = xstrdup(mbus_encode_str(e->sentry->tool));
        char *args = (char*)xmalloc(strlen(cname) + strlen(arg) + 2);
	sprintf(args, "%s %s", cname, arg);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.tool", args, TRUE);
	xfree(cname);
	xfree(arg);
        xfree(args);
}

void
ui_info_remove(rtcp_dbentry *e)
{
	if (e->sentry->cname == NULL) return;
	mbus_engine_tx(TRUE, mbus_name_ui, "source.remove", xstrdup(mbus_encode_str(e->sentry->cname)), TRUE);
}

void
ui_info_activate(rtcp_dbentry *e)
{
	if (e->sentry->cname == NULL) return;
	mbus_engine_tx(TRUE, mbus_name_ui, "source.active.now", xstrdup(mbus_encode_str(e->sentry->cname)), FALSE);
}

void
ui_info_gray(rtcp_dbentry *e)
{
	if (e->sentry->cname == NULL) return;
	mbus_engine_tx(TRUE, mbus_name_ui, "source.active.recent", xstrdup(mbus_encode_str(e->sentry->cname)), FALSE);
}

void
ui_info_deactivate(rtcp_dbentry *e)
{
	if (e->sentry->cname == NULL) return;
	mbus_engine_tx(TRUE, mbus_name_ui, "source.inactive", xstrdup(mbus_encode_str(e->sentry->cname)), FALSE);
}

void
update_stats(rtcp_dbentry *e, session_struct *sp)
{
	char	 encoding[100], *p, *my_cname, *their_cname, *args;
	int	 l;
	codec_t	*cp;

	assert(sp->db->my_dbe->sentry->cname != NULL);
	if (e->sentry->cname == NULL) {
		return;
	}

	memset(encoding, '\0', 100);
	if (e->encs[0] != -1) {
		cp = get_codec(e->encs[0]);
		strcpy(encoding, cp->name);
		for (l = 1, p = encoding; l < 10 && e->encs[l] != -1; l++) {
			p += strlen(p);
			*p++ = '+';
			cp = get_codec(e->encs[l]);
			if (cp != NULL) {
				strcpy(p, cp->name);
			} else {
				*p++ = '?';
			}
		}
	} else {
		*encoding = 0;
	}


	my_cname    = strdup(mbus_encode_str(sp->db->my_dbe->sentry->cname));
	their_cname = strdup(mbus_encode_str(e->sentry->cname));

	args = (char *) xmalloc(strlen(their_cname) + strlen(encoding) + 2);
	sprintf(args, "%s %s", their_cname, encoding);                       
	mbus_engine_tx(TRUE, mbus_name_ui, "source.codec", args, FALSE);
	xfree(args);

	args = (char *) xmalloc(strlen(their_cname) + strlen(my_cname) + 11);
	sprintf(args, "%s %s %8ld", my_cname, their_cname, (e->lost_frac * 100) >> 8);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.packet.loss", args, FALSE);
	free(my_cname);
	free(their_cname);
	xfree(args);
}

void
ui_update_input_port(session_struct *sp)
{
	switch (sp->input_mode) {
	case AUDIO_MICROPHONE:
		mbus_engine_tx_queue(TRUE, "input.port", "microphone");
		break;
	case AUDIO_LINE_IN:
		mbus_engine_tx_queue(TRUE, "input.port", "line_in");
		break;	
	case AUDIO_CD:
		mbus_engine_tx_queue(TRUE, "input.port", "cd");
		break;
	default:
		fprintf(stderr, "Invalid input port!\n");
		return ;
	}
	if (sp->sending_audio) {
		mbus_engine_tx(TRUE, mbus_name_ui, "input.mute", "0", FALSE);
	} else {
		mbus_engine_tx(TRUE, mbus_name_ui, "input.mute", "1", FALSE);
	}
}

void
ui_update_output_port(session_struct *sp)
{
	switch (sp->output_mode) {
	case AUDIO_SPEAKER:
		mbus_engine_tx_queue(TRUE, "output.port", "speaker");
		break;
	case AUDIO_HEADPHONE:
		mbus_engine_tx_queue(TRUE, "output.port", "headphone");
		break;
	case AUDIO_LINE_OUT:
		mbus_engine_tx_queue(TRUE, "output.port", "line_out");
		break;
	default:
		fprintf(stderr, "Invalid output port!\n");
		return;
	}
	if (sp->playing_audio) {
		mbus_engine_tx(TRUE, mbus_name_ui, "output.mute", "0", FALSE);
	} else {
		mbus_engine_tx(TRUE, mbus_name_ui, "output.mute", "1", FALSE);
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
	mbus_engine_tx(TRUE, mbus_name_ui, "powermeter.input", args, FALSE);
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
	mbus_engine_tx(TRUE, mbus_name_ui, "powermeter.output", args, FALSE);
	ol = level;
}

static void
ui_repair(session_struct *sp)
{
        switch(sp->repair) {
        case REPAIR_NONE:
		mbus_engine_tx(TRUE, mbus_name_ui, "repair", "None", FALSE);
                break;
        case REPAIR_REPEAT:
		mbus_engine_tx(TRUE, mbus_name_ui, "repair", "Packet Repetition", FALSE);
                break;
	case REPAIR_PATTERN_MATCH:
		mbus_engine_tx(TRUE, mbus_name_ui, "repair", "Pattern Matching", FALSE);
                break;
        }
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
                dprintf("Could not find interleaving channel coder!\n");
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
	char	 args[7];

	pcp = get_codec(sp->encodings[0]);
	sprintf(args, "%d-kHz", pcp->freq/1000);
	mbus_engine_tx(TRUE, mbus_name_ui, "frequency", mbus_encode_str(args), FALSE);
}

void
ui_update_channels(session_struct *sp)
{
	codec_t *pcp;
	char	 args[9];
        
	pcp = get_codec(sp->encodings[0]);
        switch(pcp->channels) {
        case 1:
                sprintf(args, "%s", mbus_encode_str("Mono"));
                break;
        case 2:
                sprintf(args, "%s", mbus_encode_str("Stereo"));
                break;
        default:
                dprintf("UI not ready for %d channels\n", pcp->channels);
                return;
        }
	mbus_engine_tx(TRUE, mbus_name_ui, "channels", args, FALSE);
}       

void
ui_update_primary(session_struct *sp)
{
	codec_t *pcp;

	pcp = get_codec(sp->encodings[0]);
	mbus_engine_tx(TRUE, mbus_name_ui, "primary", xstrdup(mbus_encode_str(pcp->short_name)), FALSE);
}

void
ui_update_redundancy(session_struct *sp)
{
        int  pt;
        int  ioff;
        char buf[128], *codec_name=NULL, *offset=NULL, *dummy, *args;

        pt = get_cc_pt(sp,"REDUNDANCY");
        if (pt != -1) { 
                codec_t *cp;
                query_channel_coder(sp, pt, buf, 128);
                dummy  = strtok(buf,"/");
                dummy  = strtok(NULL,"/");
                codec_name  = strtok(NULL,"/");
                /* redundant coder returns long name convert to short*/
                if (codec_name) {
                        cp         = get_codec_byname(codec_name, sp);
                        assert(cp);
                        codec_name = cp->short_name;
                }
                offset = strtok(NULL,"/");
        } else {
                dprintf("Could not find redundant channel coder!\n");
        } 

        if (codec_name != NULL && offset != NULL) {
                ioff  = atoi(offset);
        } else {
                codec_t *pcp;
                pcp   = get_codec(sp->encodings[0]);
                codec_name = pcp->short_name;
                ioff  = 1;
        } 

	codec_name = mbus_encode_str(codec_name);

	args = (char *) xmalloc(strlen(codec_name) + 4);
        sprintf(args,"%s %2d", codec_name, ioff);
        mbus_engine_tx(TRUE, mbus_name_ui, "redundancy", args, TRUE);
	xfree(args);
}

static void 
ui_update_channel(session_struct *sp) 
{
        cc_coder_t *ccp;
        char args[80];

        ccp = get_channel_coder(sp->cc_encoding);
        assert(ccp != NULL);
        switch(ccp->name[0]) {
        case 'V':
                sprintf(args, mbus_encode_str("No Loss Protection"));
                break;
        case 'R':
                sprintf(args, mbus_encode_str("Redundancy"));
                break;
        case 'I':
                sprintf(args, mbus_encode_str("Interleaving"));
                break;
        default:
                dprintf("Channel coding failed mapping.\n");
                return;
        }
        mbus_engine_tx(TRUE, mbus_name_ui, "channel.code", args, TRUE);
}

void
ui_update_input_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_gain(sp->audio_fd));   
        mbus_engine_tx_queue(TRUE,  "input.gain", args);
}

void
ui_update_output_gain(session_struct *sp)
{
	char	args[4];

        sprintf(args, "%3d", audio_get_volume(sp->audio_fd)); 
        mbus_engine_tx_queue(TRUE, "output.gain", args);
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
	        sprintf(args, "%3d", sp->output_gain); mbus_engine_tx_queue(TRUE, "output.gain", args);
		sprintf(args, "%3d", sp->input_gain ); mbus_engine_tx_queue(TRUE,  "input.gain", args);
	}

        sprintf(args, "%3d", collator_get_units(sp->collator));
	mbus_engine_tx_queue(TRUE, "rate", args);

	ui_update_output_port(sp);
	ui_update_input_port(sp);
        ui_codecs(sp->encodings[0]);
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
update_lecture_mode(session_struct *sp)
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
		if (sp->meter) {
			mix_update_ui(ms);
		}
		clear_active_senders(sp);

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

 	srce_e = xstrdup(mbus_encode_str(srce));
	dest_e = xstrdup(mbus_encode_str(dest));
	args   = (char *) xmalloc(strlen(srce_e) + strlen(dest_e) + 6);
	sprintf(args, "%s %s %3d", srce_e, dest_e, loss);
	mbus_engine_tx(TRUE, mbus_name_ui, "source.packet.loss", args, FALSE);

	xfree(args);
	xfree(srce_e);
	xfree(dest_e);
}

void
ui_update_reception(char *cname, u_int32 recv, u_int32 lost, u_int32 misordered, double jitter, int jit_tog)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);

	/* I hate this function! */
	args = (char *) xmalloc(strlen(cname_e) + 80);
	sprintf(args, "%s %6ld %6ld %6ld %f %6d", cname_e, recv, lost, misordered, jitter, jit_tog);
	mbus_engine_tx_queue(TRUE, "source.reception", args);
	free(args);
}

void
ui_update_duration(char *cname, int duration)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(5 + strlen(cname_e));

	sprintf(args, "%s %3d", cname_e, duration);
	mbus_engine_tx_queue(TRUE, "source.packet.duration", args);
	xfree(args);
}

void 
update_video_playout(char *cname, int playout)
{
	char	*cname_e, *args;

	if (cname == NULL) return;

	cname_e = mbus_encode_str(cname);
	args    = (char *) xmalloc(8 + strlen(cname_e));

	sprintf(args, "%s %6d", cname, playout);
	mbus_engine_tx(TRUE, mbus_name_video, "source_playout", args, FALSE);
	xfree(args);
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
ui_get_codecs(int pt, char *buf, int loose) 
{
	codec_t	*codec[10],*sel;
	int 	 i, nc;
        
        sel = get_codec(pt);
        
        for (nc=i=0; i<MAX_CODEC; i++) {
                codec[nc] = get_codec(i);
                if (codec[nc] != NULL ) {
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
        for(i=0;i<nc;i++) {
                sprintf(buf, " %s", codec[i]->short_name);
                buf += strlen(codec[i]->short_name) + 1;
        }

        return buf;
}

void 
ui_codecs(int pt)
{
	char	args[64];	/* Hope that's big enough... :-) */

        ui_get_codecs(pt, args, TRUE);
        mbus_encode_str(args);
	mbus_engine_tx(TRUE, mbus_name_ui, "codec.supported", args, TRUE);
        ui_get_codecs(pt, args, FALSE);
        mbus_encode_str(args);
        mbus_engine_tx(TRUE, mbus_name_ui, "redundancy.supported", args, TRUE);
}

void
ui_sampling_modes(session_struct *sp)
{
        UNUSED(sp);
        /* this is just a quick con job for the moment */
	mbus_engine_tx(TRUE, 
                       mbus_name_ui, 
                       "frequencies.supported", 
                       "8-kHz 16-kHz 32-kHz 48-kHz", 
                       TRUE);
}

void
ui_controller_init(char *cname, char *name_engine, char *name_ui, char *name_video)
{
	char	*my_cname;

	mbus_name_engine = name_engine;
	mbus_name_ui     = name_ui;
	mbus_name_video  = name_video;

	my_cname = mbus_encode_str(cname);
	mbus_engine_tx(TRUE, mbus_name_ui, "my.cname", my_cname, TRUE);
}

void
ui_title(session_struct *sp) 
{
	char	*addr, *args;

        mbus_engine_tx(TRUE, mbus_name_ui, "session.title", mbus_encode_str(sp->title), TRUE);

	addr = mbus_encode_str(sp->asc_address);
	args = (char *) xmalloc(strlen(addr) + 11);
        sprintf(args, "%s %5d %3d", addr, sp->rtp_port, sp->ttl);
        mbus_engine_tx(TRUE, mbus_name_ui, "session.address", args, TRUE);
}

void
ui_load_settings(void)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "load.settings", "", TRUE);
}

void 
ui_quit(void)
{
	mbus_engine_tx(TRUE, mbus_name_ui, "quit", "", TRUE);
}




