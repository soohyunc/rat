
#ifndef   __NEW_CHANNEL_H__
#define   __NEW_CHANNEL_H__

#include "playout.h"
#include "channel_types.h"

struct s_channel_state;

/* Channel coder query functions:
 * channel_get_coder_count returns number of available channel coders,
 * and channel_get_coder_details copies details of idx'th coder into ccd.
 */

int       channel_get_coder_count   (void);
int       channel_get_coder_details (int idx, cc_details *ccd);

/* Don't use these two functions directly use macros channel_encoder_{create, destory, reset},
 * and channel_encoder_{create, destory, reset} instead.
 */

int       channel_coder_create      (cc_id_t id, struct s_channel_state **cs, int is_encoder);
void      channel_coder_destroy     (struct s_channel_state **cs, int is_encoder);
int       channel_coder_reset       (struct s_channel_state *cs,  int is_encoder);   

/* Encoder specifics *********************************************************/

#define   channel_encoder_create(id, cs)  channel_coder_create  (id, cs, TRUE)
#define   channel_encoder_destroy(cs)     channel_coder_destroy (cs, TRUE)
#define   channel_encoder_reset(cs)       channel_coder_reset   (cs, TRUE)

int       channel_encoder_set_units_per_packet (struct s_channel_state *cs, u_int16);
u_int16   channel_encoder_get_units_per_packet (struct s_channel_state *cs);

int       channel_encoder_set_parameters (struct s_channel_state *cs, char *cmd);
int       channel_encoder_get_parameters (struct s_channel_state *cs, char *cmd, int cmd_len);

int       channel_encoder_encode (struct s_channel_state  *cs, 
                                  struct s_playout_buffer *media_buffer, 
                                  struct s_playout_buffer *channel_buffer);

/* Decoder specifics *********************************************************/
#define   channel_decoder_create(id, cs)  channel_coder_create  (id, cs, FALSE)
#define   channel_decoder_destroy(cs)     channel_coder_destroy (cs, FALSE)
#define   channel_decoder_reset(cs)       channel_coder_reset   (cs, FALSE)

int       channel_decoder_decode (struct s_channel_state  *cs, 
                                  struct s_playout_buffer *channel_buffer,
                                  struct s_playout_buffer *media_buffer, 
                                  u_int32                  now);

/* Payload mapping functions */
cc_id_t   channel_coder_get_by_payload (u_int8 payload);
int       channel_coder_set_payload    (cc_id_t id, u_int8  payload);   
int       channel_coder_get_payload    (cc_id_t id, u_int8* payload);   

#endif /* __NEW_CHANNEL_H__ */
