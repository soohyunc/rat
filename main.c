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
#include "convert.h"
#include "tcltk.h"
#include "ui.h"
#include "pckt_queue.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "rtcp.h"
#include "net.h"
#include "timers.h"
#include "statistics.h"
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
#include "rtcp.h"

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

int
main(int argc, char *argv[])
{
	u_int32			 ssrc, cur_time, ntp_time;
	int            		 num_sessions, i, elapsed_time, alc = 0, seed;
	char			*cname;
	session_struct 		*sp[2];
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

	for (i = 0; i < 2;i++) {
		sp[i] = (session_struct *) xmalloc(sizeof(session_struct));
		init_session(sp[i]);
	}
	num_sessions = parse_early_options(argc, argv, sp);
	for (i = 0; i < num_sessions; i++) {
		network_init(sp[i]);
	}
	cname = get_cname(sp[0]->rtp_socket);
	ssrc  = get_ssrc();

        audio_init_interfaces();
        converters_init();
        statistics_init();

	if (sp[0]->mode == AUDIO_TOOL) {
		sprintf(mbus_engine_addr, "(media:audio module:engine app:rat instance:%lu)", (u_int32) getpid());
		sp[0]->mbus_engine = mbus_init(mbus_engine_rx, NULL);
		mbus_addr(sp[0]->mbus_engine, mbus_engine_addr);

		if (sp[0]->ui_on) {
			sprintf(mbus_ui_addr, "(media:audio module:ui app:rat instance:%lu)", (u_int32) getpid());
			sp[0]->mbus_ui = mbus_init(mbus_ui_rx, NULL);
			mbus_addr(sp[0]->mbus_ui, mbus_ui_addr);
			tcl_init(sp[0], argc, argv, mbus_engine_addr);
		} else {
			strncpy(mbus_ui_addr, sp[0]->ui_addr, 30);
		}
	} else {
		/* We're a transcoder... set up a separate mbus instance */
		/* for each side, to make them separately controllable.  */
		abort();
	}
	sprintf(mbus_video_addr, "(media:video module:engine)");

	do {
		usleep(20000);
		mbus_heartbeat(sp[0]->mbus_engine, 1);
		mbus_heartbeat(sp[0]->mbus_ui, 1);
		network_process_mbus(sp[0]);
	} while (sp[0]->wait_on_startup);
	ui_controller_init(sp[0], ssrc, mbus_engine_addr, mbus_ui_addr, mbus_video_addr);

	for (i = 0; i < num_sessions; i++) {
                audio_device_get_safe_config(&sp[i]->new_config);
                audio_device_reconfigure(sp[i]);
                assert(audio_device_is_open(sp[i]->audio_device));
		rtcp_init(sp[i], cname, ssrc, 0 /* XXX cur_time */);
	}

        ui_initial_settings(sp[0]);
        rtcp_clock_change(sp[0]);
	network_process_mbus(sp[0]);

	parse_late_options(argc, argv, sp);
	for (i = 0; i < num_sessions; i++) {
		ui_update(sp[i]);
	}
	network_process_mbus(sp[0]);

        if (tx_is_sending(sp[0]->tb) && (sp[0]->mode != AUDIO_TOOL)) {
		for (i=0; i<num_sessions; i++) {
                	tx_start(sp[i]->tb);
		}
        }

        /* dump buffered packets - it usually takes at least 1 second
         * to this far, all packets read thus far should be ignored.  
         * This stops lots of "skew" adaption at the start because the
         * playout buffer is too long.
         */
        
        for(i = 0; i < num_sessions; i++) {
                int dropped;
                dropped = read_and_discard(sp[i]->rtp_socket);
                debug_msg("Session %d dumped %d rtp packets\n", i, dropped);
                dropped = read_and_discard(sp[i]->rtcp_socket);
                debug_msg("Session %d dumped %d rtcp packets\n", i, dropped);
        }

        i = tcl_process_all_events();
        debug_msg("process %d events at startup %d\n", i);
	
	xdoneinit();

	while (!should_exit) {
		for (i = 0; i < num_sessions; i++) {
			if (sp[i]->mode == TRANSCODER) {
				elapsed_time = audio_rw_process(sp[i], sp[1-i], sp[i]->ms);
			} else {
				elapsed_time = audio_rw_process(sp[i], sp[i], sp[i]->ms);
			}
			cur_time = get_time(sp[i]->device_clock);
			ntp_time = ntp_time32();
                        sp[i]->cur_ts   = ts_seq32_in(&sp[i]->decode_sequencer, get_freq(sp[i]->device_clock), cur_time);
			timeout.tv_sec  = 0;
			timeout.tv_usec = 0;

			udp_fd_zero();
			udp_fd_set(sp[i]->rtp_socket);
			udp_fd_set(sp[i]->rtcp_socket);

			while (udp_select(&timeout) > 0) {
				if (udp_fd_isset(sp[i]->rtp_socket)) {
					read_and_enqueue(sp[i]->rtp_socket , sp[i]->cur_ts, sp[i]->rtp_pckt_queue, PACKET_RTP);
				}
				if (udp_fd_isset(sp[i]->rtcp_socket)) {
					read_and_enqueue(sp[i]->rtcp_socket, sp[i]->cur_ts, sp[i]->rtcp_pckt_queue, PACKET_RTCP);
				}
			}

			tx_process_audio(sp[i]->tb);
                        if (tx_is_sending(sp[i]->tb)) {
                                tx_send(sp[i]->tb);
                        }

                        /* Process incoming packets */
                        statistics_process(sp[i], sp[i]->rtp_pckt_queue, sp[i]->cushion, ntp_time, sp[i]->cur_ts);

                        /* Process and mix active sources */
			if (sp[i]->playing_audio) {
                                struct s_source *s;
                                int sidx, scnt;
                                ts_t cush_ts;
                                
                                cush_ts = ts_map32(get_freq(sp[i]->device_clock), cushion_get_size(sp[i]->cushion));
                                cush_ts = ts_add(sp[i]->cur_ts, cush_ts);
                                scnt = (int)source_list_source_count(sp[i]->active_sources);
                                for(sidx = 0; sidx < scnt; sidx++) {
                                        s = source_list_get_source_no(sp[i]->active_sources, sidx);
                                        if (source_relevant(s, sp[i]->cur_ts)) {
                                                source_check_buffering(s, sp[i]->cur_ts);
                                                source_process(s, sp[i]->ms, sp[i]->render_3d, sp[i]->repair, cush_ts);
                                                source_audit(s);
                                        } else {
                                                /* Remove source as stopped */
						ui_info_deactivate(sp[i], source_get_rtcp_dbentry(s));
                                                source_remove(sp[i]->active_sources, s);
                                                sidx--;
                                                scnt--;
                                        }
                                }
			}

			if (sp[i]->mode == TRANSCODER) {
				service_rtcp(sp[i], sp[1-i], sp[i]->rtcp_pckt_queue, cur_time, ntp_time);
			} else {
				service_rtcp(sp[i],    NULL, sp[i]->rtcp_pckt_queue, cur_time, ntp_time);
			}

			if (sp[i]->mode != TRANSCODER && alc >= 50) {
				if (!sp[i]->lecture && tx_is_sending(sp[i]->tb) && sp[i]->auto_lecture != 0) {
					gettimeofday(&time, NULL);
					if (time.tv_sec - sp[i]->auto_lecture > 120) {
						sp[i]->auto_lecture = 0;
						debug_msg("Dummy lecture mode\n");
					}
				}
				alc = 0;
			} else {
				alc++;
			}
			if (sp[i]->audio_device) ui_update_powermeters(sp[i], sp[i]->ms, elapsed_time);
                	if (sp[i]->ui_on) {
				tcl_process_events(sp[i]);
				mbus_send(sp[i]->mbus_ui); 
				mbus_recv(sp[i]->mbus_ui, (void *) sp[i]);
				mbus_retransmit(sp[i]->mbus_ui);
				mbus_heartbeat(sp[i]->mbus_ui, 10);
                	}
			mbus_send(sp[i]->mbus_engine); 
			mbus_recv(sp[i]->mbus_engine, (void *) sp[i]);
			mbus_retransmit(sp[i]->mbus_engine);
			mbus_heartbeat(sp[i]->mbus_engine, 10);

                        if (sp[i]->new_config != NULL) {
                                /* wait for mbus messages - closing audio device
                                 * can timeout unprocessed messages as some drivers
                                 * pause to drain before closing.
                                 */
                                network_process_mbus(sp[i]);
                                if (audio_device_reconfigure(sp[i])) {
                                        /* Device reconfigured so
                                         * decode paths of all sources
                                         * are misconfigured.  Delete
                                         * and incoming data will
                                         * drive correct new path */
                                        source_list_clear(sp[i]->active_sources);
                                }
                        }
                        
                        /* Choke CPU usage */
                        if (!audio_is_ready(sp[i]->audio_device)) {
                                audio_wait_for(sp[i]->audio_device, 10);
                        }
		}
        }

	for (i=0; i<num_sessions; i++) {
                tx_stop(sp[i]->tb);
		rtcp_exit(sp[i], sp[1-i], sp[i]->rtcp_socket);
		if (sp[i]->in_file  != NULL) snd_read_close (&sp[i]->in_file);
		if (sp[i]->out_file != NULL) snd_write_close(&sp[i]->out_file);
                audio_device_release(sp[i], sp[i]->audio_device);
		network_process_mbus(sp[i]);
	}

        if (sp[0]->mode == AUDIO_TOOL) {
                mbus_exit(sp[0]->mbus_engine);
                if (sp[0]->ui_on) {
                        mbus_exit(sp[0]->mbus_ui);
                	tcl_exit();
                }
        }
        
        for(i = 0; i < 2; i++) {
                end_session(sp[i]);
                xfree(sp[i]);
        }

        converters_free();
        audio_free_interfaces();

        xfree(cname);
	xmemdmp();
	return 0;
}

