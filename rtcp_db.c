/*
 * Filename: rtcp_db.c
 * Author:   Colin Perkins
 * Purpose:  RTCP database routines
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996,1997 University College London
 * All rights reserved.
 *
 *
 * Modified from code with the following copyright:
 *
 * Copyright (c) 1994 Paul Stewart All rights reserved.
 * 
 * Permission is hereby granted, without written agreement and without license
 * or royalty fees, to use, copy, modify, and distribute this software and
 * its documentation for any purpose, provided that the above copyright
 * notice appears in all copies of this software.
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "net_udp.h"
#include "ts.h"
#include "convert.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "session.h"
#include "ui.h"
#include "timers.h"
#include "session.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "render_3D.h"
#include "source.h"

#define MAX_DROPOUT	3000
#define MAX_MISORDER	100
#define MIN_SEQUENTIAL	2

static void 
init_seq(rtcp_dbentry *s, u_int16 seq)
{
	s->firstseqno     = seq;
	s->lastseqno      = seq;
	s->bad_seq        = RTP_SEQ_MOD + 1;
	s->cycles         = 0;
	s->pckts_recv     = 0;
	s->received_prior = 0;
	s->expected_prior = 0;
}

int 
rtcp_update_seq(rtcp_dbentry *s, u_int16 seq)
{
	u_int16       udelta = seq - s->lastseqno;

	if (s->pckts_recv == 0) {
		s->probation = 0;
		init_seq(s, seq);
	}

	/*
	 * Source is not valid until MIN_SEQUENTIAL packets with sequential
	 * sequence numbers have been received.
	 */
	if (s->probation) {
		/* packet is in sequence */
		if (seq == s->lastseqno + 1) {
			s->probation--;
			s->lastseqno = seq;
			if (s->probation == 0) {
				init_seq(s, seq);
				s->pckts_recv++;
				return 1;
			}
		} else {
			s->probation = MIN_SEQUENTIAL - 1;
			s->lastseqno = seq;
		}
		return 0;
	} else if (udelta < MAX_DROPOUT) {
		/* in order, with permissible gap */
		if (seq < s->lastseqno) {
			/*
			 * Sequence number wrapped - count another 64K cycle.
			 */
			s->cycles += RTP_SEQ_MOD;
		}
		s->lastseqno = seq;
	} else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
		/* the sequence number made a very large jump */
		if (seq == s->bad_seq) {
			/*
			 * Two sequential packets -- assume that the other
			 * side restarted without telling us so just re-sync
			 * (i.e., pretend this was the first packet).
			 */
			init_seq(s, seq);
		} else {
			s->bad_seq = (seq + 1) & (RTP_SEQ_MOD - 1);
			return 0;
		}
	} else {
		/* duplicate or reordered packet */
                if (udelta == 0) {
                        s->duplicates ++;
                } else {
                        s->misordered ++;
                }
	}
	s->pckts_recv++;
	return 1;
}

/*
 * Gets a pointer to an SSRC database entry given an SSRC number Arguments:
 * ssrc: SSRC number Returns: The database entry.
 */
rtcp_dbentry   *
rtcp_get_dbentry(session_struct *sp, u_int32 ssrc)
{
	/* This needs to be optimised! [csp] */
	rtcp_dbentry   *dptr = sp->db->ssrc_db;

	while (dptr) {
		if (dptr->ssrc == ssrc) {
			return (dptr);
		}
		dptr = dptr->next;
	}

	/*
	 * To avoid seeing two participants when looping back we should have
	 * a check here for our ssrc and addr and return sp->db->my_dbe
	 */
	return NULL;
}

int rtcp_dbentry_valid(session_struct *sp, rtcp_dbentry *dbe)
{
	/* Checks if the specified dbe is valid... not efficient */
#ifdef DEBUG
	rtcp_dbentry   *dptr = sp->db->ssrc_db;

	while (dptr) {
		if (dptr == dbe) {
			return TRUE;
		}
		dptr = dptr->next;
	}
	return FALSE;
#else
	/* :-) */
	UNUSED(sp);
	UNUSED(dbe);
	return TRUE;
#endif
}

rtcp_dbentry *
rtcp_get_dbentry_by_cname(session_struct *sp, char *cname)
{
	/* This needs to be optimised! [csp] */
	rtcp_dbentry   *dptr = sp->db->ssrc_db;

	while (dptr) {
		if ((dptr->sentry->cname != NULL) && (strcmp(dptr->sentry->cname, cname) == 0)) {
			return dptr;
		}
		dptr = dptr->next;
	}

        if (!dptr) {
                dptr = sp->db->my_dbe;
                if (!strcmp(dptr->sentry->cname, cname)) {
			return (dptr);
		}
        }

	return NULL;
}

/*
 * Get my SSRC
 */
u_int32 
rtcp_myssrc(session_struct *sp)
{
	return sp->db->myssrc;
}

/*
 * Allocates, initializes and adds a new database entry. Modified by
 * V.J.Hardman 28/03/95 - extra stats added
 */
