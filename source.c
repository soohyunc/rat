/*
 * FILE:      source.c
 * AUTHOR(S): Orion Hodson 
 *
 * Layering support added by Tristan Henderson.
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
#include "audio_util.h"
#include "render_3D.h"
#include "repair.h"
#include "timers.h"
#include "ts.h"
#include "channel_types.h"
#include "pdb.h"
#include "pktbuf.h"
#include "source.h"
#include "debug.h"
#include "util.h"
#include "net_udp.h"
#include "mix.h"
#include "rtp.h"
#include "playout_calc.h"
#include "ui.h"
#include "session.h"
#include "auddev.h"

#define SKEW_ADAPT_THRESHOLD       5000
#define SOURCE_YOUNG_AGE             20
#define SOURCE_AUDIO_HISTORY_MS    1000
#define NO_CONT_TOGED_FOR_PLAYOUT_RECALC 3

#define SOURCE_COMPARE_WINDOW_SIZE 8
/* Match threshold is mean abs diff. lower score gives less noise, but less  */
/* adaption..., might be better if threshold adapted with how much extra     */
/* data we have buffered...                                                  */
#define MATCH_THRESHOLD 1200

/* constants for skew adjustment:
 SOURCE_SKEW_SLOW - denotes source clock appears slower than ours.
 SOURCE_SKEW_FAST - denotes source clock appears faster than ours.
*/
typedef enum { SOURCE_SKEW_SLOW, SOURCE_SKEW_FAST, SOURCE_SKEW_NONE } skew_t;

typedef struct s_source {
        struct s_source            *next;
        struct s_source            *prev;
        pdb_entry_t                *pdbe;       /* persistent database entry */
        u_int32                     age;
        ts_t                        next_played; /* anticipated next unit    */
        ts_t                        last_repair;
        ts_t                        talkstart;  /* start of latest talkspurt */
        u_int32                     post_talkstart_units;
        u_int16                     consec_lost;
        u_int32                     mean_energy;
        struct s_pktbuf            *pktbuf;
        u_int32                     packets_done;
        struct s_channel_state     *channel_state;
        struct s_codec_state_store *codec_states;
        struct s_pb                *channel;
        struct s_pb                *media;
        struct s_pb_iterator       *media_pos;
        struct s_converter         *converter;
        /* Fine grained playout buffer adjustment variables.  Used in        */
        /* attempts to correct for clock skew between source and local host. */
        skew_t 			    skew;
        ts_t   			    skew_adjust;
        /* Skew stats                                                        */
        int32                       samples_played;
        int32                       samples_added;
        /* b/w estimation variables                                          */
        u_int32                     byte_count;
        ts_t                        byte_count_start;
        double                      bps;
} source;

/* A linked list is used for sources and this is fine since we mostly expect */
/* 1 or 2 sources to be simultaneously active and so efficiency is not a     */
/* killer.                                                                   */

typedef struct s_source_list {
        source  sentinel;
        u_int16 nsrcs;
} source_list;

/*****************************************************************************/
/* Source List functions.  Source List is used as a container for sources    */
/*****************************************************************************/

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
source_get_by_ssrc(source_list *plist, u_int32 ssrc)
{
        source *curr, *stop;
        
        curr = plist->sentinel.next; 
        stop = &plist->sentinel;
        while(curr != stop) {
                if (curr->pdbe->ssrc == ssrc) {
                        return curr;
                }
                curr = curr->next;
        }
 
        return NULL;
}

/*****************************************************************************/
/* Timestamp constants and initialization                                    */
/*****************************************************************************/

static ts_t zero_ts;        /* No time at all :-)                            */
static ts_t keep_source_ts; /* How long source kept after source goes quiet  */
static ts_t history_ts;     /* How much old audio hang onto for repair usage */
static ts_t bw_avg_period;  /* Average period for bandwidth estimate         */
static ts_t skew_thresh;    /* Significant size b4 consider playout adapt    */
static ts_t skew_limit;     /* Upper bound, otherwise clock reset.           */
static int  time_constants_inited = FALSE;


static void
time_constants_init()
{
        /* We use these time constants *all* the time.   Initialize once     */
        zero_ts        = ts_map32(8000, 0);
        keep_source_ts = ts_map32(8000, 2000); 
        history_ts     = ts_map32(8000, 1000); 
        bw_avg_period  = ts_map32(8000, 8000);
        skew_thresh    = ts_map32(8000, 160);
        skew_limit     = ts_map32(8000, 4000);
        time_constants_inited = TRUE;
}

