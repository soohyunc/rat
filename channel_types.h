/*
 * FILE:      channel_types.h
 * AUTHOR(S): Orion Hodson 
 *	
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions 
 * are met:
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
#ifndef __CHANNEL_TYPES_H__
#define __CHANNEL_TYPES_H__

/* Channel coder description information */

typedef u_int32 cc_id_t;

#define CC_NAME_LENGTH 32

typedef struct {
        cc_id_t descriptor;
        char    name[CC_NAME_LENGTH];
} cc_details;

/* In and out unit types.  On input channel encoder takes a playout buffer
 * of media_units and puts channel_units on the output playout buffer
 */

#define MAX_CHANNEL_UNITS    3
#define MAX_UNITS_PER_PACKET 8

typedef struct {
        u_int8  pt;
        u_char *data;
        u_int32 data_start; /* We use data_start to indicate offset where
                             * channel data begins relative to data(packet)
                             * since this saves an allocation, copy, and free.
                             */
        u_int32 data_len;   /* This is the length for processing purposes */
} channel_unit;

typedef struct {
        u_int8        nelem;
        channel_unit *elem[MAX_CHANNEL_UNITS];
} channel_data;

int  channel_data_create  (channel_data **cd, 
                           int            nelem);

void channel_data_destroy (channel_data **cd, 
                           u_int32        cdsize);

#endif /* __CHANNEL_TYPES_H__ */
