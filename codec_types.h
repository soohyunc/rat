/*
 * FILE:    codec_types.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
 * All rights reserved.
 *
 */

#ifndef _CODEC_TYPES_H_
#define _CODEC_TYPES_H_

#define CODEC_PAYLOAD_DYNAMIC   255

typedef u_int32 codec_id_t;

typedef struct {
        u_char    *state;
        codec_id_t id;
} codec_state;

#define CODEC_SHORT_NAME_LEN   16
#define CODEC_LONG_NAME_LEN    32
#define CODEC_DESCRIPTION_LEN 128

typedef struct s_codec_format {
        char         short_name[CODEC_SHORT_NAME_LEN];
        char         long_name[CODEC_LONG_NAME_LEN];
        char         description[CODEC_DESCRIPTION_LEN];
        u_char       default_pt;
        u_int16      mean_per_packet_state_size;
        u_int16      mean_coded_frame_size;
        const audio_format format;
} codec_format_t;

typedef struct s_coded_unit {
        codec_id_t id;
	u_char  *state;
	u_int16  state_len;
	u_char	*data;
	u_int16  data_len;
} coded_unit;

#define MAX_MEDIA_UNITS  5
/* This data structure is for storing multiple representations of
 * coded audio for a given time interval.
 */
typedef struct {
        u_int8      nrep;
        coded_unit *rep[MAX_MEDIA_UNITS];
} media_data;

int  media_data_create    (media_data **m, int nrep);
void media_data_destroy   (media_data **m, u_int32 md_size);

int  coded_unit_dup       (coded_unit *dst, coded_unit *src);

#endif /* _CODEC_TYPES_H_ */

