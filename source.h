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

#include "convert.h"
#include "ts.h"

struct s_source;
struct s_source_list;
struct s_rtcp_dbentry;
struct s_mix_info;
struct s_pb;

int              source_list_create  (struct s_source_list **pplist);

void             source_list_destroy (struct s_source_list **pplist);

void             source_list_clear   (struct s_source_list *plist);

u_int32          source_list_source_count(struct s_source_list *plist);

struct s_source* source_list_get_source_no (struct s_source_list *plist,
                                            u_int32               src_no);

struct s_source* source_get_by_rtcp_dbentry (struct s_source_list  *list,
                                             struct s_rtcp_dbentry *dbe);

struct s_source* source_create             (struct s_source_list  *list, 
                                            struct s_rtcp_dbentry *dbe,
                                            converter_id_t current_id,
                                            int render_3D_enabled,
                                            u_int16 out_rate,
                                            u_int16 out_channels);

void             source_reconfigure        (struct s_source* src,
                                            converter_id_t   current_id,
                                            int              render_3D_enabled,
                                            u_int16          out_rate,
                                            u_int16          out_channels);

void             source_remove             (struct s_source_list *list,
                                            struct s_source      *src);

int              source_add_packet         (struct s_source *src, 
                                            u_char          *pckt, 
                                            u_int32          pckt_len,
                                            u_int32          data_start,
                                            u_int8           payload,
                                            ts_t             playout);

int              source_process            (struct s_source   *src,
                                            struct s_mix_info *ms,
                                            int                render_3d,
                                            int                repair,
                                            ts_t               now);

int              source_relevant           (struct s_source *src,
                                            ts_t             now);

int              source_audit              (struct s_source *src);

ts_sequencer*    source_get_sequencer      (struct s_source *src);

ts_t             source_get_audio_buffered (struct s_source *src);
ts_t             source_get_playout_delay  (struct s_source *src);

struct s_pb*
                 source_get_decoded_buffer (struct s_source *src);

struct s_rtcp_dbentry*
                 source_get_rtcp_dbentry   (struct s_source *src);

#endif /* __SOURCE_H__ */
