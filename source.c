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
 */

#include "config_unix.h"
#include "config_win32.h"

#include "ts.h"
#include "playout.h"

#include "channel.h"
#include "channel_types.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "converter.h"
#include "parameters.h"
#include "render_3D.h"
#include "repair.h"
#include "timers.h"
#include "ts.h"
#include "source.h"
#include "mix.h"
#include "debug.h"
#include "util.h"

/* And we include all of the below just so we can get at
 * the render_3d_data field of the rtcp_dbentry for the source!
 */
#include "net_udp.h"
#include "rtcp.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"

#define SKEW_OFFENSES_BEFORE_CONTRACTING_BUFFER  8 
#define SKEW_OFFENSES_BEFORE_EXPANDING_BUFFER    3
#define SKEW_ADAPT_THRESHOLD       1000
#define SOURCE_YOUNG_AGE             20
#define SOURCE_AUDIO_HISTORY_MS    1000

/* constants for skew adjustment:
 SOURCE_SKEW_SLOW - denotes source clock appears slower than ours.
 SOURCE_SKEW_FAST - denotes source clock appears faster than ours.
*/
typedef enum { SOURCE_SKEW_SLOW, SOURCE_SKEW_FAST, SOURCE_SKEW_NONE } skew_t;

typedef struct s_source {
        struct s_source            *next;
        struct s_source            *prev;
        u_int32                     age;
        ts_t                        last_played;
        ts_t                        last_repair;
        u_int16                     consec_lost;
        u_int32                     mean_energy;
        ts_sequencer                seq;
        struct s_rtcp_dbentry      *dbe;
        struct s_channel_state     *channel_state;
        struct s_codec_state_store *codec_states;
        struct s_pb                *channel;
        struct s_pb                *media;
        struct s_pb_iterator       *media_pos;
        struct s_converter         *converter;
        skew_t skew;
        ts_t   skew_adjust;
        int32  skew_offenses;
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

u_int32
source_list_source_count(source_list *plist)
{
        return plist->nsrcs;
}

source*
source_list_get_source_no(source_list *plist, u_int32 n)
{
/* This obviously does not scale, but does not have to for audio! */
        source *curr;
        assert(plist != NULL);
        if (n < plist->nsrcs) {
                curr = plist->sentinel.next;
                while(n != 0) {
                        curr = curr->next;
                        n--;
                }
                return curr;
        }
        return NULL;
}

source*
source_get_by_rtcp_dbentry(source_list *plist, struct s_rtcp_dbentry *dbe)
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
              int             render_3D_enabled,
              u_int16         out_rate,
              u_int16         out_channels)
{
        source *psrc;
        int     success;

        assert(plist != NULL);
        assert(dbe   != NULL);
        assert(source_get_by_rtcp_dbentry(plist, dbe) == NULL);

        psrc = (source*)block_alloc(sizeof(source));
        
        if (psrc == NULL) return NULL;

        memset(psrc, 0, sizeof(source));
        psrc->dbe            = dbe;
        psrc->dbe->first_mix = 1; /* Used to note we have not mixed anything
                                   * for this decode path yet */
        psrc->channel_state  = NULL;        

        psrc->skew           = SOURCE_SKEW_NONE;
        psrc->skew_offenses  = 0;
        /* Allocate channel and media buffers */
        success = pb_create(&psrc->channel, (playoutfreeproc)channel_data_destroy);
        if (!success) {
                debug_msg("Failed to allocate channel buffer\n");
                goto fail_create_channel;
        }

        success = pb_create(&psrc->media, (playoutfreeproc)media_data_destroy);
        if (!success) {
                debug_msg("Failed to allocate media buffer\n");
                goto fail_create_media;
        }

        success = pb_iterator_create(psrc->media, &psrc->media_pos);
        if (!success) {
                debug_msg("Failed to attach iterator to media buffer\n");
                goto fail_create_iterator;
        }

        success = codec_state_store_create(&psrc->codec_states, DECODER);
        if (!success) {
                debug_msg("Failed to allocate codec state storage\n");
                goto fail_create_states;
        }

        /* List maintenance    */
        psrc->next = plist->sentinel.next;
        psrc->prev = &plist->sentinel;
        psrc->next->prev = psrc;
        psrc->prev->next = psrc;
        plist->nsrcs++;

        /* Configure converter */
        source_reconfigure(psrc, 
                           conv_id, 
                           render_3D_enabled,
                           out_rate, 
                           out_channels);

        debug_msg("Created source decode path\n");

        return psrc;

        /* Failure fall throughs */
fail_create_states:
        pb_iterator_destroy(psrc->media, &psrc->media_pos);        
fail_create_iterator:
        pb_destroy(&psrc->media);
fail_create_media:
        pb_destroy(&psrc->channel);
fail_create_channel:
        block_free(psrc, sizeof(source));

        return NULL;
}

