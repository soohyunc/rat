/*
 * FILE:      cc_vanilla.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#ifndef __CC_VANILLA_H__
#define __CC_VANILLA_H__

int  vanilla_encoder_create  (u_char **state, u_int32 *len);
void vanilla_encoder_destroy (u_char **state, u_int32  len);
int  vanilla_encoder_reset   (u_char  *state);
int  vanilla_encoder_encode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              u_int32                  units_per_packet);
int  vanilla_decoder_decode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              ts_t                     now);
int vanilla_decoder_peek     (u_int8   pkt_pt,
                              u_char  *data,
                              u_int32  len,
                              u_int16  *upp,
                              u_int8   *pt);

int vanilla_decoder_describe (u_int8   pkt_pt,
                              u_char  *data,
                              u_int32  len,
                              char    *out,
                              u_int32  out_len);
 
#endif /* __CC_VANILLA_H__ */

