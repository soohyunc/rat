/*
 * FILE:    mix.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 *
 */

#ifndef _mix_h_
#define _mix_h_

#include "codec_types.h"
#include "ts.h"

struct s_mix_info;
struct s_session;
struct s_source;
struct s_rtcp_dbentry;

int  mix_create  (struct s_mix_info **ms, 
                   int sample_rate, 
                   int channels, 
                   int buffer_length);

void mix_destroy (struct s_mix_info **ms);

int  mix_process(struct s_mix_info     *ms,
                 pdb_entry_t           *pdbe,
                 coded_unit            *raw_frame,
                 ts_t                   now);

int  mix_get_audio       (struct s_mix_info *ms, int amount, sample **bufp);

void mix_get_new_cushion (struct s_mix_info *ms, 
                          int last_cushion_size, 
                          int new_cushion_size, 
                          int dry_time, 
                          sample **bufp);

void mix_update_ui       (struct s_session *sp, struct s_mix_info *ms);

int  mix_active          (struct s_mix_info *ms);

int
     mix_compatible(struct s_mix_info *ms, int sample_rate, int sample_channels);

#endif /* _mix_h_ */