/* All sources need to be reconfigured when anything changes in
 * audio path.  These include change of device frequency, change of
 * the number of channels, etc..
 */

void
source_reconfigure(source        *src,
                   converter_id_t conv_id,
                   int            render_3d,
                   u_int16        out_rate,
                   u_int16        out_channels)
{
        u_int16    src_rate, src_channels;
        codec_id_t            src_cid;
        const codec_format_t *src_cf;
        assert(src->dbe != NULL);

        /* Set age to zero and flush existing media
         * so that repair mechanism does not attempt
         * to patch across different block sizes.
         */

        src->age = 0;
        pb_flush(src->media);

        /* Get rate and channels of incoming media so we know
         * what we have to change.
         */
        src_cid = codec_get_by_payload(src->dbe->enc);
        src_cf  = codec_get_format(src_cid);
        src_rate     = (u_int16)src_cf->format.sample_rate;
        src_channels = (u_int16)src_cf->format.channels;

        if (render_3d) {
                assert(out_channels == 2);
                /* Rejig 3d renderer if there, else create */
                if (src->dbe->render_3D_data) {
                        int azi3d, fil3d, len3d;
                        render_3D_get_parameters(src->dbe->render_3D_data,
                                                 &azi3d,
                                                 &fil3d,
                                                 &len3d);
                        render_3D_set_parameters(src->dbe->render_3D_data,
                                                 (int)src_rate,
                                                 azi3d,
                                                 fil3d,
                                                 len3d);
                } else {
                        src->dbe->render_3D_data = render_3D_init((int)src_rate);
                }
                assert(src->dbe->render_3D_data);
                /* Render 3d is before sample rate/channel conversion,
                 * and output 2 channels.
                 */
                src_channels = 2;
        } else {
                /* Rendering is switched off so destroy info */
                if (src->dbe->render_3D_data != NULL) {
                        render_3D_free(&src->dbe->render_3D_data);
                }
        }

        /* Now destroy converter if it is already there */
        if (src->converter) {
                converter_destroy(&src->converter);
        }

        if (src_rate != out_rate || src_channels != out_channels) {
                converter_fmt_t c;
                c.from_freq     = src_rate;
                c.from_channels = src_channels;
                c.to_freq       = out_rate;
                c.to_channels   = out_channels;
                converter_create(conv_id, &c, &src->converter);
        }
}

void
source_remove(source_list *plist, source *psrc)
{
        assert(plist);
        assert(psrc);
        assert(source_get_by_rtcp_dbentry(plist, psrc->dbe) != NULL);

        psrc->next->prev = psrc->prev;
        psrc->prev->next = psrc->next;

        if (psrc->channel_state) {
                channel_decoder_destroy(&psrc->channel_state);
        }

        if (psrc->converter) {
                converter_destroy(&psrc->converter);
        }

        pb_iterator_destroy(psrc->media, &psrc->media_pos);
        pb_destroy(&psrc->channel);
        pb_destroy(&psrc->media);
        codec_state_store_destroy(&psrc->codec_states);
        plist->nsrcs--;

        /* This is hook into the playout_adapt, we are signalling
         * there is no source decode path.
         */
        psrc->dbe->first_pckt_flag = TRUE;

        debug_msg("Destroying source decode path\n");
        
        block_free(psrc, sizeof(source));

        assert(source_get_by_rtcp_dbentry(plist, psrc->dbe) == NULL);
}
              
