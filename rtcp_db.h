/*
 * Filename: rtcp_db.h
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

#ifndef _RTCP_DB
#define _RTCP_DB

struct session_tag;

typedef struct _rr {
	struct _rr      *next;
	u_int32          ssrc;
	u_int8           fraction_lost;
	u_int32          pckts_lost;
	u_int32          ext_seq_num;
	u_int32          jitter;
	u_int32          lsr;
	u_int32          dlsr;
} rtcp_user_rr;

/* An SSRC database entry.  Holds all the information about a given SSRC.     */
/* contains, a list of payload-type -> local decoder translations, in order   */
/* to facilitate parsing of RTP data streams from this SSRC.  The application */
/* program returns (with the fmt_decoder routine) an opaque identifier which  */
/* will be used later to identify the packet type that this SSRC is sending.  */

typedef struct s_rtcp_dbentry {
	struct s_rtcp_dbentry *next;
	u_int32         ssrc;
	ssrc_entry     *sentry;
	u_int32         pckts_recv;
	double          jitter;
	rtcp_user_rr	*rr;
	u_int32         misordered;		
        u_int32         duplicates;             /* Should be in rtcp_db ? */
	u_int32         jit_TOGed;		/* TOGed = Thrown on the Ground */
	u_int16         last_seq;		/* Last packet sequence number */
	int		loss_from_me;		/* Loss rate that this receiver heard from me */
	u_int32		last_rr_for_me;

	/* The variables must be properly set up for first data packet */
	/* - zero is no good                                           */

	u_int32		expected_prior;
	u_int32		received_prior;
	u_int32		lost_frac;
	int32		lost_tot;

	u_int16         firstseqno;
	u_int16         lastseqno;
	u_int32         cycles;
	u_int32		bad_seq;
	int8		probation;
	u_int32         last_sr;	/* The middle 32 bits from the NTP timestamp in the last SR packet we received, for use in our RR */
	u_int32		last_sr_rx;	/* The time the last SR packet was received. */
	u_int32         last_active;
	u_char          is_sender;
} rtcp_dbentry;

#define RTP_NUM_SDES 		6

typedef struct rtp_db_tag {
	u_int32         	 myssrc;
	u_int32         	 old_ssrc;
	u_int32         	 pkt_count;
	u_int32         	 byte_count;
	u_int32         	 pckts_received;
	u_int32         	 misordered;
	u_int32         	 duplicates;
	rtcp_dbentry   		*my_dbe;			/* For my info in UI 			*/
	rtcp_dbentry   		*ssrc_db;			/* Other participants...	        */
	u_int32			 members;			/* Number of other participants...	*/
	u_int32			 senders;			/* Number of senders (including us) 	*/
	u_int32			 rtcp_bw;			/* Bytes/second of allowable RTCP data	*/
	int			 avg_size;			/* Average RTCP packet size		*/
	u_int32         	 last_rpt;			/* Time of last RTCP report we sent	*/
	u_int32			 report_interval;		/* RTCP reporting interval              */
	int             	 sending;			/* TRUE if we are sending data...	*/
	u_int32         	 map_rtp_time;			/*					*/
	u_int32         	 map_ntp_time;			/*					*/
	int			 initial_rtcp;			/* TRUE until we've sent an RTCP packet */
	u_int32			 sdes_pri_count;
	u_int32			 sdes_sec_count;
	u_int32			 sdes_ter_count;
} rtp_db;

u_int32 		 rtcp_myssrc(struct session_tag *sp);
rtp_db 			*rtcp_init(struct s_time *device_clock, char *cname, u_int32 cur_time);
void                     rtcp_db_exit(struct session_tag *sp);
int 			 rtcp_update_seq(rtcp_dbentry *s, u_int16 seq);
struct s_rtcp_dbentry   *rtcp_get_dbentry(rtp_db *db, u_int32 ssrc);
struct s_rtcp_dbentry   *rtcp_get_dbentry_by_cname(struct session_tag *sp, char *cname);
struct s_rtcp_dbentry   *rtcp_new_dbentry(struct session_tag *sp, u_int32 ssrc, u_int32 cur_time);
struct s_rtcp_dbentry   *rtcp_getornew_dbentry(struct session_tag *sp, u_int32 ssrc, u_int32 cur_time);
int 			 rtcp_dbentry_valid(struct session_tag *sp, struct s_rtcp_dbentry *dbe);
void 			 rtcp_delete_dbentry(struct session_tag *sp, u_int32 ssrc);
int 			 rtcp_set_attribute(struct session_tag *sp, int type, char *val);
void			 rtcp_free_dbentry(rtcp_dbentry *dbptr);
void                     rtcp_clock_change(struct session_tag *sp);

#endif

