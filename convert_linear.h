/*
 * FILE:    convert_linear.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-9 University College London
 * All rights reserved.
 *
 */

#ifndef __CONVERT_LINEAR_H__
#define __CONVERT_LINEAR_H__

int  linear_create  (const converter_fmt_t *cfmt, u_char **state, uint32_t *state_len);
void linear_destroy (u_char **state, uint32_t *state_len);
void linear_convert (const converter_fmt_t *cfmt, u_char *state, 
                     sample *src_buf, int src_len, 
                     sample *dst_buf, int dst_len);

#endif /* __CONVERT_LINEAR_H__ */
