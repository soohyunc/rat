/*
 * FILE:    time.h
 * PROGRAM: RAT
 * AUTHOR:  I.Kouvelas
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 *
 */

#ifndef _TIMERS_H
#define _TIMERS_H

struct s_fast_time;
struct s_time;

struct s_fast_time *new_fast_time(int freq);
void                free_fast_time(struct s_fast_time *ft);
struct s_time      *new_time(struct s_fast_time *ft, int freq);
void	            free_time(struct s_time *tp);

uint32_t  convert_time(uint32_t ts, struct s_time *from, struct s_time *to);
void     time_advance(struct s_fast_time *ft, int freq, uint32_t time);
void     change_freq(struct s_time *tp, int freq);
int      get_freq(struct s_time *tp);
uint32_t  get_time(struct s_time *tp);

#endif /* _rat_time_h_ */
