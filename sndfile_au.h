/*
 * FILE:    sndfile_au.h
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

#ifndef __SNDFILE_AU_H__
#define __SNDFILE_AU_H__

int sun_read_hdr(FILE *pf, char **state);                            /* Returns true if can decode header */

int sun_read_audio(FILE *pf, char* state, sample *buf, int samples); /* Returns the number of samples read */

int sun_write_hdr(FILE *fp, char **state, sndfile_fmt_e encoding, int freq, int channels);

int sun_write_audio(FILE *fp, char *state, sample *buf, int samples);

int sun_free_state(char **state);

u_int16 sun_get_channels(char *state);

u_int16 sun_get_rate(char *state);

#endif /* __SNDFILE_AU_H__ */
