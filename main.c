/*
 * FILE:    main.c
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995,1996,1997,1998 University College London
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

#include "config.h"
#ifndef WIN32
#include <signal.h>
#endif
#include <math.h>

#include "session.h"
#include "receive.h"
#include "util.h"
#include "audio.h"
#include "convert.h"
#include "tcltk.h"
#include "ui_update.h"
#include "interfaces.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "rtcp.h"
#include "net.h"
#include "agc.h"
#include "statistics.h"
#include "parameters.h"
#include "transmit.h"
#include "speaker_table.h"
#include "mix.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"

int should_exit = FALSE;
int thread_pri;

#ifndef WIN32
static void
signal_handler(int signal)
{
  dprintf("Caught signal %d\n", signal);
  should_exit = TRUE;
}
#endif

static int waiting_on_mbus(session_struct *sp[2], int num_sessions) 
{
	int i;

	for (i=0; i<num_sessions; i++) {
		if (mbus_waiting_acks(sp[i]->mbus_engine_base)) return TRUE;
		if (mbus_waiting_acks(sp[i]->mbus_engine_chan)) return TRUE;
		if (mbus_waiting_acks(sp[i]->mbus_ui_base))     return TRUE;
		if (mbus_waiting_acks(sp[i]->mbus_ui_chan))     return TRUE;
	}
	return FALSE;
}

int
main(int argc, char *argv[])
{
	u_int32			 ssrc, cur_time;
	int            		 num_sessions, i, elapsed_time, alc = 0;
	char			*cname;
	session_struct 		*sp[2];
	int             	 tx_active = FALSE;
	int             	 rx_active = FALSE;
	struct s_mix_info 	*ms[2];
	struct timeval  	 time;

#define NEW_QUEUE(T,Q) T  Q[2]; \
                       T *Q##_p[2];

	NEW_QUEUE(pckt_queue_struct, netrx_queue)
	NEW_QUEUE(rx_queue_struct,   rx_queue)
	NEW_QUEUE(pckt_queue_struct, rtcp_pckt_queue)
	NEW_QUEUE(rx_queue_struct,   rx_unit_queue)

#define INIT_QUEUE(T,Q) Q##_p[0] = &Q##[0]; \
			Q##_p[0]->queue_empty = 1; \
                        Q##_p[0]->head_ptr    = NULL; \
                        Q##_p[0]->tail_ptr    = NULL; \
			Q##_p[1] = &Q##[1]; \
			Q##_p[1]->queue_empty = 1; \
                        Q##_p[1]->head_ptr    = NULL; \
                        Q##_p[1]->tail_ptr    = NULL;

	INIT_QUEUE(pckt_queue_struct, netrx_queue)
	INIT_QUEUE(rx_queue_struct,   rx_queue)
	INIT_QUEUE(pckt_queue_struct, rtcp_pckt_queue)
	INIT_QUEUE(rx_queue_struct,   rx_unit_queue)

	thread_pri = 2;			/* TIME_CRITICAL */
	cname      = get_cname();
	ssrc       = get_ssrc();

	for (i = 0; i < 2;i++) {
		sp[i] = (session_struct *) xmalloc(sizeof(session_struct));
		init_session(sp[i]);
	}
	num_sessions = parse_options(argc, argv, sp);

	for (i = 0; i < num_sessions; i++) {
		sp[i]->mbus_engine_addr = (char *) xmalloc(30); sprintf(sp[i]->mbus_engine_addr, "(audio engine rat %d)", (int) getpid());
		sp[i]->mbus_ui_addr     = (char *) xmalloc(30); sprintf(sp[i]->mbus_ui_addr,     "(audio     ui rat %d)", (int) getpid());
		sp[i]->mbus_video_addr  = (char *) xmalloc(30); sprintf(sp[i]->mbus_video_addr,  "(video engine   *  *)");

		sp[i]->mbus_engine_base = mbus_init(0, mbus_handler_engine, NULL); mbus_addr(sp[i]->mbus_engine_base, sp[i]->mbus_engine_addr);
		sp[i]->mbus_ui_base     = mbus_init(0, mbus_handler_ui,     NULL); mbus_addr(sp[i]->mbus_ui_base    , sp[i]->mbus_ui_addr);

		if (sp[i]->mbus_channel == 0) {
			dprintf("No mbus channel specified, using the base for all messages...\n");
			sp[i]->mbus_engine_chan = sp[i]->mbus_engine_base;
			sp[i]->mbus_ui_chan     = sp[i]->mbus_ui_base;
		} else {
			dprintf("Using mbus channel %d\n", sp[i]->mbus_channel);
			sp[i]->mbus_engine_chan = mbus_init(sp[i]->mbus_channel, mbus_handler_engine, NULL); mbus_addr(sp[i]->mbus_engine_chan, sp[i]->mbus_engine_addr);
			sp[i]->mbus_ui_chan     = mbus_init(sp[i]->mbus_channel, mbus_handler_ui,     NULL); mbus_addr(sp[i]->mbus_ui_chan    , sp[i]->mbus_ui_addr);
		}
	}

        if (sp[0]->ui_on) {
		tcl_init(sp[0], cname, argc, argv);
        }

	 /* Now initialise everything else... */
	for (i = 0; i < num_sessions; i++) {
		rtcp_init(sp[i], cname, ssrc, 0 /* XXX cur_time */);
		audio_init(sp[i]);
		network_init(sp[i]);
		if (!audio_device_take(sp[i])) {
			ui_show_audio_busy(sp[i]);
		}
                read_write_init(sp[i]);
		ms[i] = init_mix(sp[i], 32640);
	}
	agc_table_init();
        set_converter(CONVERT_LINEAR);

	/* Show the interface before starting processing */
	while (tcl_process_event()) {
		network_process_mbus(sp, num_sessions, 20);
	}
        ui_update(sp[0]);
	network_process_mbus(sp, num_sessions, 1000);

        if (!sp[0]->sending_audio && (sp[0]->mode != AUDIO_TOOL)) {
		for (i=0; i<num_sessions; i++) {
                	start_sending(sp[i]);
		}
        }

