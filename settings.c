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
#include "timers.h"
#include "session.h"
#include "repair.h"
#include "transmit.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "auddev.h"
#include "version.h"
#include "settings.h"
#include "converter.h"
#include "rtp.h"
#include "util.h"

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
static FILE       *settings_file;  /* static file pointer used during save */
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

/* settings_table_lookup points value at actual value */
/* and return TRUE if key found.                      */
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
static void open_registry(LPCTSTR subKey)
{
    HKEY			key    = HKEY_CURRENT_USER;
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
        close_registry();
        open_registry("Software\\Mbone Applications\\rat");
#endif
}

#ifndef WIN32

#define SETTINGS_FILE_RTP 0
#define SETTINGS_FILE_RAT 1

static FILE *
settings_file_open(u_int32 type, char *mode)
{
        char *fmt[] = {"%s/.RTPdefaults", "%s/.RATdefaults"};        
	char *filen;
        FILE *sfile;
        struct passwd	*p;	

        if (type < sizeof(fmt)/sizeof(fmt[0])) {
                p = getpwuid(getuid());
                if (p == NULL) {
                        perror("Unable to get passwd entry");
                        return NULL;
                }
                filen = (char *) xmalloc(strlen(p->pw_dir) + strlen(fmt[type]) + 1);
                sprintf(filen, fmt[type], p->pw_dir);
                sfile = fopen(filen, mode);
                xfree(filen);
                return sfile;
        }
        return NULL;
}

static void
settings_file_close(FILE *sfile)
{
        fclose(sfile);
}

#endif /* WIN32 */

