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

__inline converter_id_t
        converter_get_null_converter(void);

#endif /* _convert_h_ */
