/*
 * FILE:    main-engine.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
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
#include "ui.h"
#include "net.h"
#include "timers.h"
#include "parameters.h"
#include "transmit.h"
#include "source.h"
#include "mix.h"
#include "sndfile.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"
#include "crypt_random.h"
#include "net_udp.h"
#include "settings.h"
#include "rtp.h"
#include "rtp_callback.h"

int should_exit = FALSE;
int thread_pri  = 2; /* Time Critical */

#ifndef WIN32
static void
signal_handler(int signal)
{
  debug_msg("Caught signal %d\n", signal);
  should_exit = TRUE;
}
#endif

char	*c_addr, *token, *token_e; 

#define MBUS_ADDR_ENGINE "(media:audio module:engine app:rat instance:%5lu)"

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
}

int main(int argc, char *argv[])
{
	u_int32		 cur_time, ntp_time;
	int            	 seed, elapsed_time, alc;
	session_t 	*sp;
	struct timeval   time;
	struct timeval	 timeout;
        u_int8		 j;

        seed = (gethostid() << 8) | (getpid() & 0xff);
	srand48(seed);
	lbl_srandom(seed);

	sp = (session_t *) xmalloc(sizeof(session_t));
	session_init(sp);
        converters_init();
        audio_init_interfaces();
        audio_device_get_safe_config(&sp->new_config);
        audio_device_reconfigure(sp);
        assert(audio_device_is_open(sp->audio_device));
	settings_load_early(sp);
	parse_args(argc, argv);

	/* Initialise our mbus interface... once this is done we can talk to our controller */
	sp->mbus_engine = mbus_init(mbus_engine_rx, NULL);
	sp->mbus_engine_addr = (char *) xmalloc(strlen(MBUS_ADDR_ENGINE) + 3);
	sprintf(sp->mbus_engine_addr, MBUS_ADDR_ENGINE, (u_int32) getpid());
	mbus_addr(sp->mbus_engine, sp->mbus_engine_addr);

	/* The first stage is to wait until we hear from our controller. The address of the */
	/* controller is passed to us via a command line parameter, and we just wait until  */
	/* we get an mbus.hello() from that address.                                        */
	debug_msg("Waiting to validate address %s\n", c_addr);
	while (!mbus_addr_valid(sp->mbus_engine, c_addr)) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, NULL, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}
	debug_msg("Address %s is valid\n", c_addr);

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */
	mbus_engine_wait_handler_init(token);
	mbus_cmd_handler(sp->mbus_engine, mbus_engine_wait_handler);
	while (!mbus_engine_wait_handler_done()) {
		debug_msg("Waiting for token \"%s\" from controller\n", token);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, sp->mbus_engine, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}
	mbus_cmd_handler(sp->mbus_engine, mbus_engine_rx);
	mbus_qmsgf(sp->mbus_engine, c_addr, TRUE, "mbus.go", "%s", token_e);
	mbus_send(sp->mbus_engine);

#ifndef WIN32
 	signal(SIGINT, signal_handler); 
#endif

	/* At this point we know the mbus address of our controller, and have conducted */
	/* a successful rendezvous with it. It will now send us configuration commands. */
	while (sp->rtp_session[0] == NULL) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 250000;
		mbus_recv(sp->mbus_engine, sp, &timeout);
		mbus_send(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 1);
		mbus_retransmit(sp->mbus_engine);
	}
	debug_msg("Done configuration\n");

        if (pdb_create(&sp->pdb) == FALSE) {
                debug_msg("Failed to create persistent database\n");
                abort();
        }
        pdb_item_create(sp->pdb, sp->clock, (u_int16)get_freq(sp->device_clock), rtp_my_ssrc(sp->rtp_session[0])); 
	settings_load_late(sp);

	ui_controller_init(sp, sp->mbus_engine_addr, sp->mbus_ui_addr, sp->mbus_video_addr);
        ui_initial_settings(sp);
	ui_update(sp);
	network_process_mbus(sp);
        
#ifdef NDEF
        if (sp->new_config != NULL) {
                network_process_mbus(sp);
                audio_device_reconfigure(sp);
                network_process_mbus(sp);
        }
#endif

#ifdef NDEF
        ui_final_settings(sp);
	network_process_mbus(sp);
