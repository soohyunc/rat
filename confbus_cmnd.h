/*
 * FILE:    confbus_cmnd.h
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#ifndef _CONFBUS_CMND_H
#define _CONFBUS_CMND_H

typedef enum {
	CB_INTEGER, CB_FLOAT, CB_SYMBOL, CB_STRING, CB_LIST
} cb_param_type;

typedef union {
	int	 	 integer;
	float	 	 flt;
	char		*sym;
	char		*str;
	struct cb_parm	*list;
} cb_param_val;

typedef struct cb_parm {
	struct cb_parm	*next;
	struct cb_parm	*prev;
	cb_param_type	 type;
	cb_param_val	 val;
} cb_param;

typedef struct cb_cmnd {
	struct cb_cmnd	*next;
	char		*cmnd;
	cb_param	*head_param;
	cb_param	*tail_param;
	cb_param	*curr_param;
	int		 num_params;
} cb_cmnd;

cb_cmnd *cb_cmnd_init(char *cmnd_name, cb_cmnd *next);
void     cb_cmnd_free(cb_cmnd *cmnd);

void     cb_cmnd_add_param_int(cb_cmnd *cmnd, int    i);
void     cb_cmnd_add_param_flt(cb_cmnd *cmnd, float  f);
void     cb_cmnd_add_param_str(cb_cmnd *cmnd, char  *s);
void     cb_cmnd_add_param_sym(cb_cmnd *cmnd, char  *s);

int	 cb_cmnd_get_param_int(cb_cmnd *cmnd, int   *i);
int	 cb_cmnd_get_param_flt(cb_cmnd *cmnd, float *f);
int	 cb_cmnd_get_param_str(cb_cmnd *cmnd, char  **s);
int	 cb_cmnd_get_param_sym(cb_cmnd *cmnd, char  *s);

#endif
