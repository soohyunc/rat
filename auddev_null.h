/*
 * File:     auddev_null.h
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 *
 * $Revision$
 * $Date$
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
 * OR SERVICES; LNULL OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE PNULLIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _AUDDEV_NULL_H_
#define _AUDDEV_NULL_H_
int  null_audio_open       (audio_desc_t ad, audio_format* ifmt, audio_format *ofmt);
void null_audio_close      (audio_desc_t ad);
void null_audio_drain      (audio_desc_t ad);
int  null_audio_duplex     (audio_desc_t ad);
void null_audio_set_gain   (audio_desc_t ad, int gain);
int  null_audio_get_gain   (audio_desc_t ad);
void null_audio_set_volume (audio_desc_t ad, int vol);
int  null_audio_get_volume (audio_desc_t ad);
void null_audio_loopback   (audio_desc_t ad, int gain);
int  null_audio_read       (audio_desc_t ad, u_char *buf, int buf_len);
int  null_audio_write      (audio_desc_t ad, u_char *buf, int buf_len);
void null_audio_non_block  (audio_desc_t ad);
void null_audio_block      (audio_desc_t ad);
void null_audio_set_oport  (audio_desc_t ad, int port);
int  null_audio_get_oport  (audio_desc_t ad);
int  null_audio_next_oport (audio_desc_t ad);
void null_audio_set_iport  (audio_desc_t ad, int port);
int  null_audio_get_iport  (audio_desc_t ad);
int  null_audio_next_iport (audio_desc_t ad);
int  null_audio_is_ready  (audio_desc_t ad);
void null_audio_wait_for  (audio_desc_t ad, int delay_ms);

#endif /* _AUDDEV_NULL_H_ */