#endif
        audio_drain(sp->audio_device);
        if (tx_is_sending(sp->tb)) {
               	tx_start(sp->tb);
        }

	xdoneinit();

	while (!should_exit) {
                /* Process RTP/RTCP packets */
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
                for (j = 0; j < sp->rtp_session_count; j++) {
                        while(rtp_recv(sp->rtp_session[j], &timeout, cur_time));
                        rtp_send_ctrl(sp->rtp_session[j], cur_time);
                        rtp_update(sp->rtp_session[j]);
                }

                /* Process mbus */
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		mbus_send(sp->mbus_engine); 
		mbus_recv(sp->mbus_engine, (void *) sp, &timeout);
		mbus_retransmit(sp->mbus_engine);
		mbus_heartbeat(sp->mbus_engine, 10);

                /* Process audio */
		elapsed_time = audio_rw_process(sp, sp, sp->ms);
		cur_time = get_time(sp->device_clock);
		ntp_time = ntp_time32();
		sp->cur_ts   = ts_seq32_in(&sp->decode_sequencer, get_freq(sp->device_clock), cur_time);

                if (tx_is_sending(sp->tb)) {
                        tx_process_audio(sp->tb);
                        tx_send(sp->tb);
                }

		/* Process and mix active sources */
		if (sp->playing_audio) {
			struct s_source *s;
			int sidx, scnt;
			ts_t cush_ts;
			cush_ts = ts_map32(get_freq(sp->device_clock), cushion_get_size(sp->cushion));
			cush_ts = ts_add(sp->cur_ts, cush_ts);
			scnt = (int)source_list_source_count(sp->active_sources);
			for(sidx = 0; sidx < scnt; sidx++) {
				s = source_list_get_source_no(sp->active_sources, sidx);
                                if (source_relevant(s, sp->cur_ts)) {
                                        pdb_entry_t *e;
                                        ts_t         two_secs, delta;
					source_process(s, sp->ms, sp->render_3d, sp->repair, cush_ts);
					source_audit(s);
                                        /* Check for UI update necessary, updating once per 2 secs */
                                        pdb_item_get(sp->pdb, source_get_ssrc(s), &e);
                                        delta    = ts_sub(sp->cur_ts, e->last_ui_update);
                                        two_secs = ts_map32(8000, 16000);
                                        if (ts_gt(delta, two_secs)) {
                                                ui_update_stats(sp, e->ssrc);
                                        }
				} else {
					/* Remove source as stopped */
                                        u_int32 ssrc;
                                        ssrc = source_get_ssrc(s);
					ui_info_deactivate(sp, ssrc);
					source_remove(sp->active_sources, s);
					sidx--;
					scnt--;
				}
			}
		}

		if (alc >= 50) {
			if (!sp->lecture && tx_is_sending(sp->tb) && sp->auto_lecture != 0) {
				gettimeofday(&time, NULL);
				if (time.tv_sec - sp->auto_lecture > 120) {
					sp->auto_lecture = 0;
					debug_msg("Dummy lecture mode\n");
				}
			}
			alc = 0;
		} else {
			alc++;
		}
		if (sp->audio_device) {
                        ui_update_powermeters(sp, sp->ms, elapsed_time);
                }
		if (sp->new_config != NULL) {
			/* wait for mbus messages - closing audio device
			 * can timeout unprocessed messages as some drivers
			 * pause to drain before closing.
			 */
			network_process_mbus(sp);
			if (audio_device_reconfigure(sp)) {
				/* Device reconfigured so
				 * decode paths of all sources
				 * are misconfigured.  Delete
				 * and incoming data will
				 * drive correct new path */
				source_list_clear(sp->active_sources);
			}
		}
		
		/* Choke CPU usage */
		if (!audio_is_ready(sp->audio_device)) {
			audio_wait_for(sp->audio_device, 10);
		}
        }

	settings_save(sp);
	tx_stop(sp->tb);
	if (sp->in_file  != NULL) snd_read_close (&sp->in_file);
	if (sp->out_file != NULL) snd_write_close(&sp->out_file);
	audio_device_release(sp, sp->audio_device);
        pdb_destroy(&sp->pdb);

	/* Inform our controller that we're quiting. It will deal with shutting down the UI, */
	/* if it is necessary to do that.                                                    */
	if (mbus_addr_valid(sp->mbus_engine, c_addr)) {
		mbus_qmsgf(sp->mbus_engine, c_addr, TRUE, "mbus.quit", "");
		mbus_send(sp->mbus_engine);
		do {
			struct timeval	 timeout;
			mbus_send(sp->mbus_engine);
			mbus_heartbeat(sp->mbus_engine, 1);
			mbus_retransmit(sp->mbus_engine);
			timeout.tv_sec  = 0;
			timeout.tv_usec = 20000;
			mbus_recv(sp->mbus_engine, sp, &timeout);
		} while (!mbus_sent_all(sp->mbus_engine));
	}

	network_process_mbus(sp);
	mbus_exit(sp->mbus_engine);
        sp->mbus_engine = NULL;

	for (j = 0; j < sp->rtp_session_count; j++) {
		rtp_done(sp->rtp_session[j]);
		rtp_callback_exit(sp->rtp_session[j]);
	}

	session_exit(sp);
	xfree(sp);

        converters_free();
        audio_free_interfaces();

	xmemdmp();
	return 0;
}

