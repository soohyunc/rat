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

/* Decoder functions *********************************************************/

int  redundancy_decoder_create  (u_char **state, u_int32 *len);

int  redundancy_decoder_destroy (u_char **state, u_int32 len);

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

