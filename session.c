/*
 * FILE:    session.c 
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "session.h"
#include "timers.h"
#include "repair.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "channel.h"
#include "pckt_queue.h"
#include "convert.h"
#include "parameters.h"
#include "audio.h"
#include "ui.h"
#include "crypt.h"
#include "source.h"

#define PCKT_QUEUE_RTP_LEN  24
#define PCKT_QUEUE_RTCP_LEN 12

static void 
usage(void)
{
	printf("Usage: rat [options] -t <ttl> <addr>/<port>\n");
	exit(1);
}

void
init_session(session_struct *sp)
{
	struct hostent *addr;
	u_long                netaddr;
	char                  hostname[MAXHOSTNAMELEN + 1];
	codec_id_t            cid;
        const codec_format_t *cf;
        cc_details            ccd; 

	memset(sp, 0, sizeof(session_struct));

	codec_init();
        vu_table_init();

	cid = codec_get_by_name("DVI-8K-Mono");
        assert(cid);
        cf  = codec_get_format(cid);
        sp->encodings[0]		= codec_get_payload(cid);           	/* user chosen encoding for primary */
	sp->num_encodings		= 1;                                	/* Number of encodings applied */

        channel_get_coder_details(channel_get_null_coder(), &ccd);
        channel_encoder_create(ccd.descriptor, &sp->channel_coder);

        sp->converter                   = converter_get_null_converter();
	sp->clock			= new_fast_time(GLOBAL_CLOCK_FREQ); 	/* this is the global clock */
        assert(!(GLOBAL_CLOCK_FREQ%cf->format.sample_rate));                	/* just in case someone adds weird freq codecs */
	sp->mode         		= AUDIO_TOOL;	
	sp->rtp_port			= 5004;					/* default: draft-ietf-avt-profile-new-00 */
	sp->rtcp_port			= 5005;					/* default: draft-ietf-avt-profile-new-00 */
        sp->rtp_pckt_queue              = pckt_queue_create(PCKT_QUEUE_RTP_LEN);
        sp->rtcp_pckt_queue             = pckt_queue_create(PCKT_QUEUE_RTCP_LEN);
	sp->ttl				= 16;
	sp->rtp_socket			= NULL;
	sp->rtcp_socket			= NULL;
        sp->filter_loopback             = TRUE;
	sp->playing_audio		= TRUE;
	sp->lecture			= FALSE;
	sp->auto_lecture		= 0;
 	sp->receive_audit_required	= FALSE;
	sp->detect_silence		= TRUE;
	sp->sync_on			= FALSE;
	sp->agc_on			= FALSE;
        sp->ui_on                       = TRUE;
	sp->ui_addr			= NULL;
	sp->meter			= TRUE;					/* Powermeter operation */
        sp->drop                        = 0.0;
	sp->in_file 			= NULL;
	sp->out_file  			= NULL;
	sp->rtp_seq			= lrand48() & 0xffff;
	sp->speakers_active 		= NULL;
	sp->mbus_engine			= NULL;
	sp->mbus_ui			= NULL;
	sp->min_playout			= 0;
	sp->max_playout			= 1000;
	sp->wait_on_startup		= FALSE;
        sp->last_depart_ts              = 1;

        source_list_create(&sp->active_sources);

        strcpy(sp->title, "Untitled Session");
        
	if (gethostname(hostname, MAXHOSTNAMELEN + 1) != 0) {
		perror("Cannot get hostname");
		abort();
	}
	if ((addr = gethostbyname(hostname)) == NULL) {
		perror("Cannot get host address");
		abort();
	}
	memcpy(&netaddr, addr->h_addr, 4);
	sp->ipaddr = ntohl(netaddr);

	strcpy(sp->asc_address, "127.0.0.3");	/* Yeuch! This value should never be used! */
}

void
end_session(session_struct *sp)
{
        codec_exit();
        free_fast_time(sp->clock);
        if (sp->device_clock) {
                xfree(sp->device_clock);
                sp->device_clock = NULL;
        }
        pckt_queue_destroy(&sp->rtp_pckt_queue);
        pckt_queue_destroy(&sp->rtcp_pckt_queue);
        channel_encoder_destroy(&sp->channel_coder);
        source_list_destroy(&sp->active_sources);
}

