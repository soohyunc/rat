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
#include "codec_types.h"
#include "codec.h"
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
        ts_t                     cur_ts;
	int            		 num_sessions, i, elapsed_time, alc = 0;
	char			*cname;
	session_struct 		*sp[2];
	struct timeval  	 time;
	struct timeval      	 timeout;
	char			 mbus_engine_addr[50], mbus_ui_addr[50], mbus_video_addr[50];
        
#ifndef WIN32
 	signal(SIGINT, signal_handler); 
#endif

	gettimeofday(&time, NULL);
	srand48(time.tv_usec);
	lbl_srandom(time.tv_usec);

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

	ui_controller_init(sp[0], cname, mbus_engine_addr, mbus_ui_addr, mbus_video_addr);
	do {
		network_process_mbus(sp[0]);
		mbus_heartbeat(sp[0]->mbus_engine, 1);
		usleep(20000);
	} while (sp[0]->wait_on_startup);

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
                        cur_ts   = ts_seq32_in(&sp[i]->decode_sequencer,
                                               get_freq(sp[i]->device_clock),
                                               cur_time);

			timeout.tv_sec  = 0;
			timeout.tv_usec = 0;

			udp_fd_zero();
			udp_fd_set(sp[i]->rtp_socket);
			udp_fd_set(sp[i]->rtcp_socket);
			if (udp_select(&timeout) > 0) {
				if (udp_fd_isset(sp[i]->rtp_socket)) {
					read_and_enqueue(sp[i]->rtp_socket , cur_ts, sp[i]->rtp_pckt_queue, PACKET_RTP);
				}
				if (udp_fd_isset(sp[i]->rtcp_socket)) {
					read_and_enqueue(sp[i]->rtcp_socket, cur_ts, sp[i]->rtcp_pckt_queue, PACKET_RTCP);
				}
			}

			tx_process_audio(sp[i]->tb);

                        if (tx_is_sending(sp[i]->tb)) {
                                tx_send(sp[i]->tb);
                        }

                        /* Process incoming packets */
                        statistics(sp[i], 
                                   sp[i]->rtp_pckt_queue, 
                                   sp[i]->cushion, 
                                   ntp_time);

                        /* Process and mix active sources */
			if (sp[i]->playing_audio) {
                                struct s_source *s;
                                int sidx, scnt;
                                ts_t cush_ts;
                                
                                cush_ts = ts_map32(get_freq(sp[i]->device_clock), 
                                                   cushion_get_size(sp[i]->cushion));
                                cush_ts = ts_add(cur_ts, cush_ts);
                                scnt = (int)source_list_source_count(sp[i]->active_sources);
                                for(sidx = 0; sidx < scnt; sidx++) {

                                        s = source_list_get_source_no(sp[i]->active_sources, sidx);
                                        source_process(s, sp[i]->ms, sp[i]->render_3d, sp[i]->repair, cush_ts);
                                        source_audit(s);
                                        if (!source_relevant(s, cush_ts)) {
                                                /* Remove source as stopped */
                                                source_remove(sp[i]->active_sources, s);
                                                sidx--;
                                                scnt--;
                                        } else {
                                                /*
                                                source_audit(s);
                                                */
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
				mbus_send(sp[i]->mbus_ui); mbus_retransmit(sp[i]->mbus_ui);
                	}
			mbus_retransmit(sp[i]->mbus_engine);
			mbus_send(sp[i]->mbus_engine); 
			mbus_recv(sp[i]->mbus_engine, (void *) sp[i]);
			mbus_recv(sp[i]->mbus_ui    , (void *) sp[i]);
			mbus_heartbeat(sp[i]->mbus_engine, 10);

                        if (sp[i]->new_config != NULL) {
                                /* wait for mbus messages - closing audio device
                                 * can timeout unprocessed messages as some drivers
                                 * pause to drain before closing.
                                 */
                                network_process_mbus(sp[i]);
                                audio_device_reconfigure(sp[i]);
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
                }
        }

        
        if (sp[0]->ui_on) {
                tcl_exit();
        }

        for(i = 0; i<2; i++) {
                end_session(sp[i]);
                xfree(sp[i]);
        }

        xfree(cname);

        converters_free();
        audio_free_interfaces();

	xmemdmp();
	return 0;
}