static void load_init(void)
{
#ifndef WIN32
        FILE            *sfile;
        char            *buffer;
        char            *key, *value;
        u_int32          i;
        settings_table_create();

	/* The getpwuid() stuff is to determine the users home directory, into which we */
	/* write the settings file. The struct returned by getpwuid() is statically     */
	/* allocated, so it's not necessary to free it afterwards.                      */

        i = 0;
        while ((sfile = settings_file_open(i, "r")) != NULL) {
                buffer = xmalloc(SETTINGS_READ_SIZE+1);
                buffer[100] = '\0';
                while(fgets(buffer, SETTINGS_READ_SIZE, sfile) != NULL) {
                        if (buffer[0] != '*') {
                                debug_msg("Garbage ignored: %s\n", buffer);
                                continue;
                        }
                        key   = (char *) strtok(buffer, ":"); 
                        if (key == NULL) {
                                continue;
                        }
                        key = key + 1;               /* skip asterisk */
                        value = (char *) strtok(NULL, "\n");
                        if (value == NULL) {
                                continue;
                        }
                        while (*value != '\0' && isascii((int)*value) && isspace((int)*value)) {
                                /* skip leading spaces, and stop skipping if
                                 * not ascii*/
                                value++;             
                        }
                        settings_table_add(key, value);
                }
                settings_file_close(sfile);
                xfree(buffer);
                i++;
        }
#else
        open_registry("Software\\Mbone Applications\\common");
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

void settings_load_early(session_t *sp)
{
	char				*name, *primary_codec, *port;
	int				 freq, chan;
        u_int32                          i, n;
	const cc_details_t              *ccd;
	const audio_device_details_t    *add = NULL;
        const audio_port_details_t 	*apd = NULL;
        const converter_details_t       *cod = NULL;
        const repair_details_t          *r   = NULL;
        codec_id_t                       cid;

	load_init();		/* Initial settings come from the common prefs file... */
        init_part_two();	/* Switch to pulling settings from the RAT specific prefs file... */

	name = setting_load_str("audioDevice", "No Audio Device");
        n = (int)audio_get_device_count();
        for(i = 0; i < n; i++) {
                add = audio_get_device_details(i);
                if (strcmp(add->name, name) == 0) {
			audio_device_register_change_device(sp, add->descriptor);
                        break;
                }
        }

	freq = setting_load_int("audioFrequency", 8000);
	chan = setting_load_int("audioChannelsIn", 1);
	primary_codec = setting_load_str("audioPrimary", "GSM");

        cid  = codec_get_matching(primary_codec, (u_int16)freq, (u_int16)chan);
        if (codec_id_is_valid(cid) == FALSE) {
                /* Codec name is garbage...should only happen on upgrades */
                cid = codec_get_matching("GSM", (u_int16)freq, (u_int16)chan);
        }

        audio_device_register_change_primary(sp, cid);
        audio_device_reconfigure(sp);

        port = setting_load_str("audioOutputPort", "Headphone");
        n    = audio_get_oport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                if (!strcasecmp(port, apd->name)) {
                        break;
                }
        }
        audio_set_oport(sp->audio_device, apd->port);
        
        port = setting_load_str("audioInputPort", "Microphone");
        n    = audio_get_iport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                if (!strcasecmp(port, apd->name)) {
                        break;
                }
        }
        audio_set_iport(sp->audio_device, apd->port);

        audio_set_ogain(sp->audio_device, setting_load_int("audioOutputGain", 75));
        audio_set_igain(sp->audio_device, setting_load_int("audioInputGain",  75));
        tx_igain_update(sp->tb);

	name = setting_load_str("audioChannelCoding", "None");
        n    = channel_get_coder_count();
	for (i = 0; i < n; i++ ) {
		ccd = channel_get_coder_details(i);
		if (strcmp(ccd->name, name) == 0) {
                        if (sp->channel_coder) {
                                channel_encoder_destroy(&sp->channel_coder);
                        }
        		channel_encoder_create(ccd->descriptor, &sp->channel_coder);
			break;
		}
	}

        setting_load_int("audioInputMute", 1);
        setting_load_int("audioOutputMute", 1);

	channel_encoder_set_parameters(sp->channel_coder, setting_load_str("audioChannelParameters", "None"));
	channel_encoder_set_units_per_packet(sp->channel_coder, (u_int16) setting_load_int("audioUnits", 1));

        /* Set default repair to be first available */
        r          = repair_get_details(0);
        sp->repair = r->id;
        name       = setting_load_str("audioRepair", "Pattern-Match");
        n          = (int)repair_get_count();
        for(i = 0; i < n; i++) {
                r = repair_get_details((u_int16)i);
                if (strcasecmp(r->name, name) == 0) {
                        sp->repair = r->id;
                        break;
                }
        }

        /* Set default converter to be first available */
        cod           = converter_get_details(0);
        sp->converter = cod->id;
        name          = setting_load_str("audioAutoConvert", "High Quality");
        n             = (int)converter_get_count();
        /* If converter setting name matches then override existing choice */
        for(i = 0; i < n; i++) {
                cod = converter_get_details(i);
                if (strcasecmp(cod->name, name) == 0) {
                        sp->converter = cod->id;
                        break;
                }
        }
        
	sp->limit_playout  = setting_load_int("audioLimitPlayout", 0);
	sp->min_playout    = setting_load_int("audioMinPlayout", 0);
	sp->max_playout    = setting_load_int("audioMaxPlayout", 2000);
	sp->lecture        = setting_load_int("audioLecture", 0);
	sp->detect_silence = setting_load_int("audioSilence", 1);
	sp->agc_on         = setting_load_int("audioAGC", 0);
	sp->loopback_gain  = setting_load_int("audioLoopback", 0);
	sp->echo_suppress  = setting_load_int("audioEchoSuppress", 0);
	sp->meter          = setting_load_int("audioPowermeters", 1);
	sp->sync_on        = setting_load_int("audioLipSync", 0);
/* Ignore saved render_3d setting.  Break initial device config stuff.  V.fiddly to fix. */
/*	sp->render_3d      = setting_load_int("audio3dRendering", 0);                    */
        xmemchk();
	load_done();
}

void settings_load_late(session_t *sp)
{
        u_int32 my_ssrc;
        char   *field;
	load_init();		/* Initial settings come from the common prefs file... */

        my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
	field = setting_load_str("rtpName", "Unknown");
        rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NAME,  field, strlen(field));
	field = setting_load_str("rtpEmail", "");
        rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_EMAIL, field, strlen(field));
	field = setting_load_str("rtpPhone", "");
        rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_PHONE, field, strlen(field));
	field = setting_load_str("rtpLoc", "");
        rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_LOC,   field, strlen(field));
#ifdef WIN32
	field = (char *) w32_make_version_info(RAT_VERSION);
#else
	field = RAT_VERSION;
