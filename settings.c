/*
 * FILE:    settings.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 *
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 */
 
#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

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
        uint32_t hash;
        char *key;
        char *value;
        struct s_hash_tuple *next;
} hash_tuple;

#define SETTINGS_READ_SIZE 100
#define SETTINGS_TABLE_SIZE 11

#ifdef WIN32
#define SETTINGS_BUF_SIZE 1500
static HKEY cfgKey;
#endif

#ifndef WIN32
static hash_tuple **table;          /* Use hashtable to load settings    */
#endif

/* SETTINGS HASH CODE ********************************************************/

static uint32_t 
setting_hash(char *key)
{
#ifndef WIN32
        uint32_t hash = 0;

        while(*key != '\0') {
                hash = hash * 31;
                hash += ((uint32_t)*key) + 1;
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
        t->next  = table[row];
        table[row] = t;
#endif
}

static void
settings_table_remove(char *key)
{
#ifndef WIN32
        hash_tuple **t, *e;
        uint32_t hash;
        int row;

        hash = setting_hash(key);
        row  = hash % SETTINGS_TABLE_SIZE;
        t    = &table[row];
        while((*t) != NULL) {
                if (((*t)->hash == hash) && 
                    (strcmp(key, (*t)->key) == 0)) {
                        e = *t;
                        *t = e->next;
                        xfree(e->key);
                        xfree(e->value);
                        xfree(e);
                } else {
                        t = &(*t)->next;
                }
        }
#endif /* WIN32 */
}

static char* settings_table_first_key()
{
#ifndef WIN32
        int i;

        for (i = 0; i < SETTINGS_TABLE_SIZE; i++) {
                if (table[i] != NULL) {
                        return table[i]->key;
                }
        }
#endif
        return NULL;
}


/* settings_table_lookup points value at actual value */
/* and return TRUE if key found.                      */
static int
settings_table_lookup(char *key, char **value)
{
#ifndef WIN32
        hash_tuple *t;
        int          row;
        uint32_t     hash;

        hash = setting_hash(key);
        row  = hash % SETTINGS_TABLE_SIZE;

        t = table[row];
        while(t != NULL) {
                if (t->hash == hash && strcmp(key, t->key) == 0) {
                        *value = t->value;
                        return TRUE;
                }
                t = t->next;
        }
        *value = NULL;
#endif
        return FALSE;
}

static void
settings_table_create()
{
#ifndef WIN32
        table = (hash_tuple**)xmalloc(sizeof(hash_tuple*) * SETTINGS_TABLE_SIZE);
        memset(table, 0, sizeof(hash_tuple*) * SETTINGS_TABLE_SIZE);
#endif
}

static void
settings_table_destroy(void)
{
#ifndef WIN32
        hash_tuple *t;
        int i;

        for(i = SETTINGS_TABLE_SIZE-1; i >= 0; i--) {
                t = table[i];
                while (t != NULL) {
                        table[i] = t->next;
                        xfree(t->key);
                        xfree(t->value);
                        xfree(t);
                        t = table[i];
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
#define SETTINGS_FILE_TMP 2

static char *settings_file_name(uint32_t type)
{
        const char  *fmt[] = {"%s/.RTPdefaults", "%s/.RATdefaults", "%s/.RATtmp"};
        struct passwd	*p;	
        char *filen;

        if (type < sizeof(fmt)/sizeof(fmt[0])) {
                p = getpwuid(getuid());
                if (p == NULL) {
                        perror("Unable to get passwd entry");
                        return NULL;
                }
                filen = (char *)xmalloc(strlen(p->pw_dir) + strlen(fmt[type]) + 1);
                sprintf(filen, fmt[type], p->pw_dir);
                return filen;
        }
        return NULL;
}

static FILE *
settings_file_open(uint32_t type, char *mode)
{
        FILE *sfile;
        char *filen;

        if ((filen = settings_file_name(type)) != NULL) {
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
        uint32_t          i;
        settings_table_create();

        i = 0;
        for (i = 0; i < 2; i++) {
                if ((sfile = settings_file_open(i, "r")) != NULL) {
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
                }
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
	char				*name, *param, *primary_codec, *port;
	int				 freq, chan;
        uint32_t                         i, n, success;
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

        cid  = codec_get_matching(primary_codec, (uint16_t)freq, (uint16_t)chan);
        if (codec_id_is_valid(cid) == FALSE) {
                /* Codec name is garbage...should only happen on upgrades */
                cid = codec_get_matching("GSM", (uint16_t)freq, (uint16_t)chan);
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
        setting_load_int("audioInputMute", 1);
        setting_load_int("audioOutputMute", 1);

	name  = setting_load_str("audioChannelCoding", "None");
        param = setting_load_str("audioChannelParameters", "None");

        do {
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
                success = channel_encoder_set_parameters(sp->channel_coder, param);
                if (success == 0) {
                        /* Could not set parameters for channel coder, fall back to "None" */
                        name = "None";
                        param = "";
                }
        } while (success == 0);

	channel_encoder_set_units_per_packet(sp->channel_coder, (uint16_t) setting_load_int("audioUnits", 1));

        /* Set default repair to be first available */
        r          = repair_get_details(0);
        sp->repair = r->id;
        name       = setting_load_str("audioRepair", "Pattern-Match");
        n          = (int)repair_get_count();
        for(i = 0; i < n; i++) {
                r = repair_get_details((uint16_t)i);
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
        audio_loopback(sp->audio_device, sp->loopback_gain);
	sp->echo_suppress  = setting_load_int("audioEchoSuppress", 0);
	sp->meter          = setting_load_int("audioPowermeters", 1);
/* Ignore saved render_3d setting.  Break initial device config stuff.  V.fiddly to fix. */
/*	sp->render_3d      = setting_load_int("audio3dRendering", 0);                    */
        xmemchk();
	load_done();
}

void settings_load_late(session_t *sp)
{
        uint32_t my_ssrc;
        struct   utsname u;
        char     hostfmt[] = RAT_VERSION " %s %s (%s)";
        char    *field;
	load_init();		/* Initial settings come from the common prefs file... */

        /*
         * We check to see it SDES items are set first.  If they are
         * then presumeably it has come from the command line and
         * so it should override saved settings.
         */
        my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
	field = setting_load_str("rtpName", "Unknown");
        if (rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NAME) == NULL) {
                rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NAME,  field, strlen(field));
        }
	field = setting_load_str("rtpEmail", "");
        if (rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_EMAIL) == NULL) {
                rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_EMAIL, field, strlen(field));
        }
	field = setting_load_str("rtpPhone", "");
        if (rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_PHONE) == NULL) {
                rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_PHONE, field, strlen(field));
        }
	field = setting_load_str("rtpLoc", "");
        if (rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_LOC) == NULL) {
                rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_LOC,   field, strlen(field));
        }
	field = setting_load_str("rtpNote", "");
        if (rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NOTE) == NULL) {
                rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NOTE,   field, strlen(field));
        }

        field = (char*)xmalloc(3 * SYS_NMLN + sizeof(hostfmt));
        uname(&u);
        sprintf(field, hostfmt, u.sysname, u.release, u.machine);
	rtp_set_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_TOOL,  field, strlen(field));

        xfree(field);

        init_part_two();	/* Switch to pulling settings from the RAT specific prefs file... */
	load_done();
}

