#ifndef __CHANNEL_TYPES_H__
#define __CHANNEL_TYPES_H__

/* Channel coder description information */

typedef u_int32 cc_id_t;

#define CC_NAME_LENGTH 32

typedef struct {
        cc_id_t descriptor;
        char    name[CC_NAME_LENGTH];
} cc_details;

/* In and out unit types.  On input channel encoder takes a playout buffer
 * of media_units and puts channel_units on the output playout buffer
 */

#define MAX_MEDIA_UNITS      3
#define MAX_CHANNEL_UNITS    3
#define MAX_UNITS_PER_PACKET 8

typedef struct {
        u_int8      nrep;
        coded_unit *rep[MAX_MEDIA_UNITS];
} media_data;

typedef struct {
        u_int   pt;
        u_char *data;
        u_int16 data_len;
} channel_unit;

typedef struct {
        u_int8        nelem;
        channel_unit *elem[MAX_CHANNEL_UNITS];
} channel_data;

int  channel_data_create  (channel_data **cd, int nelem);
void channel_data_destroy (channel_data **cd);

int  media_data_create    (media_data **m, int nrep);
void media_data_destroy   (media_data **m);

#endif /* __CHANNEL_TYPES_H__ */
