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
#include "repair_types.h"

int repair(repair_id_t                 r,
           int                         consec_lost,
           struct s_codec_state_store *states,
           media_data                  *prev, 
           coded_unit                  *missing);


u_int16                 repair_get_count   (void);
const repair_details_t *repair_get_details (u_int16 n);

void repair_set_codec_specific_allowed(int allowed);
int  repair_get_codec_specific_allowed(void);

#endif /* _REPAIR_H_ */

