/*
 * FILE:    playout.c
 * AUTHORS: Orion Hodson
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include "config_unix.h"
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "playout.h"

/* Playout buffer types ******************************************************/

typedef struct s_pb_node {
        struct s_pb_node* prev;
        struct s_pb_node* next;
        u_int32           playout;    /* playout  timestamp */
        u_char           *data;
        u_int32           data_len;
} pb_node_t;

typedef struct s_playout_buffer {
        pb_node_t   sentinel;                              /* Stores head and tail     */
        pb_node_t  *pp;                                    /* Playout point            */
        void      (*freeproc)(u_char **data, u_int32 len); /* free proc for data added */
        u_int32     history_len;        /* Amount of data before playout point to keep */
} playout_buffer;

/* Construction / Destruction functions **************************************/

int 
playout_buffer_create(playout_buffer **ppb, void (*datafreeproc)(u_char **data, u_int32 data_len), u_int32 history_len)
{
        playout_buffer *pb;

        assert(datafreeproc  != NULL);
        assert(history_len   != 0);

        pb = (playout_buffer*)xmalloc(sizeof(playout_buffer));
        if (pb) {
                *ppb = pb;
                pb->sentinel.next = pb->sentinel.prev = &pb->sentinel;
                pb->pp            = &pb->sentinel;
                pb->freeproc      = datafreeproc;
                pb->history_len   = history_len;
                return TRUE;
        }
        return FALSE;
}

int 
playout_buffer_destroy (struct s_playout_buffer **ppb)
{
        pb_node_t *curr, *next, *stop;
        playout_buffer *pb;
        
        pb    = *ppb;
        stop  = &pb->sentinel;
        curr  =  pb->sentinel.next;

        while(curr != stop) {
                next = curr->next;
                pb->freeproc(&curr->data, curr->data_len);
                assert(curr->data == NULL);
                block_free(curr, sizeof(pb_node_t));
                curr = next;
        }
        xfree(pb);
        *ppb = NULL;

        return TRUE;
}

/* ts_gt(a,b) returns (a > b) ? TRUE : FALSE */
#define HALF_CYCLE 0x7fffffff

static int
ts_gt(u_int32 a, u_int32 b)
{
        u_int32 diff = a - b;
        return (diff < HALF_CYCLE && diff != 0);
}

/* Iterator functions ********************************************************/

int 
playout_buffer_add (playout_buffer *pb, u_char *data, u_int32 data_len, u_int32 playout)
{
        pb_node_t *curr, *stop, *made;
        
        /* Assuming that addition will be at the end of list (or near it) */
        
        stop = &pb->sentinel;
        curr = stop->prev;    /* list tail */
        while (curr != stop && ts_gt(curr->playout, playout)) {
                curr = curr->prev;
        }

        /* Check if unit already exists */
        if (curr != stop && curr->playout == playout) {
                debug_msg("playout_buffer_add failed: Unit (%u) already exists\n", playout);
                return FALSE;
        }

        /* Check if we are inserting before playout point */
        if (pb->pp != stop && ts_gt(pb->pp->playout, playout)) {
                debug_msg("Warning: unit (%u) before playout point (%u).\n", playout, pb->pp->playout);
        }

        made = (pb_node_t*)block_alloc(sizeof(pb_node_t));
        if (made) {
                made->playout  = playout;
                made->data     = data;
                made->data_len = data_len;
                made->prev = curr;                
                made->next = curr->next;
                made->next->prev = made;
                made->prev->next = made;
                return TRUE;
        } 
        debug_msg("Insufficient memory\n");
        return FALSE;
}

int
playout_buffer_get (playout_buffer *pb, u_char **data, u_int32 *data_len, u_int32 *playout)
{
        pb_node_t* sentinel = &pb->sentinel;
        
        /* This can be rewritten in fewer lines, but the obvious way is not as efficient. */
        if (pb->pp != sentinel) {
                *data     = pb->pp->data;
                *data_len = pb->pp->data_len;
                *playout  = pb->pp->playout;
                return TRUE;
        } else {
                /* We are at the start of the list so maybe we haven't tried reading before */
                pb->pp = sentinel->next;
                if (pb->pp != sentinel) {
                        *data     = pb->pp->data;
                        *data_len = pb->pp->data_len;
                        *playout  = pb->pp->playout;
                        return TRUE;
                } 
        }
        /* There is data on the list */
        *data     = NULL;
        *data_len = 0;
        *playout  = 0;
        return FALSE;
}

