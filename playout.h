/*
 * FILE:    playout.h
 * AUTHORS: Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef __UCLMM_PLAYOUT_BUFFER_H__
#define __UCLMM_PLAYOUT_BUFFER_H__

struct s_playout_buffer;

/* All functions return TRUE on success, and FALSE on failure */
int playout_buffer_create  (struct s_playout_buffer **pb, 
                            void (*datafreeproc)     (u_char **, u_int32), 
                            u_int32                   history_store);

int playout_buffer_destroy (struct s_playout_buffer **pb);

int playout_buffer_add     (struct s_playout_buffer *pb, 
                            u_char*                  data, 
                            u_int32                  datalen,
                            u_int32                  playout);

/*
 * These three functions return data stored in the playout buffer.  The playout buffer
 * has a playout point iterator.  playout_buffer_get returns the data at that point, 
 * advance steps to the next unit and returns that, and rewind steps to the previous unit
 * and returns that.
 */
int playout_buffer_advance (struct s_playout_buffer *pb, u_char** data, u_int32 *datalen, u_int32 *playout);
int playout_buffer_get     (struct s_playout_buffer *pb, u_char** data, u_int32 *datalen, u_int32 *playout);
int playout_buffer_rewind  (struct s_playout_buffer *pb, u_char** data, u_int32 *datalen, u_int32 *playout);

/* Removes data from playout point and puts it in *data */
int playout_buffer_remove  (struct s_playout_buffer *pb, u_char** data, u_int32 *datalen, u_int32 *playout); 

/* Trims data more than history_len before playout point    */
int playout_buffer_audit    (struct s_playout_buffer *pb);

/* Returns whether playout buffer has data to be played out */
int playout_buffer_relevent (struct s_playout_buffer *pb, u_int32 now);

#endif /* __UCLMM_PLAYOUT_BUFFER_H__ */
