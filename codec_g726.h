/*
 * FILE:    codec_g726.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

/* Just wrote the RAT interface, see codec_dvi.c for coder copyright [oth] */

#ifndef _CODEC_G726_H_
#define _CODEC_G726_H_

u_int16_t               g726_get_formats_count (void);
const codec_format_t* g726_get_format        (u_int16_t idx);
int                   g726_state_create      (u_int16_t idx, u_char **state);
void                  g726_state_destroy     (u_int16_t idx, u_char **state);
int                   g726_encode            (u_int16_t idx, u_char *state, sample     *in, coded_unit *out);
int                   g726_decode            (u_int16_t idx, u_char *state, coded_unit *in, sample     *out);

#endif /* _CODEC_G726_H_ */