static void 
save_init_rtp(void)
{
#ifndef WIN32
        settings_table_create();
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
        settings_table_create();
#else
        open_registry("Software\\Mbone Applications\\rat");
#endif
}

#ifndef WIN32
static void
cr_terminate(char *s, int slen)
{
        int i;
        for (i = 0; i < slen; i++) {
                if (s[i] == '\0' || s[i] == '\n') break;
        }
        
        if (s[i] == '\n') {
                return;
        } else if (i + 1 >= slen) {
                i = i - 1;
        }
        s[i] = '\n';
        s[i + 1] = '\0'; 
}
#endif /* WIN32 */

static void save_done(uint32_t type) 
{
#ifndef WIN32
        /* settings table has entries we want to write.  Settings file
         * may contain comments and info for other apps, therefore read
         * file one line at a time, see if write update line if we have one
         * and output to a new file.  Then copy from one to the other.
         */
        char linebuf[255], keybuf[255], *key, *value, *tmpname;
        FILE *n = settings_file_open(SETTINGS_FILE_TMP, "w");
        FILE *o = settings_file_open(type, "r");

        if (n == NULL) {
                debug_msg("Could not open temporary settings file\n");
                goto save_stops_here;
        }

        if (o != NULL) {
                while (fgets(linebuf, sizeof(linebuf)/sizeof(linebuf[0]), o) != NULL) {
                        debug_msg("Read: %s\n", linebuf);
                        cr_terminate(linebuf, sizeof(linebuf)/sizeof(linebuf[0]));
                        if (linebuf[0] != '*') {
                                fprintf(n, linebuf);
                        } else {
                                strcpy(keybuf, linebuf);
                                key = keybuf + 1;       /* Ignore asterisk */
                                key = strtok(key, ":"); /* key ends at colon */
                                if (settings_table_lookup(key, &value)) {
                                        /* We have a newer value */
                                        debug_msg("Newer value for key %s\n", key);
                                        fprintf(n, "*%s: %s\n", key, value);
                                        settings_table_remove(key);
                                } else {
                                        /* We have no ideas about this value */
                                        fprintf(n, linebuf);
                                }
                        }
                }
        }

        if ((key = settings_table_first_key()) != NULL) {
                do {
                        /* Write out stuff not written out already */
                        if (settings_table_lookup(key, &value)) {
                                debug_msg("Writing unreplaced: %s %s\n", key, value);
                                fprintf(n, "*%s: %s\n", key, value);
                        }
                        settings_table_remove(key);
                } while ((key = settings_table_first_key()) != NULL);
        }

        settings_file_close(n);
        if (o) {
                settings_file_close(o);
        }
        o = settings_file_open(type, "w+");
        n = settings_file_open(SETTINGS_FILE_TMP, "r");
        if (o && n) {
                debug_msg("Opened settings file\n");
                while(feof(n) == 0) {
                        if (fgets(linebuf, sizeof(linebuf)/sizeof(linebuf[0]), n)) {
                                char *x;
                                x = linebuf;
                                /* Don't use fprintf because user 
                                 * may have format modifiers in values (ugh)
                                 */
                                while (*x != '\0') {
                                        fwrite(x, 1, 1, o);
                                        x++;
                                }
                        }
                }
        } else {
                debug_msg("Failed to open settings file\n");
        }
        fflush(n);

save_stops_here:
        if (o) {
                settings_file_close(o);
        }
        if (n) {
                settings_file_close(n);
        }
        settings_table_destroy();

        tmpname = settings_file_name(SETTINGS_FILE_TMP);
        unlink(tmpname);
        xfree(tmpname);
#endif
        UNUSED(type);
}

