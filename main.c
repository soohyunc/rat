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
#include "cushion.h"
#include "convert.h"
#include "tcltk.h"
#include "ui_control.h"
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

int
main(int argc, char *argv[])
{
	u_int32			 ssrc, cur_time;
	int            		 num_sessions, i, elapsed_time, alc = 0;
	char			*cname;
	session_struct 		*sp[2];
	struct timeval  	 time;
	char			 mbus_engine_addr[30], mbus_ui_addr[30], mbus_video_addr[30];

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

	gettimeofday(&time, NULL);
	srand48(time.tv_usec);

	sprintf(mbus_engine_addr, "(audio engine rat %d)", (int) getpid());
	sprintf(mbus_ui_addr,     "(audio     ui rat %d)", (int) getpid());
	sprintf(mbus_video_addr,  "(video engine   *  *)");

	for (i = 0; i < 2;i++) {
		sp[i] = (session_struct *) xmalloc(sizeof(session_struct));
		init_session(sp[i]);
	}
	num_sessions = parse_options(argc, argv, sp);
	thread_pri   = 2;	/* TIME_CRITICAL */
	cname        = get_cname();
	ssrc         = get_ssrc();

	for (i = 0; i < num_sessions; i++) {
		mbus_engine_init(mbus_engine_addr, sp[i]->mbus_channel);
		mbus_ui_init(mbus_ui_addr, sp[i]->mbus_channel);
	}

        if (sp[0]->ui_on) {
		tcl_init(sp[0], argc, argv, mbus_engine_addr);
        }

	ui_controller_init(cname, mbus_engine_addr, mbus_ui_addr, mbus_video_addr);
        ui_sampling_modes(sp[0]);
	ui_codecs(sp[0]->encodings[0]);

	for (i = 0; i < num_sessions; i++) {
		network_init(sp[i]);
		rtcp_init(sp[i], cname, ssrc, 0 /* XXX cur_time */);
		audio_device_take(sp[i]);
	}
	agc_table_init();
        set_converter(CONVERT_LINEAR);

	ui_info_update_cname(sp[0]->db->my_dbe);
	ui_info_update_tool(sp[0]->db->my_dbe);
	ui_load_settings();
	network_process_mbus(sp, num_sessions, 1000);

        if (!sp[0]->sending_audio && (sp[0]->mode != AUDIO_TOOL)) {
		for (i=0; i<num_sessions; i++) {
                	tx_start(sp[i]);
		}
        }

#ifndef WIN32
	/* Okay, at this point we're ready to go! Set up a signal handler, to catch any nastyness, */
	/* and send an RTCP BYE packet if we're interrupted...                               [csp] */
	signal(SIGINT, signal_handler);
#endif
	
	xdoneinit();

	while (!should_exit) {
		for (i = 0; i < num_sessions; i++) {
			if (sp[i]->mode == TRANSCODER) {
				elapsed_time = read_write_audio(sp[i], sp[1-i], sp[i]->ms);
			} else {
				elapsed_time = read_write_audio(sp[i], sp[i], sp[i]->ms);
			}
			cur_time = get_time(sp[i]->device_clock);
			network_read(sp[i], netrx_queue_p[i], rtcp_pckt_queue_p[i], cur_time);
			tx_process_audio(sp[i]);

			if (!(sp[i]->playing_audio)) {
				receive_unit_audit(rx_unit_queue_p[i]);
			        clear_old_history(&sp[i]->playout_buf_list);
			}

                        if (sp[i]->sending_audio || sp[i]->last_tx_service_productive) {
                                tx_send(sp[i], sp[1-i]->speakers_active);
                        }
			statistics(sp[i], netrx_queue_p[i], rx_unit_queue_p[i], sp[i]->cushion, cur_time);
			service_receiver(sp[i], rx_unit_queue_p[i], &sp[i]->playout_buf_list, sp[i]->ms);
			if (sp[i]->mode == TRANSCODER) {
				service_rtcp(sp[i], sp[1-i], rtcp_pckt_queue_p[i], cur_time);
			} else {
				service_rtcp(sp[i],    NULL, rtcp_pckt_queue_p[i], cur_time);
			}
			mbus_engine_retransmit();
			mbus_ui_retransmit();
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
				ui_update_powermeters(sp[i], sp[i]->ms, elapsed_time);
				tcl_process_events();
                	} 
		}
        }

	for (i=0; i<num_sessions; i++) {
		rtcp_exit(sp[i], sp[1-i], sp[i]->rtcp_fd, sp[i]->net_maddress, sp[i]->rtcp_port);
		if (sp[i]->in_file  != NULL) fclose(sp[i]->in_file);
		if (sp[i]->out_file != NULL) fclose(sp[i]->out_file);
                audio_device_give(sp[i]);
	}
	network_process_mbus(sp, num_sessions, 1000);

        for(i = 0; i<2; i++) {
                end_session(sp[i]);
                xfree(sp[i]);
        }

	xmemdmp();
	return 0;
}