/*****************************************************************************/
/* Source functions.  A source is an active audio source.                    */
/*****************************************************************************/

source*
source_create(source_list    *plist, 
              u_int32         ssrc,
	      pdb_t	     *pdb)
{
        source *psrc;
        int     success;

        assert(plist != NULL);
        assert(source_get_by_ssrc(plist, ssrc) == NULL);

        /* Time constant initialization. Nothing to do with source creation  */
        /* just has to go somewhere before sources might be active, here it  */
        /* definitely is!                                                    */
        if (time_constants_inited == FALSE) {
                time_constants_init();
        }

        /* On with the show...                                               */
        psrc = (source*)block_alloc(sizeof(source));
        if (psrc == NULL) {
                return NULL;
        }
        memset(psrc, 0, sizeof(source));

        if (pdb_item_get(pdb, ssrc, &psrc->pdbe) == FALSE) {
                debug_msg("Persistent database item not found\n");
                abort();
        }

        psrc->pdbe->first_mix  = 1; /* Used to note nothing mixed anything   */
        psrc->pdbe->cont_toged = 0; /* Reset continuous thrown on ground cnt */
        psrc->channel_state    = NULL;        
        psrc->skew             = SOURCE_SKEW_NONE;
        psrc->samples_played   = 0;
        psrc->samples_added    = 0;

        /* Allocate channel and media buffers                                */
        success = pb_create(&psrc->channel, 
                            (playoutfreeproc)channel_data_destroy);
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

        success = pktbuf_create(&psrc->pktbuf, 4); 
        if (!success) {
                debug_msg("Failed to allocate packet buffer\n");
                goto fail_pktbuf;
        }

        /* List maintenance    */
        psrc->next = plist->sentinel.next;
        psrc->prev = &plist->sentinel;
        psrc->next->prev = psrc;
        psrc->prev->next = psrc;
        plist->nsrcs++;

        debug_msg("Created source decode path\n");

        return psrc;

        /* Failure fall throughs */
fail_pktbuf:
        codec_state_store_destroy(&psrc->codec_states); 
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

        assert(src->pdbe != NULL);

        /* Set age to zero and flush existing media
         * so that repair mechanism does not attempt
         * to patch across different block sizes.
         */

        src->age = 0;
        pb_flush(src->media);

        /* Get rate and channels of incoming media so we know
         * what we have to change.
         */
        src_cid = codec_get_by_payload(src->pdbe->enc);
        src_cf  = codec_get_format(src_cid);
        src_rate     = (u_int16)src_cf->format.sample_rate;
        src_channels = (u_int16)src_cf->format.channels;

        if (render_3d) {
                assert(out_channels == 2);
                /* Rejig 3d renderer if there, else create */
                if (src->pdbe->render_3D_data) {
                        int azi3d, fil3d, len3d;
                        render_3D_get_parameters(src->pdbe->render_3D_data,
                                                 &azi3d,
                                                 &fil3d,
                                                 &len3d);
                        render_3D_set_parameters(src->pdbe->render_3D_data,
                                                 (int)src_rate,
                                                 azi3d,
                                                 fil3d,
                                                 len3d);
                } else {
                        src->pdbe->render_3D_data = render_3D_init((int)src_rate);
                }
                assert(src->pdbe->render_3D_data);
                /* Render 3d is before sample rate/channel conversion, and   */
                /* output 2 channels.                                        */
                src_channels = 2;
        } else {
                /* Rendering is switched off so destroy info.                */
                if (src->pdbe->render_3D_data != NULL) {
                        render_3D_free(&src->pdbe->render_3D_data);
                }
        }

        /* Now destroy converter if it is already there.                     */
        if (src->converter) {
                converter_destroy(&src->converter);
        }

        if (src_rate != out_rate || src_channels != out_channels) {
                converter_fmt_t c;
                c.src_freq      = src_rate;
                c.from_channels = src_channels;
                c.dst_freq      = out_rate;
                c.to_channels   = out_channels;
                converter_create(conv_id, &c, &src->converter);
        }
        src->byte_count = 0;
        src->bps        = 0.0;
}

void
source_remove(source_list *plist, source *psrc)
{
        assert(plist);
        assert(psrc);
        assert(source_get_by_ssrc(plist, psrc->pdbe->ssrc) != NULL);

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
        pktbuf_destroy(&psrc->pktbuf);
        plist->nsrcs--;

        debug_msg("Destroying source decode path\n");
        
        block_free(psrc, sizeof(source));

        assert(source_get_by_ssrc(plist, psrc->pdbe->ssrc) == NULL);
}
              
