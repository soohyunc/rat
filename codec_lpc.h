/*
 * FILE:    codec_lpc.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef _CODEC_LPC_H_
#define _CODEC_LPC_H_

u_int16                      lpc_get_formats_count (void);
const struct s_codec_format* lpc_get_format(u_int16 idx);

void lpc_setup(void);

int  lpc_encoder_state_create  (u_int16 idx, u_char **state);
void lpc_encoder_state_destroy (u_int16 idx, u_char **state);
int  lpc_encoder (u_int16 idx, u_char *state, sample *in, coded_unit *out);

int  lpc_decoder_state_create  (u_int16 idx, u_char **state);
void lpc_decoder_state_destroy (u_int16 idx, u_char **state);
int  lpc_decoder               (u_int16 idx, u_char *state, coded_unit *in, sample *out);

int  lpc_repair  (u_int16 idx, u_char *state, u_int16 consec_lost,
                  coded_unit *prev, coded_unit *missing, coded_unit *next);

#endif /* _CODEC_LPC_H_ */
