/*
 * FILE:    confbus_cmnd.c
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

#include <stdio.h>
#include "assert.h"
#include "config.h"
#include "util.h"
#include "confbus_misc.h"
#include "confbus_cmnd.h"

static void cb_cmnd_add_param(cb_cmnd *cmnd, cb_param *param)
{
	param->prev = cmnd->tail_param;
	param->next = NULL;

	cmnd->tail_param = param;
	if (cmnd->head_param == NULL) {
		cmnd->head_param = param;
	}
	if (param->prev != NULL) {
		param->prev->next = param;
	}
	cmnd->num_params++;
	cmnd->curr_param = cmnd->head_param;
}

void cb_cmnd_add_param_int(cb_cmnd *cmnd, int i) 
{
	cb_param *param = (cb_param *) xmalloc(sizeof(cb_param));
	param->val.integer = i;
	param->type        = CB_INTEGER;
	cb_cmnd_add_param(cmnd, param);
}

void cb_cmnd_add_param_flt(cb_cmnd *cmnd, float f) 
{
	cb_param *param = (cb_param *) xmalloc(sizeof(cb_param));
	param->val.flt = f;
	param->type    = CB_FLOAT;
	cb_cmnd_add_param(cmnd, param);
}

void cb_cmnd_add_param_str(cb_cmnd *cmnd, char *s)
{
	cb_param *param = (cb_param *) xmalloc(sizeof(cb_param));
	param->val.str = xstrdup(s);
	param->type    = CB_STRING;
	cb_cmnd_add_param(cmnd, param);
}

void cb_cmnd_add_param_sym(cb_cmnd *cmnd, char *s)
{
	cb_param *param = (cb_param *) xmalloc(sizeof(cb_param));
	param->val.sym = xstrdup(s);
	param->type    = CB_SYMBOL;
	cb_cmnd_add_param(cmnd, param);
}

int cb_cmnd_get_param_int(cb_cmnd *cmnd, int *i)
{
	if ((cmnd->curr_param == NULL) || (cmnd->curr_param->type != CB_INTEGER)) {
		return FALSE;
	}
	*i = cmnd->curr_param->val.integer;
	cmnd->curr_param = cmnd->curr_param->next;
	return TRUE;
}

int cb_cmnd_get_param_flt(cb_cmnd *cmnd, float *f)
{
	if ((cmnd->curr_param == NULL) || (cmnd->curr_param->type != CB_FLOAT)) {
		return FALSE;
	}
	*f = cmnd->curr_param->val.flt;
	cmnd->curr_param = cmnd->curr_param->next;
	return TRUE;
}

int cb_cmnd_get_param_str(cb_cmnd *cmnd, char **s)
{
	if ((cmnd->curr_param == NULL) || (cmnd->curr_param->type != CB_STRING)) {
		return FALSE;
	}
	*s = cb_decode_str(cmnd->curr_param->val.str);
	cmnd->curr_param = cmnd->curr_param->next;
	return TRUE;
}

int cb_cmnd_get_param_sym(cb_cmnd *cmnd, char *s)
{
	if ((cmnd->curr_param == NULL) || (cmnd->curr_param->type != CB_SYMBOL)) {
		return FALSE;
	}
	if (strcmp(s, cmnd->curr_param->val.sym) != 0) {
		return FALSE;
	}
	cmnd->curr_param = cmnd->curr_param->next;
	return TRUE;
}

cb_cmnd *cb_cmnd_init(char *cmnd_name, cb_cmnd *next)
{
	cb_cmnd *cmnd = (cb_cmnd *) xmalloc(sizeof(cb_cmnd));
	cmnd->next       = next;
	cmnd->cmnd       = xstrdup(cmnd_name);
	cmnd->head_param = NULL;
	cmnd->tail_param = NULL;
	cmnd->curr_param = NULL;
	cmnd->num_params = 0;
	return cmnd;
}

void cb_cmnd_free(cb_cmnd *cmnd)
{
	cb_param *p, *q;

	xfree(cmnd->cmnd);
	p = cmnd->head_param;
	while (p != NULL) {
		q = p->next;
		xfree(p);
		p = q;
		cmnd->num_params--;
	}
	assert(cmnd->num_params == 0);
	xfree(cmnd);
}

