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

struct s_source;
struct s_source_list;
struct s_rtcp_dbentry;

int              source_list_create  (struct s_source_list **pplist);

void             source_list_destroy (struct s_source_list **pplist);

/* Methods for extracting dbe entry participants that are active */
u_int32          source_list_source_count(struct s_source_list *plist);

struct s_rtcp_dbentry* 
                 source_list_get_rtcp_dbentry(struct s_source_list *plist,
                                              u_int32               src_no);

struct s_source* source_get (struct s_source_list  *list,
                             struct s_rtcp_dbentry *dbe);

struct s_source* source_create (struct s_source_list  *list, 
                                struct s_rtcp_dbentry *dbe,
                                converter_id_t current_id,
                                u_int16 out_rate,
                                u_int16 out_channels);

void             source_reconfigure(struct s_source* src,
                                    converter_id_t   current_id,
                                    u_int16          out_rate,
                                    u_int16          out_channels);

void             source_remove         (struct s_source_list *list,
                                        struct s_source      *src);

int              source_add_packet     (struct s_source *src, 
                                        u_char          *pckt, 
                                        u_int32          pckt_len,
                                        u_char          *data_start,
                                        u_int8           payload,
                                        u_int32          playout);

int              source_process        (struct s_source *src,
                                        u_int32          now);

u_int32          source_buffer_length_ms (struct s_source *src);

#endif /* __SOURCE_H__ */
