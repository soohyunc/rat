/*
 * FILE:    vdvi.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 *
 * $Id$
 */

#ifndef _VDVI_H_
#define _VDVI_H_

#define VDVI_SAMPLES_PER_FRAME 160

int /* Returns output frame size, 0 when error*/
vdvi_encode(unsigned char *dvi_buf, unsigned int dvi_samples, bitstream_t *bs_out);

int /* Returns number of bytes in in_bytes used to generate dvi_samples */
vdvi_decode(bitstream_t *bs_in, unsigned char *buf, unsigned int dvi_samples);

#endif /* _VDVI_H_ */



