/*
 * FILE:    converter.h
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

#ifndef _converter_h_
#define _converter_h_

#include "converter_types.h"

/* Application pcm conversion functions */
void converters_init(void);
void converters_free(void);

/* Participant specific pcm conversion functions */
int  converter_create (const converter_id_t   id, 
                       const converter_fmt_t *cfmt,
                       struct s_converter   **c);
void converter_destroy(struct s_converter **c);

const converter_fmt_t*          
             converter_get_format(struct s_converter  *c);
int          converter_process   (struct s_converter  *c, 
                                  struct s_coded_unit *in, 
                                  struct s_coded_unit *out);

/* Converter selection functions */
u_int32                    converter_get_count(void);
const converter_details_t* converter_get_details(u_int32 idx);

#endif /* _converter_h_ */