/* Source Processing Routines ************************************************/

/* Returns true if fn takes ownership responsibility for data */
int
source_add_packet (source *src, 
                   u_char *pckt, 
                   u_int32 pckt_len, 
                   u_int32 data_start,
                   u_int8  payload,
                   ts_t    playout)
{
        channel_data *cd;
        channel_unit *cu;
        cc_id_t       cid;
        u_int8        clayers;

        assert(src != NULL);
        assert(pckt != NULL);
        assert(data_start != 0);

        /* If last_played is valid then enough audio is buffer
         * for the playout check to be sensible
         */
        if (ts_valid(src->last_played) &&
            ts_gt(src->last_played, playout)) {
                debug_msg("Packet late (%u > %u)- discarding\n", 
                          src->last_played.ticks,
                          playout.ticks);
                /* Up src->dbe jitter toged */
                return FALSE;
        }

        /* Need to check:
         * (i) if layering is enabled
         * (ii) if channel_data exists for this playout point (if pb_iterator_get_at...)
         * Then need to:
         * (i) create cd if doesn't exist
         * (ii) add packet to cd->elem[layer]
         * We work out layer number by deducting the base port
         * no from the port no this packet came from
         * But what if layering on one port? 
         */

        /* Or we could:
         * (i) check if cd exists for this playout point
         * (ii) if so, memcmp() to see if this packet already exists (ugh!)
         */

        cid = channel_coder_get_by_payload(payload);
        clayers = channel_coder_get_layers(cid);
        if(clayers>1) {
                struct s_pb_iterator *pi;
                u_int8 i;
                u_int32 clen;
                int dup;
                ts_t lplayout;
                pb_iterator_create(src->channel, &pi);
                while(pb_iterator_advance(pi)) {
                        pb_iterator_get_at(pi, (u_char**)&cd, &clen, &lplayout);
                       /* if lplayout==playout there is already channel_data for this playout point */
                        if(!ts_eq(playout, lplayout)) continue;
                        pb_iterator_detach_at(pi, (u_char**)&cd, &clen, &lplayout);
                        assert(cd->nelem >= 1);

                       /* if this channel_data is full, this new packet must *
                        * be a duplicate, so we don't need to check          */
                        if(cd->nelem >= clayers) {
                                debug_msg("source_add_packet failed - duplicate layer\n");
                                src->dbe->duplicates++;
                                pb_iterator_destroy(src->channel, &pi);
                                goto done;
                        }

                        cu = (channel_unit*)block_alloc(sizeof(channel_unit));
                        cu->data = pckt;
                        cu->data_start = data_start;
                        cu->data_len = pckt_len;
                        cu->pt = payload;

                        dup = 0;

                       /* compare existing channel_units to this one */

                        for(i=0; i<cd->nelem; i++) {
                                if(cu->data_len!=cd->elem[i]->data_len) break;
                                if(memcmp(cu->data, cd->elem[i]->data, cu->data_len)==0) dup=1;
                        }

                       /* duplicate, so stick the channel_data back on *
                        * the playout buffer and swiftly depart        */
                        if(dup) {
                                debug_msg("source_add_packet failed - duplicate layer\n");
                                src->dbe->duplicates++;
                                /* destroy temporary channel_unit */
                                block_free(cu->data, cu->data_len);
                                cu->data_len = 0;
                                block_free(cu, sizeof(channel_unit));
                                pb_iterator_destroy(src->channel, &pi);
                                goto done;
                        }

                       /* add this layer if not a duplicate           *
                        * NB: layers are not added in order, and thus *
                        * have to be reorganised in the layered       *
                        * channel coder                               */
                        cd->elem[cd->nelem] = cu;
                        cd->nelem++;
                        pb_iterator_destroy(src->channel, &pi);
                        goto done;
                }
                pb_iterator_destroy(src->channel, &pi);
        }

        if (channel_data_create(&cd, 1) == 0) {
                return FALSE;
        }
        
        cu               = cd->elem[0];
        cu->data         = pckt;
        cu->data_start   = data_start;
        cu->data_len     = pckt_len;
        cu->pt           = payload;

        /* Check we have state to decode this */
        cid = channel_coder_get_by_payload(cu->pt);
        if (src->channel_state && 
            channel_decoder_matches(cid, src->channel_state) == FALSE) {
                debug_msg("Channel coder changed - flushing\n");
                channel_decoder_destroy(&src->channel_state);
                pb_flush(src->channel);
        }

        /* Make state if not there and create decoder */
        if (src->channel_state == NULL && 
            channel_decoder_create(cid, &src->channel_state) == FALSE) {
                debug_msg("Cannot decode payload %d\n", cu->pt);
                channel_data_destroy(&cd, sizeof(channel_data));
        }

done:   if (pb_add(src->channel, (u_char*)cd, sizeof(channel_data), playout) == FALSE) {
                debug_msg("Packet addition failed - duplicate ?\n");
                src->dbe->duplicates++;
                channel_data_destroy(&cd, sizeof(channel_data));
        }

        return TRUE;
}