static int 
parse_early_options_common(int argc, char *argv[], session_struct *sp[], int num_sessions)
{
	/* Parse command-line options common to all the modes of
         * operation.  Variables: i scans through the options, s scans
         * through the sessions This is for options set initially,
	 * before the main initialisation is done. For example, the UI
	 * is not yet setup, and anything initialised there will
	 * overwrite changes made here...  */

	int lasti, i, s, args_processed = 0;
        lasti = 0;
	for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-ui") == 0) && (argc > i+1)) {
                        for(s = 0; s < num_sessions; s++) {
                                sp[s]->ui_on   = FALSE;
                                sp[s]->ui_addr = (char *) strdup(argv[i+1]);
                        }
                } else if (strcmp(argv[i], "-allowloopback") == 0 || strcmp(argv[i], "-allow_loopback") == 0) {
                        for(s = 0; s < num_sessions; s++) {
                                sp[s]->filter_loopback = FALSE;
                        }
                } else 	if ((strcmp(argv[i], "-C") == 0) && (argc > i+1)) {
                        for(s = 0; s < num_sessions; s++) {
                                strncpy(sp[s]->title, argv[i+1], SESSION_TITLE_LEN);
                        }
                        i++;
                } else if (strcmp(argv[i], "-wait") == 0) {
                        for(s = 0; s < num_sessions; s++) {
                                sp[s]->wait_on_startup = TRUE;
                        }
                } else if ((strcmp(argv[i], "-t") == 0) && (argc > i+1)) {
                        int ttl = atoi(argv[i + 1]);
                        if (ttl > 255) {
                                fprintf(stderr, "ttl must be in the range 0 to 255.\n");
                                usage();
                        }
                        for(s = 0; s < num_sessions; s++) {
                                sp[s]->ttl = ttl;
                        }
                        i++;
                } else if ((strcmp(argv[i], "-p") == 0) && (argc > i+1)) {
                        if ((thread_pri = atoi(argv[i + 1])) > 3) {
                                usage();
                        }
                        i++;
                } else if ((strcmp(argv[i], "-drop") == 0) && (argc > i+1)) {
                        double drop;
                        drop = atof(argv[++i]);
                        if (drop > 1.0) {
                                drop = drop/100.0;
                        }
                        printf("Drop probability %.4f\n", drop);
                        for(s = 0; s < num_sessions; s++) {
                                sp[s]->drop = drop;
                        }
                } else if (strcmp(argv[i], "-seed") == 0) {
                        srand48(atoi(argv[++i]));
                } else if (strcmp(argv[i], "-codecs") == 0) {
                        const codec_format_t *cf;
                        codec_id_t            cid;
                        u_int32     n_codecs, idx;
                        u_char      pt;
                        n_codecs = codec_get_number_of_codecs();
                        printf("# <name> <rate> <channels> [<payload>]\n");
                        for(idx = 0; idx < n_codecs; idx++) {
                                cid = codec_get_codec_number(idx);
                                cf  = codec_get_format(cid);
                                printf("%s\t\t%5d %d", 
                                       cf->long_name,
                                       cf->format.sample_rate,
                                       cf->format.channels);
                                pt = codec_get_payload(cid);
                                if (pt<=127) {
                                        printf(" %3u\n", pt);
                                } else {
                                        printf("\n");
                                }
                        }
                        exit(0);
                } else if ((strcmp(argv[i], "-pt") == 0) && (argc > i+1)) {
                        /* Dynamic payload type mapping. Format: "-pt pt/codec/clock/channels" */
                        /* pt/codec must be specified. clock and channels are optional.        */
                        /* At present we only support "-pt .../redundancy"                     */
                        codec_id_t cid;
                        char *t;
                        int pt;
                        pt = atoi((char *) strtok(argv[i + 1], "/"));
                        if ((pt > 127) || (pt < 96)) {
                                printf("Dynamic payload types must be in the range 96-127.\n");
                                usage();
                        }
                        t = (char *) strtok(NULL, "/");
                        cid = codec_get_by_name(t);
                        if (cid) {
                                codec_map_payload(cid, (u_char)pt);
                                if (codec_get_payload(cid) != pt) {
                                        printf("Payload %d either in use or invalid.\n", pt);
                                }
                        } else {
                                printf("Codec %s not recognized, check name.\n", t);
                        }
                        i++;
                } else {
                        break;
                }
                args_processed += i - lasti;
                lasti = i;
        }
        debug_msg("Processed %d / %d args\n", args_processed, argc);
        return args_processed;
}