static rtcp_dbentry *
rtcp_new_dbentry_noqueue(u_int32 ssrc, u_int32 cur_time)
{
	rtcp_dbentry   *newdb;

#ifdef LOG_PARTICIPANTS
	printf("JOIN: ssrc=%lx time=%ld\n", ssrc,  cur_time);
#endif

	newdb = (rtcp_dbentry *)xmalloc(sizeof(rtcp_dbentry));
	memset(newdb, 0, sizeof(rtcp_dbentry));
	newdb->ssrc 			= ssrc;
	newdb->sentry 			= (ssrc_entry *) xmalloc(sizeof(ssrc_entry));
	memset(newdb->sentry, 0, sizeof(ssrc_entry));
	newdb->sentry->ssrc 		= ssrc;
	newdb->firstseqno 		= 1;	/* So that "expected packets" starts out 0 */
	newdb->last_active 		= cur_time;
	newdb->first_pckt_flag 		= TRUE;
        newdb->enc                      = -1;
        newdb->enc_fmt                  = NULL;
	return (newdb);
}

rtcp_dbentry   *
rtcp_new_dbentry(session_struct *sp, u_int32 ssrc, u_int32 cur_time)
{
	rtcp_dbentry   *newdb;
	rtcp_dbentry   *dbptr;

	newdb = rtcp_new_dbentry_noqueue(ssrc, cur_time);
	newdb->clock = new_time(sp->clock, get_freq(sp->device_clock));
	if (!sp->db->ssrc_db) {
		sp->db->ssrc_db = newdb;
	} else {
		dbptr = sp->db->ssrc_db;
		while (dbptr->next) {
			dbptr = dbptr->next;
		}
		dbptr->next = newdb;
	}
	sp->db->members++;
	return newdb;
}

/*
 * Convenience routine for getting or creating a database entry if it does
 * not exist yet.
 */
rtcp_dbentry   *
rtcp_getornew_dbentry(session_struct *sp, u_int32 ssrc, u_int32 cur_time)
{
	rtcp_dbentry   *dbe;
	dbe = rtcp_get_dbentry(sp, ssrc);
	if (dbe == NULL) {
		dbe = rtcp_new_dbentry(sp, ssrc, cur_time);
	}
	return dbe;
}

/*
 * Removes memory associated with an SSRC database item.
 */
void 
rtcp_free_dbentry(rtcp_dbentry *dbptr)
{
	assert(dbptr != NULL);

	if (dbptr->clock) {
		free_time(dbptr->clock);
	}
        if (dbptr->render_3D_data) {
                render_3D_free(&dbptr->render_3D_data);
        }
	if (dbptr->sentry != NULL) {
		if (dbptr->sentry->cname) xfree(dbptr->sentry->cname);
		if (dbptr->sentry->name)  xfree(dbptr->sentry->name);
		if (dbptr->sentry->email) xfree(dbptr->sentry->email);
		if (dbptr->sentry->phone) xfree(dbptr->sentry->phone);
		if (dbptr->sentry->loc)   xfree(dbptr->sentry->loc);
		if (dbptr->sentry->txt)   xfree(dbptr->sentry->txt);
		if (dbptr->sentry->tool)  xfree(dbptr->sentry->tool);
		if (dbptr->sentry->note)  xfree(dbptr->sentry->note);
		xfree(dbptr->sentry);
                dbptr->sentry = NULL;
	}
        if (dbptr->enc_fmt != NULL) {
                xfree(dbptr->enc_fmt);
                dbptr->enc_fmt = NULL;
        }
	if (dbptr->rr != NULL) {
		rtcp_user_rr	*rr, *tmp_rr;
		rr = dbptr->rr;
		while (rr != NULL) {
			tmp_rr = rr->next;
			xfree(rr);
			rr = tmp_rr;
		}
	}
	xfree(dbptr);
}

void 
rtcp_delete_dbentry(session_struct *sp, u_int32 ssrc)
{
	/*
 	 * This function remove a participant from the RTCP database, and frees the
 	 * memory associated with the database entry. Note that we must remove the
 	 * references to the participant in sp->playout_buf_list to avoid the race
 	 * condition in service_receiver... (see comment in that function) [csp] 
 	 */
	rtcp_dbentry   	*dbptr = sp->db->ssrc_db;
	rtcp_dbentry   	*next, *prev = NULL;
        struct s_source *s;

	if (dbptr == NULL) {
		debug_msg("Freeing database entry for SSRC %lx when the database is empty! Huh?\n", ssrc);
		return;
	}

        while(dbptr != NULL) {
                next = dbptr->next;
                if (dbptr->ssrc == ssrc) {
			debug_msg("Removing RTCP database entry for SSRC 0x%08lx\n", ssrc);
                        s = source_get_by_rtcp_dbentry(sp->active_sources, dbptr);
                        if (s != NULL) {
				ui_info_deactivate(sp, source_get_rtcp_dbentry(s));
				source_remove(sp->active_sources, s);
			}
                        ui_info_remove(sp, dbptr);
			/* Remove this dbentry from the list... */
                        if (dbptr == sp->db->ssrc_db) {
                                sp->db->ssrc_db = next;
                        }
                        if (prev) {
                                prev->next = next;
                        }
			assert(!rtcp_dbentry_valid(sp, dbptr));
			/* Free the dbentry... */
                        rtcp_free_dbentry(dbptr);
			sp->db->members--;
                        return;
                } else {
                        prev = dbptr;
                }
                dbptr = next;
        }
	debug_msg("Tried to remove non-existent SSRC 0x%08lx from RTCP database\n", ssrc);
}