/* recommend_drop_dur does quick pattern match with audio that is
 * about to be played i.e. first few samples to determine how much
 * audio can be dropped with causing glitch.
 */

#define SOURCE_COMPARE_WINDOW_SIZE 8
/* Match threshold is mean abs diff. lower score gives less noise, but less
 * adaption..., might be better if threshold adapted with how much extra
 * data we have buffered... */
#define MATCH_THRESHOLD 70

static ts_t
recommend_drop_dur(media_data *md) 
{
        u_int32 score, lowest_score, lowest_begin;
        u_int16 rate, channels;
        sample *buffer;
        int i, j,samples;

        i = md->nrep - 1;
        while(i >= 0) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        break;
                }
                i--;
        }
        assert(i != -1);
        
        buffer  = (sample*)md->rep[i]->data;
        samples = md->rep[i]->data_len / (sizeof(sample) * channels);

        i = 0;
        j = samples / 16;
        lowest_score = 0xffffffff;
        lowest_begin = 0;
        while (j < samples - SOURCE_COMPARE_WINDOW_SIZE) {
                score = 0;
                for (i = 0; i < SOURCE_COMPARE_WINDOW_SIZE; i++) {
                        score += abs(buffer[i * channels] - buffer[(j+i) * channels]);
                }
                if (score < lowest_score) {
                        lowest_score = score;
                        lowest_begin = j;
                }
                j++;
        }

        if (lowest_score/SOURCE_COMPARE_WINDOW_SIZE < MATCH_THRESHOLD) {
                debug_msg("match score %d, drop dur %d\n", lowest_score/SOURCE_COMPARE_WINDOW_SIZE, lowest_begin);
                return ts_map32(rate, lowest_begin);
        } else {
                return ts_map32(8000, 0);
        }
}

#define SOURCE_MERGE_LEN_SAMPLES 5

static void
conceal_dropped_samples(media_data *md, ts_t drop_dur)
{
        /* We are dropping drop_dur samples and want signal to be
         * continuous.  So we blend samples that would have been
         * played if they weren't dropped with where signal continues
         * after the drop.  */
        u_int32 drop_samples;
        u_int16 rate, channels;
        int32 tmp, a, b, i, merge_len;
        sample *new_start, *old_start;

        i = md->nrep - 1;
        while(i >= 0) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        break;
                }
                i--;
        }

        assert(i != -1);

        drop_dur     = ts_convert(rate, drop_dur);
        drop_samples = channels * drop_dur.ticks;
        
        /* new_start is what will be played by mixer */
        new_start = (sample*)md->rep[i]->data + drop_samples;
        old_start = (sample*)md->rep[i]->data;

        for(i = SOURCE_MERGE_LEN_SAMPLES; old_start + i <= new_start; i++) {
                old_start[i] = 32767;
        }

        merge_len = SOURCE_MERGE_LEN_SAMPLES * channels;
        for (i = 0; i < merge_len; i++) {
                a   = (merge_len - i) * old_start[i] / merge_len;
                b   = i * new_start[i]/ merge_len;
                tmp =  (sample)(a + b);
                new_start[i] = (sample)tmp;
        }
        debug_msg("dropped %d samples\n", drop_samples);
}

