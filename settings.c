/*
 * FILE:    settings.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "ts.h"
#include "channel.h"
#include "net_udp.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "session.h"
#include "repair.h"
#include "timers.h"
#include "codec_types.h"
#include "codec.h"
#include "auddev.h"
#include "version.h"
#include "settings.h"

#ifndef WIN32
FILE	*settings_file;
#endif

void load_settings(session_struct *sp)
{
	UNUSED(sp);
}

static void save_init(void)
{
#ifdef WIN32
	/* Do something complicated with the registry... */
#else
	struct passwd	*p;	
	char		*filen;

	/* The getpwuid() stuff is to determine the users home directory, into which we */
	/* write the settings file. The struct returned by getpwuid() is statically     */
	/* allocated, so it's not necessary to free it afterwards.                      */
	p = getpwuid(getuid());
	if (p == NULL) {
		perror("Unable to get passwd entry");
		abort();
	}
	filen = (char *) xmalloc(strlen(p->pw_dir) + 6);
	sprintf(filen, "%s/.RTPdefaults", p->pw_dir);
	settings_file = fopen(filen, "w");
#endif
}

static void save_done(void)
{
#ifdef WIN32
	/* Do something complicated with the registry... */
#else
	fclose(settings_file);
#endif
}

static void save_str_setting(const char *name, const char *val)
{
#ifdef WIN32
	/* Do something complicated with the registry... */
#else
	fprintf(settings_file, "*%s: %s\n", name, val);
#endif
}

static void save_int_setting(const char *name, const long val)
{
#ifdef WIN32
	/* Do something complicated with the registry... */
#else
	fprintf(settings_file, "*%s: %ld\n", name, val);
#endif
}

void save_settings(session_struct *sp)
{
	codec_id_t	 		 pri_id;
        const codec_format_t 		*pri_cf;
        cc_details 			 cd;
	int				 cc_len;
	char				*cc_param;
        converter_details_t  		 converter;
	int		 		 i;
	audio_device_details_t		 ad;
        const audio_format 		*af;
        const audio_port_details_t 	*iapd, *oapd;

	pri_id   = codec_get_by_payload(sp->encodings[0]);
        pri_cf   = codec_get_format(pri_id);
        cc_len   = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cc_param = (char*) xmalloc(cc_len);
        channel_encoder_get_parameters(sp->channel_coder, cc_param, cc_len);
        channel_get_coder_identity(sp->channel_coder, &cd);

        for(i = 0; i < (int) converter_get_count(); i++) {
                converter_get_details(i, &converter);
                if (sp->converter == converter.id) {
			break;
                }
        }

        for(i = 0; i < audio_get_device_count(); i++) {
                if (audio_get_device_details(i, &ad) && sp->audio_device == ad.descriptor) {
                        break;
                }
        }

        af = audio_get_ifmt(sp->audio_device);

        for(i = 0; i < audio_get_iport_count(sp->audio_device); i++) {
                iapd = audio_get_iport_details(sp->audio_device, i);
                if (iapd->port == audio_get_iport(sp->audio_device)) {
                        break;
                }
        }

        for(i = 0; i < audio_get_oport_count(sp->audio_device); i++) {
                oapd = audio_get_oport_details(sp->audio_device, i);
                if (oapd->port == audio_get_oport(sp->audio_device)) {
                        break;
                }
        }

	save_init();
	save_str_setting("rtpName",                sp->db->my_dbe->sentry->name);
	save_str_setting("rtpEmail",               sp->db->my_dbe->sentry->email);
	save_str_setting("rtpPhone",               sp->db->my_dbe->sentry->phone);
	save_str_setting("rtpLoc",                 sp->db->my_dbe->sentry->loc);
	save_str_setting("audioTool",              sp->db->my_dbe->sentry->tool);
	save_str_setting("audioPrimary",           pri_cf->short_name);
	save_int_setting("audioUnits",             channel_encoder_get_units_per_packet(sp->channel_coder)); 
	save_str_setting("audioChannelCoding",     cd.name);
	save_str_setting("audioChannelParameters", cc_param);
	save_str_setting("audioRepair",            repair_get_name(sp->repair));
	save_str_setting("audioAutoConvert",       converter.name);
	save_int_setting("audioLimitPlayout",      sp->limit_playout);
	save_int_setting("audioMinPlayout",        sp->min_playout);
	save_int_setting("audioMaxPlayout",        sp->max_playout);
	save_int_setting("audioLecture",           sp->lecture);
	save_int_setting("audio3dRendering",       sp->render_3d);
	save_str_setting("audioDevice",            ad.name);
	save_int_setting("audioFrequency",         af->sample_rate);
	save_int_setting("audioChannelsIn",        af->channels); 
	save_int_setting("audioSilence",           sp->detect_silence);
	save_int_setting("audioAGC",               sp->agc_on);
	save_int_setting("audioLoopback",          sp->loopback_gain); 
	save_int_setting("audioEchoSuppress",      sp->echo_suppress);
	save_int_setting("audioOutputGain",        audio_get_ogain(sp->audio_device));
	save_int_setting("audioInputGain",         audio_get_igain(sp->audio_device));
	save_str_setting("audioOutputPort",        oapd->name);
	save_str_setting("audioInputPort",         iapd->name); 
	save_int_setting("audioPowermeters",       sp->meter);
	save_int_setting("audioLipSync",           sp->sync_on);
	save_done();
}

