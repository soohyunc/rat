/*
 * FILE:    cushion.h
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 * MODIFICATIONS: Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef __CUSHION_H__
#define __CUSHION_H__

#define CUSHION_MODE_LECTURE    0
#define CUSHION_MODE_CONFERENCE 1

struct s_cushion_struct;

int     cushion_create             (struct s_cushion_struct **c, int block_dur);
void    cushion_destroy            (struct s_cushion_struct **c);

void    cushion_update             (struct s_cushion_struct *c, u_int32 read_dur, int cushion_mode);

u_int32 cushion_get_size           (struct s_cushion_struct *c);
u_int32 cushion_set_size           (struct s_cushion_struct *c, u_int32 new_size);

u_int32 cushion_step_up            (struct s_cushion_struct *c);
u_int32 cushion_step_down          (struct s_cushion_struct *c);
u_int32 cushion_get_step           (struct s_cushion_struct *c);

u_int32 cushion_use_estimate       (struct s_cushion_struct *c);
int32   cushion_diff_estimate_size (struct s_cushion_struct *c);

#endif 


