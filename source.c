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

#include "ts.h"
#include "playout.h"

#include "new_channel.h"
#include "channel_types.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "convert.h"
#include "render_3D.h"
#include "repair.h"
#include "timers.h"
#include "ts.h"
#include "source.h"

#include "debug.h"
#include "util.h"

/* And we include all of the below just so we can get at
 * the render_3d_data field of the rtcp_dbentry for the source!
 */
#include "net_udp.h"
#include "rtcp.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"

#define HISTORY 1000

void mix_add_audio(struct s_rtcp_dbentry*, coded_unit *, ts_t);

typedef struct s_source {
        struct s_source            *next;
        struct s_source            *prev;
        u_int32                     age;
        ts_t                        last_played;
        ts_sequencer                seq;
        struct s_rtcp_dbentry      *dbe;
        struct s_channel_state     *channel_state;
        struct s_codec_state_store *codec_states;
        struct s_playout_buffer    *channel;
        struct s_playout_buffer    *media;
        struct s_converter         *converter;
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
source_list_clear(source_list *plist)
{
       assert(plist != NULL);
        
       while(plist->sentinel.next != &plist->sentinel) {
               source_remove(plist, plist->sentinel.next);
       }
}

void
source_list_destroy(source_list **pplist)
{
        source_list *plist = *pplist;
        source_list_clear(plist);
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
source_create(source_list    *plist, 
              rtcp_dbentry   *dbe,
              converter_id_t  conv_id,
              u_int16         out_rate,
              u_int16         out_channels)
{
        source *psrc;
        int     success;

        assert(plist != NULL);
        assert(dbe   != NULL);
        assert(source_get(plist, dbe) != NULL);

        psrc = (source*)block_alloc(sizeof(source));
        
        if (psrc == NULL) return NULL;

        memset(psrc, 0, sizeof(source));
        psrc->dbe           = dbe;
        psrc->channel_state = NULL;        

        /* Allocate channel and media buffers */
        success = playout_buffer_create(&psrc->channel,
                                        (playoutfreeproc)channel_data_destroy,
                                        ts_map32(8000,0));

        if (!success) {
                debug_msg("Failed to allocate channel buffer\n");
                block_free(psrc, sizeof(source));
                return NULL;
        }

        /* Note we hold onto HISTORY clock ticks worth audio for
         * repair purposes.
         */
        success = playout_buffer_create(&psrc->media,
                                        (playoutfreeproc)media_data_destroy,
                                        ts_map32(8000,HISTORY));
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

        /* List maintenance    */
        psrc->next = plist->sentinel.next;
        psrc->prev = &plist->sentinel;
        psrc->next->prev = psrc;
        psrc->prev->next = psrc;
        plist->nsrcs++;

        /* Configure converter */
        source_reconfigure(psrc, conv_id, out_rate, out_channels);

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
                   ts_t    playout)
{
        channel_data *cd;
        channel_unit *cu;
        cc_id_t       cid;

        assert(src != NULL);
        assert(pckt != NULL);
        assert(data_start != NULL);

        if (src->age != 0 &&
            ts_gt(src->last_played, playout)) {
                debug_msg("Packet late (%u > %u)- discarding\n", 
                          src->last_played.ticks,
                          playout.ticks);
                /* Up src->dbe jitter toged */
                return FALSE;
        }

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
source_process(source *src, int repair_type, ts_t now)
{
        media_data  *md;
        coded_unit  *cu;
        codec_state *cs;
        u_int32     md_len, src_freq;
        ts_t        playout, curr, step, cutoff;
        int         i, success;

        /* Split channel coder units up into media units */
        channel_decoder_decode(src->channel_state,
                               src->channel,
                               src->media,
                               now);

        src_freq = get_freq(src->dbe->clock);
        step = ts_map32(src_freq,src->dbe->inter_pkt_gap / src->dbe->units_per_packet);

        while(playout_buffer_get(src->media, 
                                 (u_char**)&md, 
                                 &md_len, 
                                 &playout)) {
                assert(md != NULL);
                assert(md_len == sizeof(media_data));

                cutoff = ts_add(src->last_played, 
                                ts_map32(src_freq, HISTORY));
                if (src->age != 0 && 
                    ts_eq(playout, ts_add(src->last_played, step)) &&
                    ts_gt(cutoff, playout)) {
                        media_data* md_filler;
                        int         lost = 0;

                        curr = playout;

                        /* Rewind to last_played */
                        while(ts_gt(playout, src->last_played)) {
                                playout_buffer_rewind(src->media,
                                                      (u_char**)&md,
                                                      &md_len,
                                                      &playout);
                                assert(md != NULL);
                        }

                        curr = ts_add(playout, step);

                        while(ts_gt(curr, playout)) {
                                media_data_create(&md_filler, 1);
                                repair(repair_type,
                                       lost,
                                       src->codec_states,
                                       md,
                                       md_filler->rep[0]);
                                success = playout_buffer_add(src->media, 
                                                             (u_char*)md,
                                                             sizeof(media_data),
                                                             curr);
                                assert(success);
                                curr = ts_add(curr, step);
                                md = md_filler;
                        }

                        /* Step to next to be played i.e. the first we added */
                        success = playout_buffer_advance(src->media, 
                                                         (u_char**)&md,
                                                         &md_len,
                                                         &playout);
                        assert(success);
                }

                if (ts_gt(playout, now)) {
                        /* This playout point is after now so stop */
                        break;
                }

                success = playout_buffer_remove(src->media, (u_char**)&md, &md_len, &playout);

                assert(success);

                if (codec_is_native_coding(md->rep[0]->id) == FALSE) {
                        /* There is data to be decoded.  There may not be
                         * when we have used repair.
                         */

#ifdef DEBUG
                        for(i = 0; i < md->nrep; i++) {
                                /* if there is a native coding this
                                 * unit has already been decoded and
                                 * this would be bug */
                                assert(codec_is_native_coding(md->rep[i]->id) == FALSE);
                        }
#endif /* DEBUG */
                        cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                        /* Decode frame */
                        assert(cu != NULL);
                        memset(cu, 0, sizeof(coded_unit));
                        cs = codec_state_store_get(src->codec_states, md->rep[0]->id);
                        codec_decode(cs, md->rep[0], cu);
                        xmemchk();
                        md->rep[md->nrep] = cu;
                        md->nrep++;
                }

                if (src->dbe->render_3D_data) {
                        /* 3d rendering necessary */
                        coded_unit *decoded, *render;
                        decoded = md->rep[md->nrep - 1];
                        assert(codec_is_native_coding(decoded->id));
                        
                        render = (coded_unit*)block_alloc(sizeof(coded_unit));
                        memset(render, 0, sizeof(coded_unit));
                        
                        render_3D(src->dbe->render_3D_data,decoded,render);
                        xmemchk();
                        md->rep[md->nrep] = render;
                        md->nrep++;
                }

                if (src->converter) {
                        /* convert frame */
                        coded_unit *decoded, *render;
                        decoded = md->rep[md->nrep - 1];
                        assert(codec_is_native_coding(decoded->id));

                        render = (coded_unit*)block_alloc(sizeof(coded_unit));
                        memset(render, 0, sizeof(coded_unit));
                        converter_process(src->converter,
                                          decoded,
                                          render);
                        xmemchk();
                        md->rep[md->nrep] = render;
                        md->nrep++;
                }

                /* write to mixer */

                cu = md->rep[md->nrep - 1];
                assert(codec_is_native_coding(cu->id));
                mix_add_audio(src->dbe, cu, playout);
        }

        /* Get rid of stale data */
        playout_buffer_audit(src->media);

        src->age++;
        src->last_played = now;

        UNUSED(i); /* Except for debugging */
        
        return TRUE;
}

ts_sequencer*
source_get_sequencer(source *src)
{
        return &src->seq;
}
