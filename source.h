/*
 * FILE:      source.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#ifndef __SOURCE_H__
#define __SOURCE_H__

#include "converter.h"
#include "ts.h"
#include "rtp.h"

struct s_source;
struct s_source_list;
struct s_rtcp_dbentry;
struct s_mix_info;
struct s_pb;
struct s_session;

int              source_list_create  (struct s_source_list **pplist);

void             source_list_destroy (struct s_source_list **pplist);

void             source_list_clear   (struct s_source_list *plist);

u_int32          source_list_source_count(struct s_source_list *plist);

struct s_source* source_list_get_source_no (struct s_source_list *plist,
                                            u_int32               src_no);

struct s_source* source_get_by_ssrc (struct s_source_list  *list,
                                     u_int32                ssrc);

struct s_source* source_create             (struct s_source_list  *list, 
                                            u_int32                ssrc,
					    pdb_t		  *pdb);

void             source_reconfigure        (struct s_source* src,
                                            converter_id_t   current_id,
                                            int              render_3D_enabled,
                                            u_int16          out_rate,
                                            u_int16          out_channels);

void             source_remove             (struct s_source_list *list,
                                            struct s_source      *src);

int              source_add_packet         (struct s_source *src, 
                                            rtp_packet      *p);

int              source_check_buffering    (struct s_source   *src);

int              source_process            (struct s_session  *sp,
                                            struct s_source   *src,
                                            struct s_mix_info *ms,
                                            int                render_3d,
                                            repair_id_t        repair,
                                            ts_t               start_ts,
                                            ts_t               end_ts);

int              source_relevant           (struct s_source *src,
                                            ts_t             now);

int              source_audit              (struct s_source *src);

ts_t             source_get_audio_buffered (struct s_source *src);

ts_t             source_get_playout_delay  (struct s_source *src);

double           source_get_bps            (struct s_source *src);
double           source_get_skew_rate      (struct s_source *src);

struct s_pb*
                 source_get_decoded_buffer (struct s_source *src);

u_int32          source_get_ssrc           (struct s_source *src);

#endif /* __SOURCE_H__ */
