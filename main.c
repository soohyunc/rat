/*
 * FILE:    main.c
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

struct s_pckt_queue; /* pckt_queue is being removed */

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
#include "rtp_queue.h"
#include "rtp.h"
#include "rtp_callback.h"
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
#include "net.h"
#include "settings.h"

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

static int tcl_process_events(session_t *sp)
{
	int i = 0;

        while (!audio_is_ready(sp->audio_device) && tcl_process_event() && i < 16) {
                i++;
        }
        return i;
}

int
main(int argc, char *argv[])
{
	u_int32			 cur_time, ntp_time;
	int            		 i, j, elapsed_time, alc = 0, seed;
	session_t 		*sp[2];
	struct timeval  	 time;
	struct timeval      	 timeout;
	char			 mbus_engine_addr[100], mbus_ui_addr[100], mbus_video_addr[100];

#ifndef WIN32
 	signal(SIGINT, signal_handler); 
#endif

	gettimeofday(&time, NULL);
        seed = (gethostid() << 8) | (getpid() & 0xff);
	srand48(seed);
	lbl_srandom(seed);

	for (i = 0; i < 2; i++) {
		sp[i] = (session_t *) xmalloc(sizeof(session_t));
		session_init(sp[i]);
	}
	session_parse_early_options(argc, argv, sp);

        audio_init_interfaces();
        converters_init();

	sprintf(mbus_engine_addr, "(media:audio module:engine app:rat instance:%lu)", (u_int32) getpid());
	sp[0]->mbus_engine = mbus_init(mbus_engine_rx, NULL);
	mbus_addr(sp[0]->mbus_engine, mbus_engine_addr);

	if (sp[0]->ui_on) {
		sprintf(mbus_ui_addr, "(media:audio module:ui app:rat instance:%lu)", (u_int32) getpid());
		sp[0]->mbus_ui = mbus_init(mbus_ui_rx, NULL);
		mbus_addr(sp[0]->mbus_ui, mbus_ui_addr);
		tcl_init(sp[0]->mbus_ui, argc, argv, mbus_engine_addr);
	} else {
		strncpy(mbus_ui_addr, sp[0]->ui_addr, 30);
	}
	sprintf(mbus_video_addr, "(media:video module:engine)");

        audio_device_get_safe_config(&sp[0]->new_config);
        audio_device_reconfigure(sp[0]);
        assert(audio_device_is_open(sp[0]->audio_device));

	network_process_mbus(sp[0]);
	settings_load_early(sp[0]);
        for (i = 0; i < sp[0]->layers; i++) {
                sp[0]->rtp_session[i] = rtp_init(sp[0]->asc_address[i], 
                                                 sp[0]->tx_rtp_port[i], 
                                                 sp[0]->ttl, 
                                                 5, /* Initial rtcp b/w guess */
                                                 rtp_callback);
                sp[0]->rtp_session_count++;
                rtp_callback_init(sp[0]->rtp_session[i], sp[0]);
        }
        if (pdb_create(&sp[0]->pdb) == FALSE) {
                debug_msg("Failed to create persistent database\n");
                abort();
        }
        pdb_item_create(sp[0]->pdb, 
                        sp[0]->clock, 
                        get_freq(sp[0]->device_clock), 
                        rtp_my_ssrc(sp[0]->rtp_session[0])); 
	settings_load_late(sp[0]);

/**** Up to here is in main_engine.c *****/

	ui_controller_init(sp[0], mbus_engine_addr, mbus_ui_addr, mbus_video_addr);
        ui_initial_settings(sp[0]);

	session_parse_late_options(argc, argv, sp);	/* Things which can override the settings we just loaded... */
	ui_update(sp[0]);				/* ...and push those to the UI */
	network_process_mbus(sp[0]);
        
#ifdef NDEF
        if (sp[0]->new_config != NULL) {
                network_process_mbus(sp[0]);
                audio_device_reconfigure(sp[0]);
                network_process_mbus(sp[0]);
        }
#endif

#ifdef NDEF
        ui_final_settings(sp[0]);
#endif
	network_process_mbus(sp[0]);

        if (tx_is_sending(sp[0]->tb)) {
               	tx_start(sp[0]->tb);
        }

        i = tcl_process_all_events();
        debug_msg("process %d events at startup %d\n", i);
	
	xdoneinit();

	while (!should_exit) {
		elapsed_time = audio_rw_process(sp[0], sp[0], sp[0]->ms);
		cur_time = get_time(sp[0]->device_clock);
		ntp_time = ntp_time32();
		sp[0]->cur_ts   = ts_seq32_in(&sp[0]->decode_sequencer, 
                                              get_freq(sp[0]->device_clock), 
                                              cur_time);
                tx_process_audio(sp[0]->tb);
                if (tx_is_sending(sp[0]->tb)) {
                        tx_send(sp[0]->tb);
                }

                /* Process RTP/RTCP packet  */
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
                for (j = 0; j < sp[0]->rtp_session_count; j++) {
                        rtp_recv(sp[0]->rtp_session[j], &timeout, cur_time);
                        rtp_send_ctrl(sp[0]->rtp_session[j], cur_time);
                        rtp_update(sp[0]->rtp_session[j]);
                }

		/* Process incoming packets */

		/* Process and mix active sources */
		if (sp[0]->playing_audio) {
			struct s_source *s;
			int sidx, scnt;
			ts_t cush_ts;
			
			cush_ts = ts_map32(get_freq(sp[0]->device_clock), cushion_get_size(sp[0]->cushion));
			cush_ts = ts_add(sp[0]->cur_ts, cush_ts);
			scnt = (int)source_list_source_count(sp[0]->active_sources);
			for(sidx = 0; sidx < scnt; sidx++) {
				s = source_list_get_source_no(sp[0]->active_sources, sidx);
				if (source_relevant(s, sp[0]->cur_ts)) {
					source_check_buffering(s, sp[0]->cur_ts);
					source_process(s, sp[0]->ms, sp[0]->render_3d, sp[0]->repair, cush_ts);
					source_audit(s);
				} else {
					/* Remove source as stopped */
                                        u_int32 ssrc;
                                        ssrc = source_get_ssrc(s);
					ui_info_deactivate(sp[0], ssrc);
					source_remove(sp[0]->active_sources, s);
					sidx--;
					scnt--;
				}
			}
		}

		if (alc >= 50) {
			if (!sp[0]->lecture && tx_is_sending(sp[0]->tb) && sp[0]->auto_lecture != 0) {
				gettimeofday(&time, NULL);
				if (time.tv_sec - sp[0]->auto_lecture > 120) {
					sp[0]->auto_lecture = 0;
					debug_msg("Dummy lecture mode\n");
				}
			}
			alc = 0;
		} else {
			alc++;
		}
		if (sp[0]->audio_device) ui_update_powermeters(sp[0], sp[0]->ms, elapsed_time);
		if (sp[0]->ui_on) {
			timeout.tv_sec  = 0;
			timeout.tv_usec = 0;
			tcl_process_events(sp[0]);
			mbus_send(sp[0]->mbus_ui); 
			mbus_recv(sp[0]->mbus_ui, (void *) sp[0], &timeout);
			mbus_retransmit(sp[0]->mbus_ui);
			mbus_heartbeat(sp[0]->mbus_ui, 10);
		}
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;
		mbus_send(sp[0]->mbus_engine); 
		mbus_recv(sp[0]->mbus_engine, (void *) sp[0], &timeout);
		mbus_retransmit(sp[0]->mbus_engine);
		mbus_heartbeat(sp[0]->mbus_engine, 10);

		if (sp[0]->new_config != NULL) {
			/* wait for mbus messages - closing audio device
			 * can timeout unprocessed messages as some drivers
			 * pause to drain before closing.
			 */
			network_process_mbus(sp[0]);
			if (audio_device_reconfigure(sp[0])) {
				/* Device reconfigured so
				 * decode paths of all sources
				 * are misconfigured.  Delete
				 * and incoming data will
				 * drive correct new path */
				source_list_clear(sp[0]->active_sources);
			}
		}
		
		/* Choke CPU usage */
		if (!audio_is_ready(sp[0]->audio_device)) {
			audio_wait_for(sp[0]->audio_device, 10);
		}
        }

	settings_save(sp[0]);
	tx_stop(sp[0]->tb);
        pdb_destroy(&sp[0]->pdb);
	if (sp[0]->in_file  != NULL) snd_read_close (&sp[0]->in_file);
	if (sp[0]->out_file != NULL) snd_write_close(&sp[0]->out_file);
	audio_device_release(sp[0], sp[0]->audio_device);
	network_process_mbus(sp[0]);

	mbus_exit(sp[0]->mbus_engine);
	if (sp[0]->ui_on) {
		mbus_exit(sp[0]->mbus_ui);
		tcl_exit();
	}

        for(i = 0; i < 2; i++) {
                for (j = 0; j < sp[i]->rtp_session_count; j++) {
                        rtp_done(sp[i]->rtp_session[j]);
                        rtp_callback_exit(sp[i]->rtp_session[j]);
                }
                session_exit(sp[i]);
                xfree(sp[i]);
        }

        converters_free();
        audio_free_interfaces();

	xmemdmp();
	return 0;
}

