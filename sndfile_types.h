
#ifndef __SNDFILE_TYPES_H__
#define __SNDFILE_TYPES_H__

typedef enum { 
        SNDFILE_ENCODING_PCMU,
        SNDFILE_ENCODING_PCMA,
        SNDFILE_ENCODING_L16
} sndfile_encoding_e;

typedef struct {
        sndfile_encoding_e encoding;
        u_int32 sample_rate;
        u_int32 channels;
} sndfile_fmt_t;

#endif /* __SNDFILE_TYPES_H__ */
