/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header$ */

#ifndef FreeBSD
#include <malloc.h>
#endif

#include <stdlib.h>
#include "gsm.h"

void gsm_destroy(gsm S)
{
	if (S) free((char *)S);
}
