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
 */

#ifndef _RAT_PARAMETERS_H_
#define _RAT_PARAMETERS_H_

struct s_sd;
struct s_vad;
struct s_agc;
struct session_tag;

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

struct s_agc * agc_create        (struct session_tag *sp);
void           agc_destroy       (struct s_agc *a);
void           agc_update        (struct s_agc *a, u_int16 energy, u_int32 spurtno);
void           agc_reset         (struct s_agc *a);
u_char         agc_apply_changes (struct s_agc *a);

#endif /* _RAT_PARAMETERS_H_ */


