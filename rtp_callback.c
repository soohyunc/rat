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
#include "rtp.h"
#include "rtp_callback.h"
#include "rtp_queue.h"
#include "session.h"
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

void 
rtp_callback_init(struct rtp *rtps, struct s_session *rats)
{
        rtp_assoc_t *cur, *sentinel;

        if (rtp_as.next == NULL) {
                /* First pass sentinel initialization */
                rtp_as.next = &rtp_as;
                rtp_as.prev = &rtp_as;
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
process_sdes(session_t *sp, u_int32 ssrc, rtcp_sdes_item *d)
{
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
                debug_msg("Discarding private data from (0x%08x)", 
                          ssrc);
                break;
        default:
                debug_msg("Ignoring SDES type (0x%02x) from (0x%08x).\n", 
                          ssrc);
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
        assert(sp != NULL);

	switch (e->type) {
	case RX_RTP:
                rtp_enqueue(sp->rtp_pckt_queue, (rtp_packet*)e->data);
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
	default:
		debug_msg("Unknown RTP event (type=%d)\n", e->type);
		abort();
	}
}

