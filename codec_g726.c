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
#include "bitstream.h"

#define CODEC_PAYLOAD_NO(x) (x)

#define G726_SAMPLES_PER_FRAME 160

static codec_format_t cs[] = {
        /* G726-40 **********************************************/
        {"G726-40", "G726-40-8K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(107), 0, 100, 
         {DEV_S16,  8000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-40", "G726-40-16K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(108), 0, 100, 
         {DEV_S16, 16000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-40", "G726-40-32K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(110), 0, 100, 
         {DEV_S16, 32000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-40", "G726-40-48K-Mono",  
         "ITU G.726-40 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(120), 0, 100, 
         {DEV_S16, 48000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* G726-32 ***********************************************/
        {"G726-32", "G726-32-8K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(2),   0, 80, 
         {DEV_S16,  8000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-32", "G726-32-16K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(104), 0, 80, 
         {DEV_S16, 16000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-32", "G726-32-32K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(105), 0, 80, 
         {DEV_S16, 32000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-32", "G726-32-48K-Mono",  
         "ITU G.726-32 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(106), 0, 80, 
         {DEV_S16, 48000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* Entries 0-3 G726-24 ***********************************************/
        {"G726-24", "G726-24-8K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(100), 0, 60, 
         {DEV_S16,  8000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-24", "G726-24-16K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(101), 0, 60, 
         {DEV_S16, 16000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-24", "G726-24-32K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(102), 0, 60, 
         {DEV_S16, 32000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-24", "G726-24-48K-Mono",  
         "ITU G.726-24 ADPCM codec. Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(103), 0, 60, 
         {DEV_S16, 48000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 3.3 ms */
        /* G726-16 ***********************************************/
        {"G726-16", "G726-16-8K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(96), 0, 40, 
         {DEV_S16,  8000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 20  ms */
        {"G726-16", "G726-16-16K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(97), 0, 40, 
         {DEV_S16, 16000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}}, /* 10  ms */
        {"G726-16", "G726-16-32K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(98), 0, 40, 
         {DEV_S16, 32000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 5  ms */
        {"G726-16", "G726-16-48K-Mono",  
         "ITU G.726-16 ADPCM codec. Marc Randolph modified Sun Microsystems public implementation.", 
         CODEC_PAYLOAD_NO(99), 0, 40, 
         {DEV_S16, 48000, 16, 1, G726_SAMPLES_PER_FRAME * BYTES_PER_SAMPLE}},  /* 3.3 ms */
};

#define G726_16        3
#define G726_24        2
#define G726_32        1
#define G726_40        0

#define G726_NUM_FORMATS sizeof(cs)/sizeof(codec_format_t)

/* In G726_NUM_RATES, 4 one for 16, 24, 32, 48 */
#define G726_NUM_RATES   (G726_NUM_FORMATS / 4)

typedef struct {
        struct g726_state  *gs;
        bitstream_t *bs;
} g726_t;

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
        g726_t *g;

        if (idx >=  G726_NUM_FORMATS) {
                return FALSE;
        }

        g = (g726_t*)xmalloc(sizeof(g726_t));
        if (g == NULL) {
                return FALSE;
        }

        g->gs = (struct g726_state*)xmalloc(sizeof(struct g726_state));
        if (g->gs == NULL) {
                        xfree(g);
                        return FALSE;
        }
        g726_init_state(g->gs);

        if (bs_create(&g->bs) == FALSE) {
                xfree(g->gs);
                xfree(g);
                return FALSE;
        }
        *s = (u_char*)g;

        return TRUE;
}

void
g726_state_destroy(u_int16 idx, u_char **s)
{
        g726_t *g;

        assert(idx < G726_NUM_FORMATS);

        g = (g726_t*)*s;
        xfree(g->gs);
        xfree(g->bs);
        xfree(g);
        *s = (u_char*)NULL;

        UNUSED(idx);
}

int
g726_encode(u_int16 idx, u_char *encoder_state, sample *inbuf, coded_unit *c)
{
        register sample *s;
        g726_t *g;
        int     i, cw;

        assert(encoder_state);
        assert(inbuf);
        assert(idx < G726_NUM_FORMATS);

        s = inbuf;
        g = (g726_t*)encoder_state;

        c->state     = NULL;
        c->state_len = 0;
        c->data      = (u_char*)block_alloc(cs[idx].mean_coded_frame_size);
        c->data_len  = cs[idx].mean_coded_frame_size;

        memset(c->data, 0, c->data_len);
        bs_attach(g->bs, c->data, c->data_len);

        idx = idx / G726_NUM_RATES;
        switch(idx) {
        case G726_16:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw    = g726_16_encoder(s[i], AUDIO_ENCODING_LINEAR, g->gs);
                        bs_put(g->bs, cw, 2);
                }
                break;
        case G726_24:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw    = g726_24_encoder(s[i], AUDIO_ENCODING_LINEAR, g->gs);
                        bs_put(g->bs, cw, 3);
                }
                break;
        case G726_32:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw    = g726_32_encoder(s[i], AUDIO_ENCODING_LINEAR, g->gs);
                        bs_put(g->bs, cw, 4);
                }
                break;
        case G726_40:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw    = g726_40_encoder(s[i], AUDIO_ENCODING_LINEAR, g->gs);
                        bs_put(g->bs, cw, 5);
                }
                break;
        }

        return c->data_len;
}

int
g726_decode(u_int16 idx, u_char *decoder_state, coded_unit *c, sample *data)
{
        int cw,i;
        sample *dst;
        g726_t *g; 

        /* paranoia! */
        assert(decoder_state);
        assert(c);
        assert(data);
        assert(idx < G726_NUM_FORMATS);
        assert(c->state_len == 0);
        assert(c->data_len == cs[idx].mean_coded_frame_size);

        g = (g726_t*)decoder_state;
        bs_attach(g->bs, c->data, c->data_len);

        dst = data;

        idx = idx / G726_NUM_RATES;
        switch(idx) {
        case G726_16:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw = bs_get(g->bs, 2);
                        cw = g726_16_decoder(cw, AUDIO_ENCODING_LINEAR, g->gs);
                        dst[i] = (sample)cw;
                }
                break;
        case G726_24:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw = bs_get(g->bs, 3);
                        cw = g726_24_decoder(cw, AUDIO_ENCODING_LINEAR, g->gs);
                        dst[i] = (sample)cw;
                }
                break;
        case G726_32:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw = bs_get(g->bs, 4);
                        cw = g726_32_decoder(cw, AUDIO_ENCODING_LINEAR, g->gs);
                        dst[i] = (sample)cw;
                }
                break;
        case G726_40:
                for(i = 0; i < G726_SAMPLES_PER_FRAME; i++) {
                        cw = bs_get(g->bs, 5);
                        cw = g726_40_decoder(cw, AUDIO_ENCODING_LINEAR, g->gs);
                        dst[i] = (sample)cw;
                }
                break;
        }

        return c->data_len;
}




