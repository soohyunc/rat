/*
 * FILE:    settings.h
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

void settings_load_early(session_t *sp);
void settings_load_late(session_t *sp);
void settings_save(session_t *sp);