static void save_done_rtp(void)
{
#ifndef WIN32
        save_done(SETTINGS_FILE_RTP);
#else
        close_registry();
#endif
}

static void save_done_rat(void)
{
#ifndef WIN32
        save_done(SETTINGS_FILE_RAT);
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
        settings_table_add((char*)name, (char*)val);
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
        char sval[12];
	sprintf(sval, "%ld", val);
        settings_table_add((char*)name, sval);
        xmemchk();
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
        uint16_t                          j,n;
        uint32_t                          my_ssrc;

	pri_id   = codec_get_by_payload(sp->encodings[0]);
        pri_cf   = codec_get_format(pri_id);
        cc_len   = 3 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cc_param = (char*) xmalloc(cc_len);
        channel_encoder_get_parameters(sp->channel_coder, cc_param, cc_len);
        ccd = channel_get_coder_identity(sp->channel_coder);

        n = (uint16_t)converter_get_count();
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
        setting_save_str("rtpNote",  rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_NOTE));
        save_done_rtp();
        
        save_init_rat();
        setting_save_str("audioTool", rtp_get_sdes(sp->rtp_session[0], my_ssrc, RTCP_SDES_TOOL));
	setting_save_str("audioDevice",     add->name);
	setting_save_int("audioFrequency",  af->sample_rate);
	setting_save_int("audioChannelsIn", af->channels); 
	
	/* If we save a dynamically mapped codec we crash when we reload on startup */
	if (pri_cf->default_pt != CODEC_PAYLOAD_DYNAMIC) {
                setting_save_str("audioPrimary", pri_cf->short_name);
	}

	setting_save_int("audioUnits", channel_encoder_get_units_per_packet(sp->channel_coder));
	/* Don't save the layered channel coder - you need to start it */
	/* from the command line anyway                                */
	if (strcmp(ccd->name, "Layering") == 0) {
		setting_save_str("audioChannelCoding", "Vanilla");
	} else {
                setting_save_str("audioChannelCoding", ccd->name);
        }
        setting_save_str("audioChannelParameters", cc_param);
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
	/* We do not save audioOutputMute and audioInputMute by default, but should */
	/* recognize them when reloading.                                           */
	save_done_rat();
        xfree(cc_param);
}