#endif
	rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_TOOL,  field, strlen(field));
        init_part_two();	/* Switch to pulling settings from the RAT specific prefs file... */
	load_done();
}

static void 
save_init_rtp(void)
{
#ifndef WIN32
        settings_file = settings_file_open(SETTINGS_FILE_RTP, "w");
#else
        open_registry("Software\\Mbone Applications\\common");
#endif
}

static void
save_init_rat(void)
{
/* We assume this function gets called after save_init_rtp so */
/* file/registry need closing before use.                     */
#ifndef WIN32
        settings_file = settings_file_open(SETTINGS_FILE_RAT, "w");
#else
        open_registry("Software\\Mbone Applications\\rat");
#endif
}

static void save_done(void)
{
#ifndef WIN32
	settings_file_close(settings_file);
#else
        close_registry();
#endif
}

static void 
setting_save_str(const char *name, const char *val)
{
        
#ifndef WIN32
        if (val == NULL) {
                val = "";
        }
	fprintf(settings_file, "*%s: %s\n", name, val);
#else
        int status;
        char buffer[SETTINGS_BUF_SIZE];

        if (val == NULL) {
                val = "";
        }

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

void settings_save(session_t *sp)
{
        const codec_format_t 		*pri_cf;
        const audio_port_details_t      *iapd      = NULL;
        const audio_port_details_t      *oapd      = NULL;
        const audio_format 		*af        = NULL;
        const repair_details_t          *repair    = NULL;
        const converter_details_t       *converter = NULL;
	const audio_device_details_t    *add       = NULL;
        const cc_details_t 		*ccd       = NULL;
	codec_id_t	 		 pri_id;
   
	int				 cc_len;
	char				*cc_param;
	int		 		 i;
        u_int16                          j,n;
        u_int32                          my_ssrc;

	pri_id   = codec_get_by_payload(sp->encodings[0]);
        pri_cf   = codec_get_format(pri_id);
        cc_len   = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cc_param = (char*) xmalloc(cc_len);
        channel_encoder_get_parameters(sp->channel_coder, cc_param, cc_len);
        ccd = channel_get_coder_identity(sp->channel_coder);

        n = (u_int16)converter_get_count();
        for (j = 0; j < n; j++) {
                converter = converter_get_details(j);
                if (sp->converter == converter->id) {
			break;
                }
        }
        
        n = repair_get_count();
        for (j = 0; j < n; j++) {
                repair = repair_get_details(j);
                if (sp->repair == repair->id) {
                        break;
                }
        }

        n = (int)audio_get_device_count();
        for (i = 0; i < n; i++) {
                add = audio_get_device_details(i);
                if (sp->audio_device == add->descriptor) {
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

	save_init_rtp();
        my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
        setting_save_str("rtpName",  rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NAME));
        setting_save_str("rtpEmail", rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_EMAIL));
        setting_save_str("rtpPhone", rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_PHONE));
        setting_save_str("rtpLoc",   rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_LOC));
        save_done();
        
        save_init_rat();
        setting_save_str("audioTool", rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_TOOL));
	setting_save_str("audioDevice",     add->name);
	setting_save_int("audioFrequency",  af->sample_rate);
	setting_save_int("audioChannelsIn", af->channels); 
	
	/* If we save a dynamically mapped codec we crash when we reload on startup */
	if (pri_cf->default_pt != CODEC_PAYLOAD_DYNAMIC) {
                setting_save_str("audioPrimary",           pri_cf->short_name);
                /* If vanilla channel coder don't save audioChannelParameters - it's rubbish */
                if (strcmp(ccd->name, "Vanilla") == 0) {
                        setting_save_str("audioChannelParameters", cc_param);
                } else {
                        setting_save_str("audioChannelParameters", "None");
                }
	}

	setting_save_int("audioUnits", channel_encoder_get_units_per_packet(sp->channel_coder));
	/* Don't save the layered channel coder - you need to start it */
	/* from the command line anyway                                */
	if (strcmp(ccd->name, "Layering") == 0) {
		setting_save_str("audioChannelCoding", "Vanilla");
	} else {
                setting_save_str("audioChannelCoding", ccd->name);
        }
	setting_save_str("audioChannelCoding",     ccd->name);
	setting_save_str("audioRepair",            repair->name);
	setting_save_str("audioAutoConvert",       converter->name);
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

