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
#include "transmit.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "auddev.h"
#include "version.h"
#include "settings.h"

typedef struct s_hash_tuple {
        u_int32 hash;
        char *key;
        char *value;
        struct s_hash_tuple *next;
} hash_tuple;

typedef struct s_hash_chain {
        u_int32 nelem;
        hash_tuple *head;
} hash_chain;

#define SETTINGS_READ_SIZE 100
#define SETTINGS_TABLE_SIZE 11

#ifdef WIN32
#define SETTINGS_BUF_SIZE 1500
static HKEY cfgKey;
#endif

#ifndef WIN32
static hash_chain *table;          /* Use hashtable to load settings    */
static FILE       *settings_file;  /* Write direct to this file to save */
#endif

/* SETTINGS HASH CODE ********************************************************/

static u_int32 
setting_hash(char *key)
{
#ifndef WIN32
        u_int32 hash = 0;

        while(*key != '\0') {
                hash = hash * 31;
                hash += ((u_int32)*key) + 1;
                key++;
        }

        return hash;
#endif
}

static void
settings_table_add(char *key, char *value)
{
#ifndef WIN32
        hash_tuple *t;
        int row;

        t = (hash_tuple*)xmalloc(sizeof(hash_tuple));
        /* transfer values */
        t->hash  = setting_hash(key);
        t->key   = xstrdup(key);
        t->value = xstrdup(value);

        /* Add to table */
        row      = t->hash % SETTINGS_TABLE_SIZE;
        t->next  = table[row].head;
        table[row].head = t;
        table[row].nelem++;
#endif
}

/* settings_table_lookup points value at actual value
 * and return TRUE if key found */
static int
settings_table_lookup(char *key, char **value)
{
#ifndef WIN32
        hash_tuple *t;
        u_int32     hash;

        hash = setting_hash(key);

        t = table[hash % SETTINGS_TABLE_SIZE].head;
        while(t != NULL) {
                if (t->hash == hash && strcmp(key, t->key) == 0) {
                        *value = t->value;
                        return TRUE;
                }
                t = t->next;
        }
        *value = NULL;
        return FALSE;
#endif
}

static void
settings_table_create()
{
#ifndef WIN32
        table = (hash_chain*)xmalloc(sizeof(hash_chain) * SETTINGS_TABLE_SIZE);
        memset(table, 0, sizeof(hash_chain) * SETTINGS_TABLE_SIZE);
#endif
}

static void
settings_table_destroy(void)
{
#ifndef WIN32
        hash_tuple *t;
        int i;

        for(i = SETTINGS_TABLE_SIZE-1; i >= 0; i--) {
                t = table[i].head;
                while (t != NULL) {
                        table[i].head = t->next;
                        debug_msg("Freeing %s %s\n", t->key, t->value);
                        xfree(t->key);
                        xfree(t->value);
                        xfree(t);
                        t = table[i].head;
                }
        }
        xfree(table);
        table = NULL;
        xmemchk();
#endif
}

/* SETTINGS CODE *************************************************************/

#ifdef WIN32
static void open_registry(void)
{
        HKEY			key    = HKEY_CURRENT_USER;
	LPCTSTR			subKey = "Software\\Mbone Applications\\common";
	DWORD			disp;
	char			buffer[SETTINGS_BUF_SIZE];
	LONG			status;

	status = RegCreateKeyEx(key, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &cfgKey, &disp);
	if (status != ERROR_SUCCESS) {
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
		debug_msg("Unable to open registry: %s\n", buffer);
		abort();
	}
	if (disp == REG_CREATED_NEW_KEY) {
		debug_msg("Created new registry entry...\n");
	} else {
		debug_msg("Opened existing registry entry...\n");
	}
}

static void close_registry(void)
{
	LONG status;
	char buffer[SETTINGS_BUF_SIZE];
	
	status = RegCloseKey(cfgKey);
	if (status != ERROR_SUCCESS) {
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
		debug_msg("Unable to close registry: %s\n", buffer);
		abort();
	}
	debug_msg("Closed registry entry...\n");
}
#endif

