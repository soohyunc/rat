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
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "session.h"
#include "timers.h"
#include "repair.h"
#include "codec_types.h"
#include "codec.h"
#include "channel.h"
#include "pckt_queue.h"
#include "receive.h"
#include "convert.h"
#include "channel.h"
#include "parameters.h"
#include "audio.h"
#include "ui.h"
#include "crypt.h"

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
	u_long          netaddr;
	char            hostname[MAXHOSTNAMELEN + 1];
	codec_id_t      cid;
        const codec_format_t *cf;

	memset(sp, 0, sizeof(session_struct));

	codec_init();
        vu_table_init();
	cid = codec_get_by_name("DVI-8K-MONO");
        assert(cid);
        cf  = codec_get_format(cid);
        channel_set_coder(sp, PT_VANILLA);
        sp->last_depart_ts              = 1;
        sp->encodings[0]		= codec_get_payload(cid);           /* user chosen encoding for primary */
	sp->num_encodings		= 1;                                /* Number of encodings in packet */
        sp->next_encoding               = -1;                               /* Coding to change to */
	sp->clock			= new_fast_time(GLOBAL_CLOCK_FREQ); /* this is the global clock */
        sp->device_clock                = new_time(sp->clock, cf->format.sample_rate);
        assert(!(GLOBAL_CLOCK_FREQ%cf->format.sample_rate));                        /* just in case someone adds weird freq codecs */
        sp->collator                    = collator_create();
	sp->mode         		= AUDIO_TOOL;	
        sp->input_mode                  = AUDIO_NO_DEVICE;
        sp->output_mode                 = AUDIO_NO_DEVICE;
	sp->rtp_port			= 5004;		/* default: draft-ietf-avt-profile-new-00 */
	sp->rtcp_port			= 5005;		/* default: draft-ietf-avt-profile-new-00 */
        sp->rtp_pckt_queue              = pckt_queue_create(PCKT_QUEUE_RTP_LEN);
        sp->rtcp_pckt_queue             = pckt_queue_create(PCKT_QUEUE_RTCP_LEN);
	sp->ttl				= 16;
	sp->rtp_socket			= NULL;
	sp->rtcp_socket			= NULL;
        sp->filter_loopback             = TRUE;
	sp->sending_audio		= FALSE;
	sp->playing_audio		= TRUE;
	sp->lecture			= FALSE;
	sp->auto_lecture		= 0;
	sp->transmit_audit_required	= FALSE;
	sp->receive_audit_required	= FALSE;
	sp->detect_silence		= TRUE;
	sp->sync_on			= FALSE;
	sp->agc_on			= FALSE;
        sp->ui_on                       = TRUE;
	sp->ui_addr			= NULL;
	sp->meter			= TRUE;		/* Powermeter operation */
        sp->drop                        = 0.0;
	sp->in_file 			= NULL;
	sp->out_file  			= NULL;
	sp->rtp_seq			= lrand48() & 0xffff;
	sp->speakers_active 		= NULL;
	sp->mbus_engine_base		= NULL;
	sp->mbus_engine_conf		= NULL;
	sp->mbus_ui_base		= NULL;
	sp->mbus_ui_conf		= NULL;
	sp->mbus_channel		= 0;
	sp->min_playout			= 0;
	sp->max_playout			= 1000;
	sp->wait_on_startup		= FALSE;
        sp->next_selected_device        = -1;

        strcpy(sp->title, "<Untitled Session>");
        
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

	gettimeofday(&(sp->device_time), NULL); 

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
        collator_destroy(sp->collator);
        pckt_queue_destroy(&sp->rtp_pckt_queue);
        pckt_queue_destroy(&sp->rtcp_pckt_queue);
}

