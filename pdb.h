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

typedef struct s_pdb pdb_t;
 
typedef struct {
        u_int32 ssrc; /* For checking */
        struct s_render_3D_dbentry  *render_3D_data;
} pitem_t;

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

int     pdb_item_get     (pdb_t *p, u_int32 id, pitem_t **item);
int     pdb_item_create  (pdb_t *p, u_int32 id);
int     pdb_item_destroy (pdb_t *p, u_int32 id);
u_int32 pdb_item_count   (pdb_t *p);

#endif /* __PERSIST_DB_H__ */
