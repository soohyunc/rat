/*
 * FILE:    codec_state.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996 University College London
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

#include "config_unix.h"
#include "config_win32.h"

#include "debug.h"
#include "memory.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"

#define CODEC_STORE_UNIT_SIZE 3

typedef struct s_codec_state_store {
        codec_state **buffer;
        int used;
        int allocated;
        codec_mode    mode;
} codec_state_store_t;

int 
codec_state_store_create(codec_state_store_t **s, codec_mode m)
{
        codec_state_store_t *css;

        css = (codec_state_store_t*)xmalloc(sizeof(codec_state_store_t));
        if (css) {
                css->buffer    = (codec_state**)xmalloc(CODEC_STORE_UNIT_SIZE * sizeof(codec_state*));
                css->used      = 0;
                css->allocated = CODEC_STORE_UNIT_SIZE;
                css->mode      = m;
                *s = css;
                return TRUE;
        }
        return FALSE;
}

static int
codec_state_store_expand(codec_state_store_t *css)
{
        int i;
        codec_state **buffer;

        /* This should very very rarely get called */

        buffer = (codec_state**)xmalloc((css->allocated + CODEC_STORE_UNIT_SIZE) * sizeof(codec_state*));

        memset(buffer + CODEC_STORE_UNIT_SIZE*sizeof(codec_state*), 0,
               CODEC_STORE_UNIT_SIZE*sizeof(codec_state*));

        for(i = 0; i < css->allocated; i++) {
                buffer[i] = css->buffer[i];
        }

        xfree(css->buffer);
        css->buffer     = buffer;
        css->allocated += CODEC_STORE_UNIT_SIZE;
        return TRUE;
}

codec_state *
codec_state_store_get(codec_state_store_t *css, codec_id_t id)
{
        codec_state *s;
        int i;
        
        for(i = 0; i < css->used; i++) {
                if (css->buffer[i]->id == id) {
                        return css->buffer[i];
                }
        }

        /* Did not find state */
        switch (css->mode) {
        case ENCODER:
                codec_encoder_create(id, &s);
                break;
        case DECODER:
                codec_decoder_create(id, &s);
                break;
        }
        
        if (css->used == css->allocated) {
                codec_state_store_expand(css);
                debug_msg("Expanding storage for participant states.\n");
        }

        css->buffer[css->used] = s;
        css->used++;

        return s;
}

void
codec_state_store_destroy(codec_state_store_t **css)
{
        int i;
        
        switch((*css)->mode) {
        case ENCODER:
                for(i = 0; i < (*css)->used; i++) {
                        codec_encoder_destroy(&(*css)->buffer[i]);
                }
                break;
        case DECODER:
                for(i = 0; i < (*css)->used; i++) {
                        codec_decoder_destroy(&(*css)->buffer[i]);
                }
                break;
        }
        xfree((*css)->buffer);
        xfree(*css);
        *css = NULL;
}

void
codec_state_store_remove (codec_state_store_t *css,
                          codec_id_t           id)
{
        int new_idx, old_idx;
        
        for(new_idx = old_idx = 0; old_idx < css->used; old_idx++) {
                if (css->buffer[old_idx]->id == id) {
                        switch(css->mode) {
                        case ENCODER:
                                codec_encoder_destroy(&css->buffer[old_idx]);
                                break;
                        case DECODER:
                                codec_decoder_destroy(&css->buffer[old_idx]);
                                break;
                        }
                        css->used --;
                } else {
                        /* These are not the droids we are looking for... */
                        new_idx++;
                }
        }
}