/* source_check_buffering is supposed to check amount of audio buffered
 * corresponds to what we expect from playout so we can think about
 * skew adjustment.  */

int
source_check_buffering(source *src, ts_t now)
{
        ts_t    playout_dur, buf_end;
        u_int32 buf_ms, playout_ms;

        if (ts_eq(src->dbe->last_arr, now) == FALSE) { 
                /* We are only interested in adaption if we are sure
                 * source is still sending. */
                return FALSE; 
        } 

        if (src->age < SOURCE_YOUNG_AGE) {
                /* If the source is new(ish) then not enough audio
                 * will be in the playout buffer because it hasn't
                 * arrived yet.  This age
                 */

                return FALSE;
        }

        if ((pb_get_end_ts(src->media, &buf_end) == FALSE) ||
            ts_gt(now, buf_end)) {
                /* Buffer is probably dry so no adaption will help */
                return FALSE;
        }

        buf_ms = ts_to_ms(source_get_playout_delay(src, now));

        playout_dur = ts_sub(src->dbe->playout, src->dbe->delay_in_playout_calc);
        playout_ms  = ts_to_ms(playout_dur);

        if (buf_ms > playout_ms) {
                /* buffer is longer than anticipated, src clock is faster */
                src->skew = SOURCE_SKEW_FAST;
                src->skew_adjust = ts_map32(8000, (buf_ms - playout_ms) * 8);
                src->skew_offenses++;
        } else if (buf_ms <= 2 * playout_ms / 3) {
                /* buffer is running dry so src clock is slower */
                src->skew = SOURCE_SKEW_SLOW;
                src->skew_adjust = ts_map32(8000, (playout_ms - buf_ms) * 8);
                if (src->skew_offenses > 0) {
                        /* Reset offenses for faster operation */
                        src->skew_offenses = 0;
                }
                src->skew_offenses--;
        } else {
                src->skew = SOURCE_SKEW_NONE;
                if (src->skew_offenses != 0) {
                        if (src->skew_offenses > 0) {
                                src->skew_offenses--;
                        } else {
                                src->skew_offenses++;
                        }
                }
        }

        return TRUE;
}

/* source_skew_adapt exists to shift playout units if source clock
 * appears to be fast or slow.  The media_data unit is here so that it
 * can be examined to see if it is low energy and adjustment would be okay.
 * Might want to be more sophisticated and put a silence detector in
 * rather than static threshold.
 *
 * Returns what adaption type occurred.
 */

