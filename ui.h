/*
 * FILE:    ui_control.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 *
 */

#ifndef _UI_UPDATE_H
#define _UI_UPDATE_H

#include "codec_types.h"

struct s_session;
struct s_rtcp_dbentry;
struct s_cbaddr;
struct s_mix_info;

void ui_info_update_name  (struct s_session *s, uint32_t ssrc);
void ui_info_update_cname (struct s_session *s, uint32_t ssrc);
void ui_info_update_email (struct s_session *s, uint32_t ssrc);
void ui_info_update_phone (struct s_session *s, uint32_t ssrc);
void ui_info_update_loc   (struct s_session *s, uint32_t ssrc);
void ui_info_update_tool  (struct s_session *s, uint32_t ssrc);
void ui_info_update_note  (struct s_session *s, uint32_t ssrc);
void ui_info_mute         (struct s_session *s, uint32_t ssrc);
void ui_info_gain         (struct s_session *s, uint32_t ssrc);
void ui_info_remove       (struct s_session *s, uint32_t ssrc);
void ui_info_activate     (struct s_session *s, uint32_t ssrc);
void ui_info_deactivate   (struct s_session *s, uint32_t ssrc);
void ui_info_3d_settings  (struct s_session *s, uint32_t ssrc);

void ui_input_level          (struct s_session *s, int level);
void ui_output_level         (struct s_session *s, int level);
void ui_update_input_gain    (struct s_session *sp);
void ui_update_output_gain   (struct s_session *sp);
void ui_update_input_port    (struct s_session *sp);
void ui_update_device_config (struct s_session *sp);
void ui_update_output_port   (struct s_session *sp);
void ui_update_primary       (struct s_session *sp);
void ui_update_channel       (struct s_session *sp);
void ui_update_converter     (struct s_session *sp);
void ui_update_repair        (struct s_session *sp);
void ui_periodic_updates     (struct s_session *sp, int elapsed_time);
void ui_update_stats         (struct s_session *s, uint32_t ssrc);

void ui_update_lecture_mode  (struct s_session *session_pointer);
void ui_update               (struct s_session *session_pointer);
void ui_update_loss          (struct s_session *sp, uint32_t srce, uint32_t dest, int loss);
void ui_update_reception     (struct s_session *s, uint32_t ssrc, uint32_t recv, uint32_t lost, 
                              uint32_t misordered, uint32_t duplicates, uint32_t jitter, int jit_tog);
void ui_update_duration      (struct s_session *s, uint32_t ssrc, int duration);
void ui_update_video_playout (struct s_session *s, uint32_t ssrc, int playout);
void ui_update_sync          (struct s_session *s, int sync);
void ui_update_key           (struct s_session *s, char *key);
void ui_update_playback_file (struct s_session *s, char *name);
void ui_update_record_file   (struct s_session *s, char *name);
void ui_update_file_live     (struct s_session *s, char *mode, int valid);
void ui_update_codec         (struct s_session *s, codec_id_t cid);
void ui_initial_settings     (struct s_session *s);
void ui_final_settings       (struct s_session *s);
void ui_quit                 (struct s_session *s);

#endif
