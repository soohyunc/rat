/*
 * FILE:    codec_dvi.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

/* Just wrote the RAT interface, see codec_dvi.c for coder copyright [oth] */

#ifndef _CODEC_DVI_H_
#define _CODEC_DVI_H_

u_int16               dvi_get_formats_count (void);
const codec_format_t* dvi_get_format        (u_int16 idx);
int                   dvi_state_create      (u_int16 idx, u_char **state);
void                  dvi_state_destroy     (u_int16 idx, u_char **state);
int                   dvi_encode            (u_int16 idx, u_char *state, sample     *in, coded_unit *out);
int                   dvi_decode            (u_int16 idx, u_char *state, coded_unit *in, sample     *out);

#endif /* _CODEC_DVI_H_ */
