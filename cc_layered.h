/*
 * FILE:      cc_layered.h
 * AUTHOR(S): Orion Hodson + Tristan Henderson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#ifndef __CC_LAYER_H__
#define __CC_LAYER_H__

#define LAY_MAX_LAYERS 2

int  layered_encoder_create  (u_char **state,
                              u_int32_t *len);
void layered_encoder_destroy (u_char **state,
                              u_int32_t  len);
int  layered_encoder_set_parameters(u_char *state,
                                    char *cmd);
int  layered_encoder_get_parameters(u_char *state,
                                    char *cmd,
                                    u_int32_t cmd_len);
int  layered_encoder_reset   (u_char  *state);
int  layered_encoder_encode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              u_int32_t                  units_per_packet);
int  layered_decoder_decode  (u_char                  *state,
                              struct s_pb *in,
                              struct s_pb *out,
                              ts_t                     now);
int layered_decoder_peek     (u_int8_t   pkt_pt,
                              u_char  *data,
                              u_int32_t  len,
                              u_int16_t  *upp,
                              u_int8_t   *pt);

int layered_decoder_describe (u_int8_t   pkt_pt,
                              u_char  *data,
                              u_int32_t  len,
                              char    *out,
                              u_int32_t  out_len);
 
#endif /* __CC_layered_H__ */

