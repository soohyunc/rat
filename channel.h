/*
 * FILE:      channel.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

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
int       channel_get_null_coder    (void);

/* channel_get_coder_identity fills coder name and descriptor into ccd */
int       channel_get_coder_identity(struct s_channel_state *cs, cc_details *ccd);

/* Don't use these two functions directly use macros channel_encoder_{create, destory, reset},
 * and channel_encoder_{create, destory, reset} instead.
 */

int       _channel_coder_create      (cc_id_t id, struct s_channel_state **cs, int is_encoder);
void      _channel_coder_destroy     (struct s_channel_state **cs, int is_encoder);
int       _channel_coder_reset       (struct s_channel_state *cs,  int is_encoder);   

/* Encoder specifics *********************************************************/

#define   channel_encoder_create(id, cs)  _channel_coder_create  (id, cs, TRUE)
#define   channel_encoder_destroy(cs)     _channel_coder_destroy (cs, TRUE)
#define   channel_encoder_reset(cs)       _channel_coder_reset   (cs, TRUE)

int       channel_encoder_set_units_per_packet (struct s_channel_state *cs, u_int16);
u_int16   channel_encoder_get_units_per_packet (struct s_channel_state *cs);

int       channel_encoder_set_parameters (struct s_channel_state *cs, char *cmd);
int       channel_encoder_get_parameters (struct s_channel_state *cs, char *cmd, int cmd_len);

int       channel_encoder_encode (struct s_channel_state  *cs, 
                                  struct s_pb *media_buffer, 
                                  struct s_pb *channel_buffer);

/* Decoder specifics *********************************************************/
#define   channel_decoder_create(id, cs)  _channel_coder_create  (id, cs, FALSE)
#define   channel_decoder_destroy(cs)     _channel_coder_destroy (cs, FALSE)
#define   channel_decoder_reset(cs)       _channel_coder_reset   (cs, FALSE)

int       channel_decoder_decode (struct s_channel_state  *cs, 
                                  struct s_pb *channel_buffer,
                                  struct s_pb *media_buffer, 
                                  ts_t                     now);

int       channel_decoder_matches (cc_id_t                 cid, 
                                   struct s_channel_state *cs);

int       channel_get_compatible_codec (u_int8  pt, 
                                        u_char *data, 
                                        u_int32 data_len);

int       channel_verify_and_stat (cc_id_t  cid,
                                   u_int8   pktpt,
                                   u_char  *data,
                                   u_int32  data_len,
                                   u_int16 *units_per_packet,
                                   u_char  *codec_pt);

int       channel_describe_data   (cc_id_t cid,
                                   u_int8  pktpt,
                                   u_char *data,
                                   u_int32 data_len,
                                   char   *outstr,
                                   u_int32 out_len);
                                   

/* Payload mapping functions */
cc_id_t   channel_coder_get_by_payload (u_int8 pt);
u_int8    channel_coder_get_payload    (struct s_channel_state* st, u_int8 media_pt);   
int       channel_coder_exist_payload  (u_int8 pt);

/* Layered coding functions */
u_int8    channel_coder_get_layers     (cc_id_t cid);

#endif /* __NEW_CHANNEL_H__ */