/* Source Processing Routines ************************************************/

/* Returns true if fn takes ownership responsibility for data */
static int
source_process_packet (source *src, 
                       u_char *pckt, 
                       u_int32 pckt_len, 
                       u_int8  payload,
                       ts_t    playout)
{
        channel_data *cd;
        channel_unit *cu;
        cc_id_t       cid;
        u_int8        clayers;

        assert(src != NULL);
        assert(pckt != NULL);

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
        if (clayers > 1) {
                struct s_pb_iterator *pi;
                u_int8 i;
                u_int32 clen;
                int dup;
                ts_t lplayout;
                pb_iterator_create(src->channel, &pi);
                while(pb_iterator_advance(pi)) {
                        pb_iterator_get_at(pi, (u_char**)&cd, &clen, &lplayout);
                       /* if lplayout==playout there is already channel_data for this playout point */
                        if(!ts_eq(playout, lplayout)) {
                                continue;
                        }
                        pb_iterator_detach_at(pi, (u_char**)&cd, &clen, &lplayout);
                        assert(cd->nelem >= 1);

                       /* if this channel_data is full, this new packet must *
                        * be a duplicate, so we don't need to check          */
                        if(cd->nelem >= clayers) {
                                debug_msg("source_process_packet failed - duplicate layer\n");
                                src->pdbe->duplicates++;
                                pb_iterator_destroy(src->channel, &pi);
                                goto done;
                        }

                        cu = (channel_unit*)block_alloc(sizeof(channel_unit));
                        cu->data     = pckt;
                        cu->data_len = pckt_len;
                        cu->pt       = payload;

                        dup = 0;

                       /* compare existing channel_units to this one */
                        for(i=0; i<cd->nelem; i++) {
                                if(cu->data_len!=cd->elem[i]->data_len) break;
                                /* This memcmp arbitrarily only checks
                                 * 20 bytes, otherwise it takes too
                                 * long */
                                if (memcmp(cu->data, cd->elem[i]->data, 20) == 0) {
                                        dup=1;
                                }
                        }

                       /* duplicate, so stick the channel_data back on *
                        * the playout buffer and swiftly depart        */
                        if(dup) {
                                debug_msg("source_process_packet failed - duplicate layer\n");
                                src->pdbe->duplicates++;
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
        src->age++;
done:   
        if (pb_add(src->channel, (u_char*)cd, sizeof(channel_data), playout) == FALSE) {
                src->pdbe->duplicates++;
                channel_data_destroy(&cd, sizeof(channel_data));
        }

        return TRUE;
}

#ifdef SOURCE_LOG_PLAYOUT

static FILE *psf; /* Playout stats file */
static u_int32 t0;

static void
source_close_log(void)
{
        if (psf) {
                fclose(psf);
                psf = NULL;
        }
}

static void
source_playout_log(source *src, u_int32 ts)
{
        if (psf == NULL) {
                psf = fopen("playout.log", "w");
                if (psf == NULL) {
                        fprintf(stderr, "Could not open playout.log\n");
                } else {
                        atexit(source_close_log);
                        fprintf(psf, "# <RTP timestamp> <jitter> <transit> <avg transit> <last transit> <playout del>\n");
                }
                t0 = ts - 1000; /* -1000 in case of out of order first packet */
        }

        fprintf(psf, "%13lu % 5d % 5d % 5d % 5d %5d\n",
                ts - t0,
                src->pdbe->jitter.ticks,
                src->pdbe->transit.ticks,
                src->pdbe->avg_transit.ticks,
                src->pdbe->last_transit.ticks,
                src->pdbe->playout.ticks);
}

#endif /* SOURCE_LOG_PLAYOUT */

static void
source_process_packets(session_t *sp, source *src, ts_t now)
{
        ts_t    src_ts, playout, transit;
        pdb_entry_t     *e;
        rtp_packet      *p;
        cc_id_t          ccid = -1;
        u_int16          units_per_packet = -1;
        u_int32          delta_ts, delta_seq;
        u_char           codec_pt;
        int              adjust_playout;

        e = src->pdbe;
        while(pktbuf_dequeue(src->pktbuf, &p)) {
                adjust_playout = FALSE;
                if (p->m) {
                        adjust_playout = TRUE;
                        debug_msg("New Talkspurt: %lu\n", p->ts);
                }
                
                ccid = channel_coder_get_by_payload((u_char)p->pt);
                if (channel_verify_and_stat(ccid, (u_char)p->pt, 
                                            p->data, p->data_len,
                                            &units_per_packet, &codec_pt) == FALSE) {
                        debug_msg("Packet discarded: packet failed channel verify.\n");
                        xfree(p);
                        continue;
                }

                if (e->channel_coder_id != ccid || 
                    e->enc              != codec_pt || 
                    e->units_per_packet != units_per_packet ||
                    src->packets_done == 0) {
                        /* Something has changed or is uninitialized...      */
                        const codec_format_t *cf;
                        const audio_format   *dev_fmt;
                        codec_id_t           cid;
                        u_int32              samples_per_frame;

                        cid = codec_get_by_payload(codec_pt);
                        cf  = codec_get_format(cid);
                        /* Fix clock.                                        */
                        change_freq(e->clock, cf->format.sample_rate);
                        /* Fix details.                                      */
                        e->enc              = codec_pt;
                        e->units_per_packet = units_per_packet;
                        e->channel_coder_id = ccid;        
                        samples_per_frame   = codec_get_samples_per_frame(cid);
                        debug_msg("Samples per frame %d rate %d\n", samples_per_frame, cf->format.sample_rate);
                        e->inter_pkt_gap    = e->units_per_packet * (u_int16)samples_per_frame;
                        e->frame_dur        = ts_map32(cf->format.sample_rate, samples_per_frame);

                        debug_msg("Encoding change\n");
                        /* Get string describing encoding.                   */
                        channel_describe_data(ccid, codec_pt, 
                                              p->data, p->data_len, 
                                              e->enc_fmt, e->enc_fmt_len);
                        if (sp->mbus_engine) {
                                ui_update_stats(sp, e->ssrc);
                        }
                        /* Configure converter */
                        dev_fmt = audio_get_ofmt(sp->audio_device);
                        source_reconfigure(src, 
                                           sp->converter, 
                                           sp->render_3d,
                                           (u_int16)dev_fmt->sample_rate,
                                           (u_int16)dev_fmt->channels);
                        adjust_playout      = TRUE;
                }
                
                /* Check for talkspurt start indicated by change in          */
                /* relationship between timestamps and sequence numbers.     */
                delta_seq = p->seq - e->last_seq;
                delta_ts  = p->ts  - e->last_ts;
                if (delta_seq * e->inter_pkt_gap != delta_ts) {
                        debug_msg("Seq no / timestamp realign (%lu * %lu != %lu)\n", 
                                  delta_seq, e->inter_pkt_gap, delta_ts);
                        adjust_playout = TRUE;
                }

                if (ts_gt(e->jitter, e->playout)) {
                        /* Network conditions have changed drastically.      */
                        /* We are in the wrong ball park change immediately. */
                        adjust_playout = TRUE;
                }

                /* Check for continuous number of packets being discarded.   */
                /* This happens when jitter or transit estimate is no longer */
                /* consistent with the real world.                           */
                if (e->cont_toged >= NO_CONT_TOGED_FOR_PLAYOUT_RECALC) {
                        adjust_playout = TRUE;
                        e->cont_toged  = 0;
                } else if (e->cont_toged != 0) {
                        debug_msg("cont_toged %d\n", e->cont_toged);
                }

                /* Calculate the playout point for this packet.              */
                src_ts = ts_seq32_in(&e->seq, get_freq(e->clock), p->ts);

                /* Transit delay is the difference between our local clock   */
                /* and the packet timestamp (src_ts).  Note: we expect       */
                /* packet clumping at talkspurt start because of VAD's       */
                /* fetching previous X seconds of audio on signal detection  */
                /* in order to send unvoiced audio at start.                 */
                if (adjust_playout && pktbuf_get_count(src->pktbuf)) {
                        rtp_packet *p;
                        ts_t        last_ts;
                        pktbuf_peak_last(src->pktbuf, &p);
                        assert(p != NULL);
                        last_ts = ts_seq32_in(&e->seq, get_freq(e->clock), p->ts);
                        transit = ts_sub(now, last_ts);
                        debug_msg("Used transit of last packet\n");
                } else {
                        transit = ts_sub(now, src_ts);
                }

                playout = playout_calc(sp, e->ssrc, transit, adjust_playout);
                if ((p->m || src->packets_done == 0) && ts_gt(playout, e->frame_dur)) {
                        /* Packets are likely to be compressed at talkspurt start */
                        /* because of VAD going back and grabbing frames.         */
                        playout = ts_sub(playout, e->frame_dur);
                        debug_msg("New ts shift XXX\n");
                }
                playout = ts_add(e->transit, playout);
                playout = ts_add(src_ts, playout);

                if (adjust_playout) {
/*
                        debug_msg("last  % 5d avg % 5d\n", transit.ticks, e->avg_transit.ticks);
                        if (ts_gt(now, playout)) {
                                ts_t shortfall;
                                */
                                /* Unit would have been discarded.  Jitter has    */
                                /* affected our first packets transit time.       */
/*
                                shortfall       = ts_sub(now, playout);
                                playout         = ts_add(playout, shortfall);
                                e->transit      = ts_add(e->transit, shortfall);
                                e->last_transit = e->transit;
                                e->avg_transit  = e->transit;
                                debug_msg("Push back %d samples\n", shortfall.ticks);
                        }
*/
                        src->talkstart = playout; /* Note start of new talkspurt  */
                        src->post_talkstart_units = 0;
                } else {
                        src->post_talkstart_units++;
                }

                if (src->packets_done == 0) {
                        /* This is first packet so expect next played to have its */
                        /* playout.                                               */
                        src->next_played = playout;
                }

                if (!ts_gt(now, playout)) {
                        u_char  *u;
                        u    = (u_char*)block_alloc(p->data_len);
                        /* Would be great if memcpy occured after validation */
                        /* in source_process_packet (or not at all)          */
                        memcpy(u, p->data, p->data_len);
                        if (source_process_packet(src, u, p->data_len, codec_pt, playout) == FALSE) {
                                block_free(u, (int)p->data_len);
                        }
                        src->pdbe->cont_toged = 0;
                } else {
                        /* Packet being decoded is before start of current  */
                        /* so there is now way it's audio will be played    */
                        /* Playout recalculation gets triggered in          */
                        /* rtp_callback if cont_toged hits a critical       */
                        /* threshold.  It signifies current playout delay   */
                        /* is inappropriate.                                */
                        debug_msg("Packet late (compared to now)\n");
                        src->pdbe->cont_toged++;
                        src->pdbe->jit_toged++;
                } 

                /* Update persistent database fields.                        */
                if (e->last_seq > p->seq) {
                        e->misordered++;
                }
                e->last_seq = p->seq;
                e->last_ts  = p->ts;
                e->last_arr = now;

#ifdef SOURCE_LOG_PLAYOUT
                source_playout_log(src, p->ts);
#endif /* SOURCE_LOG_PLAYOUT */
                src->packets_done++;
                xfree(p);
        }
}

int
source_add_packet (source     *src, 
                   rtp_packet *pckt)
{
        src->byte_count += pckt->data_len;
        return pktbuf_enqueue(src->pktbuf, pckt);
}

static void
source_update_bps(source *src, ts_t now)
{
        ts_t delta;
        if (!ts_valid(src->byte_count_start)) {
                src->byte_count_start = now;
                src->byte_count       = 0;
                src->bps              = 0.0;
                return;
        }

        delta = ts_sub(now, src->byte_count_start);
        
        if (ts_gt(delta, bw_avg_period)) {
                double this_est;
                this_est = 8.0 * src->byte_count * 1000.0/ ts_to_ms(delta);
                if (src->bps == 0.0) {
                        src->bps = this_est;
                } else {
                        src->bps += (this_est - src->bps)/2.0;
                }
                src->byte_count = 0;
                src->byte_count_start = now;
        }
}

double 
source_get_bps(source *src)
{
        return src->bps;
}

/* recommend_drop_dur does quick pattern match with audio that is about to   */
/* be played i.e. first few samples to determine how much audio can be       */
/* dropped with causing glitch.                                              */

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
                if (score <= lowest_score) {
                        lowest_score = score;
                        lowest_begin = j;
                }
                j++;
        }

        if (lowest_score/SOURCE_COMPARE_WINDOW_SIZE < MATCH_THRESHOLD) {
                return ts_map32(rate, lowest_begin);
        } else {
                return zero_ts;
        }
}

#define SOURCE_MERGE_LEN_SAMPLES 5

static void
conceal_dropped_samples(media_data *md, ts_t drop_dur)
{
        /* We are dropping drop_dur samples and want signal to be            */
        /* continuous.  So we blend samples that would have been played if   */
        /* they weren't dropped with where signal continues after the drop.  */
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

        merge_len = SOURCE_MERGE_LEN_SAMPLES * channels;
        for (i = 0; i < merge_len; i++) {
                a   = (merge_len - i) * old_start[i] / merge_len;
                b   = i * new_start[i]/ merge_len;
                tmp =  (sample)(a + b);
                new_start[i] = (short)tmp;
        }
}

/* source_check_buffering is supposed to check amount of audio buffered      */
/* corresponds to what we expect from playout so we can think about skew     */
/* adjustment.                                                               */

int
source_check_buffering(source *src)
{
        ts_t actual, desired, diff;

        if (src->post_talkstart_units < 20) {
                /* If the source is new(ish) then not enough audio will be   */
                /* in the playout buffer because it hasn't arrived yet.      */
                return FALSE;
        }

        actual  = source_get_audio_buffered(src);
        desired = source_get_playout_delay(src);
        diff    = ts_abs_diff(actual, desired);

        if (ts_gt(diff, skew_thresh)) {
                src->skew_adjust = diff;
                if (ts_gt(actual, desired)) {
                        /* We're accumulating audio, their clock faster   */
                        src->skew = SOURCE_SKEW_FAST; 
                } else {
                        /* We're short of audio, so their clock is slower */
                        src->skew = SOURCE_SKEW_SLOW;
                }
                return TRUE;
        }
        src->skew = SOURCE_SKEW_NONE;
        return FALSE;
}

/* source_skew_adapt exists to shift playout units if source clock appears   */
/* to be fast or slow.  The media_data unit is here so that it can be        */
/* examined to see if it is low energy and adjustment would be okay.  Might  */
/* want to be more sophisticated and put a silence detector in rather than   */
/* static threshold.                                                         */
/*                                                                           */
/* Returns what adaption type occurred.                                      */

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
                /* don't adapt if unit has not been decoded (error) or       */
                /* signal has too much energy                                */
                return SOURCE_SKEW_NONE;
        }

        /* When we are making the adjustment we must shift playout buffers   */
        /* and timestamps that the source decode process uses.  Must be      */
        /* careful with last repair because it is not valid if no repair has */
        /* taken place.                                                      */

        if (src->skew == SOURCE_SKEW_FAST/* &&
                (2*e <=  src->mean_energy || e < 200) */) {
                /* source is fast so we need to bring units forward.
                 * Should only move forward at most a single unit
                 * otherwise we might discard something we have not
                 * classified.  */

                if (ts_gt(skew_limit, src->skew_adjust)) {
                        adjustment = recommend_drop_dur(md); 
                } else {
                        /* Things are really skewed.  We're more than        */
                        /* skew_limit off of where we ought to be.  Just     */
                        /* drop a frame and don't worry.                     */
                        debug_msg("Dropping Frame\n");
                        adjustment = ts_div(src->pdbe->frame_dur, 2);
                }

                if (ts_gt(adjustment, src->skew_adjust) || adjustment.ticks == 0) {
                        /* adjustment needed is greater than adjustment      */
                        /* period that best matches dropable by signal       */
                        /* matching.                                         */
                        return SOURCE_SKEW_NONE;
                }
                debug_msg("dropping %d / %d samples\n", adjustment.ticks, src->skew_adjust.ticks);
                pb_shift_forward(src->media,   adjustment);
                pb_shift_forward(src->channel, adjustment);

                src->samples_added     += adjustment.ticks;

                src->pdbe->transit      = ts_sub(src->pdbe->transit, adjustment);
                /* avg_transit and last_transit are fine.  Difference in     */
                /* avg_transit and transit triggered this adjustment.        */

                if (ts_valid(src->last_repair)) {
                        src->last_repair = ts_sub(src->last_repair, adjustment);
                }

                src->next_played = ts_sub(src->next_played, adjustment);

                /* Remove skew adjustment from estimate of skew outstanding */
                if (ts_gt(src->skew_adjust, adjustment)) {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                } else {
                        src->skew = SOURCE_SKEW_NONE;
                }

                conceal_dropped_samples(md, adjustment); 

                return SOURCE_SKEW_FAST;
        } else if (src->skew == SOURCE_SKEW_SLOW) {
                adjustment = ts_map32(rate, samples);
                if (ts_gt(src->skew_adjust, adjustment)) {
                        adjustment = ts_map32(rate, samples);
                }
                pb_shift_units_back_after(src->media,   playout, adjustment);
                pb_shift_units_back_after(src->channel, playout, adjustment);
                src->pdbe->transit = ts_add(src->pdbe->transit, adjustment);

                if (ts_gt(adjustment, src->skew_adjust)) {
                        src->skew_adjust = zero_ts;
                } else {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                }

                src->samples_added -= samples;

                debug_msg("Playout buffer shift back %d samples.\n", adjustment.ticks);
                src->skew = SOURCE_SKEW_NONE;
                return SOURCE_SKEW_SLOW;
        }

        return SOURCE_SKEW_NONE;
}