static skew_t
source_skew_adapt(source *src, media_data *md, ts_t playout)
{
        u_int32 i, e = 0, samples = 0;
        u_int16 rate, channels;
        ts_t adjustment;

        assert(src);
        assert(md);
        assert(src->skew != SOURCE_SKEW_NONE);

        for(i = 0; i < md->nrep; i++) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        samples = md->rep[i]->data_len / (channels * sizeof(sample));
                        e = avg_audio_energy((sample*)md->rep[i]->data, samples * channels, channels);
                        src->mean_energy = (15 * src->mean_energy + e)/16;
                        break;
                }
        }

        if (i == md->nrep) {
                /* don't adapt if unit has not been decoded (error) or
                 *  signal has too much energy 
                 */
                return SOURCE_SKEW_NONE;
        }

        /* When we are making the adjustment we must shift playout
         * buffers and timestamps that the source decode process
         * uses. Must be careful with last repair because it is not
         * valid if no repair has taken place.
         */

        if (src->skew == SOURCE_SKEW_FAST &&
            abs((int)src->skew_offenses) >= SKEW_OFFENSES_BEFORE_CONTRACTING_BUFFER && 
                2*e <=  src->mean_energy) {
                /* source is fast so we need to bring units forward.
                 * Should only move forward at most a single unit
                 * otherwise we might discard something we have not
                 * classified.  */

                adjustment =  recommend_drop_dur(md); 
                if (ts_gt(adjustment, src->skew_adjust) || adjustment.ticks == 0) {
                        /* adjustment needed is greater than adjustment period
                         * that best matches dropable by signal matching.
                         */
                        return SOURCE_SKEW_NONE;
                }
                debug_msg("dropping %d / %d samples\n", adjustment.ticks, src->skew_adjust.ticks);
                pb_shift_forward(src->media,   adjustment);
                pb_shift_forward(src->channel, adjustment);
                src->dbe->playout               = ts_sub(src->dbe->playout, adjustment);
                src->dbe->delay_in_playout_calc = ts_sub(src->dbe->delay_in_playout_calc, adjustment);
                src->last_played = ts_sub(src->last_played, adjustment);
                src->skew_offenses = 0;

                if (ts_valid(src->last_repair)) {
                        src->last_repair = ts_sub(src->last_repair, adjustment);
                }

                if (ts_valid(src->last_played)) {
                        src->last_played = ts_sub(src->last_played, adjustment);
                }

                /* Remove skew adjustment from estimate of skew outstanding */
                if (ts_gt(src->skew_adjust, adjustment)) {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                } else {
                        src->skew = SOURCE_SKEW_NONE;
                }

                conceal_dropped_samples(md, adjustment); 

                return SOURCE_SKEW_FAST;
        } else if (src->skew == SOURCE_SKEW_SLOW && 
                   abs(src->skew_offenses) >= SKEW_OFFENSES_BEFORE_EXPANDING_BUFFER) {
                adjustment = ts_map32(rate, samples);
                if (ts_gt(src->skew_adjust, adjustment)) {
                        adjustment = ts_map32(rate, samples * 2);
                }
                pb_shift_units_back_after(src->media,   playout, adjustment);
                pb_shift_units_back_after(src->channel, playout, adjustment);
                src->dbe->playout               = ts_add(src->dbe->playout, adjustment);
                src->dbe->delay_in_playout_calc = ts_add(src->dbe->delay_in_playout_calc, adjustment);

                if (ts_gt(adjustment, src->skew_adjust)) {
                        src->skew_adjust = ts_map32(8000, 0);
                } else {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                }
                
                src->skew_offenses /= 2;
/* shouldn't have to make this adjustment since we are now adjusting
 * units in future only. 
                src->last_played = ts_add(src->last_played, adjustment);
                if (ts_valid(src->last_repair)) {
                        src->last_repair = ts_add(src->last_repair, adjustment);
                }
                */
                debug_msg("Playout buffer shift back\n");
                src->skew = SOURCE_SKEW_NONE;
                return SOURCE_SKEW_SLOW;
        }

        return SOURCE_SKEW_NONE;
}

static int
source_repair(source *src,
              int     repair_type,
              ts_t    step)
{
        media_data* fill_md, *prev_md;
        ts_t        fill_ts,  prev_ts;
        u_int32     success,  prev_len;

        /* Check for need to reset of consec_lost count */

        if (ts_valid(src->last_repair) == FALSE || 
            ts_eq(src->last_played, src->last_repair) == FALSE) {
                src->consec_lost = 0;
        }

        /* We repair one unit at a time since it may be all we need */
        pb_iterator_retreat(src->media_pos);
        pb_iterator_get_at(src->media_pos,
                           (u_char**)&prev_md,
                           &prev_len,
                           &prev_ts);

        assert(prev_md != NULL);

        if (!ts_eq(prev_ts, src->last_played)) {
                debug_msg("prev_ts and last_played don't match\n");
                return FALSE;
        }

        media_data_create(&fill_md, 1);
        repair(repair_type,
               src->consec_lost,
               src->codec_states,
               prev_md,
               fill_md->rep[0]);
        fill_ts = ts_add(src->last_played, step);

#ifdef NDEF
        debug_msg("lp %d (%d) fl %d (%d) delta %d (%d)\n", 
                  src->last_played.ticks,  ts_get_freq(src->last_played),
                  fill_ts.ticks,           ts_get_freq(fill_ts),
                  step.ticks,              ts_get_freq(step));
#endif

        success = pb_add(src->media, 
                         (u_char*)fill_md,
                         sizeof(media_data),
                         fill_ts);
        if (success) {
                src->consec_lost ++;
                src->last_repair = fill_ts;
                pb_iterator_advance(src->media_pos);

#ifndef NDEBUG
        /* Reusing prev_* - c'est mal, je sais */
                pb_iterator_get_at(src->media_pos,
                                   (u_char**)&prev_md,
                                   &prev_len,
                                   &prev_ts);
                if (ts_eq(prev_ts, fill_ts) == FALSE) {
                        debug_msg("Added at %d, but got %d when tried to get it back!\n", fill_ts.ticks, prev_ts.ticks);
                        return FALSE;
                }
                
#endif
        } else {
                /* This should only ever fail at when source changes
                 * sample rate in less time than playout buffer
                 * timeout.  This should be a very very rare event...  
                 */
                debug_msg("Repair add data failed (%d), last_played %d.\n", fill_ts.ticks, src->last_played.ticks);
                media_data_destroy(&fill_md, sizeof(media_data));
                src->consec_lost = 0;
                return FALSE;
        }
        return TRUE;
}