static void init_part_two(void)
{
#ifdef WIN32
        HKEY			key    = HKEY_CURRENT_USER;
	LPCTSTR			subKey = "Software\\Mbone Applications\\rat";
	DWORD			disp;
	char			buffer[SETTINGS_BUF_SIZE];
	LONG			status;

        close_registry();
	status = RegCreateKeyEx(key, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &cfgKey, &disp);
	if (status != ERROR_SUCCESS) {
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
		debug_msg("Unable to open registry: %s\n", buffer);
		abort();
	}
	if (disp == REG_CREATED_NEW_KEY) {
		debug_msg("Created new registry entry...\n");
	} else {
		debug_msg("Opened existing registry entry...\n");
	}
#endif
}


static void load_init(void)
{
#ifndef WIN32
        FILE            *sfile;
	struct passwd	*p;	
	char		*filen;
        char            *buffer;
        char            *key, *value;

	/* The getpwuid() stuff is to determine the users home directory, into which we */
	/* write the settings file. The struct returned by getpwuid() is statically     */
	/* allocated, so it's not necessary to free it afterwards.                      */
	p = getpwuid(getuid());
	if (p == NULL) {
		perror("Unable to get passwd entry");
		abort();
	}

        settings_table_create();

	filen = (char *) xmalloc(strlen(p->pw_dir) + 15);
	sprintf(filen, "%s/.RTPdefaults", p->pw_dir);
	sfile = fopen(filen, "r");
        xfree(filen);

        if (sfile == NULL) {
                debug_msg("No file to open\n");
                return;
        }

        buffer = xmalloc(SETTINGS_READ_SIZE+1);
        buffer[100] = '\0';

        while(fgets(buffer, SETTINGS_READ_SIZE, sfile) != NULL) {
                if (buffer[0] != '*') {
                        debug_msg("Garbage ignored: %s\n", buffer);
                        continue;
                }
                key   = strtok(buffer, ":"); 
                assert(key != NULL);
                key = key + 1;               /* skip asterisk */
                value = strtok(NULL, "\n");
                assert(value != NULL);
                while (*value != '\0' && isspace((int) *value)) {
                        value++;             /* skip leading whitespace */
                }
                settings_table_add(key, value);
        }
        fclose(sfile);
        xfree(buffer);

#else
        open_registry();
#endif
}

static void load_done(void)
{
#ifdef WIN32
        close_registry();
#else
        settings_table_destroy();
#endif
}

static int 
setting_load(char *key, char **value)
{
#ifndef WIN32
        return settings_table_lookup(key, value);
#endif
}

static char *
setting_load_str(char *name, char *default_value)
{
#ifndef WIN32
        char *value;
        if (setting_load(name, &value)) {
                return value;
        }
        return default_value;
#else
        LONG status;
        char buffer[SETTINGS_BUF_SIZE];
        DWORD ValueType;
        int val_len;
        char *value;

        ValueType = REG_SZ;
        /* call RegQueryValueEx once first to get size of string */
        status = RegQueryValueEx(cfgKey, name, NULL, &ValueType, NULL, &val_len);
        if (status != ERROR_SUCCESS) {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
                debug_msg("Unable to load setting: %s\n", buffer);
                return default_value;
        }	
        /* now that we know size we can allocate memory and call RegQueryValueEx again */
        value = (char*)xmalloc(val_len * sizeof(char));
        status = RegQueryValueEx(cfgKey, name, NULL, &ValueType, value, &val_len);
        if (status != ERROR_SUCCESS) {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
                debug_msg("Unable to load setting %s: %s\n", name, buffer);
                return default_value;
        }	
        return value;
#endif
}

static int 
setting_load_int(char *name, int default_value)
{
#ifndef WIN32
        char *value;

        if (setting_load(name, &value)) {
                return atoi(value);
        }
        return default_value;
#else
        LONG status;
        char buffer[SETTINGS_BUF_SIZE];
        DWORD ValueType;
        int value, val_len;

        ValueType = REG_DWORD;
        val_len = sizeof(int);
        status = RegQueryValueEx(cfgKey, name, NULL, &ValueType, &(char)value, &val_len);
        if (status != ERROR_SUCCESS) {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
                debug_msg("Unable to load setting %s: %s\n", name, buffer);
                return default_value;
        }	
        return value;
#endif
}

