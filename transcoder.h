/*
 * FILE:     transcoder.h
 * PROGRAM:  Rat
 * AUTHOR:   Colin Perkins
 *
 * $Revision$
 * $Date$
 *
 * Copyright (C) 1996,1997 University College London
 * All rights reserved.
 *
 */

#ifndef _TRANSCODER
#define _TRANSCODER

struct s_audio_format;

int  transcoder_open(void);
void transcoder_close(int id);
int  transcoder_read(int id, sample *buf, int buf_size);
int  transcoder_write(int id, sample *buf, int buf_size);

#endif

