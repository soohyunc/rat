
#ifndef __CC_VANILLA_H__
#define __CC_VANILLA_H__

int  vanilla_encoder_create  (u_char **state, int *len);
void vanilla_encoder_destroy (u_char **state, int  len);
int  vanilla_encoder_reset   (u_char  *state);
int  vanilla_encoder_encode  (u_char  *state,
                              struct s_playout_buffer *in,
                              struct s_playout_buffer *out,
                              int units_per_packet);
int  vanilla_decoder_decode  (u_char  *state,
                              struct s_playout_buffer *in,
                              struct s_playout_buffer *out,
                              u_int32 now);
 
#endif /* __CC_VANILLA_H__ */

