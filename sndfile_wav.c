/*
 * FILE:    sndfile_wav.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "codec_g711.h"
#include "sndfile_types.h"
#include "sndfile_wav.h"

/* Microsoft WAV file handling (severly restricted subset) 
 * Spec. was a text file called RIFF-format
 * Mirrored lots of places, try http://ftpsearch.ntnu.no/
 *
 * We use the same partial implementation on Windows and UNIX
 * to save code.  This implementation only passes first block
 * of data if it is waveform audio, strictly we should decode
 * all blocks we understand and just ignore those we don't.
 * It's not that hard to do properly - just want to get 
 * something up and running for the time being.
 */

typedef struct {
        u_int32 ckId; /* First four characters of chunk type e.g RIFF, WAVE, LIST */
        u_int32 ckSize;
} riff_chunk;

typedef struct {
        riff_chunk rc;
        u_int32    type;
} riff_chunk_hdr;

/* Note PCM_FORMAT_SIZE is 18 bytes as it has no extra information
 * cbExtra (below) is not included.  We only record 16-bit pcm
 */

#define PCM_FORMAT_SIZE 18

typedef struct {
        u_int16 wFormatTag;
        u_int16 wChannels;
        u_int32 dwSamplesPerSec;
        u_int32 dwAvgBytesPerSec;
        u_int16 wBlockAlign;
        u_int16 wBitsPerSample;
        u_int16 cbExtra;
} wave_format; /* Same as WAVEFORMATEX */

typedef struct {
        wave_format  wf;
        int          cbRemain; /* Number of bytes read    */
        int          cbUsed;   /* Number of bytes written */
} riff_state;

#define MS_AUDIO_FILE_ENCODING_PCM  (0x0001)
#define MS_AUDIO_FILE_ENCODING_ULAW (0x0007)
#define MS_AUDIO_FILE_ENCODING_ALAW (0x0006)
/* In the spec IBM u/alaw are 0x0102/0x0101 but this seems to be outdated. */

#define btoll(x) (((x) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | ((x) << 24))
#define btols(x) (((x) >> 8) | ((x&0xff) << 8))

#ifndef WIN32
static u_int32
MAKEFOURCC(char a, char b, char c, char d)
{
        u_int32 r;
        if (htons(1) == 1) {
		r = (((u_int32)(char)(a) <<24 )| ((u_int32)(char)(b) << 16) |
                     ((u_int32)(char)(c) << 8) | (u_int32)(char)(d));
        } else {
                r = (((u_int32)(char)(d) <<24 )| ((u_int32)(char)(c) << 16) |
                     ((u_int32)(char)(b) << 8) | (u_int32)(char)(a));
        }
        return r;
}
#endif /* WIN32 */

static void
wave_fix_hdr(wave_format *wf)
{
        /* If we are a big endian machine convert from little endian */
        if (htonl(1) == 1) {
                wf->wFormatTag       = (u_int16)btols(wf->wFormatTag);
                wf->wChannels        = (u_int16)btols(wf->wChannels);
                wf->dwSamplesPerSec  = btoll(wf->dwSamplesPerSec);
                wf->dwAvgBytesPerSec = btoll(wf->dwAvgBytesPerSec);
                wf->wBlockAlign      = (u_int16)btols(wf->wBlockAlign);
                wf->wBitsPerSample   = (u_int16)btols(wf->wBitsPerSample);
                wf->cbExtra          = (u_int16)btols(wf->cbExtra);
        }
}

static void
riff_fix_chunk_hdr(riff_chunk *rc)
{
        if (htonl(1) == 1) {
                rc->ckSize = btoll(rc->ckSize);
        }
}

static u_int32
riff_proceed_to_chunk(FILE *fp, char *id)
{
        riff_chunk rc;
        u_int32 ckId;

        ckId = MAKEFOURCC(id[0],id[1],id[2],id[3]);
        
        while(fread(&rc, sizeof(rc), 1, fp)) {
                riff_fix_chunk_hdr(&rc);
                if (rc.ckId == ckId) {
                        return rc.ckSize;
                }
                fseek(fp, rc.ckSize, SEEK_CUR);
        }
        
        return 0;
}

