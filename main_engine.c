/*
 * FILE:    main-engine.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins 
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 */
 
#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "session.h"
#include "audio.h"
#include "auddev.h"
#include "cushion.h"
#include "converter.h"
#include "tcltk.h"
#include "pdb.h"
#include "ui_send_rtp.h"
#include "ui_send_audio.h"
#include "ui_send_stats.h"
#include "net.h"
#include "parameters.h"
#include "transmit.h"
#include "source.h"
#include "mix.h"
#include "sndfile.h"
#include "mbus_ui.h"
#include "mbus_engine.h"
#include "crypt_random.h"
#include "net_udp.h"
#include "settings.h"
#include "rtp.h"
#include "rtp_callback.h"
#include "tonegen.h"
#include "voxlet.h"
#include "fatal_error.h"

#include "mbus_parser.h"
#include "mbus.h"
#include "util.h"

const char* appname;
char*       c_addr, *token, *token_e; /* Could be in the session struct */
int         should_exit = FALSE, mbus_shutdown_error = FALSE;

pid_t ppid;

#ifndef WIN32
static void
signal_handler(int signal)
{
        debug_msg("Caught signal %d\n", signal);
        exit(-1);
}
#endif

#define MBUS_ADDR_ENGINE "(media:audio module:engine app:rat id:%lu)"

static void parse_args(int argc, char *argv[])
{
	int 	i;

	if (argc != 5) {
		printf("Usage: %s -ctrl <addr> -token <token>\n", argv[0]);
		exit(1);
	}
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-ctrl") == 0) {
			c_addr = xstrdup(argv[++i]);
		} else if (strcmp(argv[i], "-token") == 0) {
			token   = xstrdup(argv[++i]);
			token_e = mbus_encode_str(token);
		} else {
			printf("Unknown argument \"%s\"\n", argv[i]);
			abort();
		}
	}

        /*
         * Want app instance to be same across all processes that
         * consitute this rat.  Parent pid appears after last colon.
         * Obviously on Un*x we could use getppid...
         */
        i = strlen(c_addr) - 1;
        while(i > 1 && c_addr[i - 1] != ':') {
                i--;
        }
        ppid = (pid_t)atoi(&c_addr[i]);
}

static void 
mbus_error_handler(int seqnum, int reason)
{
        debug_msg("mbus message failed (%d:%d)\n", seqnum, reason);
        if (should_exit == FALSE) {
                char msg[64];
                sprintf(msg, "MBUS message failed (%d:%d)\n", seqnum, reason);
                fatal_error(appname, msg);
                abort();
        } 
	mbus_shutdown_error = TRUE;
        UNUSED(seqnum);
        UNUSED(reason);
        /* Ignore error we're closing down anyway */
}

