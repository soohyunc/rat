/*
 * FILE:    ui_control.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
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

#ifndef _UI_UPDATE_H
#define _UI_UPDATE_H

struct session_tag;
struct s_rtcp_dbentry;
struct s_cbaddr;
struct s_mix_info;

void	   ui_info_update_name(struct s_rtcp_dbentry *e);
void	  ui_info_update_cname(struct s_rtcp_dbentry *e);
void	  ui_info_update_email(struct s_rtcp_dbentry *e);
void	  ui_info_update_phone(struct s_rtcp_dbentry *e);
void	    ui_info_update_loc(struct s_rtcp_dbentry *e);
void	   ui_info_update_tool(struct s_rtcp_dbentry *e);
void	   ui_info_update_note(struct s_rtcp_dbentry *e);
void	        ui_info_remove(struct s_rtcp_dbentry *e);
void	      ui_info_activate(struct s_rtcp_dbentry *e);
void	    ui_info_deactivate(struct s_rtcp_dbentry *e);

void	ui_show_audio_busy(void);
void	ui_hide_audio_busy(void);
void	ui_input_level(int level);
void	ui_output_level(int level);
void    ui_update_input_gain(struct session_tag *sp);
void    ui_update_output_gain(struct session_tag *sp);
void 	ui_update_input_port(struct session_tag *sp);
void    ui_update_frequency(struct session_tag *sp);
void    ui_update_channels(struct session_tag *sp);
void 	ui_update_output_port(struct session_tag *sp);
void	ui_update_primary(struct session_tag *sp);
void	ui_update_redundancy(struct session_tag *sp);
void	ui_update_interleaving(struct session_tag *sp);
void	ui_update_channel(struct session_tag *sp) ;
void	ui_update_powermeters(struct session_tag *sp, struct s_mix_info *ms, int elapsed_time);

void	ui_update_stats(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	ui_update_lecture_mode(struct session_tag *session_pointer);
void	ui_update(struct session_tag *session_pointer);
void	ui_update_loss(char *srce, char *dest, int loss);
void	ui_update_reception(char *cname, u_int32 recv, u_int32 lost, u_int32 misordered, u_int32 duplicates, u_int32 jitter, int jit_tog);
void	ui_update_duration(char *cname, int duration);

void	ui_update_video_playout(char *cname, int playout);
void	ui_update_sync(int sync);
void	ui_update_key(char *key);

void    ui_update_playback_file(char *name);
void    ui_update_record_file(char *name);
void    ui_update_file_live(char *mode, int valid);

void	ui_codecs(int pt);
void    ui_converters(void);
void    ui_sampling_modes(struct session_tag *sp);
void    ui_title(struct session_tag *sp);
void	ui_controller_init(char *cname, char *name_engine, char *name_ui, char *name_video);
void	ui_load_settings(void);
void    ui_quit(void);

#endif
