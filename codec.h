/*
 * FILE:    codec.h
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
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
 *
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

#ifndef _CODEC_H_
#define _CODEC_H_

/* Codec module startup and end */

void codec_init (void);
void codec_exit (void);

/* Use these two functions to finder number of available codecs 
 * and to get an codec id of the num'th codec.
 */
u_int32    codec_get_number_of_codecs (void);
codec_id_t codec_get_codec_number     (u_int32 num);

/* Use this function to check if codec id is valid / corrupted */
int codec_id_is_valid(codec_id_t id);

/* Use these functions to see what formats a codec supports
 * and whether they are encode from or decode to.
 */

const codec_format_t* codec_get_format        (codec_id_t id);
int                   codec_can_encode_from   (codec_id_t id, 
                                               const audio_format *qfmt);
int                   codec_can_encode        (codec_id_t id);
int                   codec_can_decode_to     (codec_id_t id, 
                                               const audio_format *qfmt);
int                   codec_can_decode        (codec_id_t id);
int                   codec_audio_formats_compatible(codec_id_t id1,
                                                     codec_id_t id2);

/* This is easily calculable but crops up everywhere */
u_int32               codec_get_samples_per_frame (codec_id_t id);

/* Codec encoder functions */
int  codec_encoder_create  (codec_id_t id, codec_state **cs);
void codec_encoder_destroy (codec_state **cs);
int  codec_encode          (codec_state* cs, 
                            sample*      in_buf, 
                            u_int16      in_bytes, 
                            coded_unit*  cu);

/* Codec decoder functions */
int  codec_decoder_create  (codec_id_t id, codec_state **cs);
void codec_decoder_destroy (codec_state **cs);
int  codec_decode          (codec_state* cs, 
                            coded_unit*  cu,
                            sample*      out_buf, 
                            u_int16      out_bytes);

/* Repair related */
int  codec_decoder_can_repair (codec_id_t id);
int  codec_decoder_repair     (codec_id_t id, 
                               codec_state *cs,
                               u_int16 consec_missing,
                               coded_unit *prev, 
                               coded_unit *miss, 
                               coded_unit *next);

/* Peek function for variable frame size codecs */
u_int32 codec_peek_frame_size(codec_id_t id, u_char *data, u_int16 blk_len);

int     codec_clear_coded_unit(coded_unit *u);

/* RTP payload mapping interface */
int        payload_is_valid     (u_char pt);
int        codec_map_payload    (codec_id_t id, u_char pt);
int        codec_unmap_payload  (codec_id_t id, u_char pt);
u_char     codec_get_payload    (codec_id_t id);
codec_id_t codec_get_by_payload (u_char pt);

/* For compatibility only */
codec_id_t codec_get_first_mapped_with(u_int16 sample_rate, u_int16 channels);

/* Name to codec mappings */
codec_id_t codec_get_by_name      (const char *name);
codec_id_t codec_get_matching     (const char *short_name, u_int16 sample_rate, u_int16 channels);

codec_id_t codec_get_native_coding (u_int16 sample_rate, u_int16 channels);
int        codec_is_native_coding  (codec_id_t id);
int        codec_get_native_info   (codec_id_t cid, 
                                    u_int16 *sample_rate, 
                                    u_int16 *channels);
#endif /* _CODEC_H_ */
