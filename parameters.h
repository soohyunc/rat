/*
 *	FILE: parameters.h
 *      PROGRAM: RAT
 *      AUTHOR: O.Hodson
 *
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _RAT_PARAMETERS_H_
#define _RAT_PARAMETERS_H_

struct s_sd;
struct s_vad;
struct s_agc;
struct s_session;

#define VU_INPUT  0
#define VU_OUTPUT 1

void    vu_table_init(void);
int     lin2vu(uint16_t avg_energy, int peak, int io_dir);

struct  s_sd *sd_init (uint16_t blk_dur, uint16_t freq);
void    sd_destroy    (struct s_sd *s);
void	sd_reset      (struct s_sd *s);
int	sd            (struct s_sd *s, uint16_t energy);
struct  s_sd *sd_dup  (struct s_sd *s);

#define VAD_MODE_LECT     0
#define VAD_MODE_CONF     1

struct s_vad * vad_create        (uint16_t blockdur, uint16_t freq);
void           vad_config        (struct s_vad *v, uint16_t blockdur, uint16_t freq);
void           vad_reset         (struct s_vad *v);
void           vad_destroy       (struct s_vad *v);
uint16_t        vad_to_get        (struct s_vad *v, u_char silence, u_char mode);
uint16_t        vad_max_could_get (struct s_vad *v);
u_char         vad_in_talkspurt  (struct s_vad *v);
uint32_t        vad_talkspurt_no  (struct s_vad *v);
void           vad_dump          (struct s_vad *v);

struct s_agc * agc_create        (struct s_session *sp);
void           agc_destroy       (struct s_agc *a);
void           agc_update        (struct s_agc *a, uint16_t energy, uint32_t spurtno);
void           agc_reset         (struct s_agc *a);
u_char         agc_apply_changes (struct s_agc *a);

#endif /* _RAT_PARAMETERS_H_ */


