/*
 * FILE:    pdb.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 * These functions provide a means of maintaining persistent
 * information on conference participants that is not contained in the
 * RTCP database.  Entries are stored in a binary table, identified with
 * a unique 32 bit unsigned identifer (probably the same as SSRC).
 */

#ifndef __PERSIST_DB_H__
#define __PERSIST_DB_H__

/* RAT specific includes for entries in pdb_entry_t */
#include "channel_types.h"
#include "codec_types.h"
#include "timers.h"
#include "ts.h"
#include "render_3D.h"

typedef struct s_pdb pdb_t;
 
typedef struct {
        u_int32         ssrc;                        /* Unique Id */
        u_char          first_mix:1;
        struct s_render_3D_dbentry  *render_3D_data; /* Participant 3d state */
        double          gain;                        /* Participant gain */
	u_char		mute:1;                      /* source muted */
	struct s_time  *clock;
	u_int16		units_per_packet;
        u_int16         inter_pkt_gap;               /* expected time between pkt arrivals */
        u_char          enc;
        char*           enc_fmt;
        int             enc_fmt_len;
        u_int32         last_ts;
        u_int32         last_seq;
        ts_t            last_arr;                    /* ts_t representation of last_ts */

        /* Playout info */
        ts_t            jitter;
        ts_t            transit;
        ts_t            last_transit;
        cc_id_t         channel_coder_id;            /* channel_coder of last received packet */
	ts_t            last_mixed;                  /* Used to check mixing */
	ts_t            playout;                     /* Playout delay for this talkspurt */


        /* Display Info */
        ts_t            last_ui_update;              /* Used for periodic update of packet counts, etc */

        /* Packet info */
        u_int32         received;
        u_int32         duplicates;
        u_int32         misordered;
        u_int32         jit_toged;                   /* Packets discarded because late ("Thrown on ground") */
	u_char		cont_toged;		     /* Toged in a row */

	/* Variables for playout time calculation */
	int		video_playout;		     /* Playout delay in the video tool -- for lip-sync [csp] */
        u_char          video_playout_received:1;    /* video playout is relevent */
	int		sync_playout_delay;	     /* same interpretation as delay, used when sync is on [dm] */

	/* Mapping between rtp time and NTP time for this sender */
	int             mapping_valid;
	u_int32         last_ntp_sec;	/* NTP timestamp */
	u_int32         last_ntp_frac;
	u_int32         last_rtp_ts;	/* RTP timestamp */
} pdb_entry_t;

/* Functions for creating and destroying persistent database.  Return
 * TRUE on success and fill in p accordingly, FALSE on failure.  */

int pdb_create  (pdb_t **p);
int pdb_destroy (pdb_t **p);

/* pdb_get_{first,next}_id attempt to get keys from database.  Return
 * TRUE on succes and fill in id.  FALSE on failure.  */

int pdb_get_first_id (pdb_t *p, u_int32 *id);

int pdb_get_next_id  (pdb_t *p, u_int32 cur_id, u_int32 *next_id);

/* Functions for manipulating persistent database items. id is key in
 * database and must be unique. */

int     pdb_item_get     (pdb_t *p, u_int32 id, pdb_entry_t **item);

int     pdb_item_create  (pdb_t *p, 
                          struct s_fast_time *clock, 
                          u_int16 freq, 
                          u_int32 id);

int     pdb_item_destroy (pdb_t *p, u_int32 id);

u_int32 pdb_item_count   (pdb_t *p);

#endif /* __PERSIST_DB_H__ */
