/*
 * FILE:    transmit.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Isidor Kouvelas
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-1999 University College London
 * All rights reserved.
 *
 */

#ifndef _transmit_h_
#define _transmit_h_

#include "session.h"

struct s_tx_buffer;
struct session_tag;
struct s_speaker_table;
struct s_minibuf;

int   tx_create      (struct s_tx_buffer **tb,
                      struct session_tag  *sp,
                      struct s_time       *clock,
                      u_int16 unit_size, 
                      u_int16 channels);

void  tx_destroy     (struct s_tx_buffer **tb);
void  tx_start       (struct s_tx_buffer  *tb);
void  tx_stop        (struct s_tx_buffer  *tb);

__inline int   
      tx_is_sending  (struct s_tx_buffer  *tb);

int   tx_read_audio    (struct s_tx_buffer *tb);
int   tx_process_audio (struct s_tx_buffer *tb);
void  tx_send          (struct s_tx_buffer *tb);
void  tx_update_ui     (struct s_tx_buffer *tb);
void  tx_igain_update  (struct s_tx_buffer *tb);

#endif /* _transmit_h_ */
