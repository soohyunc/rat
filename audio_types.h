/*
 * FILE:     audio_types.h
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998 University College London
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
 * OR SERVICES; LLUIGI OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE PLUIGIIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _RAT_AUDIO_TYPES_H_
#define _RAT_AUDIO_TYPES_H_

/* This version of the code can only work with a constant device      */
/* encoding. To use different encodings the parameters below have     */
/* to be changed and the program recompiled.                          */
/* The clock translation problems have to be taken into consideration */
/* if different users use different base encodings...                 */

typedef enum {
	DEV_PCMU,
	DEV_S8,
	DEV_S16
} deve_e;

typedef struct s_audio_format {
  deve_e encoding;
  int    sample_rate; 		/* Should be one of 8000, 16000, 24000, 32000, 48000 */
  int    bits_per_sample;	/* Should be 8 or 16 */
  int    channels;  	/* Should be 1 or 2  */
  int    bytes_per_block;       /* size of unit we will read/write in */
} audio_format;

typedef short sample;       /* Sample representation 16 bit signed */
typedef int audio_desc_t;   /* Unique handle for identifying audio devices */

#define AUDIO_DEVICE_NAME_LENGTH 63

typedef struct {
        audio_desc_t descriptor;
        char         name[AUDIO_DEVICE_NAME_LENGTH + 1];
} audio_device_details_t;

typedef int audio_port_t;

#define AUDIO_PORT_NAME_LENGTH 20

#define AUDIO_PORT_SPEAKER    "Speaker"
#define AUDIO_PORT_HEADPHONE  "Headphone"
#define AUDIO_PORT_LINE_OUT   "Line-Out"
#define AUDIO_PORT_MICROPHONE "Microphone"
#define AUDIO_PORT_LINE_IN    "Line-In"
#define AUDIO_PORT_CD         "CD"

typedef struct {
        audio_port_t port;
        char         name[AUDIO_PORT_NAME_LENGTH + 1];
} audio_port_details_t;

#define BYTES_PER_SAMPLE sizeof(sample)
#define PCMU_AUDIO_ZERO	127
#define L16_AUDIO_ZERO	0
#define MAX_AMP		100
#define DEVICE_REC_BUF	16000
#define DEVICE_BUF_UNIT	320

#endif /* _RAT_AUDIO_TYPES_H_ */




