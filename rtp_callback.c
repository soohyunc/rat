/*
 * FILE:    rtp_callback.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins / Orion Hodson
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
#include "debug.h"
#include "audio_types.h"
#include "auddev.h"
#include "channel.h"
#include "codec.h"
#include "rtp.h"
#include "rtp_callback.h"
#include "session.h"
#include "pdb.h"
#include "source.h"
#include "playout_calc.h"
#include "ui.h"

/* We need to be able to resolve the rtp session to a rat session in */
/* order to get persistent participant information, etc.  We use a   */
/* double linked list with sentinel for this.  We normally don't     */
/* expect to have more than 2 sessions (i.e. transcoder mode), but   */
/* layered codecs may require more.                                  */

typedef struct s_rtp_assoc {
        struct s_rtp_assoc *next;
        struct s_rtp_assoc *prev;
        struct rtp         *rtps;
        struct s_session *rats;
} rtp_assoc_t;

/* Sentinel for linked list that is used as small associative array */
static rtp_assoc_t rtp_as;
static int rtp_as_inited;


void 
rtp_callback_init(struct rtp *rtps, struct s_session *rats)
{
        rtp_assoc_t *cur, *sentinel;

        if (!rtp_as_inited) {
                /* First pass sentinel initialization */
                rtp_as.next = &rtp_as;
                rtp_as.prev = &rtp_as;
                rtp_as_inited = 1;
        }

        sentinel = &rtp_as;
        cur   = sentinel->next;

        while (cur != sentinel) {
                if (cur->rtps == rtps) {
                        /* Association already exists, over-riding */
                        cur->rats = rats;
                        return;
                }
        }

        cur = (rtp_assoc_t*)xmalloc(sizeof(rtp_assoc_t));
        cur->rtps   = rtps;
        cur->rats   = rats;

        cur->next       = sentinel->next;
        cur->prev       = sentinel;
        cur->next->prev = cur;
        cur->prev->next = cur;
}

void rtp_callback_exit(struct rtp *rtps)
{
        rtp_assoc_t *cur, *sentinel;
        
        sentinel = &rtp_as;
        cur = sentinel->next;
        while(cur != sentinel) {
                if (cur->rtps == rtps) {
                        cur->prev->next = cur->next;
                        cur->next->prev = cur->prev;
                        xfree(cur);
                        return;
                }
                cur = cur->next;
        }
}

/* get_session maps an rtp_session to a rat session */
static struct s_session *
get_session(struct rtp *rtps)
{
        rtp_assoc_t *cur, *sentinel;

        if (!rtp_as_inited) {
                return NULL;
        }
        
        sentinel = &rtp_as;
        cur = sentinel->next;
        while(cur != sentinel) {
                if (cur->rtps == rtps) {
                        return cur->rats;
                }
                cur = cur->next;
        }        
        return NULL;
}

/* Callback utility functions                                                */

static void
process_rtp_data(session_t *sp, u_int32 ssrc, rtp_packet *p)
{
        struct s_source *s;
        pdb_entry_t     *e;
        cc_id_t          ccid;
        u_int16          units_per_packet;
        u_char           codec_pt;
        ts_t             playout, src_ts;
        int              adjust_playout;
        u_int32          delta_seq, delta_ts;

        adjust_playout = p->m;

        if (p->m) {
                debug_msg("New talkspurt\n");
        }

        if (sp->filter_loopback && ssrc == rtp_my_ssrc(sp->rtp_session[0])) {
                /* This packet is from us and we are filtering our own       */
                /* packets.                                                  */
                xfree(p);
                return;
        }

        if (sp->playing_audio == FALSE) {
                /* We are not playing audio out */
                debug_msg("Packet discarded: audio output muted.\n");
                xfree(p);
                return;
        }

        if (pdb_item_get(sp->pdb, ssrc, &e) == FALSE) {
                debug_msg("Packet discarded: unknown source (0x%08x).\n", ssrc);
                xfree(p);
                return;
        }
        
        if (e->mute) {
                debug_msg("Packet discarded: source muted (0x%08x).\n", ssrc);
                xfree(p);
                return;
        }

        ccid = channel_coder_get_by_payload(p->pt);
        if (channel_verify_and_stat(ccid, p->pt, p->data, p->data_len, &units_per_packet, &codec_pt) == FALSE) {
                debug_msg("Packet discarded: packet failed channel verification.\n");
                xfree(p);
                return;
        }

        if (e->channel_coder_id != ccid ||
            e->enc              != codec_pt || 
            e->units_per_packet != units_per_packet) {
                /* Something has changed or is uninitialized... */
                const codec_format_t *cf;
                codec_id_t cid;
                cid = codec_get_by_payload(codec_pt);
                cf  = codec_get_format(cid);
                /* Fix clock                                    */
                change_freq(e->clock, cf->format.sample_rate);
                /* Fix details                                  */
                e->enc              = codec_pt;
                e->units_per_packet = units_per_packet;
                e->channel_coder_id = ccid;        
                e->inter_pkt_gap    = e->units_per_packet * (u_int16)codec_get_samples_per_frame(cid);
                adjust_playout      = TRUE;
                debug_msg("Encoding change\n");
                /* Get string describing encoding               */
                channel_describe_data(ccid, codec_pt, p->data, p->data_len, e->enc_fmt, e->enc_fmt_len);
                if (sp->mbus_engine) {
                        ui_update_stats(sp, ssrc);
                }
        }

        s = source_get_by_ssrc(sp->active_sources, ssrc);
        if (s == NULL) {
                const audio_format *dev_fmt;
                dev_fmt = audio_get_ofmt(sp->audio_device);
                s = source_create(sp->active_sources, ssrc, sp->pdb, 
                                  sp->converter, sp->render_3d, 
                                  (u_int16)dev_fmt->sample_rate,
                                  (u_int16)dev_fmt->channels);
                adjust_playout = TRUE;
                debug_msg("Source created\n");
        }
        
        /* Check for talkspurt start indicated by change in relationship */
        /* between timestamps and sequence numbers                       */
        delta_seq = p->seq - e->last_seq;
        delta_ts  = p->ts  - e->last_ts;
        if (delta_seq * e->inter_pkt_gap != delta_ts) {
                debug_msg("Seq no / timestamp realign (%lu * %lu != %lu)\n", 
                          delta_seq,
                          e->inter_pkt_gap,
                          delta_ts);
                adjust_playout = TRUE;
        }

        src_ts = ts_seq32_in(source_get_sequencer(s), get_freq(e->clock), p->ts);
        playout = playout_calc(sp, ssrc, src_ts, adjust_playout);
        e->last_seq = p->seq;
        e->last_ts  = p->ts;
        e->last_arr = sp->cur_ts;
        xfree(p);
}

