/*
 * FILE:    codec.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996 University College London
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

#ifndef _RAT_CODEC_H_
#define _RAT_CODEC_H_

#include "rat_types.h"

#define MAX_CODEC        128

#define DUMMY			255

#define DVI_UNIT_SIZE		80
#define DVI_STATE_SIZE		sizeof(struct adpcm_state)

#define POST_LPC_BLEND_LENGTH 10

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif

struct s_codec;
struct s_dpt;
struct session_tag;
struct rx_element_tag;
struct s_codec_state;

typedef struct s_coded_unit {
	struct s_codec *cp;
	char	*state;
	int	state_len;
	char	*data;
	int	data_len;
} coded_unit;

typedef void (*init_f)(struct s_codec_state *s, struct s_codec *c);
typedef void (*code_f)(sample *in, coded_unit *c, struct s_codec_state *s, struct s_codec *cp);
typedef void (*dec_f)(struct s_coded_unit *c, sample *data, struct s_codec_state *s, struct s_codec *cp);

typedef struct s_codec {
	char	*name;
	int	value;		/* Value for sorting redundancy in receiver */
	int	pt;

	/* Raw input format description */
	int	freq;		/* Sampling frequency required in Hz */
	int	sample_size;	/* Number of bytes per sample */
	int	channels;	/* 1 mono, 2 stereo */
	int	sent_state_sz;	/* Transmitted sate size in bytes */

	int	unit_len;	/* Duration of unit in samples.
				 * This does not include the number of channels
				 * like the transmitter unit */
	int	max_unit_sz;	/* Maximum size of coded unit in bytes */
	init_f	enc_init;
	code_f	encode;
	init_f	dec_init;
	dec_f	decode;
	int     native;         /* indicates whether format is understood by device -
				 * needed because of multiple decompressed data 
				 * because of channel/rate changes. XXX other ideas?*/
} codec_t;

struct s_codec *get_codec(int pt);
struct s_codec *get_codec_byname(char *name, struct session_tag *sp);
void	set_dynamic_payload(struct s_dpt **listp, char *name, int pt);
int	get_dynamic_payload(struct s_dpt **listp, char *name);
void	codec_init(struct session_tag *sp);
void	encoder(struct session_tag *sp, sample *data, int coding, coded_unit *c);
void	decode_unit(struct rx_element_tag *u);
void	clear_coded_unit(coded_unit *u);
int	codec_compatible(struct s_codec *c1, struct s_codec *c2);

#endif /* _RAT_CODEC_H_ */

