/*
 * FILE:      source.c
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

#include "playout.h"
#include "source.h"

#include "channel_types.h"
#include "codec_types.h"

#include "debug.h"
#include "util.h"

typedef struct s_source {
        struct s_source         *next;
        struct s_source         *prev;
        struct s_rtcp_dbentry   *dbe;
        struct s_playout_buffer *channel;
        struct s_playout_buffer *media;
} source;

/* A linked list is used for sources and this is fine since we mostly
 * expect 1 or 2 sources to be simultaneously active and so efficiency
 * is not a killer.  */

typedef struct s_source_list {
        source  sentinel;
        u_int16 nsrcs;
} source_list;

int
source_list_create(source_list **pplist)
{
        source_list *plist = (source_list*)xmalloc(sizeof(source_list));
        if (plist != NULL) {
                *pplist = plist;
                plist->sentinel.next = &plist->sentinel;
                plist->sentinel.prev = &plist->sentinel;
                plist->nsrcs = 0;
                return TRUE;
        }
        return FALSE;
}

void
source_list_destroy(source_list **pplist)
{
        source_list *plist = *pplist;
        assert(plist != NULL);
        
        while(plist->sentinel.next != &plist->sentinel) {
                source_remove(plist, plist->sentinel.next);
        }

        assert(plist->nsrcs == 0);
        xfree(plist);
        *pplist = NULL;
}

/* The following two functions are provided so that we can estimate
 * the sources that have gone in to mixer for transcoding.  
 */

u_int32
source_list_source_count(source_list *plist)
{
        return plist->nsrcs;
}

struct s_rtcp_dbentry*
source_list_get_rtcp_dbentry(source_list *plist, u_int32 n)
{
        source *curr;
        assert(plist != NULL);
        if (n < plist->nsrcs) {
                curr = plist->sentinel.next;
                while(n != 0) {
                        curr = curr->next;
                        n--;
                }
                return curr->dbe;
        }
        return NULL;
}

source*
source_get(source_list *plist, struct s_rtcp_dbentry *dbe)
{
        source *curr, *stop;
        assert(plist != NULL);
        assert(dbe   != NULL);
        
        curr = plist->sentinel.next; 
        stop = &plist->sentinel;
        while(curr != stop) {
                if (curr->dbe == dbe) return curr;
                curr = curr->next;
        }
 
        return NULL;
}

source*
source_create(source_list *plist, struct s_rtcp_dbentry *dbe)
{
        source *psrc;
        int     success;

        assert(plist != NULL);
        assert(dbe   != NULL);
        assert(source_get(plist, dbe) != NULL);

        psrc = (source*)block_alloc(sizeof(source));
        
        if (psrc == NULL) return NULL;

        psrc->dbe = dbe;
        
        /* Allocate channel and media buffers */
        success = playout_buffer_create(&psrc->channel,
                                        (playoutfreeproc)channel_data_destroy,
                                        0);
        if (!success) {
                debug_msg("Failed to allocate channel buffer\n");
                block_free(psrc, sizeof(source));
                return NULL;
        }

        /* Note we hold onto 1000 clock ticks worth audio for
         * repair purposes.
         */
        success = playout_buffer_create(&psrc->media,
                                        (playoutfreeproc)media_data_destroy,
                                        1000);
        if (!success) {
                debug_msg("Failed to allocate media buffer\n");
                playout_buffer_destroy(&psrc->channel);
                block_free(psrc, sizeof(source));
                return NULL;
        }

        /* List maintenance */
        psrc->next = plist->sentinel.next;
        psrc->prev = &plist->sentinel;
        psrc->next->prev = psrc;
        psrc->prev->next = psrc;
        plist->nsrcs++;

        return psrc;
}

void
source_remove(source_list *plist, source *psrc)
{
        assert(plist);
        assert(psrc);
        assert(source_get(plist, psrc->dbe) != NULL);

        psrc->next->prev = psrc->prev;
        psrc->prev->next = psrc->next;

        playout_buffer_destroy(&psrc->channel);
        playout_buffer_destroy(&psrc->media);

        plist->nsrcs--;

        block_free(psrc, sizeof(source));
}
              
