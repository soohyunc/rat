/*
 * FILE:      new_channel.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

                                   

/* Payload mapping functions */
cc_id_t   channel_coder_get_by_payload (u_int8 payload);
int       channel_coder_set_payload    (cc_id_t id, u_int8  payload);   
int       channel_coder_get_payload    (cc_id_t id, u_int8* payload);   

#endif /* __NEW_CHANNEL_H__ */
