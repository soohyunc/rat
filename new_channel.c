#include "config_unix.h"
#include "config_win32.h"

#include "codec_types.h"
#include "new_channel.h"
#include "playout.h"

#include "memory.h"

typedef struct s_channel_state {
        u_char  coder;            /* Index of coder in coder table      */
        u_char *state;            /* Pointer to state relevent to coder */
        int     state_len;        /* The size of that state             */
        u_int16 units_per_packet; /* The number of units per packet     */
} channel_state_t;

typedef struct {
        char    name[CC_NAME_LENGTH];

        int     (*enc_create_state)   (u_char                **state,
                                       int                    *len);

        void    (*enc_destroy_state)  (u_char                **state, 
                                       int                     len);

        int     (*enc_set_parameters) (u_char                 *state, 
                                       char                   *cmd);

        int     (*enc_get_parameters) (u_char                 *state, 
                                       char                   *cmd, 
                                       int                     cmd_len);

        int     (*enc_reset)          (u_char                  *state);

        int     (*enc_encode)         (u_char                  *state, 
                                       struct s_playout_buffer *in, 
                                       struct s_playout_buffer *out, 
                                       int                      units_per_packet);

        int     (*dec_create_state)   (u_char                 **state,
                                       int                     *len);

        void    (*dec_destroy_state)  (u_char                 **state, 
                                       int                      len);

        int     (*dec_reset)          (u_char                  *state);
        int     (*dec_decode)         (u_char                  *state, 
                                       struct s_playout_buffer *in, 
                                       struct s_playout_buffer *out,
                                       u_int32                  now);

} channel_coder_t;

#include "cc_vanilla.h"
static channel_coder_t table[] = {
        {"No channel coder",
         vanilla_encoder_create,
         vanilla_encoder_destroy,
         NULL,                   /* No parameters to set ...*/
         NULL,                   /* ... or get. */
         vanilla_encoder_reset,
         vanilla_encoder_encode,
         NULL,
         NULL,
         NULL,
         vanilla_decoder_decode}
};

#define CC_ID_TO_IDX(x) ((x) + 0xff01)
#define CC_IDX_TO_ID(x) (u_char)((x) - 0xff01)

#define CC_NUM_CODERS (sizeof(table)/sizeof(channel_coder_t))

int
channel_get_coder_count()
{
        return (int)CC_NUM_CODERS;
}

int
channel_get_coder_details(int idx, cc_details *ccd)
{
        if (idx >=  0 && 
            idx < channel_get_coder_count()) {
                ccd->descriptor = CC_IDX_TO_ID(idx);
                strcpy(ccd->name, table[idx].name);
                return TRUE;
        }
        return FALSE;
}

/* The create, destroy, and reset functions take the same arguments and so use
 * is_encoder to determine which function in the table to call.  It's dirty
 * but it saves typing.
 */

int
channel_coder_create(cc_id_t id, channel_state_t **ppcs, int is_encoder)
{
        channel_state_t *pcs;
        int (*create_state)(u_char**, int *len);

        pcs = (channel_state_t*)xmalloc(sizeof(channel_state_t));
        
        if (pcs == NULL) {
                return FALSE;
        }

        if (is_encoder) {
                create_state = table[pcs->coder].enc_create_state;
        } else {
                create_state = table[pcs->coder].dec_create_state;
        }

        *ppcs = pcs;

        pcs->coder = CC_ID_TO_IDX(id);
        pcs->units_per_packet = 2;

        if (create_state) {
                create_state(&pcs->state, &pcs->state_len);
        } else {
                pcs->state     = NULL;
                pcs->state_len = 0;
        }

        return TRUE;
}

void
channel_coder_destroy(channel_state_t **ppcs, int is_encoder)
{
        channel_state_t *pcs = *ppcs;

        void (*destroy_state)(u_char**, int);

        if (is_encoder) {
                destroy_state = table[pcs->coder].enc_destroy_state;
        } else {
                destroy_state = table[pcs->coder].dec_destroy_state;
        }

        if (destroy_state) {
                destroy_state(&pcs->state, pcs->state_len);
        }

        assert(pcs->state     == NULL);
        assert(pcs->state_len == 0);

        xfree(pcs);
        *ppcs = NULL;
}

int
channel_coder_reset(channel_state_t *cs, int is_encoder)
{
        int (*reset) (u_char *state);
        
        if (is_encoder) {
                reset = table[cs->coder].enc_reset; 
        } else {
                reset = table[cs->coder].dec_reset; 
        }
        
        return (reset != NULL) ? reset(cs->state) : TRUE;
}

/* Encoder specifics */

int
channel_encoder_set_units_per_packet(channel_state_t *cs, u_int16 units)
{
        /* This should not be hardcoded, it should be based on packet size [oth] */
        if (units != 0 && units <= MAX_UNITS_PER_PACKET) {
                cs->units_per_packet = units;
                return TRUE;
        }
        return FALSE;
}

u_int16 
channel_encoder_get_units_per_packet(channel_state_t *cs)
{
        return cs->units_per_packet;
}

int
channel_encoder_set_parameters(channel_state_t *cs, char *cmd)
{
        if (table[cs->coder].enc_set_parameters) {
                return table[cs->coder].enc_set_parameters(cs->state, cmd);
        }
        return TRUE;
}

int
channel_encoder_get_parameters(channel_state_t *cs, char *cmd, int cmd_len)
{
        if (table[cs->coder].enc_get_parameters) {
                return table[cs->coder].enc_get_parameters(cs->state, cmd, cmd_len);
        }
        return TRUE;
}

int
channel_encoder_encode(channel_state_t         *cs, 
                       struct s_playout_buffer *media_buffer, 
                       struct s_playout_buffer *channel_buffer)
{
        assert(table[cs->coder].enc_encode != NULL);
        return table[cs->coder].enc_encode(cs->state, media_buffer, channel_buffer, cs->units_per_packet);
}

int
channel_decoder_decode(channel_state_t         *cs, 
                       struct s_playout_buffer *media_buffer, 
                       struct s_playout_buffer *channel_buffer,
                       u_int32                  now)
{
        assert(table[cs->coder].dec_decode != NULL);
        return table[cs->coder].dec_decode(cs->state, media_buffer, channel_buffer, now);
}
