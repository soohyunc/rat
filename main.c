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
#include "converter.h"
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

static int tcl_process_events(session_struct *sp)
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
	u_int32			 ssrc, cur_time, ntp_time;
	int            		 i, elapsed_time, alc = 0, seed;
	char			*cname;
	session_struct 		*sp[2];
	struct timeval  	 time;
	struct timeval      	 timeout;
	char			 mbus_engine_addr[100], mbus_ui_addr[100], mbus_video_addr[100];
        u_int8                   j;

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
	parse_early_options(argc, argv, sp);
	network_init(sp[0]);
	cname = get_cname(sp[0]->rtp_socket[0]);
	ssrc  = get_ssrc();

        audio_init_interfaces();
        converters_init();
        statistics_init();

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

	ui_controller_init(sp[0], ssrc, mbus_engine_addr, mbus_ui_addr, mbus_video_addr);

        audio_device_get_safe_config(&sp[0]->new_config);
        audio_device_reconfigure(sp[0]);				/* UI */
        assert(audio_device_is_open(sp[0]->audio_device));
	sp[0]->db = rtcp_init(sp[0]->device_clock, cname, ssrc, 0);
        rtcp_clock_change(sp[0]);

	settings_load(sp[0]);
        ui_initial_settings(sp[0]);

	parse_late_options(argc, argv, sp);	/* Things which can override the settings we just loaded... */
	ui_update(sp[0]);			/* ...and push those to the UI */
	network_process_mbus(sp[0]);
        
        if (sp[0]->new_config != NULL) {
                network_process_mbus(sp[0]);
                audio_device_reconfigure(sp[0]);
                network_process_mbus(sp[0]);
        }

#ifdef NDEF
        ui_final_settings(sp[0]);
#endif
	network_process_mbus(sp[0]);

        if (tx_is_sending(sp[0]->tb)) {
               	tx_start(sp[0]->tb);
        }

        /* dump buffered packets - it usually takes at least 1 second
         * to this far, all packets read thus far should be ignored.  
         * This stops lots of "skew" adaption at the start because the
         * playout buffer is too long.
         */
        for(j=0; j<sp[0]->layers; j++) {
                read_and_discard(sp[0]->rtp_socket[j]);
        }
	read_and_discard(sp[0]->rtcp_socket);

        i = tcl_process_all_events();
        debug_msg("process %d events at startup %d\n", i);
	
	xdoneinit();

	while (!should_exit) {
		elapsed_time = audio_rw_process(sp[0], sp[0], sp[0]->ms);
		cur_time = get_time(sp[0]->device_clock);
		ntp_time = ntp_time32();
		sp[0]->cur_ts   = ts_seq32_in(&sp[0]->decode_sequencer, get_freq(sp[0]->device_clock), cur_time);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 0;

                udp_fd_zero();
                for(j=0; j<sp[0]->layers; j++) 
                        udp_fd_set(sp[0]->rtp_socket[j]);
                udp_fd_set(sp[0]->rtcp_socket);
                        
                while (udp_select(&timeout) > 0) {
                        for(j=0; j<sp[0]->layers; j++) {
                                if (udp_fd_isset(sp[0]->rtp_socket[j])) {
                                        read_and_enqueue(sp[0]->rtp_socket[j], sp[0]->cur_ts, sp[0]->rtp_pckt_queue, PACKET_RTP);
                                        
                                }
                        }
                        if (udp_fd_isset(sp[0]->rtcp_socket)) {
                                read_and_enqueue(sp[0]->rtcp_socket, sp[0]->cur_ts, sp[0]->rtcp_pckt_queue, PACKET_RTCP);
                        }
                }
                        
                tx_process_audio(sp[0]->tb);
                if (tx_is_sending(sp[0]->tb)) {
                        tx_send(sp[0]->tb);
                }

                        /* Need to either:                               *
                         * (i) join layers together by this stage        *
                         * (ii) modify statistics_process to join layers */
                        
		/* Process incoming packets */
		statistics_process(sp[0], sp[0]->rtp_pckt_queue, sp[0]->cushion, ntp_time, sp[0]->cur_ts);

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
					ui_info_deactivate(sp[0], source_get_rtcp_dbentry(s));
					source_remove(sp[0]->active_sources, s);
					sidx--;
					scnt--;
				}
			}
		}

		service_rtcp(sp[0], NULL, sp[0]->rtcp_pckt_queue, cur_time, ntp_time);

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
	rtcp_exit(sp[0], NULL, sp[0]->rtcp_socket);
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
                end_session(sp[i]);
                xfree(sp[i]);
        }

        converters_free();
        audio_free_interfaces();

        xfree(cname);
	xmemdmp();
	return 0;
}

