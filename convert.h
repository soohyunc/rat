/*
 * FILE:    convert.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-99 University College London
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

#ifndef _convert_h_
#define _convert_h_

struct s_coded_unit;

typedef struct s_converter_fmt {
        u_int16 from_channels;
        u_int16 from_freq;
        u_int16 to_channels;
        u_int16 to_freq;
} converter_fmt_t;

typedef u_int32 converter_id_t;

typedef struct {
        converter_id_t id;
        const char*    name;
} converter_details_t;

struct  s_converter;

/* Application pcm conversion functions */
void         converters_init(void);
void         converters_free(void);

/* Participant specific pcm conversion functions */
struct s_converter* converter_create  (converter_id_t   id, 
                                       converter_fmt_t *cf);

const converter_fmt_t*          
             converter_get_format(struct s_converter  *c);

int          converter_process   (struct s_converter  *c, 
                                  struct s_coded_unit *in, 
                                  struct s_coded_unit *out);
void         converter_destroy   (struct s_converter **c);

/* Converter selection functions */
u_int32 converter_get_count(void);
int     converter_get_details(u_int32              idx, 
                              converter_details_t *cd);

#endif /* _convert_h_ */