int
source_process(source *src, struct s_mix_info *ms, int render_3d, int repair_type, ts_t now)
{
        media_data  *md;
        coded_unit  *cu;
        codec_state *cs;
        u_int32     md_len, src_freq;
        ts_t        playout, step, cutoff;
        int         i, success, hold_repair = 0;

        /* Note: hold_repair is used to stop repair occuring.
         * Occasionally, there is a race condition when the playout
         * point is recalculated causing overlap, and when playout
         * buffer shift occurs in middle of a loss.
         */

        /* Split channel coder units up into media units */
        channel_decoder_decode(src->channel_state,
                               src->channel,
                               src->media,
                               now);

        src_freq = get_freq(src->dbe->clock);
        step = ts_map32(src_freq,src->dbe->inter_pkt_gap / src->dbe->units_per_packet);

        while (pb_iterator_advance(src->media_pos)) {
                pb_iterator_get_at(src->media_pos, 
                                  (u_char**)&md, 
                                  &md_len, 
                                  &playout);
                assert(md != NULL);
                assert(md_len == sizeof(media_data));

                /* Conditions for repair: 
                 * (a) last_played has meaning. 
                 * (b) playout point does not what we expect.
                 * (c) repair type is not no repair.
                 * (d) last decoded was not too long ago.
                 */
                cutoff = ts_sub(now, ts_map32(src_freq, SOURCE_AUDIO_HISTORY_MS));

                assert((ts_valid(src->last_played) == FALSE) || ts_eq(playout, src->last_played) == FALSE);

                if (ts_valid(src->last_played) && 
                    ts_gt(playout, ts_add(src->last_played, step)) &&
                    repair_type != REPAIR_TYPE_NONE &&
                    ts_gt(src->last_played, cutoff) &&
                    hold_repair == 0) {
                        /* If repair was successful media_pos is moved,
                         * so get data at media_pos again.
                         */
                        if (source_repair(src, repair_type, step) == FALSE) {
                                hold_repair += 2; /* 1 works, but 2 is probably better */
                        }
                        success = pb_iterator_get_at(src->media_pos, 
                                                     (u_char**)&md, 
                                                     &md_len, 
                                                     &playout);
                        assert(success);
                } else if (hold_repair > 0) {
                        hold_repair --;
                }

                if (ts_gt(playout, now)) {
                        /* This playout point is after now so stop */
                        pb_iterator_retreat(src->media_pos);
                        break;
                }

                for(i = 0; i < md->nrep; i++) {
                        if (codec_is_native_coding(md->rep[i]->id)) {
                                break;
                        }
                }

                if (i == md->nrep) {
                        /* We need to decode this unit, may not have to
                         * when repair has been used.
                         */
#ifdef DEBUG
                        for(i = 0; i < md->nrep; i++) {
                                /* if there is a native coding this
                                 * unit has already been decoded and
                                 * this would be bug */
                                assert(md->rep[i] != NULL);
                                assert(codec_id_is_valid(md->rep[i]->id));
                                assert(codec_is_native_coding(md->rep[i]->id) == FALSE);
                        }
#endif /* DEBUG */
                        cu = (coded_unit*)block_alloc(sizeof(coded_unit));
                        /* Decode frame */
                        assert(cu != NULL);
                        memset(cu, 0, sizeof(coded_unit));
                        cs = codec_state_store_get(src->codec_states, md->rep[0]->id);
                        codec_decode(cs, md->rep[0], cu);
                        assert(md->rep[md->nrep] == NULL);
                        md->rep[md->nrep] = cu;
                        md->nrep++;
                }

                if (render_3d && src->dbe->render_3D_data) {
                        /* 3d rendering necessary */
                        coded_unit *decoded, *render;
                        decoded = md->rep[md->nrep - 1];
                        assert(codec_is_native_coding(decoded->id));
                        
                        render = (coded_unit*)block_alloc(sizeof(coded_unit));
                        memset(render, 0, sizeof(coded_unit));
                        
                        render_3D(src->dbe->render_3D_data,decoded,render);
                        assert(md->rep[md->nrep] == NULL);
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
                        assert(md->rep[md->nrep] == NULL);
                        md->rep[md->nrep] = render;
                        md->nrep++;
                }

                if (src->skew != SOURCE_SKEW_NONE && source_skew_adapt(src, md, playout) != SOURCE_SKEW_NONE) {
                        /* We have skew and we have adjusted playout
                         *  buffer timestamps, so re-get unit to get
                         *  correct timestamp info */
                        pb_iterator_get_at(src->media_pos, 
                                           (u_char**)&md, 
                                           &md_len, 
                                           &playout);
                        assert(md != NULL);
                        assert(md_len == sizeof(media_data));
                }

                if (mix_process(ms, src->dbe, md->rep[md->nrep - 1], playout) == FALSE) {
                        /* Sources sampling rate changed mid-flow?,
                         * dump data, make source look irrelevant, it
                         * should get destroyed and the recreated with
                         * proper decode path when new data arrives.
                         * Not graceful..  A better way would be just
                         * to flush media then invoke source_reconfigure 
                         * if this is ever really an issue.  */
                        pb_flush(src->media);
                        pb_flush(src->channel);
                }

                src->last_played = playout;
        }

        src->age++;

        UNUSED(i); /* Except for debugging */
        
        return TRUE;
}

