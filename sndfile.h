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

struct s_snd_file;

int  snd_read_open (struct s_snd_file **sf, 
                    char    *path);

int  snd_read_close (struct s_snd_file **sf);

int  snd_read_audio (struct s_snd_file **sf, 
                     sample *buf, 
                     u_int16 buf_len);

int  snd_write_open (struct s_snd_file **sf,
                     char *path,
                     u_int16  freq,
                     u_int16  channels);

int  snd_write_close (struct s_snd_file **sf);

int  snd_write_audio (struct s_snd_file **sf, 
                      sample *buf, 
                      u_int16 buf_len);

u_int16  snd_get_channels(struct s_snd_file *sf);

u_int16  snd_get_rate    (struct s_snd_file *sf);

int  snd_pause (struct s_snd_file *sf);

int  snd_resume (struct s_snd_file *sf);

#ifdef __cplusplus
}
#endif
#endif /* __SND_FILE_H */
