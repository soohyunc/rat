/*
 * FILE:    sndfile.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#ifndef __SND_FILE_H
#define __SND_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The interface below allows access to Sun .au files and MS .wav files.
 *
 * The decision about which format is used depends on the extension of
 * the file name.
 *
 * The functions snd_{read,write}_open take a pointer to a pointer 
 * of type struct s_sndfile.  This makes allocation/access more
 * restrictive, but safer.
 *
 * NOTE: for snd_read_audio and snd_write_audio, buf_len is the
 * product of the number of sampling intervals to be written and
 * the number of channels.
 */

#include "sndfile_types.h"
struct s_sndfile;

int  snd_read_open (struct s_sndfile **sf, 
                    char    *path);

int  snd_read_close (struct s_sndfile **sf);

int  snd_read_audio (struct s_sndfile **sf, 
                     sample *buf, 
                     u_int16 buf_len);

int  snd_write_open (struct s_sndfile **sf,
                     char         *path,
                     const sndfile_fmt_t *fmt);

int  snd_write_close (struct s_sndfile **sf);

int  snd_write_audio (struct s_sndfile **sf, 
                      sample *buf, 
                      u_int16 buf_len);

int  snd_pause (struct s_sndfile *sf);
int  snd_resume (struct s_sndfile *sf);

int  snd_get_format(struct s_sndfile *sf, sndfile_fmt_t *fmt);

#ifdef __cplusplus
}
#endif
#endif /* __SND_FILE_H */
