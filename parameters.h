/*
 *	FILE: parameters.h
 *      PROGRAM: RAT
 *      AUTHOR: O.Hodson
 *
 *	$Revision$
 *	$Date$
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

#ifndef _RAT_PARAMETERS_H_
#define _RAT_PARAMETERS_H_

struct s_sd;
struct s_vad;
struct s_agc;

u_int16	avg_audio_energy (sample *buf, u_int32 dur, u_int32 channels);

#define VU_INPUT  0
#define VU_OUTPUT 1

void    vu_table_init(void);
int     lin2vu(u_int16 avg_energy, int peak, int io_dir);

struct  s_sd *sd_init (u_int16 blk_dur, u_int16 freq);
void    sd_destroy    (struct s_sd *s);
void	sd_reset      (struct s_sd *s);
int	sd            (struct s_sd *s, u_int16 energy);
struct  s_sd *sd_dup  (struct s_sd *s);

#define VAD_MODE_LECT     0
#define VAD_MODE_CONF     1

struct s_vad * vad_create        (u_int16 blockdur, u_int16 freq);
void           vad_config        (struct s_vad *v, u_int16 blockdur, u_int16 freq);
void           vad_reset         (struct s_vad *v);
void           vad_destroy       (struct s_vad *v);
u_int16        vad_to_get        (struct s_vad *v, u_char silence, u_char mode);
u_int16        vad_max_could_get (struct s_vad *v);
u_char         vad_in_talkspurt  (struct s_vad *v);
u_int32        vad_talkspurt_no  (struct s_vad *v);
void           vad_dump          (struct s_vad *v);

struct s_agc * agc_create        (session_struct *sp);
void           agc_destroy       (struct s_agc *a);
void           agc_update        (struct s_agc *a, u_int16 energy, u_int32 spurtno);
void           agc_reset         (struct s_agc *a);
u_char         agc_apply_changes (struct s_agc *a);

#endif /* _RAT_PARAMETERS_H_ */


