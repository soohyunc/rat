/*
 * FILE:    codec_wbs.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef _CODEC_WBS_H_
#define _CODEC_WBS_H_

u_int16               wbs_get_formats_count (void);
const codec_format_t* wbs_get_format        (u_int16 idx);
int                   wbs_state_create      (u_int16 idx, u_char **state);
void                  wbs_state_destroy     (u_int16 idx, u_char **state);
int                   wbs_encoder           (u_int16 idx, u_char *state, sample     *in, coded_unit *out);
int                   wbs_decoder           (u_int16 idx, u_char *state, coded_unit *in, sample     *out);
u_int8				  wbs_max_layers        (void);
int                   wbs_get_layer         (coded_unit *in, u_int8 layer, u_int8 *markers, coded_unit *out);
int                   wbs_combine_layer     (coded_unit *in, coded_unit *out);

#endif /* _CODEC_WBS_H_ */



