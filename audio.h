/*
 * FILE:    audio.h
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

#ifndef _RAT_AUDIO_H_
#define _RAT_AUDIO_H_

#include "rat_types.h"

#define MAX_CUSHION	4000
#define MIN_CUSHION	320

typedef struct s_cushion_struct {
	int             cushion_estimate;
	int             cushion_size;
	int             cushion_step;
	sample          audio_zero_buf[MAX_CUSHION];
	int		*read_history;	/* Circular buffer of read lengths */
	int		hi;		/* History index */
	int		*histogram;	/* Histogram of read lengths */
} cushion_struct;

/* This version of the code can only work with a constant device      */
/* encoding. To use different encodings the parameters below have     */
/* to be changed and the program recompiled.                          */
/* The clock translation problems have to be taken into consideration */
/* if different users use different base encodings...                 */
typedef enum {
	DEV_PCMU,
	DEV_L8,
	DEV_L16
} deve_e;

typedef struct s_audio_format {
  deve_e encoding;
  int    sample_rate; 		/* Should be one of 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000 */
  int    bits_per_sample;	/* Should be 8 or 16 */
  int    num_channels;  	/* Should be 1 or 2  */
} audio_format;

#define BYTES_PER_SAMPLE sizeof(sample)

#define SAMPLING_RATE	8000
#define PCMU_AUDIO_ZERO	127
#define L16_AUDIO_ZERO	0

#define MAX_AMP		100
#define DEVICE_REC_BUF	16000
#define DEVICE_BUF_UNIT	320

#define BD_THRESHOLD    16
#define BD_CONSECUTIVE  54

#define AUDIO_NO_DEVICE -1

/* Structures used in function declarations below */
struct s_cushion_struct;
struct session_tag;
struct s_bias_ctl;
struct s_mix_info;

int	audio_open(audio_format format);
void	audio_close(int audio_fd);
void	audio_drain(int audio_fd);
void	audio_switch_out(int audio_fd, struct s_cushion_struct *cushion);
void	audio_switch_in(int audio_fd);
void	audio_set_gain(int audio_fd, int gain);
int	audio_get_gain(int audio_fd);
void	audio_set_volume(int audio_fd, int vol);
int	audio_get_volume(int audio_fd);
int	audio_read(int audio_fd, sample *buf, int samples);
int	audio_write(int audio_fd, sample *buf, int samples);
int	audio_is_dry(int audio_fd);
void	audio_non_block(int audio_fd);
void	audio_block(int audio_fd);
int	audio_requested(int audio_fd);
void	audio_set_oport(int audio_fd, int port);
int	audio_get_oport(int audio_fd);
int	audio_next_oport(int audio_fd);
void	audio_set_iport(int audio_fd, int port);
int	audio_get_iport(int audio_fd);
int	audio_next_iport(int audio_fd);
int	audio_duplex(int audio_fd);

/* Stuff in audio.c */
void	mix_init(void);
void	mix2_pcmu(u_int8 *v0, u_int8 *v1, size_t len);
void	mix2_l16(int16 *v0, int16 *v1, size_t len);
void	mix2_l8(int8 *v0, int8 *v1, size_t len);
int	is_audio_zero(sample *buf, int len, deve_e type);
void	audio_zero(sample *buf, int len, deve_e type);
int     read_write_audio(struct session_tag *spi, struct session_tag *spo, struct s_cushion_struct *cushion, struct s_mix_info *ms);
void	read_write_init(struct s_cushion_struct *cushion, struct session_tag *session_pointer);
void	audio_init(struct session_tag *sp, struct s_cushion_struct *cushion);
int	audio_device_read(struct session_tag *sp, sample *buf, int len);
int	audio_device_write(struct session_tag *sp, sample *buf, int samples);
int	audio_device_take(struct session_tag *sp);
void	audio_device_give(struct session_tag *sp);
void    audio_unbias(struct s_bias_ctl **bc, sample *buf, int len);
void	pcmu_linear_init(void);

/* Use the IRIX definition in all cases..... [csp] */
extern short mulawtolin[256];
extern unsigned char lintomulaw[65536];

extern short         alawtolin[256];
extern unsigned char lintoalaw[8192]; 

#define s2u(x)	lintomulaw[((unsigned short)(x)) & 0xffff]
#define u2s(x)	mulawtolin[x]
#define s2a(x)  lintoalaw[((unsigned short)(x))>>3]
#define a2s(x)  alawtolin[((unsigned char)x)]

#endif /* _RAT_AUDIO_H_ */