int main(int argc, char *argv[])
{
	uint32_t	 ntp_time = 0;
	int            	 seed, elapsed_time, alc = 0, scnt = 0;
	session_t 	*sp;
	struct timeval   time;
	struct timeval	 timeout;
        uint8_t		 j;

        appname = get_appname(argv[0]);

#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#else
	signal(SIGINT, signal_handler); 
        debug_set_core_dir(argv[0]);
#endif

	/* Setup things which are independent of the session. These should */
	/* be create static data only, since it will be accessed by BOTH   */
	/* instances when running as a transcoder.                         */
        seed = (gethostid() << 8) | (getpid() & 0xff);
	srand48(seed);
	lbl_srandom(seed);
        converters_init();
        audio_init_interfaces();
	parse_args(argc, argv);

	/* Initialize the session structure, and all session specific data */
	sp = (session_t *) xmalloc(sizeof(session_t));
	session_init(sp);
        audio_device_get_safe_config(&sp->new_config);
        audio_device_reconfigure(sp);
        assert(audio_device_is_open(sp->audio_device));

	/* Initialise our mbus interface... once this is done we can talk to our controller */
	sp->mbus_engine_addr = (char *) xmalloc(strlen(MBUS_ADDR_ENGINE) + 10);
	sprintf(sp->mbus_engine_addr, MBUS_ADDR_ENGINE, (unsigned long) ppid);
	sp->mbus_engine      = mbus_init(mbus_engine_rx, mbus_error_handler, sp->mbus_engine_addr);
        if (sp->mbus_engine == NULL) {
                fatal_error(appname, "Could not initialize Mbus: Is multicast enabled?");
                return FALSE;
        }

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */
	debug_msg("Waiting for mbus.waiting(%s) from controller...\n", token);
	mbus_rendezvous_go(sp->mbus_engine, token, (void *) sp);
	debug_msg("...got it\n");

	/* At this point we know the mbus address of our controller, and have conducted */
	/* a successful rendezvous with it. It will now send us configuration commands. */
	debug_msg("Waiting for mbus.go(%s) from controller...\n", token);
	mbus_rendezvous_waiting(sp->mbus_engine, c_addr, token, (void *) sp);
	debug_msg("...got it\n");
	assert(sp->rtp_session[0] != NULL);

	settings_load_early(sp);
        if (pdb_create(&sp->pdb) == FALSE) {
                debug_msg("Failed to create persistent database\n");
                abort();
        }
        pdb_item_create(sp->pdb, (uint16_t)ts_get_freq(sp->cur_ts), rtp_my_ssrc(sp->rtp_session[0])); 
	settings_load_late(sp);

	session_validate(sp);
	xmemchk();
	xdoneinit();

	while (!should_exit) {
                elapsed_time = 0;
		alc++;

                /* Process audio */
		elapsed_time = audio_rw_process(sp, sp, sp->ms);
		ntp_time     = ntp_time32();

                if (tx_is_sending(sp->tb)) {
                        tx_process_audio(sp->tb);
                        tx_send(sp->tb);
                }

                /* Process RTP/RTCP packets */
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
                for (j = 0; j < sp->rtp_session_count; j++) {
                        while(rtp_recv(sp->rtp_session[j], &timeout, ntp_time));
                        rtp_send_ctrl(sp->rtp_session[j], ntp_time, NULL);
                        rtp_update(sp->rtp_session[j]);
                }

                /* Process mbus */
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		mbus_recv(sp->mbus_engine, (void *) sp, &timeout);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
		mbus_send(sp->mbus_engine); 

                if (tx_is_sending(sp->tb)) {
                        tx_process_audio(sp->tb);
                        tx_send(sp->tb);
                }

		/* Process and mix active sources */
		if (sp->playing_audio) {
			struct s_source *s;
			int 		 sidx;
			ts_t 		 cush_ts;

			session_validate(sp);
			cush_ts = ts_map32(ts_get_freq(sp->cur_ts), cushion_get_size(sp->cushion));
			cush_ts = ts_add(sp->cur_ts, cush_ts);
			scnt = (int)source_list_source_count(sp->active_sources);
			for(sidx = 0; sidx < scnt; sidx++) {
				s = source_list_get_source_no(sp->active_sources, sidx);
                                if (source_relevant(s, sp->cur_ts)) {
                                        pdb_entry_t *e;
                                        ts_t         two_secs, delta;
                                        source_check_buffering(s);
					source_process(sp, s, sp->ms, sp->render_3d, sp->repair, sp->cur_ts, cush_ts);
					source_audit(s);
                                        /* Check for UI update necessary, updating once per 2 secs */
                                        pdb_item_get(sp->pdb, source_get_ssrc(s), &e);
                                        delta    = ts_sub(sp->cur_ts, e->last_ui_update);
                                        two_secs = ts_map32(8000, 16000);
                                        if (ts_gt(delta, two_secs)) {
                                                ui_send_stats(sp, sp->mbus_ui_addr, e->ssrc);
                                                e->last_ui_update = sp->cur_ts;
                                        }
				} else {
					/* Remove source as stopped */
                                        uint32_t ssrc;
                                        ssrc = source_get_ssrc(s);
					ui_send_rtp_inactive(sp, sp->mbus_ui_addr, ssrc);
					source_remove(sp->active_sources, s);
					sidx--;
					scnt--;
				}
			}
                        /* Play local file if playing */
                        if (sp->local_file_player) {
                                if (voxlet_play(sp->local_file_player, sp->cur_ts, cush_ts) == FALSE) {
                                        voxlet_destroy(&sp->local_file_player);
                                }
                        }
                        /* Play loopback tone if present */
                        if (sp->tone_generator) {
                                tonegen_play(sp->tone_generator, sp->cur_ts, cush_ts);
                        }
                } else {
                        /* Destroy localfile player if not playing audio */
                        if (sp->local_file_player) {
                                voxlet_destroy(&sp->local_file_player);
                        }
                }

                /* Echo Suppression - cut off transmitter when receiving     */
                /* audio, enable when stop receiving.                        */
		session_validate(sp);
                if (sp->echo_suppress) {
                        if (scnt > 0) {
                                if (tx_is_sending(sp->tb)) {
                                        tx_stop(sp->tb);
                                        sp->echo_tx_active = TRUE;
                                        debug_msg("Echo suppressor (disabling tx)\n");
                                }
                        } else if (sp->echo_tx_active) {
                                /* Transmitter was stopped because we were   */
                                /* playing out audio.  Restart it.           */
                                if (tx_is_sending(sp->tb) == FALSE) {
                                        tx_start(sp->tb);
                                        debug_msg("Echo suppressor (enabling tx)\n");
                                }
                                sp->echo_tx_active = FALSE;
                        }
                }
                /* Lecture Mode */
		if ((alc % 50) == 0) {
			if (!sp->lecture && tx_is_sending(sp->tb) && sp->auto_lecture != 0) {
				gettimeofday(&time, NULL);
				if (time.tv_sec - sp->auto_lecture > 120) {
					sp->auto_lecture = 0;
					debug_msg("Dummy lecture mode\n");
				}
			}
		}

		if (sp->ui_on) {
			/* We have a user interface... do any periodic updates needed. */
			if (sp->audio_device && elapsed_time != 0) {
				ui_send_periodic_updates(sp, sp->mbus_ui_addr, elapsed_time);
			}
		} else {
			/* We don't yet have a user interface... send out a message soliciting one... */
			if ((alc % 25) == 0) {
				mbus_qmsgf(sp->mbus_engine, "()", FALSE, "mbus.waiting", "\"rat-ui-requested\"");
			}
		}
		if (sp->new_config != NULL) {
			/* wait for mbus messages - closing audio device    */
			/* can timeout unprocessed messages as some drivers */
			/* pause to drain before closing.                   */
			network_process_mbus(sp);
			if (audio_device_reconfigure(sp)) {
				int saved_playing_audio = sp->playing_audio;
				/* Device reconfig takes a second or two, discard
				 * data that has arrived during this time
				 */
				sp->playing_audio = 0;
				timeout.tv_sec  = 0;
				timeout.tv_usec = 0;
				for (j = 0; j < sp->rtp_session_count; j++) {
					while(rtp_recv(sp->rtp_session[j], &timeout, ntp_time));
				}
				sp->playing_audio = saved_playing_audio;
				/* Device reconfigured so decode paths of all sources */
				/* are misconfigured. Delete the source, and incoming */
				/* data will drive the correct new path.              */
				source_list_clear(sp->active_sources);
				ui_send_audio_update(sp, sp->mbus_ui_addr);
                                if (sp->local_file_player) {
                                        voxlet_destroy(&sp->local_file_player);
                                }
			}
		}
		
		/* Choke CPU usage */
		if (!audio_is_ready(sp->audio_device)) {
			audio_wait_for(sp->audio_device, 50);
		}

                /* Check controller is still alive */
                if (mbus_addr_valid(sp->mbus_engine, c_addr) == FALSE) {
                        should_exit = TRUE;
                        debug_msg("Controller address is no longer valid.  Assuming it exited\n");
                }

		/* Debugging sanity check of the session... */
		session_validate(sp);
		/* ...and check that nothing has trashed memory too badly! */
		xmemchk();
        }

	settings_save(sp);
	tx_stop(sp->tb);

	for (j = 0; j < sp->rtp_session_count; j++) {
		rtp_send_bye(sp->rtp_session[j]);
		rtp_done(sp->rtp_session[j]);
		rtp_callback_exit(sp->rtp_session[j]);
	}

	/* Inform other processes that we're about to quit... */
	mbus_qmsgf(sp->mbus_engine, "()", FALSE, "mbus.bye", "");
	mbus_send(sp->mbus_engine);

	/* Free audio device and clean up */
	audio_device_release(sp, sp->audio_device);
	audio_free_interfaces();
	
	if (mbus_addr_valid(sp->mbus_engine, c_addr)) {
		do {
			struct timeval	 timeout;
			mbus_send(sp->mbus_engine); 
			/* At this stage we no longer care about acks for messages */
			/* mbus_retransmit(sp->mbus_engine); */
			timeout.tv_sec  = 0;
			timeout.tv_usec = 20000;
			mbus_recv(sp->mbus_engine, sp, &timeout);
		} while (!mbus_sent_all(sp->mbus_engine) && mbus_shutdown_error == FALSE);
	}
	
	mbus_exit(sp->mbus_engine);

	session_validate(sp);
	session_exit(sp);
        
	converters_free();
        
	xfree(c_addr);
	xfree(token);
	xfree(token_e);

	xmemdmp();
	debug_msg("Media engine exit\n");
	return 0;
}

