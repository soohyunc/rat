/*
 * FILE:    ui_update.h
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

void	   ui_info_update_name(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	  ui_info_update_cname(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	  ui_info_update_email(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	  ui_info_update_phone(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	    ui_info_update_loc(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	   ui_info_update_tool(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	        ui_info_remove(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	      ui_info_activate(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	          ui_info_gray(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	    ui_info_deactivate(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	ui_update_loss_from_me(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	  ui_update_loss_to_me(struct s_rtcp_dbentry *e, struct session_tag *sp);

void	    ui_show_audio_busy(struct session_tag *sp);
void	    ui_hide_audio_busy(struct session_tag *sp);
void	        ui_input_level(int level, struct session_tag *sp);
void	       ui_output_level(int level, struct session_tag *sp);
void 	  ui_update_input_port(struct session_tag *sp);
void 	 ui_update_output_port(struct session_tag *sp);
void	  ui_update_redundancy(struct session_tag *sp);
void	  ui_update_interleaving(struct session_tag *sp);

void	update_stats(struct s_rtcp_dbentry *e, struct session_tag *sp);
void	update_lecture_mode(struct session_tag *session_pointer);
void	ui_update(struct session_tag *session_pointer);

#endif
