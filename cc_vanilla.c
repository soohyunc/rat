/*
 * FILE:      cc_vanilla.c
 * AUTHOR(S): Orion Hodson 
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

#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "playout.h"
#include "cc_vanilla.h"

#include "memory.h"
#include "util.h"
#include "debug.h"

#include "timers.h"

typedef struct {
        /* Encoder state is just buffering of media data to compose a packet */
        codec_id_t  codec_id;
        ts_t        playout;
        u_int32     nelem;
        media_data *elem[MAX_UNITS_PER_PACKET];
} ve_state;

int
vanilla_encoder_create(u_char **state, u_int32 *len)
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
vanilla_encoder_destroy(u_char **state, u_int32 len)
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
                media_data_destroy(&ve->elem[i], sizeof(media_data));
        }
        ve->nelem = 0;
        
        return TRUE;
}

/* vanilla_encoder_output transfers media data into channel_unit */

static void
vanilla_encoder_output(ve_state *ve, struct s_pb *out)
{
        u_int32 i, used;
        channel_data *cd;

        /* We have state for first unit and data for all others */
        channel_data_create(&cd, ve->nelem + 1);
        
        /* Fill in payload */
        cd->elem[0]->pt           = codec_get_payload(ve->codec_id);

        used = 0;

        /* Get state for first unit if there */
        if (ve->elem[0]->rep[0]->state) {
                cd->elem[0]->data     = ve->elem[0]->rep[0]->state;
                cd->elem[0]->data_len = ve->elem[0]->rep[0]->state_len;
                ve->elem[0]->rep[0]->state     = NULL;
                ve->elem[0]->rep[0]->state_len = 0;
                used++;
        }

        /* Transfer coded data to channel_data */
        for(i = 0; i < ve->nelem; i++) {
                cd->elem[used]->data     = ve->elem[i]->rep[0]->data;
                cd->elem[used]->data_len = ve->elem[i]->rep[0]->data_len;
                ve->elem[i]->rep[0]->data = NULL;
                ve->elem[i]->rep[0]->data_len = 0;
                used++;
                media_data_destroy(&ve->elem[i], sizeof(media_data));
        }
        ve->nelem = 0;

        assert(used <= cd->nelem);

        pb_add(out, 
               (u_char*)cd, 
               sizeof(channel_data), 
               ve->playout);
}

