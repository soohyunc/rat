/*
 * FILE:    pdb.c
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
 * RTCP database.  Entries are stored in a binary tree, identified
 * with a unique 32 bit unsigned identifer (probably the same as
 * SSRC).
 */

#include <assert.h>

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "btree.h"
#include "pdb.h"

struct s_pdb {
        btree_t *db;
        u_int32  nelem;
};

int 
pdb_create(pdb_t **pp)
{
        pdb_t *p;

        p = (pdb_t*)xmalloc(sizeof(pdb_t));
        if (p == NULL) {
                *pp = NULL;
                return FALSE;
        }

        if (btree_create(&p->db) == FALSE) {
                xfree(p);
                *pp = NULL;
                return FALSE;
        }

        p->nelem = 0;
        *pp = p;
        return TRUE;
}

int 
pdb_destroy(pdb_t **pp)
{
        pdb_t   *p = *pp;
        u_int32 id;
        
        while(pdb_get_first_id(p, &id)) {
                if (pdb_item_destroy(p, id) == FALSE) {
                        debug_msg("Failed to destroy item\n");
                        return FALSE;
                }
        }

        if (btree_destroy(&p->db) == FALSE) {
                debug_msg("Failed to destroy tree\n");
                return FALSE;
        }

        xfree(p);
        *pp = NULL;
        return TRUE;
}

u_int32
pdb_item_count(pdb_t *p)
{
        return p->nelem;
}

int
pdb_get_first_id(pdb_t *p, u_int32 *id)
{
        return btree_get_root_key(p->db, id);
}

int
pdb_get_next_id(pdb_t *p, u_int32 cur, u_int32 *next)
{
        return btree_get_next_key(p->db, cur, next);
}

int
pdb_item_get(pdb_t *p, u_int32 id, pitem_t **item)
{
        void *v;
        if (btree_find(p->db, id, &v) == FALSE) {
                *item = NULL;
                return FALSE;
        }
        *item = (pitem_t*)v;

        return TRUE;
}

/* RAT specific includes here */
#include "render_3D.h"

int
pdb_item_create(pdb_t *p, u_int32 id)
{
        pitem_t *item;

        if (btree_find(p->db, id, (void**)&item)) {
                debug_msg("Item already exists\n");
                return FALSE;
        }

        item = (pitem_t*)xmalloc(sizeof(pitem_t));
        if (item == NULL) {
                return FALSE;
        }

        /* Initialize elements of item here as necesary **********************/

        item->ssrc = id;
        item->render_3D_data = NULL;

        /*********************************************************************/

        if (btree_add(p->db, id, (void*)item) == FALSE) {
                debug_msg("failed to add item to persistent database!\n");
                return FALSE;
        }

        p->nelem++;
        return TRUE;
}

int
pdb_item_destroy(pdb_t *p, u_int32 id)
{
        pitem_t *item;

        if (btree_remove(p->db, id, (void**)&item) == FALSE) {
                debug_msg("Cannot delete item because it does not exist!\n");
                return FALSE;
        }

        assert(id == item->ssrc);

        /* clean up elements of item here ************************************/

        if (item->render_3D_data != NULL) {
                render_3D_free(&item->render_3D_data);
        }

        /*********************************************************************/

        debug_msg("Removing persistent database entry for SSRC 0x%08lx\n", 
                  item->ssrc);
        xfree(item);
        p->nelem--;
        return TRUE;
}