#ifndef WIN32
	/* Okay, at this point we're ready to go! Set up a signal handler, to catch any nastyness, */
	/* and send an RTCP BYE packet if we're interrupted...                               [csp] */
	signal(SIGINT, signal_handler);
#endif
	
	xdoneinit();

	while (TRUE) {
		for (i = 0; i < num_sessions; i++) {
			if (sp[i]->mode == TRANSCODER) {
				elapsed_time = read_write_audio(sp[i], sp[1-i], ms[i]);
			} else {
				elapsed_time = read_write_audio(sp[i], sp[i], ms[i]);
			}
			cur_time = get_time(sp[i]->device_clock);
			network_read(sp[i], netrx_queue_p[i], rtcp_pckt_queue_p[i], cur_time);
			tx_active = process_read_audio(sp[i]);

			if (sp[i]->playing_audio) {
				rx_active = !netrx_queue_p[i]->queue_empty || !rx_unit_queue_p[i]->queue_empty ||
					      	sp[i]->playout_buf_list != NULL;
			} else {
				receive_unit_audit(rx_unit_queue_p[i]);
			        clear_old_history(&sp[i]->playout_buf_list);
				rx_active = FALSE;
			}

                        if (sp[i]->sending_audio || sp[i]->last_tx_service_productive) {
                            if (!rx_active || (sp[i]->voice_switching != NET_MUTES_MIKE)) {
                                service_transmitter(sp[i], sp[1-i]->speakers_active);
                                if (sp[i]->voice_switching == MIKE_MUTES_NET) {
                                    rx_active = FALSE;
                                }
                            }
                        }

			/* Impose RTP formatting on the packets in the netrx_queue and update RTP */
			/* reception statistics. Packets are moved to rx_unit_queue.        [csp] */
			statistics(sp[i], netrx_queue_p[i], rx_unit_queue_p[i], sp[i]->cushion, cur_time);

			if (rx_active) {
				audio_switch_out(sp[i]->audio_fd, sp[i]->cushion);
				service_receiver(sp[i], rx_unit_queue_p[i], &sp[i]->playout_buf_list, ms[i]);
				if (tx_active && (sp[i]->voice_switching == NET_MUTES_MIKE)) {
					sp[i]->transmit_audit_required = TRUE;
				}
			} else {
				audio_switch_in(sp[i]->audio_fd);
				if (!sp[i]->playing_audio || (sp[i]->voice_switching == MIKE_MUTES_NET)) {
					sp[i]->receive_audit_required = TRUE;
				}
			}

			/* Do funky RTCP stuff... */
			if (sp[i]->mode == TRANSCODER) {
				service_rtcp(sp[i], sp[1-i], rtcp_pckt_queue_p[i], cur_time);
			} else {
				service_rtcp(sp[i],    NULL, rtcp_pckt_queue_p[i], cur_time);
			}

			/* Schedule any outstanding retransmissions of mbus messages... */
			mbus_retransmit(sp[i]->mbus_engine_base);
			mbus_retransmit(sp[i]->mbus_ui_base);
			if (sp[i]->mbus_channel != 0) {
				mbus_retransmit(sp[i]->mbus_engine_chan);
				mbus_retransmit(sp[i]->mbus_ui_chan);
			}

			/* Maintain last_sent dummy lecture var */
			if (sp[i]->mode != TRANSCODER && alc >= 50) {
				if (!sp[i]->lecture && !sp[i]->sending_audio && sp[i]->auto_lecture != 0) {
					gettimeofday(&time, NULL);
					if (time.tv_sec - sp[i]->auto_lecture > 120) {
						sp[i]->auto_lecture = 0;
						dprintf("Dummy lecture mode\n");
					}
				}
				alc = 0;
			} else {
				alc++;
			}

                	if (sp[i]->ui_on) {
				ui_update_powermeters(sp[i], ms[i], elapsed_time);
				if (tcl_active()) {
					tcl_process_events();
				} else {
					should_exit = TRUE;
				}
                	} 

			if ((sp[i]->mode == FLAKEAWAY) && (sp[i]->flake_go == 0) && (sp[i]->flake_os < 0)) {
                                should_exit = TRUE;
                        }

			/* Exit? */
			if (should_exit && !waiting_on_mbus(sp, num_sessions)) {
				if (tcl_active()) {
					tcl_send("destroy .");
				}
				for (i=0; i<num_sessions; i++) {
                                	if (sp[i]->in_file  != NULL) fclose(sp[i]->in_file);
                                	if (sp[i]->out_file != NULL) fclose(sp[i]->out_file);
                                	rtcp_exit(sp[i], sp[1-i], sp[i]->rtcp_fd, sp[i]->net_maddress, sp[i]->rtcp_port);
					if (sp[i]->mode != TRANSCODER) {
                                		audio_close(sp[i]->audio_fd);
					}
				}
                                xmemdmp();
                                return 0;
                        }
		}
        }
}

