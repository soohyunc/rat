
#ifndef   __NEW_CHANNEL_H__
#define   __NEW_CHANNEL_H__

#include "playout.h"

#define CC_NAME_LENGTH 16

typedef u_int32 cc_id_t;

typedef struct {
        cc_id_t descriptor;
        char    name [CC_NAME_LENGTH];
} cc_details;

#define MAX_MEDIA_UNITS   3
#define MAX_CHANNEL_UNITS 3

typedef struct {
        u_int8      nelem;
        coded_unit *cu[MAX_MEDIA_UNITS];
} media_data;

typedef struct {
        u_int   pt;
        u_char *data;
        u_int16 data_len;
} channel_unit;

typedef struct {
        u_int8 nelem;
        coded_unit *cu[MAX_CHANNEL_UNITS];
}

struct s_channel_state;

/* Channel coder query functions:
 * channel_get_coder_count returns number of available channel coders,
 * and channel_get_coder_details copies details of idx'th coder into ccd.
 */

int       channel_get_coder_count   (void);
int       channel_get_coder_details (int idx, cc_details *ccd);

int       channel_coder_create      (cc_id_t id, struct s_channel_state **cs);
int       channel_coder_destory     (struct s_channel_state **cs);

/* These are for coder specific parameters */
int       channel_coder_set_parameters (struct s_channel_state *cs, char *cmd);
int       channel_coder_get_parameters (struct s_channel_state *cs, char *cmd, int cmd_len);

/* This is a universal parameter */
int       channel_coder_set_units_per_packet (struct s_channel_state *cs, u_int16);
u_int16   channel_coder_get_units_per_packet (struct s_channel_state *cs);

int       channel_coder_encode (struct s_channel_state  *cs, 
                                struct s_playout_buffer *media_buffer, 
                                struct s_playout_buffer *channel_buffer,
                                u_int32                  now);
int       channel_coder_decode (struct s_channel_state  *cs, 
                                struct s_playout_buffer *media_buffer, 
                                struct s_playout_buffer *channel_buffer,
                                u_int32                  now);
int       channel_coder_reset  (struct s_channel_state  *cs);

/* Payload mapping functions */
cc_id_t   channel_coder_get_by_payload (u_int8 payload);
int       channel_coder_set_payload    (cc_id_t id, u_int8  payload);   
int       channel_coder_get_payload    (cc_id_t id, u_int8* payload);   

#endif /* __NEW_CHANNEL_H__ */