static int
source_repair(source     *src,
              repair_id_t r,
              ts_t        fill_ts) 
{
        media_data* fill_md, *prev_md;
        ts_t        prev_ts;
        u_int32     success,  prev_len;

        /* We repair one unit at a time since it may be all we need */
        if (pb_iterator_retreat(src->media_pos) == FALSE) {
                /* New packet when source still active, but dry, e.g. new talkspurt */
                debug_msg("Repair not possible no previous unit!\n");
                return FALSE;
        }

        pb_iterator_get_at(src->media_pos,
                           (u_char**)&prev_md,
                           &prev_len,
                           &prev_ts);

        media_data_create(&fill_md, 1);
        repair(r,
               src->consec_lost,
               src->codec_states,
               prev_md,
               fill_md->rep[0]);

        success = pb_add(src->media, 
                         (u_char*)fill_md,
                         sizeof(media_data),
                         fill_ts);

        if (success) {
                src->consec_lost++;
                src->last_repair = fill_ts;
                /* Advance to unit we just added */
                pb_iterator_advance(src->media_pos);
        } else {
                /* This should only ever fail at when source changes
                 * sample rate in less time than playout buffer
                 * timeout.  This should be a very very rare event...  
                 */
                debug_msg("Repair add data failed (%d).\n", fill_ts.ticks);
                media_data_destroy(&fill_md, sizeof(media_data));
                src->consec_lost = 0;
                return FALSE;
        }
        return TRUE;
}