static void 
parse_early_options_common(int argc, char *argv[], session_struct *sp[], int sp_size)
{
	/* Parse command-line options common to all the modes of operation.     */
	/* Variables: i scans through the options, s scans through the sessions */
	/* This is for options set initially, before the main initialisation is */
	/* done. For example, the UI is not yet setup, and anything initialised */
	/* there will overwrite changes made here...                            */
	int i, s;

	for (i = 1; i < argc; i++) {
		for (s = 0; s < sp_size; s++) {
			if ((strcmp(argv[i], "-ui") == 0) && (argc > i+1)) {
				sp[s]->ui_on   = FALSE;
				sp[s]->ui_addr = strdup(argv[i+1]);
			}
            		if (strcmp(argv[i], "-allowloopback") == 0 || strcmp(argv[i], "-allow_loopback") == 0) {
                		sp[s]->filter_loopback = FALSE;
            		}
			if ((strcmp(argv[i], "-C") == 0) && (argc > i+1)) {
                                strncpy(sp[s]->title, argv[i+1], SESSION_TITLE_LEN);
				i++;
			}
			if ((strcmp(argv[i], "-mbus") == 0) && (argc > i+1)) {
				sp[s]->mbus_channel = atoi(argv[i + 1]);
				i++;
			}
			if (strcmp(argv[i], "-wait") == 0) {
				sp[s]->wait_on_startup = TRUE;
			}
			if ((strcmp(argv[i], "-t") == 0) && (argc > i+1)) {
				sp[s]->ttl = atoi(argv[i + 1]);
				if (sp[s]->ttl > 255) {
					usage();
				}
				i++;
			}
			if ((strcmp(argv[i], "-p") == 0) && (argc > i+1)) {
				if ((thread_pri = atoi(argv[i + 1])) > 3) {
					usage();
				}
				i++;
			}
			if ((strcmp(argv[i], "-drop") == 0) && (argc > i+1)) {
                        	sp[s]->drop = (float)atof(argv[++i]);
                        	if (sp[s]->drop > 1.0) {
                                	sp[s]->drop = sp[s]->drop/100;
                        	}
				i++;
                	}
                	if (strcmp(argv[i], "-seed") == 0) {
                        	srand48(atoi(argv[++i]));
                	}
			if ((strcmp(argv[i], "-pt") == 0) && (argc > i+1)) {
                		/* Dynamic payload type mapping. Format: "-pt pt/codec/clock/channels" */
				/* pt/codec must be specified. clock and channels are optional.        */
				/* At present we only support "-pt .../redundancy"                     */
                                char *t;
                                int pt;
				pt = atoi(strtok(argv[i + 1], "/"));
				if ((pt > 127) || (pt < 96)) {
					printf("Dynamic payload types must be in the range 96-127. So there.\n");
					usage();
				}
                                t = strtok(NULL, "/");
                                if (!set_cc_pt(t,pt)) {
                                    printf("Hmmm.... Don't understand that -pt option\n");
                                    usage();
                                }
                                i++;
			}
                }
	}
}

