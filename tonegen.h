/*
 * FILE:    tonegen.h
 * PROGRAM: RAT
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

typedef struct s_tonegen tonegen_t;

int  tonegen_create  (tonegen_t          **ppv, 
                      struct s_mixer     *mixer, 
                      struct s_fast_time *clock,
                      struct s_pdb       *pdb, 
                      uint16_t            tonefreq,
                      uint16_t            toneamp);
int  tonegen_play    (tonegen_t           *ppv, 
                      ts_t                start, 
                      ts_t                end);
void tonegen_destroy (tonegen_t          **ppv);


