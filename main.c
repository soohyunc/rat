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
#include <pwd.h>
#include <signal.h>
#endif
#include <math.h>
#include <tcl.h>
#include <tk.h>

#include "session.h"
#include "receive.h"
#include "transmit.h"
#include "util.h"
#include "audio.h"
#include "convert.h"
#include "ui.h"
#include "mix.h"
#include "interfaces.h"
#include "lbl_confbus.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "rtcp.h"
#include "net.h"
#include "agc.h"
#include "statistics.h"
#include "parameters.h"
#include "speaker_table.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_engine.h"

/* Global variable: if TRUE, rat will exit on the next iteration of the main loop [csp] */
int should_exit = FALSE;

extern Tcl_Interp *interp; 
int thread_pri;

#ifndef WIN32
void
signal_handler(int signal)
{
#ifdef DEBUG
  printf("Caught signal %d\n", signal);
#endif
  should_exit = TRUE;
}
#endif

int
main(int argc, char *argv[])
{
	u_int32			 ssrc = 0, cur_time;
	int            		 num_sessions, i, l, elapsed_time, power_time = 0, alc = 0;
	char           		*uname;
	char	       		*hname;
	char			 cname[MAXHOSTNAMELEN + 10];
	session_struct 		*sp[2];
	cushion_struct	 	 cushion[2];
	int             	 transmit_active_flag = FALSE;
	int             	 receive_active_flag  = FALSE;
	struct s_mix_info 	*ms[2];
	struct passwd  		*pwent;
	struct hostent 		*hent;
	struct in_addr  	 iaddr;
	struct timeval  	 time;
	struct mbus		*mbus_engine, *mbus_ui;
	char			*mbus_engine_addr, *mbus_ui_addr;

#define NEW_QUEUE(T,Q) T  Q[2]; \
                       T *Q##_ptr[2];

	NEW_QUEUE(pckt_queue_struct, netrx_queue)
	NEW_QUEUE(rx_queue_struct,   rx_queue)
	NEW_QUEUE(pckt_queue_struct, rtcp_pckt_queue)
	NEW_QUEUE(rx_queue_struct,   rx_unit_queue)

#define INIT_QUEUE(T,Q) Q##_ptr[0] = &Q##[0]; \
			Q##_ptr[0]->queue_empty_flag = 1; \
                        Q##_ptr[0]->head_ptr         = NULL; \
                        Q##_ptr[0]->tail_ptr         = NULL; \
			Q##_ptr[1] = &Q##[1]; \
			Q##_ptr[1]->queue_empty_flag = 1; \
                        Q##_ptr[1]->head_ptr         = NULL; \
                        Q##_ptr[1]->tail_ptr         = NULL;

	INIT_QUEUE(pckt_queue_struct, netrx_queue)
	INIT_QUEUE(rx_queue_struct,   rx_queue)
	INIT_QUEUE(pckt_queue_struct, rtcp_pckt_queue)
	INIT_QUEUE(rx_queue_struct,   rx_unit_queue)

	thread_pri = 2;	/* TIME_CRITICAL */

	for (i = 0; i < 2;i++) {
		sp[i] = (session_struct *) xmalloc(sizeof(session_struct));
		init_session(sp[i]);
	}
	num_sessions = parse_options(argc, argv, sp);

	if (sp[0]->mode == AUDIO_TOOL) {
		lbl_cb_init(sp[0]);
	}

	/* Set the CNAME. This is "user@hostname" or just "hostname" if the username cannot be found. */
	/* First, fill in the username.....                                                           */
#ifdef WIN32
	if ((uname = getenv("USER")) == NULL) {
		uname = "unknown";
	}
#else 		/* ...it's a unix machine... */
	pwent = getpwuid(getuid());
	uname = pwent->pw_name;
#endif
	sprintf(cname, "%s@", uname);

	/* Now the hostname. Must be FQDN or dotted-quad IP address (RFC1889) */
	hname = cname + strlen(cname);
	if (gethostname(hname, MAXHOSTNAMELEN) != 0) {
		perror("Cannot get hostname!");
		return 1;
	}
	hent = gethostbyname(hname);
	memcpy(&iaddr.s_addr, hent->h_addr, sizeof(iaddr.s_addr));
	strcpy(hname, inet_ntoa(iaddr));
	/* Pick an SSRC value... */
	gettimeofday(&time, NULL);
	srand48(time.tv_usec);
	while (!(ssrc = lrand48()));	/* Making 0 a special value */

	mbus_engine_addr = "(audio engine rat 0)";
	mbus_ui_addr     = "(audio     ui rat 0)";
	mbus_engine      = mbus_init(mbus_engine_addr, mbus_handler_engine);
	mbus_ui          = mbus_init(mbus_ui_addr, mbus_handler_ui);
	for (i = 0; i < num_sessions; i++) {
		sp[i]->mbus_engine      = mbus_engine;
		sp[i]->mbus_ui          = mbus_ui;
		sp[i]->mbus_engine_addr = mbus_engine_addr;
		sp[i]->mbus_ui_addr     = mbus_ui_addr;
	}

	for (i = 0; i < num_sessions; i++) {
		rtcp_init(sp[i], cname, ssrc, 0 /* XXX cur_time */);
	}

        if (sp[0]->ui_on) {
		ui_init(sp[0], argc, argv);
        }

	 /* Now initialise everything else... */
	for (i = 0; i < num_sessions; i++) {
		audio_init(sp[i], &cushion[i]);
		network_init(sp[i]);
		if (audio_device_take(sp[i]) == FALSE) {
			if (sp[i]->ui_on) {
				ui_show_audio_busy(sp[i]);
			}
		}
                read_write_init(&cushion[i], sp[i]);
		ms[i] = init_mix(sp[i], 32640);
	}
	agc_table_init();
        set_converter(CONVERT_LINEAR);
	/* Show the interface before starting processing */
        if (sp[0]->ui_on) {
                while(Tcl_DoOneEvent(TCL_DONT_WAIT | TK_X_EVENTS | TCL_IDLE_EVENTS)) {
			/* Do nothing! */
		}
                ui_update(sp[0]);
        }

        if ((sp[0]->sending_audio == FALSE) && (sp[0]->mode != AUDIO_TOOL)) {
		for (i=0; i<num_sessions; i++) {
                	start_sending(sp[i]);
		}
        }

#ifndef WIN32
	/* Okay, at this point we're ready to go! Set up a signal handler, to catch any nastyness, */
	/* and send an RTCP BYE packet if we're interrupted...                               [csp] */
	signal(SIGINT, signal_handler);
#endif
        
	for (;;) {
		for (i = 0; i < num_sessions; i++) {
			if (sp[i]->mode == TRANSCODER) {
				elapsed_time = read_write_audio(sp[i], sp[1-i], &cushion[i], ms[i]);
			} else {
				elapsed_time = read_write_audio(sp[i], sp[i], &cushion[i], ms[i]);
			}
			cur_time = get_time(sp[i]->device_clock);
			network_read(sp[i], netrx_queue_ptr[i], rtcp_pckt_queue_ptr[i], cur_time);
	
			if (sp[i]->sending_audio == TRUE) {
				transmit_active_flag = process_read_audio(sp[i]);
			} else {
				transmit_active_flag = FALSE;
			}
	
			if (sp[i]->playing_audio == TRUE) {
				receive_active_flag = !netrx_queue_ptr[i]->queue_empty_flag    ||
					      	!rx_unit_queue_ptr[i]->queue_empty_flag  ||
					      	sp[i]->playout_buf_list != NULL;
			} else {
				receive_unit_audit(rx_unit_queue_ptr[i]);
			        clear_old_history(&sp[i]->playout_buf_list, sp[i]);
				receive_active_flag = FALSE;
			}
	
			if (sp[i]->sending_audio == TRUE) {
				if (receive_active_flag == FALSE || sp[i]->voice_switching != NET_MUTES_MIKE) {
					service_transmitter(sp[i], sp[1-i]->speakers_active);
					if (sp[i]->voice_switching == MIKE_MUTES_NET) {
						receive_active_flag = FALSE;
					}
				}
			}
	
			/* Impose RTP formatting on the packets in the netrx_queue
			 * and update RTP reception statistics. Packets are moved to
			 * rx_unit_queue. [csp[i]] */
			while (netrx_queue_ptr[i]->queue_empty_flag == FALSE) {
				statistics(sp[i], netrx_queue_ptr[i], rx_unit_queue_ptr[i], &cushion[i], cur_time);
			}
		
			if (receive_active_flag == TRUE) {
				audio_switch_out(sp[i]->audio_fd, &cushion[i]);
				service_receiver(&cushion[i], sp[i], rx_unit_queue_ptr[i], &sp[i]->playout_buf_list, ms[i]);
				if (transmit_active_flag && (sp[i]->voice_switching == NET_MUTES_MIKE)) {
					sp[i]->transmit_audit_required = TRUE;
				}
			} else {
				audio_switch_in(sp[i]->audio_fd);
				if (sp[i]->playing_audio == FALSE || sp[i]->voice_switching == MIKE_MUTES_NET) {
					sp[i]->receive_audit_required = TRUE;
				}
			}
	
			/* Do funky RTCP stuff... */
			if (rtcp_pckt_queue_ptr[i]->queue_empty_flag == FALSE) {
				if (sp[i]->mode == TRANSCODER) {
					service_rtcp(sp[i], sp[1-i], rtcp_pckt_queue_ptr[i], cur_time);
				} else {
					service_rtcp(sp[i],    NULL, rtcp_pckt_queue_ptr[i], cur_time);
				}
			}
			rtcp_update(sp[i], sp[i]->rtcp_fd, sp[i]->net_maddress, sp[i]->rtcp_port);
	
			if (sp[0]->mode == AUDIO_TOOL) {
				/* LBL Conference bus... */
				lbl_cb_read(sp[i]);
			}

			if (power_time == 0 && sp[i]->ui_on) {
				if (sp[i]->meter)
					mix_update_ui(ms[i], sp[i]);
				clear_active_senders(sp[i]);
			}
	
			if (power_time > 400) {
				if (sp[i]->sending_audio && sp[i]->ui_on) {
					transmitter_update_ui(sp[i]);
				}
				power_time = 0;
			} else {
				power_time += elapsed_time;
			}

			/* Schedule any outstanding retransmissions of mbus messages... */
			mbus_retransmit(sp[i]->mbus_engine);
			mbus_retransmit(sp[i]->mbus_ui);

			/* Maintain last_sent dummy lecture var */
			if (sp[i]->mode != TRANSCODER && alc >= 50) {
				if (sp[i]->lecture == FALSE && sp[i]->sending_audio == FALSE && sp[i]->auto_lecture != 0) {
					gettimeofday(&time, NULL);
					if (time.tv_sec - sp[i]->auto_lecture > 120) {
						sp[i]->auto_lecture = 0;
#ifdef DEBUG
						printf("Dummy lecture mode\n");
#endif
					}
				}
				alc = 0;
			} else
				alc++;

			/* Process UI Might want to not update every cycle, or bring up to date in one go... */
                	if (sp[i]->ui_on) {
                        	if (Tk_GetNumMainWindows() > 0) {
                                	for (l = sp[i]->ui_response; l > 0; l--) {
                                        	if (!Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT)) {
                                                	break;
						}
                                	}
                        	} else {
					/* Someone's closed the main RAT window... */
					should_exit = TRUE;
				}
                	} 
			
			if ((sp[i]->mode == FLAKEAWAY) && (sp[i]->flake_go == 0) && (sp[i]->flake_os < 0)) {
                                should_exit = TRUE;
                        }

			/* Exit? */
			if (should_exit) {
				if (Tk_GetNumMainWindows() > 0) {
					Tcl_Eval(interp, "destroy .");
				}
				for (i=0; i<num_sessions; i++) {
                                	if (sp[i]->in_file  != NULL) fclose(sp[i]->in_file);
                                	if (sp[i]->out_file != NULL) fclose(sp[i]->out_file);
                                	rtcp_exit(sp[i], sp[1-i], sp[i]->rtcp_fd, sp[i]->net_maddress, sp[i]->rtcp_port);
					if (sp[i]->mode != TRANSCODER) {
                                		audio_close(sp[i]->audio_fd);
					}
					lbl_cb_send_release(sp[i], 0);
				}
                                return 0;
                        }
		}
        }
}

