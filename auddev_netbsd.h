/*
 * FILE:     auddev_netbsd.h
 * PROGRAM:  RAT
 * AUTHOR:   Brook Milligan
 *
 * $Id$
 *
 * Copyright (c) 2002-2004 Brook Milligan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AUDDEV_NETBSD_H_
#define _AUDDEV_NETBSD_H_

#include "audio_types.h"

int	netbsd_audio_init(void);
int	netbsd_audio_device_count(void);
char*	netbsd_audio_device_name (audio_desc_t ad);
int	netbsd_audio_open        (audio_desc_t ad,
				  audio_format* ifmt, audio_format* ofmt);
void	netbsd_audio_close       (audio_desc_t ad);
void	netbsd_audio_drain       (audio_desc_t ad);
int	netbsd_audio_duplex      (audio_desc_t ad);
int	netbsd_audio_read        (audio_desc_t ad, u_char *buf, int buf_len);
int	netbsd_audio_write       (audio_desc_t ad, u_char *buf, int buf_len);
void	netbsd_audio_non_block   (audio_desc_t ad);
void	netbsd_audio_block       (audio_desc_t ad);

void	netbsd_audio_iport_set   (audio_desc_t ad, audio_port_t port);
audio_port_t
	netbsd_audio_iport_get   (audio_desc_t ad);
void	netbsd_audio_oport_set   (audio_desc_t ad, audio_port_t port);
audio_port_t
	netbsd_audio_oport_get   (audio_desc_t ad);

void	netbsd_audio_set_igain   (audio_desc_t ad, int gain);
int	netbsd_audio_get_igain   (audio_desc_t ad);
void	netbsd_audio_set_ogain   (audio_desc_t ad, int vol);
int	netbsd_audio_get_ogain   (audio_desc_t ad);

const audio_port_details_t*
	netbsd_audio_iport_details (audio_desc_t ad, int idx);
const audio_port_details_t*
	netbsd_audio_oport_details (audio_desc_t ad, int idx);

int	netbsd_audio_iport_count (audio_desc_t ad);
int	netbsd_audio_oport_count (audio_desc_t ad);

void	netbsd_audio_loopback    (audio_desc_t ad, int gain);

int	netbsd_audio_is_ready    (audio_desc_t ad);
void	netbsd_audio_wait_for    (audio_desc_t ad, int delay_ms);
int	netbsd_audio_supports    (audio_desc_t ad, audio_format *fmt);

int	auddev_netbsd_setfd      (int fd);

#endif	/* _AUDDEV_NETBSD_H_ */
