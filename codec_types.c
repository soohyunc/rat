/*
 * FILE:      codec_types.c
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
#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"

#include "util.h"
#include "debug.h"

int  
media_data_create(media_data **ppmd, int nrep)
{
        media_data *pmd;
        int i;

        *ppmd = NULL;
        pmd   = (media_data*)block_alloc(sizeof(media_data));
        
        if (pmd) {
                memset(pmd, 0, sizeof(media_data));
                for(i = 0; i < nrep; i++) {
                        pmd->rep[i] = block_alloc(sizeof(coded_unit));
                        if (pmd->rep[i] == NULL) {
                                pmd->nrep = i;
                                media_data_destroy(&pmd, sizeof(media_data));
                                return FALSE;
                        }
                        memset(pmd->rep[i], 0, sizeof(coded_unit));
                }
                pmd->nrep    = nrep;
                *ppmd = pmd;
                return TRUE;
        }
        return FALSE;
}

void 
media_data_destroy(media_data **ppmd, u_int32 md_size)
{
        media_data *pmd;
        coded_unit *pcu;
        int         i;
        
        pmd = *ppmd;

        assert(pmd != NULL);
        assert(md_size == sizeof(media_data));

        for(i = 0; i < pmd->nrep; i++) {
                pcu = pmd->rep[i];
                if (pcu->state) {
                        block_free(pcu->state, pcu->state_len);
                        pcu->state     = 0;
                        pcu->state_len = 0;
                }
                assert(pcu->state_len == 0);
                if (pcu->data) {
                        block_free(pcu->data, pcu->data_len);
                        pcu->data     = 0;
                        pcu->data_len = 0;
                }
                assert(pcu->data_len == 0);
                block_free(pcu, sizeof(coded_unit));
        }
        block_free(pmd, sizeof(media_data));
        *ppmd = NULL;
}
