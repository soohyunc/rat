/*
 * FILE:    ui.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 * 	
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996,1997 University College London
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

#include "config.h"
#include "session.h"
#include "codec.h"
#include "mbus.h"
#include "ui.h"

static int
codec_bw_cmp(const void *a, const void *b)
{
        int bwa, bwb;
        bwa = (*((codec_t**)a))->max_unit_sz;
        bwb = (*((codec_t**)b))->max_unit_sz;
        if (bwa<bwb) {
                return 1;
        } else if (bwa>bwb) {
                return -1;
        } 
        return 0;
}
 
void 
ui_codecs(session_struct *sp)
{
	char	 arg[1000], *a;
	codec_t	*codec[10],*sel;
	int 	 i, nc;

	a = &arg[0];
        sel = get_codec(sp->encodings[0]);
        
	for (nc=i=0; i<MAX_CODEC; i++) {
		codec[nc] = get_codec(i);
		if (codec[nc] != NULL && codec_compatible(sel,codec[nc])) {
                        nc++;
                        assert(nc<10); 
		}
	}

        /* sort by bw as this makes handling of acceptable redundant codecs easier in ui */
        qsort(codec,nc,sizeof(codec_t*),codec_bw_cmp);
        for(i=0;i<nc;i++) {
                sprintf(a, " %s", codec[i]->name);
                a += strlen(codec[i]->name) + 1;
        }

	mbus_send(sp->mbus_engine_chan, sp->mbus_ui_addr, "codec.supported", arg, TRUE);
}

