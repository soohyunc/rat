/*
 * FILE:    codec_g726.c
 * AUTHORS: Orion Hodson
 * 
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec_g726.h"
#include "cx_g726.h"

#define CODEC_PAYLOAD_NO(x) (x)

static codec_format_t cs[] = {
        /* G726-16 ***********************************************/
        {"G726-16", "G726-16-8K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 40, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-16", "G726-16-16K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 40, {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-16", "G726-16-32K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 40, {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-16", "G726-16-48K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 40, {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* Entries 0-3 G726-24 ***********************************************/
        {"G726-24", "G726-24-8K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 60, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-24", "G726-24-16K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 60, {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-24", "G726-24-32K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 60, {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-24", "G726-24-48K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 60, {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* Entries 4-7 G726-24 ***********************************************/
        {"G726-32", "G726-32-8K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(2),   0, 80, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-32", "G726-32-16K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 80, {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-32", "G726-32-32K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 80, {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-32", "G726-32-48K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 80, {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* Entries 8-11 G726-40 **********************************************/
        {"G726-40", "G726-40-8K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 100, {DEV_S16,  8000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-40", "G726-40-16K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 100, {DEV_S16, 16000, 16, 1, 160 * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-40", "G726-40-32K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 100, {DEV_S16, 32000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-40", "G726-40-48K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_DYNAMIC, 0, 100, {DEV_S16, 48000, 16, 1, 160 * BYTES_PER_SAMPLE}},  /* 3.3 ms */
};

#define G726_16        0
#define G726_24        1
#define G726_32        2
#define G726_40        3

#define G726_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

/* In G726_NUM_RATES, 4 one for 16, 24, 32, 48 */
#define G726_NUM_RATES   (G726_NUM_FORMATS / 4)

u_int16
g726_get_formats_count()
{
        return (u_int16)G726_NUM_FORMATS;
}

const codec_format_t *
g726_get_format(u_int16 idx)
{
        assert(idx < G726_NUM_FORMATS);
        return &cs[idx];
}

int 
g726_state_create(u_int16 idx, u_char **s)
{
        struct g726_state *gs;

        if (idx < G726_NUM_FORMATS) {
                gs = (struct g726_state*)xmalloc(sizeof(struct g726_state));
                if (gs) {
                        g726_init_state(gs);
                        *s = (u_char*)gs;
                        return TRUE;
                }
        }

        return 0;
}

void
g726_state_destroy(u_int16 idx, u_char **s)
{
        UNUSED(idx);

        assert(idx < G726_NUM_FORMATS);
        xfree(*s);
        *s = (u_char*)NULL;
}

int
g726_encode(u_int16 idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        register u_char *dst;
        register sample *s, *se;
        struct g726_state *gs;
        int     cw;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < G726_NUM_FORMATS);

        c->state     = NULL;
        c->state_len = 0;
        c->data      = (u_char*)block_alloc(cs[idx].mean_coded_frame_size);
        c->data_len  = cs[idx].mean_coded_frame_size;

        s  = inbuf;
        se = inbuf + 160; /* 160 samples in a frame */
        dst = c->data;

        gs = (struct g726_state*)encoder_state;

        idx = idx / G726_NUM_RATES;
        switch(idx) {
        case G726_16:
                while (s != se) {
                        cw    = g726_16_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 1 */
                        *dst  = (u_char)(cw << 6);
                        cw    = g726_16_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 2 */
                        *dst |= (u_char)(cw << 4);
                        cw    = g726_16_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 3 */
                        *dst |= (u_char)(cw << 2);
                        cw    = g726_16_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 4 */
                        *dst |= (u_char)(cw);
                        dst++;
                }
        case G726_24:
                while (s != se) {
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 1 */
                        *dst  = (u_char)(cw << 5);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 2 */
                        *dst |= (u_char)(cw << 2);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 3 */
                        *dst |= (u_char)((cw >> 1) & 0x3);
                        dst++;
                        *dst  = (u_char)((cw & 0x1) << 7);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 4 */
                        *dst |= (u_char)(cw << 4);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 5 */
                        *dst |= (u_char)(cw << 1);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 6 */
                        *dst |= (u_char)(cw >> 2);
                        dst++;
                        *dst  = (u_char)((cw & 0x3) << 6);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 7 */
                        *dst |= (u_char)(cw << 3);
                        cw    = g726_24_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 8 */
                        *dst |= (u_char)cw;
                        dst ++;
                }

                break;
        case G726_32:
                /* Packing in sample order not as RTP profile new draft 4 doc */
                while (s != se) {
                        cw    = g726_32_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 1 */
                        *dst  = (u_char)(cw << 4);
                        cw    = g726_32_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 2 */
                        *dst |= (u_char)cw;
                        dst++;
                }
                break;
        case G726_40:
                while (s != se) {
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 1 */
                        *dst  = (u_char)(cw << 3);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 2 */
                        *dst |= (u_char)(cw >> 2);
                        dst++;
                        *dst  = (u_char)((cw & 0x3) << 6);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 3 */
                        *dst |= (u_char)(cw << 1);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 4 */
                        *dst |= (u_char)(cw >> 4);
                        dst++;
                        *dst  = (u_char)(cw << 4);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 5 */
                        *dst |= (u_char)(cw >> 1);
                        dst++;
                        *dst  = (u_char)(cw << 7);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 6 */
                        *dst |= (u_char)(cw << 2);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 7 */
                        *dst |= (u_char)(cw >> 3);
                        dst++;
                        *dst  = (u_char)(cw << 5);
                        cw    = g726_40_encoder(*s++, AUDIO_ENCODING_LINEAR, gs); /* 8 */
                        *dst  |= (u_char)cw;
                        dst++;
                }

                break;
        }

        assert(s   - inbuf == 160u);
        assert(dst - c->data == c->data_len);

        return c->data_len;
}

int
g726_decode(u_int16 idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        int cw;
        register u_char *s, *se;
        register sample *dst;
        struct g726_state *gs; 

        /* paranoia! */
        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < G726_NUM_FORMATS);
        assert(c->state_len == 0);
        assert(c->data_len == cs[idx].mean_coded_frame_size);

        gs = (struct g726_state*)decoder_state;

        s   = c->data;
        se  = c->data + c->data_len;
        dst = data;

        idx = idx / G726_NUM_RATES;
        switch(idx) {
        case G726_16:
                while(s != se) {
                        cw     = *s >> 6;                                      /* 1 */
                        cw     = g726_16_decoder(cw, AUDIO_ENCODING_LINEAR, gs);
                        *dst++ = (sample)cw;
                        cw     = (*s >> 4) & 0x03;                             /* 2 */
                        cw     = g726_16_decoder(cw, AUDIO_ENCODING_LINEAR, gs);
                        *dst++ = (sample)cw;
                        cw     = (*s >> 2) & 0x03;                             /* 3 */
                        cw     = g726_16_decoder(cw, AUDIO_ENCODING_LINEAR, gs);
                        *dst++ = (sample)cw;
                        cw     = *s & 0x03;                                    /* 4 */
                        cw     = g726_16_decoder(cw, AUDIO_ENCODING_LINEAR, gs);
                        *dst++ = (sample)cw;
                        s++;
                }
                break;
        case G726_24:
                while(s != se) {
                        cw = *s >> 5;                                          /* 1 */
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s >> 2) & 0x7;                                  /* 2 */ 
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s++ & 0x3) << 1;                                /* 3 */
                        cw |= (*s & 0x80) >> 7;
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s & 0x70) >> 4;                                 /* 4 */
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s & 0x0e) >> 1;                                 /* 5 */
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s++ & 0x01) << 2;                               /* 6 */
                        cw |= (*s & 0xc0) >> 6;                               
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s & 0x34) >> 3;                                 /* 7 */
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = *s++ & 0x07;                                      /* 8 */
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                }
                break;
        case G726_32:
                while(s != se) {
                        cw = (*s >> 4);                                        /* 1 */
                        cw = g726_32_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw = (*s++ & 0x0f);                                    /* 2 */
                        cw = g726_32_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                }
                break;
        case G726_40:
                while (s != se) {
                        cw  = (*s >> 3);                                       /* 1 */
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s++ & 0x07) << 2;                              /* 2 */
                        cw |= (*s >> 6);
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s & 0x3e) >> 1;                                /* 3 */
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s++ & 0x01) << 4;                              /* 4 */
                        cw |= (*s >> 4);
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s++ & 0x0f) << 1;                              /* 5 */
                        cw |= (*s >> 7);
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s & 0x7c) >> 2;                                /* 6 */
                        cw  = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s++ & 0x03) << 3;                              /* 7 */
                        cw |= (*s >> 5);
                        cw = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                        cw  = (*s++ & 0x1f);                                   /* 8 */
                        cw = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, gs); 
                        *dst++ = (sample)cw;
                }
                break;
        }

        assert(s - c->data == c->data_len);

        return c->data_len;
}




