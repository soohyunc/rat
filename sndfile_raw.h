/*
 * FILE:    sndfile_raw.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 */

#ifndef __SNDFILE_RAW_H__
#define __SNDFILE_RAW_H__

int raw_read_hdr(FILE *pf, char **state, sndfile_fmt_t *fmt);        /* Returns true if can decode header */

int raw_read_audio(FILE *pf, char* state, sample *buf, int samples); /* Returns the number of samples read */

int raw_write_hdr(FILE *fp, char **state, const sndfile_fmt_t *fmt);

int raw_write_audio(FILE *fp, char *state, sample *buf, int samples);

int raw_free_state(char **state);

int raw_get_format(char *state, sndfile_fmt_t *fmt);
#endif /* __SNDFILE_RAW_H__ */
