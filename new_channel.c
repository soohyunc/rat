#include "config_unix.h"
#include "config_win32.h"

#include "new_channel.h"
#include "playout.h"

typedef struct s_channel_state {
        u_char  coder;            /* Index of coder in coder table */
        u_char *state;
        int     state_len;
        u_int16 units_per_packet;
} channel_state_t;

typedef struct {
        char    name[CC_NAME_LENGTH];
        int     (*create_state)   (u_char** state,
                                   int    * len);
        void    (*destroy_state)  (u_char** state, 
                                   int      len);
        int     (*set_parameters) (u_char * state, 
                                   char   * cmd);
        int     (*get_parameters) (u_char * state, 
                                   char   * cmd, 
                                   int      cmd_len);
        int     (*encode)         (u_char * state, 
                                   struct s_playout_buffer *in, 
                                   struct s_playout_buffer *out, 
                                   int      units_per_packet);
        int     (*decode)         (u_char * state, 
                                   struct s_playout_buffer *in, 
                                   struct s_playout_buffer *out);
        int     (*reset)          (u_char * state);
} channel_coder_t;

static channel_coder_t table[] = {};

#define CC_ID_TO_IDX(x) ((x) + 0xff01)
#define CC_IDX_TO_ID(x) (u_char)((x) - 0xff01)

int
channel_coder_create(cc_id_t id, channel_state_t **ppcs)
{
        channel_state_t *pcs;
        pcs = (channel_state_t*)xmalloc(sizeof(channel_state_t));

        if (pcs == NULL) {
                return FALSE;
        }

        *ppcs = pcs;

        pcs->coder = CC_ID_TO_IDX(id);
        pcs->units_per_packet = 2;

        if (table[pcs->coder].create_state) {
                table[pcs->coder].create_state(&pcs->state, &pcs->state_len);
        } else {
                pcs->state     = NULL;
                pcs->state_len = 0;
        }

        return TRUE;
}

int
channel_coder_destroy(channel_state_t **ppcs)
{
        channel_state_t *pcs = *ppcs;

        if (table[pcs->coder].destroy_state) {
                table[pcs->coder].destroy_state(&pcs->state, pcs->state_len);
        }

        assert(pcs->state     == NULL);
        assert(pcs->state_len == 0);

        xfree(pcs);
        *ppcs = NULL;
}

int
channel_coder_set_units_per_packet(channel_state_t *cs, u_int16 units)
{
        /* This should not be hardcoded, it should be based on packet size [oth] */
        if (units != 0 && units <= 8) {
                cs->units_per_packet = units;
                return TRUE;
        }
        return FALSE;
}

u_int16 
channel_coder_get_units_per_packet(channel_state_t *cs)
{
        return cs->units_per_packet;
}

int
channel_coder_set_parameters(channel_state_t *cs, char *cmd)
{
        if (table[cs->coder].set_parameters) {
                return table[cs->coder].set_parameters(cs->state, cmd);
        }
        return TRUE;
}

int
channel_coder_get_parameters(channel_state_t *cs, char *cmd, int cmd_len)
{
        if (table[cs->coder].get_parameters) {
                return table[cs->coder].get_parameters(cs->state, cmd, cmd_len);
        }
        return TRUE;
}

int
channel_coder_reset(channel_state_t *cs)
{
        if (table[cs->coder].reset) {
                return table[cs->coder].reset(cs->state);
        }
        return TRUE;
}