void
rtcp_set_encoder_format(session_struct *sp, rtcp_dbentry *e, char *enc_fmt)
{
        if (e->enc_fmt) {
                xfree(e->enc_fmt);
                e->enc_fmt = NULL;
        }
        e->enc_fmt = xstrdup(enc_fmt);
        ui_update_stats(sp, e);
}

/*
 * Set a database attribute
 */
int 
rtcp_set_attribute(session_struct *sp, int type, char *val)
{
	assert(val != NULL);
	assert(sp->db->my_dbe != NULL);

	switch (type) {
	case RTCP_SDES_LOC :
		if (sp->db->my_dbe->sentry->loc) xfree(sp->db->my_dbe->sentry->loc);
		sp->db->my_dbe->sentry->loc = xstrdup(val);
		ui_info_update_loc(sp, sp->db->my_dbe);
		break;
	case RTCP_SDES_PHONE :
		if (sp->db->my_dbe->sentry->phone) xfree(sp->db->my_dbe->sentry->phone);
		sp->db->my_dbe->sentry->phone = xstrdup(val);
		ui_info_update_phone(sp, sp->db->my_dbe);
		break;
	case RTCP_SDES_EMAIL :
		if (sp->db->my_dbe->sentry->email) xfree(sp->db->my_dbe->sentry->email);
		sp->db->my_dbe->sentry->email = xstrdup(val);
		ui_info_update_email(sp, sp->db->my_dbe);
		break;
	case RTCP_SDES_NAME :
		if (sp->db->my_dbe->sentry->name) xfree(sp->db->my_dbe->sentry->name);
		sp->db->my_dbe->sentry->name = xstrdup(val);
		ui_info_update_name(sp, sp->db->my_dbe);
		break;
	case RTCP_SDES_NOTE :
		if (sp->db->my_dbe->sentry->note) xfree(sp->db->my_dbe->sentry->note);
		sp->db->my_dbe->sentry->note = xstrdup(val);
		ui_info_update_note(sp, sp->db->my_dbe);
		break;
	default :
		debug_msg("Unknown SDES attribute type! (This should never happen)\n");
		break;
	}
	return 0;
}

/*
 * Initializes the RTP database. (sp->db) Arguments:
 * decoder: Function pointer to the routine this program uses cname: Initial
 * canonical name. Modified by V.J.Hardman No decode arguments needed etc.
 * want to remove them Added more stats counters
 *
 * Most of this work is done in init_session() now, we just have to set up the
 * dynamic stuff here. [csp]
 */
void 
rtcp_init(session_struct *sp, char *cname, u_int32 ssrc, u_int32 cur_time)
{
	sp->db = (rtp_db*)xmalloc(sizeof(rtp_db));
	memset(sp->db, 0, sizeof(rtp_db));

	sp->db->members		= 1;
	sp->db->rtcp_bw		= 417; /* 5% of 8350 bytes/sec (8khz, pcmu) session bandwidth */
	sp->db->initial_rtcp	= TRUE;
	sp->db->report_interval = rtcp_interval(sp->db->members, sp->db->senders, sp->db->rtcp_bw, sp->db->sending, 
					        128, &(sp->db->avg_size), sp->db->initial_rtcp, get_freq(sp->device_clock));

	sp->db->myssrc = ssrc;

	sp->db->my_dbe                = rtcp_new_dbentry_noqueue(sp->db->myssrc, cur_time);
	sp->db->my_dbe->sentry->cname = xstrdup(cname);
#ifdef WIN32
        /* Would like to know exactly what they are running for W32 and
         * not just what the build platform was. */
        sp->db->my_dbe->sentry->tool  = xstrdup(w32_make_version_info(RAT_VERSION));
#else
	sp->db->my_dbe->sentry->tool  = xstrdup(RAT_VERSION);
#endif
	sp->db->last_rpt     = get_time(sp->device_clock);
	sp->db->initial_rtcp = TRUE;
	sp->db->sending      = FALSE;
	sp->db->senders      = 0;
}

void
rtcp_clock_change(session_struct *sp)
{
        	sp->db->report_interval = rtcp_interval(sp->db->members, 
                                                        sp->db->senders, 
                                                        sp->db->rtcp_bw, 
                                                        sp->db->sending, 
                                                        128, 
                                                        &(sp->db->avg_size), 
                                                        sp->db->initial_rtcp, 
                                                        get_freq(sp->device_clock));
                sp->db->last_rpt = get_time(sp->device_clock);
}

void
rtcp_db_exit(session_struct *sp)
{
        xfree(sp->db);
        sp->db = NULL;
}
