/*
 * FILE:     transcoder.h
 * PROGRAM:  Rat
 * AUTHOR:   Colin Perkins
 *
 * Copyright (C) 1996-2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _TRANSCODER
#define _TRANSCODER

struct s_audio_format;

int  transcoder_open(void);
void transcoder_close(int id);
int  transcoder_read(int id, sample *buf, int buf_size);
int  transcoder_write(int id, sample *buf, int buf_size);

#endif

