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

#include "new_channel.h"
#include "channel_types.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "timers.h"

#include "debug.h"
#include "util.h"

typedef struct s_source {
        struct s_source            *next;
        struct s_source            *prev;
        struct s_rtcp_dbentry      *dbe;
        struct s_channel_state     *channel_state;
        struct s_codec_state_store *codec_states;
        struct s_playout_buffer    *channel;
        struct s_playout_buffer    *media;
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

        psrc->dbe           = dbe;
        psrc->channel_state = NULL;        

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

        success = codec_state_store_create(&psrc->codec_states, DECODER);
        if (!success) {
                debug_msg("Failed to allocate codec state storage\n");
                playout_buffer_destroy(&psrc->media);
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

        if (psrc->channel_state) channel_decoder_destroy(&psrc->channel_state);
        playout_buffer_destroy(&psrc->channel);
        playout_buffer_destroy(&psrc->media);
        codec_state_store_destroy(&psrc->codec_states);

        plist->nsrcs--;

        block_free(psrc, sizeof(source));
}
              
/* Source Processing Routines ************************************************/

/* Returns true if fn takes ownership responsibility for data */
int
source_add_packet (source *src, 
                   u_char *pckt, 
                   u_int32 pckt_len, 
                   u_char *data_start,
                   u_int8  payload,
                   u_int32 playout)
{
        channel_data *cd;
        channel_unit *cu;
        cc_id_t       cid;

        assert(src != NULL);
        assert(pckt != NULL);
        assert(data_start != NULL);

        if (channel_data_create(&cd, 1) == 0) {
                return FALSE;
        }
        
        cu             = cd->elem[0];
        cu->data       = pckt;
        cu->data_len   = pckt_len;
        cu->data_start = (u_int32)(data_start - pckt);
        cu->pt         = payload;

        /* Check we have state to decode this */
        cid = channel_coder_get_by_payload(cu->pt);
        if (src->channel_state && 
            channel_decoder_matches(cid, src->channel_state) == FALSE) {
                debug_msg("Channel coder changed - flushing\n");
                channel_decoder_destroy(&src->channel_state);
                playout_buffer_flush(src->channel);
        }

        /* Make state if not there and create decoder */
        if (src->channel_state == NULL && 
            channel_decoder_create(cid, &src->channel_state)) {
                debug_msg("Cannot decode payload %d\n", cu->pt);
                channel_data_destroy(&cd, sizeof(channel_data));
        }

        if (playout_buffer_add(src->channel, (u_char*)cd, sizeof(channel_data), playout) == FALSE) {
                debug_msg("Packet addition failed - duplicate ?\n");
                channel_data_destroy(&cd, sizeof(channel_data));
        }

        return TRUE;
}

int
source_process(source *src, u_int32 render_3d, u_int32 now)
{
        media_data  *md;
        coded_unit  *cu;
        codec_state *cs;
        u_int32     playout, md_len, curr_frame;

        /* Split channel coder units up into media units */
        channel_decoder_decode(src->channel_state,
                               src->channel,
                               src->media,
                               now);

        while(playout_buffer_get(src->media, (u_char**)&md, &md_len, &playout)) {
                assert(md != NULL);
                assert(md_len == sizeof(media_data));

                if (src->age != 0 && playout != src->last_playout + src->last_unit_dur) {
                        /* Repair necessary 
                         * - create unit at src->last_playout + src->last_unit_dur;
                         * - write repair data
                         * - rewind a step
                         * - continue
                         * probably want to iterate forwards and do all repairs in one go.
                         */

                }

                if (ts_gt(playout, now)) {
                        /* This playout point is after now so stop */
                        break;
                }

                playout_buffer_remove(src->media, (u_char**)&md, &md_len, &playout);

                /* Decode frame */
                assert(cu != NULL);
                cu = (coded_unit*)block_alloc(sizeof(coded_unit));

                cs = codec_state_get(src->codec_states, md->rep[0]->id);
                codec_decode_into_coded_unit(md->rep[0], cu);
                md->rep[md->nrep] = cu;
                md->nrep++;

                if (render_3d) {
                        /* 3d rendering necessary */
                }

                if (conversion_necessary) {
                        /* convert frame */

                }

                /* write to mixer */

        }
        
        return TRUE;
}
