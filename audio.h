/*
 * FILE:    audio.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Colin Perkins / Isidor Kouvelas
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

#ifndef _RAT_AUDIO_H_
#define _RAT_AUDIO_H_

#include "audio_types.h"
#include "codec_types.h"
#include "auddev.h"

/* Structures used in function declarations below */
struct s_cushion_struct;
struct session_tag;
struct s_mix_info;

/* Structure used for reconfiguration processing */

struct s_audio_config;

/* General audio processing functions */
int     audio_rw_process (struct session_tag *spi, struct session_tag *spo, struct s_mix_info *ms);

/* audio_device_take_initial takes safe config of null audio device.  All
 * further devices used in rat accessed through audio_device_reconfigure.
 * It is a nasty hack, but seemingly necessary in this kludge layer that 
 * should not exist.
 */

int     audio_device_take_initial(struct session_tag *sp, audio_desc_t ad);
int	audio_device_release     (struct session_tag *sp, audio_desc_t ad);

/* Functions used for changing the device set up.  Since the time it
 * takes to reconfigure the device is non-deterministic and often
 * longer than the mbus can tolerate we have to store requests to
 * change device config.  In the main process loop we can check if
 * device config change is pending, if so process all outstanding mbus
 * messages, then do device reconfig.
 */

void    audio_device_register_change_device(struct session_tag *sp, 
                                            audio_desc_t ad);

void    audio_device_register_change_primary(struct session_tag *sp,
                                             codec_id_t  cid);

void    audio_device_register_change_render_3d(struct session_tag *sp,
                                               int enabled);

int     audio_device_reconfigure (struct session_tag *sp);
int     audio_device_get_safe_config(struct s_audio_config **config);

#endif /* _RAT_AUDIO_H_ */