static void 
parse_early_options_audio_tool(int argc, char *argv[], session_struct *sp)
{
	/* Parse command-line options specific to the audio tool */
	char *p;

	p = (char *) strtok(argv[argc - 1], "/");
	strcpy(sp->asc_address, p);
	if ((p = (char *) strtok(NULL, "/")) != NULL) {
		sp->rtp_port = atoi(p);
		sp->rtp_port &= ~1;
		sp->rtcp_port = sp->rtp_port + 1;
	}
}

static void 
parse_early_options_transcoder(int argc, char *argv[], session_struct *sp[])
{
	/* Parse command-line options specific to the transcoder */
	int   i, j;
	char *p;

	if (argc < 4) {
		usage();
	}

	for (i = 0; i < 2; i++) {
		/* addr */
		p = (char *) strtok(argv[argc-i-1], "/");
		strcpy(sp[i]->asc_address, p);
		/* port */
		if ((p = (char *) strtok(NULL, "/")) != NULL) {
			sp[i]->rtp_port  = atoi(p);
			sp[i]->rtp_port &= ~1;
			sp[i]->rtcp_port = sp[i]->rtp_port + 1;
		} else {
			continue;
		}
		/* ttl */
		if ((p = (char *) strtok(NULL, "/")) != NULL) {
			sp[i]->ttl = atoi(p);
		} else {
			continue;
		}
		/* encoding */
		j = 0;
		while ((p = (char *) strtok(NULL, "/")) != NULL) {
			codec_id_t cid;
			char *pu;
			for (pu = p; *pu; pu++)
					*pu = toupper(*pu);
			if ((cid = codec_get_by_name(p)) == 0)
				usage();
			else {
				sp[i]->encodings[j]  = codec_get_payload(cid);
				sp[i]->num_encodings = ++j;
			}
		}
	}
}

int
parse_early_options(int argc, char *argv[], session_struct *sp[])
{
	int	i, num_sessions = 0;

	if (argc < 2) {
		usage();
	}
	/* Set the mode of operation, and number of valid sessions, based on the first command line option. */
	if (strcmp(argv[1], "-version") == 0) {
		printf("%s\n", RAT_VERSION);
		exit(0);
	} else if (strcmp(argv[1], "-T") == 0) {
		sp[0]->mode = TRANSCODER;
		sp[1]->mode = TRANSCODER;
		num_sessions= 2;
	} else {
		sp[0]->mode = AUDIO_TOOL;
		num_sessions= 1;
	}

        if (parse_early_options_common(argc, argv, sp, num_sessions) > argc - 2) {
                /* parse_early_options commmon returns number of args processed.
                 * At least two argv[0] (the appname) and argv[argc - 1] (address)
                 * should not be processed.  Other args may not ve processed, but
                 * these should be picked up by parse_late_* or the audiotool/transcoder
                 * specific bits.  Hopefully more people will RTFM.
                 */
                usage();
        }

	switch (sp[0]->mode) {
		case AUDIO_TOOL: parse_early_options_audio_tool(argc, argv, sp[0]);
				 break;
		case TRANSCODER: parse_early_options_transcoder(argc, argv, sp);
				 break;
		default        : abort();
	}

	for (i=0; i<num_sessions; i++) {
		if (sp[i]->rtp_port == 0) {
			usage();
		}
	}
	return num_sessions;
}

/************************************************************************************************************/