void settings_load(session_struct *sp)
{
	audio_device_details_t	 ad;
	char			*ad_name, *primary_codec, *cc_name, *port;
	int			 i, freq, chan, gain;
	cc_details		 ccd;
        const audio_port_details_t 	*apd;

	load_init();
	sp->db->my_dbe->sentry->name  = xstrdup(setting_load_str("rtpName", "Unknown"));/* We don't use rtcp_set_attribute() */ 
	sp->db->my_dbe->sentry->email = xstrdup(setting_load_str("rtpEmail", ""));      /* here, since that updates the UI   */
	sp->db->my_dbe->sentry->phone = xstrdup(setting_load_str("rtpPhone", ""));       /* and we don't want to do that yet. */
	sp->db->my_dbe->sentry->loc   = xstrdup(setting_load_str("rtpLoc", ""));
        init_part_two();
        sp->db->my_dbe->sentry->tool  = xstrdup(setting_load_str("audioTool", RAT_VERSION));

	ad_name = setting_load_str("audioDevice", "No Audio Device");
        for(i = 0; i < audio_get_device_count(); i++) {
                if (audio_get_device_details(i, &ad) && (strcmp(ad.name, ad_name) == 0)) {
			audio_device_register_change_device(sp, ad.descriptor);
                        break;
                }
        }

	freq = setting_load_int("audioFrequency", 8000);
	chan = setting_load_int("audioChannelsIn", 1);
	primary_codec = setting_load_str("audioPrimary", "GSM");

        audio_device_register_change_primary(sp, codec_get_matching(primary_codec, (u_int16)freq, (u_int16)chan));
        audio_device_reconfigure(sp);

        port = setting_load_str("audioOutputPort", "Headphone");
        for(i = 0; i < audio_get_oport_count(sp->audio_device); i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                if (!strcasecmp(port, apd->name)) {
                        break;
                }
        }
        audio_set_oport(sp->audio_device, apd->port);
        
        port = setting_load_str("audioInputPort", "Microphone");
        for(i = 0; i < audio_get_iport_count(sp->audio_device); i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                if (!strcasecmp(port, apd->name)) {
                        break;
                }
        }
        audio_set_iport(sp->audio_device, apd->port);

        gain = setting_load_int("audioOutputGain", 75);
        audio_set_ogain(sp->audio_device, gain);

        gain = setting_load_int("audioInputGain",  75);
        audio_set_igain(sp->audio_device, gain);

        tx_igain_update(sp->tb);

	cc_name = setting_load_str("audioChannelCoding", "None");
	for (i = 0; i < channel_get_coder_count(); i++ ) {
		channel_get_coder_details(i, &ccd);
		if (strcmp(ccd.name, cc_name) == 0) {
        		channel_encoder_create(ccd.descriptor, &sp->channel_coder);
			break;
		}
	}

	channel_encoder_set_parameters(sp->channel_coder, setting_load_str("audioChannelParameters", "None"));
	channel_encoder_set_units_per_packet(sp->channel_coder, (u_int16) setting_load_int("audioUnits", 2));

        setting_load_str("audioRepair", "Pattern-Match");
        setting_load_str("audioAutoConvert", "High Quality");
	sp->limit_playout  = setting_load_int("audioLimitPlayout", 0);
	sp->min_playout    = setting_load_int("audioMinPlayout", 0);
	sp->max_playout    = setting_load_int("audioMaxPlayout", 2000);
	sp->lecture        = setting_load_int("audioLecture", 0);
	sp->render_3d      = setting_load_int("audio3dRendering", 0);
	sp->detect_silence = setting_load_int("audioSilence", 1);
	sp->agc_on         = setting_load_int("audioAGC", 0);
	sp->loopback_gain  = setting_load_int("audioLoopback", 0);
	sp->echo_suppress  = setting_load_int("audioEchoSuppress", 0);
	sp->meter          = setting_load_int("audioPowermeters", 1);
	sp->sync_on        = setting_load_int("audioLipSync", 0);
        setting_load_int("audioOutputMute", 1);
        setting_load_int("audioInputMute", 1);
        xmemchk();
	load_done();
}

