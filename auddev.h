/*
 * FILE:    auddev.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Note: Original audio interface by Isidor Kouvelas, Colin Perkins, 
 * and OH.  OH has gone through and modularised this code so that RAT 
 * can detect and use multiple audio devices.
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

#ifndef _AUDDEV_H_
#define _AUDDEV_H_

#include "audio_types.h"

/* Audio interface fn's for dealing with multiple devices/device interfaces */
int     audio_init_interfaces       (void);
int     audio_free_interfaces       (void);
int     audio_get_device_count      (void);
int     audio_get_device_details    (int idx, audio_device_details_t *desc);
int     audio_get_null_device       (void); /* gets null dev interface */
int     audio_device_supports       (audio_desc_t ad, u_int16 rate, u_int16 channels);
int     audio_device_is_open        (audio_desc_t ad);

/* Audio functions implemented by device interfaces */
int     audio_open          (audio_desc_t ad, audio_format *in_format, audio_format *out_format);
void	audio_close         (audio_desc_t ad);
void	audio_drain         (audio_desc_t ad);
void	audio_set_igain      (audio_desc_t ad, int gain);
int     audio_duplex        (audio_desc_t ad);
int	audio_get_igain      (audio_desc_t ad);
void	audio_set_ogain    (audio_desc_t ad, int vol);
int	audio_get_ogain    (audio_desc_t ad);
void    audio_loopback      (audio_desc_t ad, int gain);
int	audio_read          (audio_desc_t ad, sample *buf, int samples);
int	audio_write         (audio_desc_t ad, sample *buf, int samples);
void	audio_non_block     (audio_desc_t ad);
void	audio_block         (audio_desc_t ad);

void	                    audio_set_oport         (audio_desc_t ad, audio_port_t);
audio_port_t                audio_get_oport         (audio_desc_t ad);
int	                    audio_get_oport_count   (audio_desc_t ad);
const audio_port_details_t* audio_get_oport_details (audio_desc_t ad, int port_idx);

void	                    audio_set_iport         (audio_desc_t ad, audio_port_t);
audio_port_t                audio_get_iport         (audio_desc_t ad);
int	                    audio_get_iport_count   (audio_desc_t ad);
const audio_port_details_t* audio_get_iport_details (audio_desc_t ad, int port_idx);

int     audio_is_ready      (audio_desc_t ad);
void    audio_wait_for      (audio_desc_t ad, int granularity_ms);

const audio_format* audio_get_ifmt (audio_desc_t ad);
const audio_format* audio_get_ofmt (audio_desc_t ad);

/* audio_get_samples_{read,written} return the actual number of
 * sample instances that have been read/written.  audio_get_device_time
 * returns the number of samples read rounded down to the nearest
 * number of whole sample blocks.
 */

u_int32 audio_get_device_time     (audio_desc_t ad);
u_int32 audio_get_samples_written (audio_desc_t ad);
u_int32 audio_get_samples_read    (audio_desc_t ad);

#endif /* _AUDDEV_H_ */