static void 
parse_late_options_common(int argc, char *argv[], session_struct *sp[], int sp_size)
{
	/* Parse command-line options common to all the modes of operation.     */
	/* Variables: i scans through the options, s scans through the sessions */
	/* This is the final chance to set any options, before the main program */
	/* starts. In particular, it is done after the UI has been configured.  */
	/* Remember: if anything here changes the state of the system, you must */
	/* update the UI too!                                                   */
	int i, s;

	for (i = 1; i < argc; i++) {
		for (s = 0; s < sp_size; s++) {
			if (strcmp(argv[i], "-K") == 0) {
				argv[i] = "-crypt";
			}
			if ((strcmp(argv[i], "-crypt") == 0) && (argc > i+1)) {
				Set_Key(argv[i+1]);
				ui_update_key(sp[s], argv[i+1]);
				i++;
			}
                        if (strcmp(argv[i], "-sync") == 0) {
                               	sp[s]->sync_on = TRUE;
                        }
			if ((strcmp(argv[i], "-agc") == 0) && (argc > i+1)) {
       				if (strcmp(argv[i+1], "on") == 0) {
       					sp[s]->agc_on = TRUE;
       					i++;
       				} else if (strcmp(argv[i+1], "off") == 0) {
       					sp[s]->agc_on = FALSE;
       					i++;
       				} else {
       					printf("Unrecognized -agc option.\n");
       				}
			}
			if ((strcmp(argv[i], "-silence") == 0) && (argc > i+1)) {
                                if (strcmp(argv[i+1], "on") == 0) {
                                        sp[s]->detect_silence = TRUE;
                                        i++;
                                } else if (strcmp(argv[i+1], "off") == 0) {
                                        sp[s]->detect_silence = FALSE;
                                        i++;
                                } else {
                                        printf("Unrecognized -silence option.\n");
                                }
                        }        
                        if ((strcmp(argv[i], "-repair") == 0) && (argc > i+1)) {
                                sp[s]->repair = repair_get_by_name(argv[i+1]);
                        }
                        if ((strcmp(argv[i], "-interleave") == 0) && (argc > i+1)) {
                                printf("%s: not supported in this release\n", 
                                       argv[i]);
                            	i++;
			}
                        if ((strcmp(argv[i], "-redundancy") == 0) && (argc > i+1)) {
                                printf("%s: not supported in this release\n", 
                                       argv[i]);
                                i++;
                        }
			if ((strcmp(argv[i], "-f") == 0) && (argc > i+1)) {
                                printf("%s: not supported in this release\n", 
                                       argv[i]);
				i++;
			}
		}
	}
}

static void parse_late_options_audio_tool(int argc, char *argv[], session_struct *sp)
{
	/* Audio tool specific late setup... */
	UNUSED(argc);
	UNUSED(argv);
	UNUSED(sp);
}

static void parse_late_options_transcoder(int argc, char *argv[], session_struct *sp[])
{
	/* Transcoder specific late setup... */
	int	i;

	UNUSED(argc);
	UNUSED(argv);

	for (i = 0; i < 2; i++) {
		sp[i]->playing_audio = TRUE;
		sp[i]->agc_on        = FALSE;
	}
}

void parse_late_options(int argc, char *argv[], session_struct *sp[])
{
	int	i, num_sessions = 0;

	if (argc < 2) {
		usage();
	}
	/* Set the mode of operation, and number of valid sessions, based on the first command line option. */
	if (strcmp(argv[1], "-T") == 0) {
		sp[0]->mode = TRANSCODER;
		sp[1]->mode = TRANSCODER;
		num_sessions= 2;
	} else {
		sp[0]->mode = AUDIO_TOOL;
		num_sessions= 1;
	}
	parse_late_options_common(argc, argv, sp, num_sessions);
	switch (sp[0]->mode) {
		case AUDIO_TOOL: parse_late_options_audio_tool(argc, argv, sp[0]);
				 break;
		case TRANSCODER: parse_late_options_transcoder(argc, argv, sp);
				 break;
		default        : abort();
	}
	for (i=0; i<num_sessions; i++) {
		if (sp[i]->rtp_port == 0) {
			usage();
		}
	}
}

