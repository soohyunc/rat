/*
 * FILE:    rtp_callback.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins / Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999-2000 University College London
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
#include "cushion.h"
#include "pdb.h"
#include "source.h"
#include "playout_calc.h"
#include "util.h"
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
        struct s_session   *rats;
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
process_rtp_data(session_t *sp, uint32_t ssrc, rtp_packet *p)
{
        struct s_source *s;
        pdb_entry_t     *e;

        if (sp->filter_loopback && ssrc == rtp_my_ssrc(sp->rtp_session[0])) {
                /* This packet is from us and we are filtering our own       */
                /* packets.                                                  */
                xfree(p);
                return;
        }

        if (pdb_item_get(sp->pdb, ssrc, &e) == FALSE) {
                debug_msg("Packet discarded: unknown source (0x%08x).\n", ssrc);
                xfree(p);
                return;
        }
        e->received++;
        
        s = source_get_by_ssrc(sp->active_sources, ssrc);
        if (s == NULL) {
                s = source_create(sp->active_sources, ssrc, sp->pdb);
                ui_info_activate(sp, ssrc);
                debug_msg("Source created\n");
        }

	/* Calculate the relative playout delay, for this source. Needed for lip-sync. */

	/* Discard packet if output is muted... no point wasting time decoding it... */
        if ((sp->playing_audio == FALSE) || (e->mute)) {
                debug_msg("Packet discarded since output muted\n");
                xfree(p);
                return;
        }

        source_add_packet(s, p);
}

static void
process_sr(session_t *sp, uint32_t ssrc, rtcp_sr *s)
{
        pdb_entry_t     *e;

        if (pdb_item_get(sp->pdb, ssrc, &e) == FALSE) {
                debug_msg("Sender report discarded: unknown source (0x%08x).\n", ssrc);
                return;
        }
}

static void
process_rr(session_t *sp, uint32_t ssrc, rtcp_rr *r)
{
        uint32_t fract_lost;
        /* Just update loss statistic in UI for this report if there */
        /* is somewhere to send them.                                */
        if (sp->mbus_engine != NULL) {
                fract_lost = (r->fract_lost * 100) >> 8;
                ui_update_loss(sp, ssrc, r->ssrc, fract_lost);
        }
}


static void
process_rr_timeout(session_t *sp, uint32_t ssrc, rtcp_rr *r)
{
        /* Just update loss statistic in UI for this report if there */
        /* is somewhere to send them.                                */
        if (sp->mbus_engine != NULL) {
                ui_update_loss(sp, ssrc, r->ssrc, 101);
        }
}

static void
process_sdes(session_t *sp, uint32_t ssrc, rtcp_sdes_item *d)
{
        pdb_entry_t *e;

	assert(pdb_item_get(sp->pdb, ssrc, &e) == TRUE);

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
process_create(session_t *sp, uint32_t ssrc)
{
	if (pdb_item_create(sp->pdb, sp->clock, (uint16_t)get_freq(sp->device_clock), ssrc) == FALSE) {
		debug_msg("Unable to create source 0x%08lx\n", ssrc);
	}
}

static void
process_delete(session_t *sp, uint32_t ssrc)
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
                /* Should only happen when SOURCE_CREATED is generated in */
                /* rtp_init.                                              */
                debug_msg("Could not find session (0x%08x)\n", (uint32_t)s);
                return;
        }

	switch (e->type) {
	case RX_RTP:
                process_rtp_data(sp, e->ssrc, (rtp_packet*)e->data);
                break;
	case RX_RTCP_START:
		break;
	case RX_RTCP_FINISH:
		break;
	case RX_SR:
                process_sr(sp, e->ssrc, (rtcp_sr*)e->data);
		break;
	case RX_RR:
                process_rr(sp, e->ssrc, (rtcp_rr*)e->data);
		break;
	case RX_RR_EMPTY:
		break;
	case RX_SDES:
                process_sdes(sp, e->ssrc, (rtcp_sdes_item*)e->data);
		break;
	case RX_BYE:
	case SOURCE_DELETED:
                process_delete(sp, e->ssrc);
		break;
	case SOURCE_CREATED:
                debug_msg("Source create (0x%08x)\n", e->ssrc);
		process_create(sp, e->ssrc);
		break;
	case RR_TIMEOUT:
                process_rr_timeout(sp, e->ssrc, (rtcp_rr*)e->data);
		break;
	default:
		debug_msg("Unknown RTP event (type=%d)\n", e->type);
		abort();
	}
}