int
riff_read_hdr(FILE *fp, char **state)
{
        riff_chunk_hdr rch;
        wave_format wf;
        u_int32 chunk_size;

        riff_state *rs;

        if (!fread(&rch, 1, sizeof(rch), fp)) {
                debug_msg("Could read RIFF header");
                return FALSE;
        }
        riff_fix_chunk_hdr(&rch.rc);  
        debug_msg("Header chunk size (%d)\n", rch.rc.ckSize);

        if (MAKEFOURCC('R','I','F','F') != rch.rc.ckId ||
            MAKEFOURCC('W','A','V','E') != rch.type) {
                u_int32 riff = MAKEFOURCC('R','I','F','F');
                u_int32 wave = MAKEFOURCC('W','A','V','E');

                debug_msg("Riff 0x%08x 0x%08x\n", riff, rch.rc.ckId);
                debug_msg("Wave 0x%08x 0x%08x\n", wave, rch.type);
                debug_msg("Not WAVE file\n");
                return FALSE;
        }

        chunk_size = riff_proceed_to_chunk(fp, "fmt ");
        if (!chunk_size) {
                debug_msg("Format chunk not found\n");
                return FALSE;
        }
        debug_msg("Fmt chunk size (%d)\n", chunk_size);

        memset(&wf,0,sizeof(wf));
        if (chunk_size > sizeof(wave_format)||
            !fread(&wf,  1, chunk_size, fp)) {
                /* the formats we are interested in carry no extra information */
                debug_msg("Wave format too big (%d).\n", chunk_size);
                return FALSE;
        }
        
        wave_fix_hdr(&wf);

        switch(wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ULAW:
                debug_msg("ulaw\n");
                break;
        case MS_AUDIO_FILE_ENCODING_ALAW:
                debug_msg("alaw\n");
                break;
        case MS_AUDIO_FILE_ENCODING_PCM:
                if (wf.wBitsPerSample != 16) {
                        debug_msg("%d bits per sample not supported.\n", wf.wBitsPerSample);
                        return FALSE;
                }
                debug_msg("l16\n");
                break;
        default:
                /* We could be really flash and open an acm stream and convert any
                 * windows audio file data, but that would be too much ;-)
                 */
                debug_msg("Format (%4x) not supported.\n", wf.wFormatTag);
                return FALSE;
        }

        debug_msg("Channels (%d) SamplesPerSec (%d) BPS (%d) Align (%d) bps (%d)\n",
                  wf.wChannels,
                  wf.dwSamplesPerSec,
                  wf.dwAvgBytesPerSec,
                  wf.wBlockAlign,
                  wf.wBitsPerSample);
        
        chunk_size = riff_proceed_to_chunk(fp, "data");
        if (!chunk_size) {
                debug_msg("No data ?\n");
                return FALSE;
        }
        
        rs = (riff_state*)xmalloc(sizeof(riff_state));
        if (rs) {
                rs->cbRemain = chunk_size;
                memcpy(&rs->wf, &wf, sizeof(wf));
                *state = (char*)rs;
                return TRUE;
        }

        return FALSE;
}

int /* Returns the number of samples read */
riff_read_audio(FILE *pf, char* state, sample *buf, int samples)
{
        riff_state *rs;
        int unit_sz, samples_read, i;
        u_char *law;
        sample *bp;

        rs = (riff_state*)state;

        switch(rs->wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ALAW:
        case MS_AUDIO_FILE_ENCODING_ULAW:
                unit_sz = 1;
                break;
        case MS_AUDIO_FILE_ENCODING_PCM: /* just linear 16 */
                unit_sz = 2;
                break;
        default:
                return 0;
        }
        
        samples_read = fread(buf, unit_sz, samples, pf);

        switch(rs->wf.wFormatTag) {
        case MS_AUDIO_FILE_ENCODING_ALAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = a2s(*law--);
                }
                break;
        case MS_AUDIO_FILE_ENCODING_ULAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = u2s(*law--);
                }
                break;
        case MS_AUDIO_FILE_ENCODING_PCM:
                if (htons(1) == 1) {
                        for(i = 0; i < samples_read; i++) {
                                buf[i] = (u_int16)btols((u_char)buf[i]);
                        }
                }
                break;
        }

        rs->cbRemain -= samples * unit_sz;

        /* Make sure we don't write any other data from riff file out 
         *(i.e. copyright) */
        if (rs->cbRemain > 0) {
                return samples_read; 
        } else {
                return 0;
        }
}

