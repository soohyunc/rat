/*
 * FILE:    sndfile_wav.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-99 University College London
 * All rights reserved.
 *
 */

#ifndef __SNDFILE_WAV_H__
#define __SNDFILE_WAV_H__

int riff_read_hdr(FILE *pf, char **state);                            /* Returns true if can decode header */

int riff_read_audio(FILE *pf, char* state, sample *buf, int samples); /* Returns the number of samples read */

int riff_write_hdr(FILE *fp, char **state, sndfile_fmt_e encoding, int freq, int channels);

int riff_write_audio(FILE *fp, char *state, sample *buf, int samples);

int riff_write_end(FILE *fp, char *state);

int riff_free_state(char **state);

u_int16 riff_get_channels(char *state);

u_int16 riff_get_rate(char *state);

#endif /* __SNDFILE_WAV_H__ */
