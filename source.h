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
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
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
