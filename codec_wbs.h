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

u_int16_t               wbs_get_formats_count (void);
const codec_format_t* wbs_get_format        (u_int16_t idx);
int                   wbs_state_create      (u_int16_t idx, u_char **state);
void                  wbs_state_destroy     (u_int16_t idx, u_char **state);
int                   wbs_encoder           (u_int16_t idx, u_char *state, sample     *in, coded_unit *out);
int                   wbs_decoder           (u_int16_t idx, u_char *state, coded_unit *in, sample     *out);
u_int8_t	              wbs_max_layers        (void);
int                   wbs_get_layer         (u_int16_t idx, coded_unit *in, u_int8_t layer, u_int16_t *markers, coded_unit *out);
int                   wbs_combine_layer     (u_int16_t idx, coded_unit *in, coded_unit *out, u_int8_t nelem, u_int16_t *markers);

#endif /* _CODEC_WBS_H_ */



