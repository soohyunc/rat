/*
 * FILE:    repair_types.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-99 University College London
 * All rights reserved.
 *
 */

#ifndef __REPAIR_TYPES__
#define __REPAIR_TYPES__

typedef u_int32_t repair_id_t;

typedef struct {
        const char  *name;
        repair_id_t  id;
} repair_details_t;

#endif /* __REPAIR_TYPES__ */
