#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_vanilla.h"

#include "memory.h"
#include "util.h"

typedef struct {
        /* Encoder state is just buffering of media data to compose a packet */
        u_int8      pt;
        u_int32     playout;
        u_int32     nelem;
        media_data *elem[MAX_UNITS_PER_PACKET];
} ve_state;

int
vanilla_encoder_create(u_char **state, int *len)
{
        ve_state *ve = (ve_state*)xmalloc(sizeof(ve_state));

        if (ve) {
                *state = (u_char*)ve;
                *len   = sizeof(ve_state);
                return TRUE;
        }

        return FALSE;
}

void
vanilla_encoder_destroy(u_char **state, int len)
{
        assert(len == sizeof(ve_state));
        xfree(*state);
        *state = NULL;
}

static void
vanilla_encoder_output(ve_state *ve, struct s_playout_buffer *out)
{
        u_int32 size, done, i;
        u_char *buffer;
        channel_data *cd;

        /* Find size of block to copy data into */
        size  = ve->elem[0]->rep[0]->state_len;        
        size += ve->elem[0]->rep[0]->data_len;

        for(i = 0; i < ve->nelem; i++) {
                size += ve->elem[0]->rep[0]->data_len;
        }
        
        /* Allocate block and get ready */
        channel_data_create(&cd, 1);
        cd->elem[0]->pt       = ve->pt;
        cd->elem[0]->data     = (u_char*)block_alloc(size);
        cd->elem[0]->data_len = size;

        /* Copy data out of coded units and into continguous blocks */
        buffer = cd->elem[0]->data;

        done = 0;
        for(i = 0; i < ve->nelem; i++) {
                if (i == 0 && ve->elem[0]->rep[0]->state_len != 0) {
                        memcpy(buffer, ve->elem[0]->rep[0]->state, ve->elem[0]->rep[0]->state_len);
                        buffer += ve->elem[0]->rep[0]->state_len;
                }
                memcpy(buffer, ve->elem[i]->rep[0]->data, ve->elem[i]->rep[0]->data_len);
                buffer += ve->elem[i]->rep[0]->data_len;

                media_data_destroy(&ve->elem[i]);
        }
        ve->nelem = 0;

        playout_buffer_add(out, (u_char*)cd, sizeof(channel_data), ve->playout);
}

/* Payload change check ***!!!!****/

int
vanilla_encoder_encode (u_char *state,
                        struct s_playout_buffer *in,
                        struct s_playout_buffer *out,
                        int    upp)
{
        u_int32     playout, m_len;
        media_data *m;
        ve_state   *ve = (ve_state*)state;

        while(playout_buffer_get(in, (u_char**)&m, &m_len, &playout)) {
                /* Remove element from playout buffer - it belongs to
                 * the vanilla encoder now.
                 */
                playout_buffer_remove(in, (u_char**)&m, &m_len, &playout);

                if (ve->nelem != 0) {
                        /* Check for early send required */
                        /* !!!! CHECK PAYLOAD !!!! */
                        /* if changed send.  */
                        /* check if unit null and ve->nelem != 0 if so send */
                }

                assert(m_len == sizeof(media_data));

                /* If it's the first unit make a note of it's playout */
                if (ve->nelem == 0) {
                        ve->playout = playout;
                }
                
                ve->elem[ve->nelem] = m;
                ve->nelem++;

                if (ve->nelem >= upp) {
                        vanilla_encoder_output(ve, out);
                }
        }
        return TRUE;
}