int
riff_write_hdr(FILE *fp, char **state, sndfile_fmt_e encoding, int freq, int channels)
{
        riff_state *rs;
        int         hdr_len;

        rs = (riff_state*)xmalloc(sizeof(riff_state));
        if (!rs) {
                return FALSE;
        }
        *state = (char*)rs;

        UNUSED(encoding); /* Ignore encoding for the time being */

        rs->cbUsed = 0;
        rs->wf.wFormatTag       = MS_AUDIO_FILE_ENCODING_PCM;
        rs->wf.wChannels        = (u_int16)channels;
        rs->wf.dwSamplesPerSec  = freq;
        rs->wf.wBitsPerSample   = 16;
        rs->wf.dwAvgBytesPerSec = freq * channels * sizeof(sample);
        rs->wf.wBlockAlign      = (u_int16)(channels * sizeof(sample));
       
        hdr_len = sizeof(riff_chunk_hdr) /* RIFF header */
                + 2 * sizeof(riff_chunk) /* Sub-block ("data") */
                + PCM_FORMAT_SIZE;   /* Wave format description (PCM ONLY) */
        
        /* Roll forward leaving space for header. We don't write it here because 
         * we need to know the amount of audio written before we can write header.
         */
        if (fseek(fp, hdr_len, SEEK_SET)) {
                debug_msg("Seek Failed.\n");
                return FALSE;
        }

        return TRUE;
}

int
riff_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i;
        riff_state *rs = (riff_state*)state;
        
        if (ntohs(1) == 1) { 
                /* If we are on a big endian machine fix samples before
                 * writing them out.  
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = (u_int16)btols((u_int16)buf[i]);
                }
        }

        fwrite(buf, sizeof(sample), samples, fp);
        rs->cbUsed += sizeof(sample) * samples;

        if (ntohs(1) == 1) {
                /* This audio still has to be played through audio device so fix 
                 * ordering.
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = (u_int16)btols((u_int16)buf[i]);
                }
        }
        
        return TRUE;
}

int
riff_write_end(FILE *fp, char *state)
{
        riff_chunk_hdr rch;
        riff_chunk fmt, data;
        riff_state *rs = (riff_state*)state;

        /* Back to the beginning */
        if (fseek(fp, 0, SEEK_SET)) {
                debug_msg("Rewind failed\n");
                return FALSE;
        }

        rch.rc.ckId   = MAKEFOURCC('R','I','F','F');
        /* Size includes FOURCC(WAVE) and size of all sub-components. */
        rch.rc.ckSize = 4 + sizeof(fmt) + sizeof(data) + PCM_FORMAT_SIZE + rs->cbUsed;
        rch.type      = MAKEFOURCC('W','A','V','E');
        riff_fix_chunk_hdr(&rch.rc);
        fwrite(&rch, sizeof(rch), 1, fp);

        /* Write format header */
        fmt.ckId   = MAKEFOURCC('f','m','t',' ');
        fmt.ckSize = PCM_FORMAT_SIZE;
        riff_fix_chunk_hdr(&fmt);
        fwrite(&fmt, sizeof(fmt), 1, fp);

        /* Write wave format - cannot use wave fix hdr as constituents
         * must be written out in correct order */
        if (htonl(1) == 1) {
                wave_fix_hdr(&rs->wf);
                fwrite(&rs->wf.wFormatTag,       sizeof(u_int16), 1, fp);
                fwrite(&rs->wf.wChannels,        sizeof(u_int16), 1, fp);
                fwrite(&rs->wf.dwSamplesPerSec,  sizeof(u_int32), 1, fp);
                fwrite(&rs->wf.dwAvgBytesPerSec, sizeof(u_int32), 1, fp);
                fwrite(&rs->wf.wBlockAlign,      sizeof(u_int16), 1, fp);
                fwrite(&rs->wf.wBitsPerSample,   sizeof(u_int16), 1, fp);
                fwrite(&rs->wf.cbExtra,          sizeof(u_int16), 1, fp);
        } else {
                fwrite(&rs->wf, PCM_FORMAT_SIZE, 1, fp);
        }
        
        /* Write data header */
        data.ckId   = MAKEFOURCC('d','a','t','a');
        data.ckSize = rs->cbUsed;
        riff_fix_chunk_hdr(&data);
        fwrite(&data, sizeof(data), 1, fp);

        return TRUE;
}

int
riff_free_state(char **state)
{
        riff_state *rs;
        
        if (state && *state) {
                rs = (riff_state*)*state;
                debug_msg("Used (%d) Remain (%d)\n", rs->cbUsed, rs->cbRemain);
                xfree(rs);
                *state = NULL;
        }
        return TRUE;
}

u_int16
riff_get_channels(char *state)
{
        wave_format *wf = (wave_format*)state;
        return wf->wChannels;
}

u_int16
riff_get_rate(char *state)
{
        wave_format *wf = (wave_format*)state;
        return (u_int16)wf->dwSamplesPerSec;
}

