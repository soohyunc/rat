/*
 * FILE:      cc_rdncy.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#ifndef __CC_RDNCY_H__
#define __CC_RDNCY_H__

/* Encoder functions *********************************************************/

int  redundancy_encoder_create  (u_char **state, u_int32 *len);

void redundancy_encoder_destroy (u_char **state, u_int32  len);

int  redundancy_encoder_reset   (u_char  *state);

int  redundancy_encoder_encode  (u_char                  *state,
                                 struct s_pb *in,
                                 struct s_pb *out,
                                 u_int32                  units_per_packet);

int  redundancy_encoder_set_parameters(u_char *state, char *cmd);
int  redundancy_encoder_get_parameters(u_char *state, char *buf, u_int32 blen);

/* Decoder functions *********************************************************/

int  redundancy_decoder_decode  (u_char                  *state,
                                 struct s_pb *in,
                                 struct s_pb *out,
                                 ts_t                     now);

int redundancy_decoder_peek     (u_int8   pkt_pt,
                                 u_char  *data,
                                 u_int32  len,
                                 u_int16  *upp,
                                 u_int8   *pt);

int redundancy_decoder_describe (u_int8   pkt_pt,
                                 u_char  *data,
                                 u_int32  len,
                                 char    *out,
                                 u_int32  out_len);
 
#endif /* __CC_RDNCY_H__ */