int
source_process(session_t *sp,
               source            *src, 
               struct s_mix_info *ms, 
               int                render_3d, 
               repair_id_t        repair_type, 
               ts_t               start_ts,    /* Real-world time           */
               ts_t               end_ts)      /* Real-world time + cushion */
{
        media_data  *md;
        coded_unit  *cu;
        codec_state *cs;
        u_int32     md_len;
        ts_t        playout, step;
        int         i, success, hold_repair = 0;
        u_int16     sample_rate, channels;

        /* Note: hold_repair is used to stop repair occuring.
         * Occasionally, there is a race condition when the playout
         * point is recalculated causing overlap, and when playout
         * buffer shift occurs in middle of a loss.
         */
        
        source_process_packets(sp, src, start_ts);

        /* Split channel coder units up into media units */
        if (pb_node_count(src->channel)) {
                channel_decoder_decode(src->channel_state,
                                       src->channel,
                                       src->media,
                                       end_ts);
        }

        while (ts_gt(end_ts, src->next_played) && 
               pb_iterator_advance(src->media_pos)) {
                pb_iterator_get_at(src->media_pos, 
                                   (u_char**)&md, 
                                   &md_len, 
                                   &playout);
                assert(md != NULL);
                assert(md_len == sizeof(media_data));
                
                /* Conditions for repair:                                    */
                /* (a) playout point of unit is further away than expected.  */
                /* (b) playout does not correspond to new talkspurt (don't   */
                /* fill between end of last talkspurt and start of next).    */
                /* NB Use post_talkstart_units as talkspurts maybe longer    */
                /* than timestamp wrap period and want to repair even if     */
                /* timestamps wrap.                                          */
                /* (c) not start of a talkspurt.                             */
                /* (d) don't have a hold on.                                 */

                if (ts_gt(playout, src->next_played) &&
                    ((ts_gt(src->next_played, src->talkstart) && ts_gt(playout, src->talkstart)) || src->post_talkstart_units > 100) &&
                    hold_repair == 0) {
                        /* If repair was successful media_pos is moved,      */
                        /* so get data at media_pos again.                   */
                        if (source_repair(src, repair_type, src->next_played) == TRUE) {
                                debug_msg("Repair % 2d got % 6d exp % 6d talks % 6d\n", 
                                          src->consec_lost, playout.ticks, src->next_played.ticks, src->talkstart.ticks);
                                success = pb_iterator_get_at(src->media_pos, 
                                                             (u_char**)&md, 
                                                             &md_len, 
                                                             &playout);
                                assert(success);
                                assert(ts_eq(playout, src->next_played));
                        } else {
                                /* Repair failed for some reason.  Wait a    */
                                /* while before re-trying.                   */
                                debug_msg("Repair failed unexpectedly\n");
                                hold_repair += 2; 
                        }
                } else {
                        if (hold_repair > 0) {
                                hold_repair --;
                        }
                        src->consec_lost = 0;
                }

                if (ts_gt(playout, end_ts)) {
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

                if (render_3d && src->pdbe->render_3D_data) {
                        /* 3d rendering necessary */
                        coded_unit *decoded, *render;
                        decoded = md->rep[md->nrep - 1];
                        assert(codec_is_native_coding(decoded->id));
                        
                        render = (coded_unit*)block_alloc(sizeof(coded_unit));
                        memset(render, 0, sizeof(coded_unit));
                        
                        render_3D(src->pdbe->render_3D_data,decoded,render);
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

                if (src->skew != SOURCE_SKEW_NONE && 
                    source_skew_adapt(src, md, playout) != SOURCE_SKEW_NONE) {
                        /* We have skew and we have adjusted playout buffer  */
                        /* timestamps, so re-get unit to get correct         */
                        /* timestamp info.                                   */
                        pb_iterator_get_at(src->media_pos, 
                                           (u_char**)&md, 
                                           &md_len, 
                                           &playout);
                        assert(md != NULL);
                        assert(md_len == sizeof(media_data));
                }

                if (src->pdbe->gain != 1.0 && codec_is_native_coding(md->rep[md->nrep - 1]->id)) {
                        audio_scale_buffer((sample*)md->rep[md->nrep - 1]->data,
                                           md->rep[md->nrep - 1]->data_len / sizeof(sample),
                                           src->pdbe->gain);
                }

                assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
                codec_get_native_info(md->rep[md->nrep - 1]->id, &sample_rate, &channels);
                step = ts_map32(sample_rate, md->rep[md->nrep - 1]->data_len / (channels * sizeof(sample)));
                src->next_played = ts_add(playout, step);
                src->samples_played += md->rep[md->nrep - 1]->data_len / (channels * sizeof(sample));

                if (mix_process(ms, src->pdbe, md->rep[md->nrep - 1], playout) == FALSE) {
                        /* Sources sampling rate changed mid-flow? dump data */
                        /* make source look irrelevant, it should get        */
                        /* destroyed and the recreated with proper decode    */
                        /* path when new data arrives.  Not graceful..       */
                        /* A better way would be just to flush media then    */
                        /* invoke source_reconfigure if this is ever really  */
                        /* an issue.                                         */
                        pb_flush(src->media);
                        pb_flush(src->channel);
                }
        }

        source_update_bps(src, start_ts);

        UNUSED(i); /* Except for debugging */
        
        return TRUE;
}

int
source_audit(source *src) 
{
        if (src->age != 0) {
                /* Keep 1/8 seconds worth of audio */
                pb_iterator_audit(src->media_pos, history_ts);
                return TRUE;
        }
        return FALSE;
}

ts_t
source_get_audio_buffered (source *src)
{
        /* Changes in avg_transit change amount of audio buffered. */
        /* It's how much transit is off from start.                */
        ts_t delta;
        delta = ts_sub(src->pdbe->transit, src->pdbe->avg_transit);
        return ts_add(src->pdbe->playout, delta);
}

ts_t
source_get_playout_delay (source *src)
{
        return src->pdbe->playout;
}

int
source_relevant(source *src, ts_t now)
{
        assert(src);

        if (pb_relevant(src->media, now) || pb_relevant(src->channel, now) || src->age < 50) {
                return TRUE;
        } if (ts_valid(src->next_played)) {
                /* Source is quiescent                                     */
                ts_t quiet;        
                quiet = ts_sub(now, src->next_played);
                if (ts_gt(keep_source_ts, quiet)) {
                        return TRUE;
                }
        }
        return FALSE;
}

struct s_pb*
source_get_decoded_buffer(source *src)
{
        return src->media;
}

u_int32
source_get_ssrc(source *src)
{
        return src->pdbe->ssrc;
}

double
source_get_skew_rate(source *src)
{
        if (src->samples_played) {
                double r = (double)(src->samples_played + src->samples_added) / (double)src->samples_played;
                return r;
        }
        return 1.0;
}