int
source_audit(source *src) {
        if (src->age != 0) {
                ts_t history;
                /* Keep 1/8 seconds worth of audio */
                history =  ts_map32(8000,1000);
                pb_iterator_audit(src->media_pos,history);
                return TRUE;
        }
        return FALSE;
}


ts_sequencer*
source_get_sequencer(source *src)
{
        return &src->seq;
}

ts_t
source_get_audio_buffered (source *src)
{
        ts_t start, end;

        /* Total audio buffered is start of media buffer
         * to end of channel buffer.
         */

        if (pb_get_start_ts(src->media, &start) &&
            pb_get_end_ts(src->channel, &end)) {
                assert(ts_gt(end, start));
                return ts_sub(end, start);
        }

        return ts_map32(8000,0);
}

ts_t
source_get_playout_delay (source *src, ts_t now)
{
        ts_t end;

        /* Note at start of a source_process this will use the end of
         * the channel coder buffer, but after source process it often
         * uses end of media since all in channel buffer has been
         * processed.  */

        if ((pb_get_end_ts(src->channel, &end) || pb_get_end_ts(src->media, &end)) &&
            ts_gt(end, now)) {
                return ts_sub(end, now);
        }

        return ts_map32(8000,0);
}

int
source_relevant(source *src, ts_t now)
{
        ts_t keep_source_time;
        assert(src);

        keep_source_time = ts_map32(8000, 2000); /* 1 quarter of a second */

        if (!ts_eq(source_get_playout_delay(src, now), ts_map32(8000, 0)) ||
                ts_gt(ts_add(src->dbe->last_arr, keep_source_time), now)) {
                return TRUE;
        }
        
        return pb_relevant(src->media, now) || pb_relevant(src->channel, now);
}

struct s_pb*
source_get_decoded_buffer(source *src)
{
        return src->media;
}

struct s_rtcp_dbentry*
source_get_rtcp_dbentry(source *src)
{
        return src->dbe;
}
