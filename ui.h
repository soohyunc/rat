/*
 * FILE:    ui_control.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef _UI_UPDATE_H
#define _UI_UPDATE_H

#include "codec_types.h"

struct session_tag;
struct s_rtcp_dbentry;
struct s_cbaddr;
struct s_mix_info;

void	   ui_info_update_name(struct session_tag *s, struct s_rtcp_dbentry *e);
void	  ui_info_update_cname(struct session_tag *s, struct s_rtcp_dbentry *e);
void	  ui_info_update_email(struct session_tag *s, struct s_rtcp_dbentry *e);
void	  ui_info_update_phone(struct session_tag *s, struct s_rtcp_dbentry *e);
void	    ui_info_update_loc(struct session_tag *s, struct s_rtcp_dbentry *e);
void	   ui_info_update_tool(struct session_tag *s, struct s_rtcp_dbentry *e);
void	   ui_info_update_note(struct session_tag *s, struct s_rtcp_dbentry *e);
void	          ui_info_mute(struct session_tag *s, struct s_rtcp_dbentry *e);
void	          ui_info_gain(struct session_tag *s, struct s_rtcp_dbentry *e);
void	        ui_info_remove(struct session_tag *s, struct s_rtcp_dbentry *e);
void	      ui_info_activate(struct session_tag *s, struct s_rtcp_dbentry *e);
void	    ui_info_deactivate(struct session_tag *s, struct s_rtcp_dbentry *e);
void       ui_info_3d_settings(struct session_tag *s, struct s_rtcp_dbentry *e);

void	ui_show_audio_busy(struct session_tag *s);
void	ui_hide_audio_busy(struct session_tag *s);
void	ui_input_level(struct session_tag *s, int level);
void	ui_output_level(struct session_tag *s, int level);
void    ui_update_input_gain(struct session_tag *sp);
void    ui_update_output_gain(struct session_tag *sp);
void 	ui_update_input_port(struct session_tag *sp);
void    ui_update_device_config(struct session_tag *sp);
void 	ui_update_output_port(struct session_tag *sp);
void	ui_update_primary(struct session_tag *sp);
void	ui_update_channel(struct session_tag *sp) ;
void    ui_update_converter(struct session_tag *sp);
void    ui_update_repair(struct session_tag *sp);
void	ui_update_powermeters(struct session_tag *sp, struct s_mix_info *ms, int elapsed_time);

void	ui_update_stats(struct session_tag *s, struct s_rtcp_dbentry *e);
void	ui_update_lecture_mode(struct session_tag *session_pointer);
void	ui_update(struct session_tag *session_pointer);
void	ui_update_loss(struct session_tag *sp, u_int32 srce, u_int32 dest, int loss);
void	ui_update_reception(struct session_tag *s, u_int32 ssrc, u_int32 recv, u_int32 lost, u_int32 misordered, u_int32 duplicates, u_int32 jitter, int jit_tog);
void	ui_update_duration(struct session_tag *s, u_int32 ssrc, int duration);

void	ui_update_video_playout(struct session_tag *s, u_int32 ssrc, int playout);
void	ui_update_sync(struct session_tag *s, int sync);
void	ui_update_key(struct session_tag *s, char *key);

void    ui_update_playback_file(struct session_tag *s, char *name);
void    ui_update_record_file(struct session_tag *s, char *name);
void    ui_update_file_live(struct session_tag *s, char *mode, int valid);
void    ui_update_codec(struct session_tag *s, codec_id_t cid);
void	ui_controller_init(struct session_tag *s, u_int32 ssrc, char *name_engine, char *name_ui, char *name_video);
void    ui_initial_settings(struct session_tag *s);
void    ui_final_settings(struct session_tag *s);
void    ui_quit(struct session_tag *s);

#endif