int playout_buffer_advance(playout_buffer *pb, u_char **data, u_int32 *data_len, u_int32 *playout)
{
        pb_node_t* sentinel = &pb->sentinel;
 
       if (pb->pp->next != sentinel) {
                pb->pp    = pb->pp->next;
                *data     = pb->pp->data;
                *data_len = pb->pp->data_len;
                *playout  = pb->pp->playout;
                return TRUE;
        }
       /* There is no next... */
        *data     = NULL;
        *data_len = 0;
        *playout  = 0;
        return FALSE;
}

int playout_buffer_rewind(playout_buffer *pb, u_char **data, u_int32 *data_len, u_int32 *playout)
{
        pb_node_t* sentinel = &pb->sentinel;

        if (pb->pp->prev != sentinel) {
                pb->pp    = pb->pp->prev;
                *data     = pb->pp->data;
                *data_len = pb->pp->data_len;
                *playout  = pb->pp->playout;
                return TRUE;
        }
        /* There is no previous */
        *data     = NULL;
        *data_len = 0;
        *playout  = 0;
        return FALSE;
}

/* Book-keeping functions ****************************************************/

int
playout_buffer_audit(playout_buffer *pb)
{
        u_int32 cutoff;
        pb_node_t *stop, *curr, *next;

        stop = &pb->sentinel;
        if (pb->pp != stop) {
                cutoff = pb->pp->playout - pb->history_len;
                curr = stop->next; /* head */
                while(curr != stop && ts_gt(cutoff, curr->playout)) {
                        next = curr->next;
                        curr->next->prev = curr->prev;
                        curr->prev->next = curr->next;
                        pb->freeproc(&curr->data, curr->data_len);
                        block_free(curr, sizeof(pb_node_t));
                        curr = next;
                }
        }
                
        return TRUE;
}

int
playout_buffer_relevent (struct s_playout_buffer *pb, u_int32 now)
{
        pb_node_t *last;
        last = pb->sentinel.prev; /* tail */
        if (last == &pb->sentinel || ts_gt(now, last->playout)) {
                return FALSE;
        }
        return TRUE;
}

#ifdef TEST_PLAYOUT

void datafree(u_char **data, u_int32 datalen)
{
        *data = NULL;
}

#define OFFSET 0xffffffef  /* 0 */

int main()
{
        playout_buffer *p;
        pb_node_t      *iter;
        u_char *data;
        u_int32 playout, data_len;

        playout_buffer_create (&p, datafree, 5);
        playout_buffer_add    (p, NULL, 0, 10 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 20 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 15 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 17 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 22 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 12 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 11 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 15 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 11 + OFFSET);
        playout_buffer_add    (p, NULL, 0, 18 + OFFSET); 

        debug_msg("Forward order\n");
        playout = 0;
        playout_buffer_get(p, &data, &data_len, &playout);
        debug_msg("%u\n", playout);
        
        while(playout_buffer_advance(p, &data, &data_len, &playout)) {
                debug_msg("%u\n", playout);
        }

        playout_buffer_audit(p);
        debug_msg("Auditing\n");
        debug_msg("Backward order\n");
        playout_buffer_get(p, &data, &data_len, &playout);
        debug_msg("%u\n", playout);
        while(playout_buffer_rewind(p, &data, &data_len, &playout)) {
                debug_msg("%u\n", playout);
        }

        debug_msg("Forward order\n");
        playout = 0;
        playout_buffer_get(p, &data, &data_len, &playout);
        debug_msg("%u\n", playout);
        
        while(playout_buffer_advance(p, &data, &data_len, &playout)) {
                debug_msg("%u\n", playout);
        }

        playout_buffer_destroy(&p);
}

#endif TEST_PLAYOUT
