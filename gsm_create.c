/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "assert.h"
#include "gsm.h"

#ifndef FreeBSD
#include <malloc.h>
#endif

struct gsm_state *gsm_create(void)
{
	struct gsm_state *r;

	r = (struct gsm_state *) malloc(sizeof(struct gsm_state));
	assert(r != NULL);

	memset(r, 0, sizeof(struct gsm_state));
	r->nrp = 40;

	return r;
}

