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
#include "audio.h"
#include "auddev.h"
#include "version.h"
#include "settings.h"

#ifndef WIN32
FILE	*settings_file;
#endif

static void load_init(void)
{
#ifndef WIN32
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
	filen = (char *) xmalloc(strlen(p->pw_dir) + 15);
	sprintf(filen, "%s/.RTPdefaults", p->pw_dir);
	settings_file = fopen(filen, "r");
#else
#endif
}

static void load_done(void)
{
#ifdef WIN32
#else
	fclose(settings_file);
#endif
}

#ifndef WIN32
static void load_setting(char **n, char **v)
{
	static char *buffer = NULL;
	
	if (buffer == NULL) {
		buffer = (char *) xmalloc(100);
	}

	if (fgets(buffer, 100, settings_file) != NULL) {
		assert(buffer[0] == '*');
		*n = strtok(buffer, ":") + 1;
		*v = strtok(NULL, "\n");
		while ((**v == ' ') && (*v < (buffer + 100))) {
			(*v)++;
		}
	} else {
		*n = "\0";
		*v = "\0";
	}
}
#endif

static char *load_str_setting(char *name, char *default_value)
{
#ifndef WIN32
	char	*n, *v;
	/* First try, see if it's the next entry in the file... (optimize the common case) */
	load_setting(&n, &v);
	if (strcmp(name, n) == 0) {
		goto found_it;
	}
	/* No? Rewind to the beginning, and search through the entire list... */
	fseek(settings_file, 0, SEEK_SET);
	while (!feof(settings_file)) {
		load_setting(&n, &v);
		if (strcmp(name, n) == 0) {
			goto found_it;
		}
	}
	/* The setting we're looking for isn't in the preferences file... */
	return xstrdup(default_value);
found_it:
	if (v == NULL) {
		return xstrdup(default_value);
	} else {
		return xstrdup(v);
	}
#else
#endif
}

static int load_int_setting(char *name, int default_value)
{
#ifndef WIN32
	char	*n, *v;
	/* First try, see if it's the next entry in the file... (optimize the common case) */
	load_setting(&n, &v);
	if (strcmp(name, n) == 0) {
		goto found_it;
	}
	/* No? Rewind to the beginning, and search through the entire list... */
	fseek(settings_file, 0, SEEK_SET);
	while (!feof(settings_file)) {
		load_setting(&n, &v);
		if (strcmp(name, n) == 0) {
			goto found_it;
		}
	}
	/* The setting we're looking for isn't in the preferences file... */
	return default_value;
found_it:
	if (v == NULL) {
		return default_value;
	} else {
		return atoi(v);
	}
#else
#endif
}

void load_settings(session_struct *sp)
{
#ifndef WIN32
	audio_device_details_t	 ad;
	char			*ad_name, *primary_codec, *primary_long, *cc_name;
	int			 i, freq, chan;
	cc_details		 ccd;

	load_init();
	sp->db->my_dbe->sentry->name  = load_str_setting("rtpName", "Unknown");		/* We don't use rtcp_set_attribute() */ 
	sp->db->my_dbe->sentry->email = load_str_setting("rtpEmail", "");		/* here, since that updates the UI   */
	sp->db->my_dbe->sentry->phone = load_str_setting("rtpPhone", "");		/* and we don't want to do that yet. */
	sp->db->my_dbe->sentry->loc   = load_str_setting("rtpLoc", "");
	sp->db->my_dbe->sentry->tool  = load_str_setting("audioTool", RAT_VERSION);

	ad_name = load_str_setting("audioDevice", "No Audio Device");
        for(i = 0; i < audio_get_device_count(); i++) {
                if (audio_get_device_details(i, &ad) && (strcmp(ad.name, ad_name) == 0)) {
			audio_device_register_change_device(sp, ad.descriptor);
                        break;
                }
        }
	xfree(ad_name);

	freq = load_int_setting("audioFrequency", 8000);
	chan = load_int_setting("audioChannelsIn", 1);
	primary_codec = load_str_setting("audioPrimary", "GSM");
	primary_long  = (char *) xmalloc(strlen(primary_codec) + 10);
	sprintf(primary_long, "%s/%4d/%1d", primary_codec, freq, chan);
	audio_device_register_change_primary(sp, codec_get_by_name(primary_long));
	xfree(primary_codec);
	xfree(primary_long);

	cc_name = load_str_setting("audioChannelCoding", "None");
	for (i = 0; i < channel_get_coder_count(); i++ ) {
		channel_get_coder_details(i, &ccd);
		if (strcmp(ccd.name, cc_name) == 0) {
        		channel_encoder_create(ccd.descriptor, &sp->channel_coder);
			break;
		}
	}
	xfree(cc_name);
	channel_encoder_set_parameters(sp->channel_coder, load_str_setting("audioChannelParameters", "None"));
	channel_encoder_set_units_per_packet(sp->channel_coder, (u_int16) load_int_setting("audioUnits", 2));

		load_str_setting("audioRepair", "Pattern-Match");
		load_str_setting("audioAutoConvert", "High Quality");
	sp->limit_playout  = load_int_setting("audioLimitPlayout", 0);
	sp->min_playout    = load_int_setting("audioMinPlayout", 0);
	sp->max_playout    = load_int_setting("audioMaxPlayout", 2000);
	sp->lecture        = load_int_setting("audioLecture", 0);
	sp->render_3d      = load_int_setting("audio3dRendering", 0);
	sp->detect_silence = load_int_setting("audioSilence", 1);
	sp->agc_on         = load_int_setting("audioAGC", 0);
	sp->loopback_gain  = load_int_setting("audioLoopback", 0);
	sp->echo_suppress  = load_int_setting("audioEchoSuppress", 0);
		load_int_setting("audioOutputGain", 75);
		load_int_setting("audioInputGain", 75);
		load_str_setting("audioOutputPort", "Headphone");
		load_str_setting("audioInputPort", "Microphone");
	sp->meter          = load_int_setting("audioPowermeters", 1);
	sp->sync_on        = load_int_setting("audioLipSync", 0);
		load_int_setting("audioOutputMute", 1);
		load_int_setting("audioInputMute", 1);
	load_done();
#endif
}

static void save_init(void)
{
#ifndef WIN32
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
	filen = (char *) xmalloc(strlen(p->pw_dir) + 15);
	sprintf(filen, "%s/.RTPdefaults", p->pw_dir);
	settings_file = fopen(filen, "w");
#endif
}

static void save_done(void)
{
#ifndef WIN32
	fclose(settings_file);
#else
#endif
}

static void save_str_setting(const char *name, const char *val)
{
#ifndef WIN32
	fprintf(settings_file, "*%s: %s\n", name, val);
#else
#endif
}

static void save_int_setting(const char *name, const long val)
{
#ifndef WIN32
	fprintf(settings_file, "*%s: %ld\n", name, val);
#else
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
	save_str_setting("audioDevice",            ad.name);
	save_int_setting("audioFrequency",         af->sample_rate);
	save_int_setting("audioChannelsIn",        af->channels); 
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
	/* We do not save audioOutputMute and audioInputMute by default, but should */
	/* recognize them when reloading.                                           */
	save_done();
}

