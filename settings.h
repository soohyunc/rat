/*
 * FILE:    settings.h
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 *
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

void settings_load_early(session_t *sp);
void settings_load_late(session_t *sp);
void settings_save(session_t *sp);

