/*
 * FILE:    codec_compat.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

/* Backward compatibility name mapping for earlier MBONE audio applications */
/* Returns compatible name if found, or cname passed to it if not.          */
const char *codec_get_compatible_name(const char *cname);
