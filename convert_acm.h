/*
 * FILE:    convert_acm.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 */

#ifndef __CONVERT_ACM_H__
#define __CONVERT_ACM_H__

int  acm_cv_startup  (void);
void acm_cv_shutdown (void);

int  acm_cv_create  (const converter_fmt_t *cfmt, u_char **state, uint32_t *state_len);
void acm_cv_destroy (u_char **state, uint32_t *state_len);
void acm_cv_convert (const converter_fmt_t *cfmt, u_char *state, 
                     sample *src_buf, int src_len, 
                     sample *dst_buf, int dst_len);

#endif /* __CONVERT_ACM_H__ */