static void 
save_init(void)
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
        xfree(filen);
#else
        open_registry();
#endif
}

static void save_done(void)
{
#ifndef WIN32
	fclose(settings_file);
        settings_file = NULL;
#else
        close_registry();
#endif
}

static void 
setting_save_str(const char *name, const char *val)
{
#ifndef WIN32
	fprintf(settings_file, "*%s: %s\n", name, val);
#else
        LONG status;
        char buffer[SETTINGS_BUF_SIZE];

        status = RegSetValueEx(cfgKey, name, 0, REG_SZ, val, strlen(val) + 1);
        if (status != ERROR_SUCCESS) {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
                debug_msg("Unable to save setting %s: %s\n", name, buffer);
                abort();
        }	
#endif
}

static void setting_save_int(const char *name, const long val)
{
#ifndef WIN32
	fprintf(settings_file, "*%s: %ld\n", name, val);
#else
        LONG status;
        char buffer[SETTINGS_BUF_SIZE];

        status = RegSetValueEx(cfgKey, name, 0, REG_DWORD, &(char)val, sizeof(val));
        if (status != ERROR_SUCCESS) {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, buffer, SETTINGS_BUF_SIZE, NULL);
                debug_msg("Unable to save setting %s: %s\n", name, buffer);
                abort();
        }	
#endif
}

void settings_save(session_struct *sp)
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
	setting_save_str("rtpName",                sp->db->my_dbe->sentry->name);
	setting_save_str("rtpEmail",               sp->db->my_dbe->sentry->email);
	setting_save_str("rtpPhone",               sp->db->my_dbe->sentry->phone);
	setting_save_str("rtpLoc",                 sp->db->my_dbe->sentry->loc);
        init_part_two();
        setting_save_str("audioTool",              sp->db->my_dbe->sentry->tool);
	setting_save_str("audioDevice",            ad.name);
	setting_save_int("audioFrequency",         af->sample_rate);
	setting_save_int("audioChannelsIn",        af->channels); 
	setting_save_str("audioPrimary",           pri_cf->short_name);
	setting_save_int("audioUnits",             channel_encoder_get_units_per_packet(sp->channel_coder)); 
	setting_save_str("audioChannelCoding",     cd.name);
	setting_save_str("audioChannelParameters", cc_param);
	setting_save_str("audioRepair",            repair_get_name((u_int16)sp->repair));
	setting_save_str("audioAutoConvert",       converter.name);
	setting_save_int("audioLimitPlayout",      sp->limit_playout);
	setting_save_int("audioMinPlayout",        sp->min_playout);
	setting_save_int("audioMaxPlayout",        sp->max_playout);
	setting_save_int("audioLecture",           sp->lecture);
	setting_save_int("audio3dRendering",       sp->render_3d);
	setting_save_int("audioSilence",           sp->detect_silence);
	setting_save_int("audioAGC",               sp->agc_on);
	setting_save_int("audioLoopback",          sp->loopback_gain); 
	setting_save_int("audioEchoSuppress",      sp->echo_suppress);
	setting_save_int("audioOutputGain",        audio_get_ogain(sp->audio_device));
	setting_save_int("audioInputGain",         audio_get_igain(sp->audio_device));
	setting_save_str("audioOutputPort",        oapd->name);
	setting_save_str("audioInputPort",         iapd->name); 
	setting_save_int("audioPowermeters",       sp->meter);
	setting_save_int("audioLipSync",           sp->sync_on);
	/* We do not save audioOutputMute and audioInputMute by default, but should */
	/* recognize them when reloading.                                           */
	save_done();
        xfree(cc_param);
}

