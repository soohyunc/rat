/*
 *	FILE:    repair.h
 *	PROGRAM: RAT
 *	AUTHOR:  Orion Hodson
 *
 * 	$Revision$
 * 	$Date$
 *
 * Copyright (c) 1995,1996 University College London
 * All rights reserved.
 *
 */

#ifndef _REPAIR_H_
#define _REPAIR_H_

#include "codec_types.h"
#include "codec_state.h"

int repair(int                          repair,
           int                          consec_lost,
           struct s_codec_state_store *states,
           media_data                  *prev, 
           coded_unit                  *missing);


u_int16         repair_get_count   (void);
const char     *repair_get_name    (u_int16 scheme);
u_int16         repair_get_by_name (const char *name);

void repair_set_codec_specific_allowed(int allowed);
int  repair_get_codec_specific_allowed(void);

#endif /* _REPAIR_H_ */