static void
process_sdes(session_t *sp, u_int32 ssrc, rtcp_sdes_item *d)
{
        pdb_entry_t *e;

	assert(pdb_item_get(sp->pdb, ssrc, &a) == TRUE);

        if (sp->mbus_engine == NULL) {
                /* Nowhere to send updates to, so ignore them.               */
                return;
        }

        switch(d->type) {
        case RTCP_SDES_END:
                /* This is the end of the SDES list of a packet.  Nothing    */
                /* for us to deal with.                                      */
                break;
        case RTCP_SDES_CNAME:
                ui_info_update_cname(sp, ssrc);
                break;
        case RTCP_SDES_NAME:
                ui_info_update_name(sp, ssrc);
                break;
        case RTCP_SDES_EMAIL:
                ui_info_update_email(sp, ssrc);
                break;
        case RTCP_SDES_PHONE:
                ui_info_update_phone(sp, ssrc);
                break;
        case RTCP_SDES_LOC:
                ui_info_update_loc(sp, ssrc);
                break;
        case RTCP_SDES_TOOL:
                ui_info_update_tool(sp, ssrc);
                break;
        case RTCP_SDES_NOTE:
                ui_info_update_note(sp, ssrc);
                break;
        case RTCP_SDES_PRIV:
                debug_msg("Discarding private data from (0x%08x)", ssrc);
                break;
        default:
                debug_msg("Ignoring SDES type (0x%02x) from (0x%08x).\n", ssrc);
        }
}

static void
process_create(session_t *sp, u_int32 ssrc)
{
	if (pdb_item_create(sp->pdb, sp->clock, get_freq(sp->device_clock), ssrc) == FALSE) {
		debug_msg("Unable to create source 0x%08lx\n", ssrc);
	}
}

static void
process_delete(session_t *sp, u_int32 ssrc)
{
        if (ssrc != rtp_my_ssrc(sp->rtp_session[0]) &&
            sp->mbus_engine != NULL) {
                ui_info_remove(sp, ssrc);
        }
}

void 
rtp_callback(struct rtp *s, rtp_event *e)
{
        struct s_session *sp;
        
	assert(s != NULL);
	assert(e != NULL);

        sp = get_session(s);
        if (sp == NULL) {
                /* Should only happen when SOURCE_CREATED is generated in rtp_init */
                debug_msg("Could not find session (0x%08x)\n", (u_int32)s);
                return;
        }

	switch (e->type) {
	case RX_RTP:
                process_rtp_data(sp, e->ssrc, (rtp_packet*)e->data);
                break;
	case RX_SR:
		break;
	case RX_RR:
		break;
	case RX_SDES:
                process_sdes(sp, e->ssrc, (rtcp_sdes_item*)e->data);
		break;
	case RX_BYE:
	case SOURCE_DELETED:
                process_delete(sp, e->ssrc);
		break;
	case SOURCE_CREATED:
		process_create(sp, e->ssrc);
		break;
	default:
		debug_msg("Unknown RTP event (type=%d)\n", e->type);
		abort();
	}
}

