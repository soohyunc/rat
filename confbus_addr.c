/*
 * FILE:    confbus_addr.c
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

#include <string.h>
#include <stdio.h>
#include "config.h"
#include "assert.h"
#include "util.h"
#include "confbus_addr.h"

typedef struct s_cbaddr {
	char	*media_type;
	char	*module_type;
	char	*media_handling;
	char	*app_name;
	char	*app_instance;
} cb_addr;

static int cb_strcmp(char *s1, char *s2)
{
	if (s1 == NULL) return 0;
	if (s2 == NULL) return 0;
	return strcmp(s1, s2);
}

static char *cb_strdup(char *s)
{
	if (s == NULL) return NULL;
	return xstrdup(s);
}

int cb_addr_match(cb_addr *a1, cb_addr *a2)
{
	assert(a1 != NULL);
	assert(a2 != NULL);

	if (cb_strcmp(a1->media_type,     a2->media_type)     != 0) return FALSE;
	if (cb_strcmp(a1->module_type,    a2->module_type)    != 0) return FALSE;
	if (cb_strcmp(a1->media_handling, a2->media_handling) != 0) return FALSE;
	if (cb_strcmp(a1->app_name,       a2->app_name)       != 0) return FALSE;
	if (cb_strcmp(a1->app_instance,   a2->app_instance)   != 0) return FALSE;
	return TRUE;
}

char *cb_addr2str(cb_addr *a)
{
	int   l;
	char *s;
	
	l  = 7;
	l += a->media_type     == NULL ? 1 : strlen(a->media_type);
	l += a->module_type    == NULL ? 1 : strlen(a->module_type);
	l += a->media_handling == NULL ? 1 : strlen(a->media_handling);
	l += a->app_name       == NULL ? 1 : strlen(a->app_name);
	l += a->app_instance   == NULL ? 1 : strlen(a->app_instance);

	s = (char *) xmalloc(l);
	sprintf(s, "(%s %s %s %s %s)", a->media_type     == NULL ? "*" : a->media_type, 
	                               a->module_type    == NULL ? "*" : a->module_type, 
				       a->media_handling == NULL ? "*" : a->media_handling, 
				       a->app_name       == NULL ? "*" : a->app_name, 
				       a->app_instance   == NULL ? "*" : a->app_instance);
	return s;
}

cb_addr *cb_addr_dup(cb_addr *x)
{
	cb_addr	*addr = (cb_addr *) xmalloc(sizeof(cb_addr));
	addr->media_type     = cb_strdup(x->media_type);
	addr->module_type    = cb_strdup(x->module_type);
	addr->media_handling = cb_strdup(x->media_handling);
	addr->app_name       = cb_strdup(x->app_name);
	addr->app_instance   = cb_strdup(x->app_instance);
	return addr;
}

cb_addr *cb_addr_init(char *media_type, char *module_type, char *media_handling, char *app_name, char *app_instance)
{
	cb_addr	*addr = (cb_addr *) xmalloc(sizeof(cb_addr));
	addr->media_type     = cb_strdup(media_type);
	addr->module_type    = cb_strdup(module_type);
	addr->media_handling = cb_strdup(media_handling);
	addr->app_name       = cb_strdup(app_name);
	addr->app_instance   = cb_strdup(app_instance);
	return addr;
}

void cb_addr_free(cb_addr *addr)
{
	if (addr == NULL) {
		/* Nothing to free... */
		return;
	}
	if (addr->media_type     != NULL) xfree(addr->media_type);
	if (addr->module_type    != NULL) xfree(addr->module_type);
	if (addr->media_handling != NULL) xfree(addr->media_handling);
	if (addr->app_name       != NULL) xfree(addr->app_name);
	if (addr->app_instance   != NULL) xfree(addr->app_instance);
	xfree(addr);
}


