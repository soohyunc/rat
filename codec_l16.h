/*
 * FILE:    codec_l16.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef _CODEC_L16_H_
#define _CODEC_L16_H_

u_int16               l16_get_formats_count (void);
const codec_format_t* l16_get_format        (u_int16 idx);
int                   l16_encode            (u_int16 idx, u_char *state, sample     *in, coded_unit *out);
int                   l16_decode            (u_int16 idx, u_char *state, coded_unit *in, sample     *out);
int                   l16_peek_frame_size   (u_int16 idx, u_char *data,  int data_len);

#endif /* _CODEC_L16_H_ */
