/*
 * FILE:    codec_types.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
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

#ifndef _CODEC_TYPES_H_
#define _CODEC_TYPES_H_

#define CODEC_PAYLOAD_DYNAMIC   255

typedef u_int32 codec_id_t;

typedef struct {
        u_char    *state;
        codec_id_t id;
} codec_state;

typedef struct s_codec_format {
        char         short_name[10];
        char         long_name[32];
        char         description[128];
        u_char       default_pt;
        u_int16      mean_per_packet_state_size;
        u_int16      mean_coded_frame_size;
        const audio_format format;
} codec_format_t;

typedef struct s_coded_unit {
        codec_id_t id;
	u_char	*state;
	int	state_len;
	u_char	*data;
	int	data_len;
} coded_unit;

#define MAX_MEDIA_UNITS  3
/* This data structure is for storing multiple representations of
 * coded audio for a given time interval.
 */
typedef struct {
        u_int8      nrep;
        coded_unit *rep[MAX_MEDIA_UNITS];
} media_data;

int  media_data_create    (media_data **m, int nrep);
void media_data_destroy   (media_data **m, u_int32 md_size);

#endif /* _CODEC_TYPES_H_ */