int
vanilla_encoder_encode (u_char      *state,
                        struct s_pb *in,
                        struct s_pb *out,
                        u_int32      upp)
{
        u_int32     m_len;
        ts_t        playout;
        struct      s_pb_iterator *pi;
        media_data *m;
        ve_state   *ve = (ve_state*)state;

        assert(upp != 0 && upp <= MAX_UNITS_PER_PACKET);

        pb_iterator_create(in, &pi);
        pb_iterator_advance(pi); /* Move to first element */

        while(pb_iterator_detach_at(pi, (u_char**)&m, &m_len, &playout)) {
                /* Remove element from playout buffer - it belongs to
                 * the vanilla encoder now.
                 */
                assert(m != NULL);

                if (ve->nelem == 0) {
                        /* If it's the first unit make a note of it's
                         *  playout */
                        ve->playout = playout;
                        if (m->nrep == 0) {
                                /* We have no data ready to go and no data
                                 * came off on incoming queue.
                                 */
                                media_data_destroy(&m, sizeof(media_data));
                                continue;
                        }
                } else {
                        /* Check for early send required:      
                         * (a) if this unit has no media respresentations 
                         *     e.g. end of talkspurt.
                         * (b) codec type of incoming unit is different 
                         *     from what is on queue.
                         */
                        if (m->nrep == 0) {
                                vanilla_encoder_output(ve, out);
                                media_data_destroy(&m, sizeof(media_data));
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
        }

        pb_iterator_destroy(in, &pi);

        return TRUE;
}


static void
vanilla_decoder_output(channel_unit *cu, struct s_pb *out, ts_t playout)
{
        const codec_format_t *cf;
        codec_id_t            id;
        u_int32               data_len;
        u_char               *p, *end;
        media_data           *m;
        ts_t                  playout_step;

        id = codec_get_by_payload(cu->pt);
        cf = codec_get_format(id);

        media_data_create(&m, 1);
        assert(m->nrep == 1);

        /* Do first unit separately as that may have state */
        p    = cu->data;
        end  = cu->data + cu->data_len;

        if (cf->mean_per_packet_state_size) {
                m->rep[0]->state_len = cf->mean_per_packet_state_size;
                m->rep[0]->state     = (u_char*)block_alloc(m->rep[0]->state_len);
                memcpy(m->rep[0]->state, p, cf->mean_per_packet_state_size);
                p += cf->mean_per_packet_state_size;
        }

        data_len = codec_peek_frame_size(id, p, (u_int16)(end - p));
        m->rep[0]->id = id;
        m->rep[0]->data_len = (u_int16)data_len;
        m->rep[0]->data     = (u_char*)block_alloc(data_len);
        memcpy(m->rep[0]->data, p, data_len);
        p += data_len;

        if (pb_add(out, (u_char *)m, sizeof(media_data), playout) == FALSE) {
                debug_msg("XXX Failed to add unit\n");
                media_data_destroy(&m, sizeof(media_data));
                return;
        }
        /* Now do other units which do not have state*/
        playout_step = ts_map32(cf->format.sample_rate, codec_get_samples_per_frame(id));
        while(p < end) {
                playout = ts_add(playout, playout_step);
                media_data_create(&m, 1);
                m->rep[0]->id = id;
                assert(m->nrep == 1);

                data_len            = codec_peek_frame_size(id, p, (u_int16)(end - p));
                m->rep[0]->data     = (u_char*)block_alloc(data_len);
                m->rep[0]->data_len = (u_int16)data_len;

                memcpy(m->rep[0]->data, p, data_len);
                p += data_len;
                if (pb_add(out, (u_char *)m, sizeof(media_data), playout) == FALSE) {
                        debug_msg("Vanilla decode failed\n");
                }
        }
        assert(p == end);
}

int
vanilla_decoder_decode(u_char      *state,
                       struct s_pb *in, 
                       struct s_pb *out, 
                       ts_t         now)
{
        struct s_pb_iterator *pi;
        channel_unit *cu;
        channel_data *c;
        u_int32       clen;
        ts_t          playout;

        assert(state == NULL); /* No decoder state needed */
        UNUSED(state);

        pb_iterator_create(in, &pi);
        assert(pi != NULL);
        
        while(pb_iterator_get_at(pi, (u_char**)&c, &clen, &playout)) {
                assert(c != NULL);
                assert(clen == sizeof(channel_data));

                if (ts_gt(playout, now)) {
                        /* Playout point of unit is after now.  Stop! */
                        break;
                }
                pb_iterator_detach_at(pi, (u_char**)&c, &clen, &playout);
                
                assert(c->nelem == 1);
                cu = c->elem[0];
                vanilla_decoder_output(cu, out, playout);
                channel_data_destroy(&c, sizeof(channel_data));
        }

        pb_iterator_destroy(in, &pi);

        return TRUE;
}

int
vanilla_decoder_peek(u_int8   pkt_pt,
                     u_char  *buf,
                     u_int32  len,
                     u_int16  *upp,
                     u_int8   *pt)
{
        codec_id_t cid;

        assert(buf != NULL);
        assert(upp != NULL);
        assert(pt  != NULL);

        cid = codec_get_by_payload(pkt_pt);
        if (cid) {
                const codec_format_t *cf;
                u_int32               unit, done, step;
                /* Vanilla coding does nothing but group
                 * units.
                 */
                cf   = codec_get_format(cid);
                unit = 0;
                done = cf->mean_per_packet_state_size;
                while(done < len) {
                        step = codec_peek_frame_size(cid, buf+done, (u_int16)(len - done));
                        if (step == 0) {
                                debug_msg("Zero data len for audio unit ?\n");
                                goto fail;
                        }
                        done += step;
                        unit ++;
                }

                assert(done <= len);

                if (done != len) goto fail;
                *upp = (u_int16)unit;
                *pt  = pkt_pt;
                return TRUE;
        }
fail:
        *upp = 0;
        *pt  = 255;
        return FALSE;
}

int 
vanilla_decoder_describe (u_int8   pkt_pt,
                          u_char  *data,
                          u_int32  data_len,
                          char    *out,
                          u_int32  out_len)
{
	codec_id_t            pri_id;
        const codec_format_t *pri_cf;

        pri_id = codec_get_by_payload(pkt_pt);
        if (pri_id) {
                pri_cf = codec_get_format(pri_id);
                strncpy(out, pri_cf->long_name, out_len);
        } else {
                strncpy(out, "Unknown", out_len);
        }

        /* string safety - strncpy not always safe */
        out[out_len - 1] = '\0';

        UNUSED(data);
        UNUSED(data_len);

        return TRUE;
}







