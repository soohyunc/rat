#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_vanilla.h"

#include "memory.h"
#include "util.h"

#include "timers.h"

typedef struct {
        /* Encoder state is just buffering of media data to compose a packet */
        codec_id_t  codec_id;
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
                memset(ve, 0, sizeof(ve_state));
                return TRUE;
        }

        return FALSE;
}

void
vanilla_encoder_destroy(u_char **state, int len)
{
        assert(len == sizeof(ve_state));
        vanilla_encoder_reset(*state);
        xfree(*state);
        *state = NULL;
}

int
vanilla_encoder_reset(u_char *state)
{
        ve_state *ve = (ve_state*)state;
        u_int32   i;

        for(i = 0; i < ve->nelem; i++) {
                media_data_destroy(&ve->elem[i]);
        }
        ve->nelem = 0;
        
        return TRUE;
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
        cd->elem[0]->pt       = codec_get_payload(ve->codec_id);
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

int
vanilla_encoder_encode (u_char                  *state,
                        struct s_playout_buffer *in,
                        struct s_playout_buffer *out,
                        int                      upp)
{
        u_int32     playout, m_len;
        media_data *m;
        ve_state   *ve = (ve_state*)state;

        while(playout_buffer_get(in, (u_char**)&m, &m_len, &playout)) {
                /* Remove element from playout buffer - it belongs to
                 * the vanilla encoder now.
                 */
                playout_buffer_remove(in, (u_char**)&m, &m_len, &playout);

                if (ve->nelem == 0) {
                        /* If it's the first unit make a note of it's playout */
                        ve->playout = playout;
                        if (m->nrep == 0) {
                                /* We have no data ready to go and no data
                                 * came off on incoming queue.
                                 */
                                media_data_destroy(&m);
                                continue;
                        }
                } else {
                        /* Check for early send required:      
                         * (a) if this unit has no media respresentations.
                         * (b) codec type of incoming unit is different from what is on queue.
                         */
                        if (m->nrep == 0) {
                                vanilla_encoder_output(ve, out);
                                media_data_destroy(&m);
                                continue;
                        } else if (m->rep[0]->id != ve->codec_id) {
                                vanilla_encoder_output(ve, out);
                        }
                } 

                assert(m_len == sizeof(media_data));

                ve->codec_id = m->rep[0]->id;                
                ve->elem[ve->nelem] = m;
                ve->nelem++;

                if (ve->nelem >= (u_int32)upp) {
                        vanilla_encoder_output(ve, out);
                }
                media_data_destroy(&m);
        }
        return TRUE;
}


__inline static void
vanilla_decoder_output(channel_unit *cu, struct s_playout_buffer *out, u_int32 playout)
{
        const codec_format_t *cf;
        codec_id_t            id;
        u_int32               data_len, playout_step;
        u_char               *p, *end;
        media_data           *m;
        
        id = codec_get_by_payload(cu->pt);
        cf = codec_get_format(id);

        media_data_create(&m, 1);
        assert(m->nrep == 1);

        /* Do first unit separately as that may have state */
        p    = cu->data;
        end  = cu->data + cu->data_len;

        if (cf->mean_per_packet_state_size) {
                memcpy(m->rep[0]->state, p, cf->mean_per_packet_state_size);
                m->rep[0]->state_len = cf->mean_per_packet_state_size;
                p                   += cf->mean_per_packet_state_size;
        }

        data_len = codec_peek_frame_size(id, p, end - p);
        memcpy(m->rep[0]->data, p, data_len);
        m->rep[0]->data_len = data_len;
        p                  += data_len;
        playout_buffer_add(out, (u_char *)m, sizeof(media_data), playout);

        /* Now do other units which do not have state*/
        playout_step = codec_get_samples_per_frame(id);
        while(p < end) {
                playout += playout_step;
                media_data_create(&m, 1);
                assert(m->nrep == 1);
                data_len = codec_peek_frame_size(id, p, end - p);
                memcpy(m->rep[0]->data, p, data_len);
                m->rep[0]->data_len = data_len;
                p                  += data_len;
                playout_buffer_add(out, (u_char *)m, sizeof(media_data), playout);
        }
        assert(p == end);
}

int
vanilla_decoder_decode(u_char *state,
                       struct s_playout_buffer *in, 
                       struct s_playout_buffer *out, 
                       u_int32 now)
{
        channel_unit           *cu;
        channel_data         *c;
        u_int32               playout, clen;

        assert(state == NULL); /* No decoder state needed */
                
        while(playout_buffer_get(in, (u_char**)&c, &clen, &playout)) {
                assert(c != NULL);
                assert(clen == sizeof(channel_data));

                if (ts_gt(playout, now)) {
                        /* Playout point of unit is after now.  Stop already!  */
                        break;
                }
                playout_buffer_remove(in, (u_char**)&c, &clen, &playout);
                
                assert(c->nelem == 1);
                cu = c->elem[0];
                vanilla_decoder_output(cu, out, playout);
                channel_data_destroy(&c);
        }
        return TRUE;
}
