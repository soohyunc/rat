/*
 * FILE:    audio.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas / Orion Hodson / Colin Perkins
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef __AUDIO_UTIL_H__
#define __AUDIO_UTIL_H__

void	audio_zero   (sample *buf, int len, deve_e type);

void    audio_mix    (sample *dst, sample *in, int len);

#ifdef WIN32
BOOL    mmx_present();
void    audio_mix_mmx(sample *dst, sample *in, int len);
#endif

/* Biasing operations */

struct s_bias_ctl;

struct s_bias_ctl*
        bias_ctl_create(int channels, int freq);

void    bias_ctl_destroy(struct s_bias_ctl *bc);

void    bias_remove (struct s_bias_ctl *bc, sample *buf, int len);

/* Energy calculation */
u_int16	avg_audio_energy (sample *buf, u_int32 dur, u_int32 channels);

#endif /* __AUDIO_UTIL_H__ */