static void 
parse_early_options_audio_tool(int argc, char *argv[], session_struct *sp)
{
	/* Parse command-line options specific to the audio tool */
	char *p;

	p = strtok(argv[argc - 1], "/");
	strcpy(sp->asc_address, p);
	if ((p = strtok(NULL, "/")) != NULL) {
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
		p = strtok(argv[argc-i-1], "/");
		strcpy(sp[i]->asc_address, p);
		/* port */
		if ((p = strtok(NULL, "/")) != NULL) {
			sp[i]->rtp_port  = atoi(p);
			sp[i]->rtp_port &= ~1;
			sp[i]->rtcp_port = sp[i]->rtp_port + 1;
		} else {
			continue;
		}
		/* ttl */
		if ((p = strtok(NULL, "/")) != NULL) {
			sp[i]->ttl = atoi(p);
		} else {
			continue;
		}
		/* encoding */
		j = 0;
		while ((p = strtok(NULL, "/")) != NULL) {
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
	parse_early_options_common(argc, argv, sp, num_sessions);
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
				if (sp[s]->mbus_channel != 0) {
                                	sp[s]->sync_on = TRUE;
				} else {
					printf("Lip-sync can only be used if an mbus channel is specified\n");
					usage();
				}
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
				/* expects string of form "codec/units/separation" */
				int pt = get_cc_pt(sp[s], "interleaver");
				if (pt != -1) {
					debug_msg("Configure interleaver %d %s\n", pt, argv[i+1]);
					config_channel_coder(sp[s], pt, argv[i+1]);
					channel_set_coder(sp[s], pt);
					ui_update_channel(sp[s]);
				} else {
					printf("Can't determine interleaver payload type\n");
					abort();
				}
                            	i++;
			}
                        if ((strcmp(argv[i], "-redundancy") == 0) && (argc > i+1)) {
				/* Takes "-redundancy  redundant_codec/offset" */
				codec_id_t pcid = codec_get_by_payload(sp[s]->encodings[0]);
                                const codec_format_t *cf = codec_get_format(pcid);
                            	int      pt  = get_cc_pt(sp[s], "redundancy");
				char     cfg[80];

				sprintf(cfg, "%s/0/%s", cf->long_name, argv[i+1]);
				debug_msg("Configure redundancy %s\n", cfg);
                            	config_channel_coder(sp[s], pt, cfg);
				channel_set_coder(sp[s], pt);
				ui_update_primary(sp[s]);
				ui_update_channel(sp[s]);
                            	i++;
                        }
			if ((strcmp(argv[i], "-f") == 0) && (argc > i+1)) {
				codec_id_t cid;
				char    *pu;
                                for (pu = argv[i+1]; *pu; pu++) {
                                        *pu = toupper(*pu);
				}
                                pu = argv[i+1];
                                if ((cid = codec_get_by_name(pu)) != 0) {
                                        const codec_format_t *cf = codec_get_format(cid);
                                        change_freq(sp[s]->device_clock, cf->format.sample_rate);
                                        sp[s]->encodings[0]  = codec_get_payload(cid);
					sp[s]->num_encodings = 1;
					channel_set_coder(sp[s], get_cc_pt(sp[s], "vanilla"));
					debug_msg("Configure codec %d\n", sp[s]->encodings[0]);
				} else if ((cid = codec_get_matching(pu, 8000, 1)) != 0) {
                                        const codec_format_t *cf = codec_get_format(cid);
                                        change_freq(sp[s]->device_clock, cf->format.sample_rate);
                                        sp[s]->encodings[0]  = codec_get_payload(cid);
					sp[s]->num_encodings = 1;
					channel_set_coder(sp[s], get_cc_pt(sp[s], "vanilla"));
					debug_msg("Configure codec %d\n", sp[s]->encodings[0]);
                                } else {
					/* This is not a valid codec name. One last chance: it might be a */
					/* combined codec and redundancy specifier such as "dvi+lpc" used */
					/* by sdr. Try to parse it that way, just in case...        [csp] */
					char *pri_name = strtok(argv[i+1], "/");
					char *sec_name = strtok(NULL, "/");
					codec_id_t pri_cid, sec_cid;
                                        const codec_format_t *pri_cf, *sec_cf;
					char     cfg[80];
					int	red_pt;

					if (pri_name == NULL || sec_name == NULL) {
						printf("Unknown codec\n");
						usage();
					}
					if ((pri_cid = codec_get_matching(pri_name, 8000, 1)) == 0) {
						printf("Unknown primary codec\n");
						usage();
					}
					if ((sec_cid = codec_get_matching(sec_name, 8000, 1)) == 0) {
						printf("Unknown redundant codec\n");
						usage();
					}
                                        pri_cf = codec_get_format(pri_cid);
                                        sec_cf = codec_get_format(sec_cid);
                                        change_freq(sp[s]->device_clock, pri_cf->format.sample_rate);
                                        sp[s]->encodings[0]  = codec_get_payload(pri_cid);
					sp[s]->num_encodings = 1;
					sprintf(cfg, "%s/0/%s/1", pri_cf->long_name,sec_cf->long_name);
					debug_msg("Configuring codec+redundancy: %s+%s\n", pri_name, sec_name);
					red_pt = get_cc_pt(sp[s], "redundancy");
					config_channel_coder(sp[s], red_pt, cfg);
					channel_set_coder(sp[s], red_pt);
                                }
				ui_update_primary(sp[s]);
				ui_update_channel(sp[s]);
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

