/*
 * FILE:    sndfile.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted, for non-commercial use only, provided
 * that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>

#include "config_unix.h"
#include "config_win32.h"
#include "rat_types.h"
#include "util.h"
#include "codec_g711.h"
#include "sndfile.h"

/* This code uses the same function pointer arrangement as other files in RAT.
 * It is basically just a cumbersome way of implementing virtual functions
 */

/* NeXT/Sun .au file handling ****************************************************
   Portions of the definitions come from the Sun's header file audio_filehdr.h ***/

typedef struct {
	u_int32		magic;		/* magic number */
	u_int32		hdr_size;	/* size of this header */
	u_int32		data_size;	/* length of data (optional) */
	u_int32		encoding;	/* data encoding format */
	u_int32		sample_rate;	/* samples per second */
	u_int32		channels;	/* number of interleaved channels */
} sun_audio_filehdr;

/* Define the magic number */
#define	SUN_AUDIO_FILE_MAGIC		((u_int32)0x2e736e64)

/* Define the encoding fields */
/* We only support trivial conversions */
#define	SUN_AUDIO_FILE_ENCODING_ULAW	        (1u)	/* u-law PCM         */
#define	SUN_AUDIO_FILE_ENCODING_LINEAR_16	(3u)	/* 16-bit linear PCM */
#define SUN_AUDIO_FILE_ENCODING_ALAW	       (27u) 	/* A-law PCM         */

static void 
sun_ntoh_hdr(sun_audio_filehdr *saf)
{
        u_int32 *pi;
        int j;
        pi = (u_int32*)saf;
        for(j = 0; j < 6; j++) {
                pi[j] = htonl(pi[j]);
        }
}

static int /* Returns true if can decode header */
sun_read_hdr(FILE *pf, char **state)
{
        sun_audio_filehdr afh;

        /* Attempt to read header */
        if (!fread(&afh, sizeof(sun_audio_filehdr), 1, pf)) {
                /* file must be too small! */
                debug_msg("Could not read header\n");
                return FALSE;
        }
        
        /* Fix byte ordering */
        sun_ntoh_hdr(&afh);
        
        /* Test if supported */
        if ((afh.magic == SUN_AUDIO_FILE_MAGIC) &&
            (afh.encoding == SUN_AUDIO_FILE_ENCODING_ULAW ||
            afh.encoding == SUN_AUDIO_FILE_ENCODING_LINEAR_16 ||     
            afh.encoding == SUN_AUDIO_FILE_ENCODING_ALAW)) {
                /* File ok copy header */
                sun_audio_filehdr *pafh = (sun_audio_filehdr *)xmalloc(sizeof(sun_audio_filehdr));
                memcpy(pafh,&afh, sizeof(sun_audio_filehdr));
                
                /* For .au's state is just file header */
                *state = (char*)pafh;
                
                /* Roll past header */
                fseek(pf, afh.hdr_size, SEEK_SET);
                
                return TRUE;
        }
        return FALSE;
}

static int /* Returns the number of samples read */
sun_read_audio(FILE *pf, char* state, sample *buf, int samples)
{
        sun_audio_filehdr *afh;
        int unit_sz, samples_read, i;
        u_char *law;
        sample *bp;

        afh = (sun_audio_filehdr*)state;

        switch(afh->encoding) {
        case SUN_AUDIO_FILE_ENCODING_ALAW:
        case SUN_AUDIO_FILE_ENCODING_ULAW:
                unit_sz = 1;
                break;
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
                unit_sz = 2;
                break;
        default:
                return 0;
        }
        
        samples_read = fread(buf, unit_sz, samples, pf);

        switch(afh->encoding) {
        case SUN_AUDIO_FILE_ENCODING_ALAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = a2s(*law--);
                        
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_ULAW:
                law = ((u_char*)buf) + samples_read - 1;
                bp  = buf + samples_read - 1;
                for(i = 0; i < samples_read; i++) {
                        *bp-- = u2s(*law--);
                        
                }
                break;
        case SUN_AUDIO_FILE_ENCODING_LINEAR_16:
                for(i = 0; i < samples_read; i++) {
                        buf[i] = htons(buf[i]);
                }
                break;
        }
        return samples_read;
}

static int
sun_write_hdr(FILE *fp, char **state, int freq, int channels)
{
        sun_audio_filehdr saf;

        UNUSED(state);

        saf.magic     = SUN_AUDIO_FILE_MAGIC;
        saf.hdr_size  = sizeof(sun_audio_filehdr);
        saf.data_size = 0; /* Optional - we could fill this in when the file closes */
        saf.encoding  = SUN_AUDIO_FILE_ENCODING_LINEAR_16;
        saf.sample_rate = freq;
        saf.channels  = channels;
        
        sun_ntoh_hdr(&saf);
        
        if (fwrite(&saf, sizeof(sun_audio_filehdr), 1, fp)) {
                return TRUE;
        } else {
                return FALSE;
        }
}

static int
sun_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i;

        UNUSED(state);

        if (ntohs(1) != 1) {
                /* If we are on a little endian machine fix samples before
                 * writing them out.
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = ntohs(buf[i]);
                }
        }

        fwrite(buf, sizeof(sample), samples, fp);

        if (ntohs(1) != 1) {
                /* This audio still has to be played through audio device so fix 
                 * ordering.
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = ntohs(buf[i]);
                }
        }

        return TRUE;
}

static int
sun_free_state(char **state)
{
        if (state && *state) {
                xfree(*state);
                *state = NULL;
        }
        return TRUE;
}

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

#ifndef WIN32
#define MAKEFOURCC(a, b, c, d) \
		(((u_int32)(char)(a) <<24 )| ((u_int32)(char)(b) << 16) | \
		((u_int32)(char)(c) << 8) | (u_int32)(char)(d))
#endif

#define btoll(x) (((x) >> 24) | (((x)&0x00ff0000) >> 8) | (((x)&0x0000ff00) << 8) | ((x) << 24))
#define btols(x) (((x) >> 8) | ((x) << 8))

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

static int
riff_read_hdr(FILE *fp, char **state)
{
        riff_chunk_hdr rch;
        wave_format wf;
        int chunk_size;

        riff_state *rs;

        if (!fread(&rch, 1, sizeof(rch), fp)) {
                debug_msg("Could read RIFF header");
                return FALSE;
        }
        riff_fix_chunk_hdr(&rch.rc);  
        debug_msg("Header chunk size (%d)\n", rch.rc.ckSize);

        if (MAKEFOURCC('R','I','F','F') != rch.rc.ckId ||
            MAKEFOURCC('W','A','V','E') != rch.type) {
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

static int /* Returns the number of samples read */
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
                                buf[i] = (u_int16)btols(buf[i]);
                        }
                }
                break;
        }

        if (samples_read != samples) {
                memset(buf+samples_read, 0, sizeof(sample) * (samples - samples_read));
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

static int
riff_write_hdr(FILE *fp, char **state, int freq, int channels)
{
        riff_state *rs;
        int         hdr_len;

        rs = (riff_state*)xmalloc(sizeof(riff_state));
        if (!rs) {
                return FALSE;
        }
        *state = (char*)rs;

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

static int
riff_write_audio(FILE *fp, char *state, sample *buf, int samples)
{
        int i;
        riff_state *rs = (riff_state*)state;

        if (ntohs(1) == 1) {
                /* If we are on a big endian machine fix samples before
                 * writing them out.  
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = (u_int16)btols(buf[i]);
                }
        }

        fwrite(buf, sizeof(sample), samples, fp);
        rs->cbUsed += sizeof(sample) * samples;

        if (ntohs(1) == 1) {
                /* This audio still has to be played through audio device so fix 
                 * ordering.
                 */
                for(i = 0; i < samples; i++) {
                        buf[i] = (u_int16)btols(buf[i]);
                }
        }

        return TRUE;
}

static int
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

static int
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

/* Generic file handling *********************************************************/

typedef int (*pf_open_hdr)    (FILE *, char **state);
typedef int (*pf_read_audio)  (FILE *, char * state, sample *buf, int samples);
typedef int (*pf_write_hdr)   (FILE *, char **state, int freq,    int channels);
typedef int (*pf_write_audio) (FILE *, char * state, sample *buf, int samples);
typedef int (*pf_write_end)   (FILE *, char *state);
typedef int (*pf_free_state)  (char **state);

typedef struct s_file_handler {
        char name[12];           /* Sound handler name        */
        char extension[12];      /* Recognized file extension */
        pf_open_hdr    open_hdr;
        pf_read_audio  read_audio;
        pf_write_hdr   write_hdr;
        pf_write_audio write_audio;
        pf_write_end   write_end;
        pf_free_state  free_state;
} snd_file_handler_t;

/* Sound file handlers */

#define NUM_SND_HANDLERS 2
static snd_file_handler_t snd_handlers[] = {
        {"NeXT/Sun", 
         "au", 
         sun_read_hdr, 
         sun_read_audio,
         sun_write_hdr,
         sun_write_audio,
         NULL, /* No post write handling required */
         sun_free_state},
        {"MS RIFF",
         "wav",
         riff_read_hdr,
         riff_read_audio,
         riff_write_hdr,
         riff_write_audio,
         riff_write_end,
         riff_free_state
        }
};

#define SND_ACTION_PLAYING    1
#define SND_ACTION_RECORDING  2
#define SND_ACTION_PAUSED     4

typedef struct s_snd_file {
        FILE *fp;
        char *state;
        snd_file_handler_t *sfh;
        u_int32 action; /* Playing, recording, paused */
} snd_file_t;

int  
snd_read_open (snd_file_t **snd_file, char *path) 
{
        snd_file_t *s;
        FILE       *fp;
        int         i;

        if (*snd_file) {
                debug_msg("File not closed before opening\n");
                snd_read_close(snd_file);
        }

        fp = fopen(path, "rb");
        if (!fp) {
                debug_msg("Could not open %s\n",path);
                return FALSE;
        }
        
        s     = (snd_file_t*)xmalloc(sizeof(snd_file_t));
        if (!s) return FALSE;
        
        s->fp = fp;
        
        for(i = 0; i < NUM_SND_HANDLERS; i++) {
                if (snd_handlers[i].open_hdr(fp,&s->state)) {
                        s->sfh    = snd_handlers + i;
                        s->action = SND_ACTION_PLAYING;
                        *snd_file = s;
                        return TRUE;
                }
                rewind(fp);
        }
        
        xfree(s);
        
        return FALSE;
}

int  
snd_read_close(snd_file_t **sf)
{
        snd_file_handler_t *sfh = (*sf)->sfh;

        /* Release state */
        sfh->free_state(&(*sf)->state);

        /* Close file */
        fclose((*sf)->fp);

        /* Release memory */
        xfree(*sf);
        *sf = NULL;

        return TRUE;
}

int
snd_read_audio(snd_file_t **sf, sample *buf, u_int16 samples)
{
        snd_file_handler_t *sfh;
        int samples_read;

        if ((*sf)->action & SND_ACTION_PAUSED) return FALSE;

        sfh = (*sf)->sfh;
        
        samples_read = sfh->read_audio((*sf)->fp, (*sf)->state, buf, samples);

        if (samples_read != samples) {
                /* Looks like the end of the line */
                snd_read_close(sf);
        }

        return TRUE;
}

int
snd_write_open (snd_file_t **sf, char *path, char *extension, u_int16 freq, u_int16 channels)
{
        snd_file_t *s;
        FILE       *fp;
        int         i;

        if (*sf) {
                debug_msg("File not closed before opening\n");
                snd_write_close(sf);
        }

        fp = fopen(path, "wb");
        if (!fp) {
                debug_msg("Could not open %s\n",path);
                return FALSE;
        }
        
        s     = (snd_file_t*)xmalloc(sizeof(snd_file_t));
        if (!s) return FALSE;

        s->fp = fp;
        
        for(i = 0; i < NUM_SND_HANDLERS; i++) {
                if (!strcasecmp(extension, snd_handlers[i].extension)) {
                        s->sfh    = snd_handlers + i;
                        s->action = SND_ACTION_RECORDING;
                        s->sfh->write_hdr(fp, &s->state, freq, channels);
                        *sf = s;
                        return TRUE;
                }
        }
        
        xfree(s);
        
        return FALSE;
}

int  
snd_write_close(snd_file_t **pps)
{
        snd_file_t *ps = *pps;

        if (ps->sfh->write_end) ps->sfh->write_end(ps->fp, ps->state);
        
        ps->sfh->free_state(&ps->state);
        
        fclose(ps->fp);
        
        xfree(ps);
        *pps = NULL;
        
        return TRUE;
}

int  
snd_write_audio(snd_file_t **pps, sample *buf, u_int16 buf_len)
{
        snd_file_t *ps = *pps;
        int success;
        
        success = ps->sfh->write_audio(ps->fp, ps->state, buf, buf_len);
        if (!success) {
                debug_msg("Closing file\n");
                snd_write_close(pps);        
                return FALSE;
        }

        return TRUE;
}

int 
snd_pause(snd_file_t  *sf)
{
        sf->action = sf->action | SND_ACTION_PAUSED;
        return TRUE;
}

int
snd_resume(snd_file_t *sf)
{
        sf->action = sf->action & ~SND_ACTION_PAUSED;
        return TRUE;
}

#ifdef TEST_RIFF

char 
*get_extension(char *path)
{
        if (path) {
                char *ext = path + strlen(path) - 1;
                while(ext > path) {
                        if (*ext == '.') return ext + 1;
                        ext--;
                }
        }
        return NULL;
}

int 
main(int argc, char*argv[])
{
        snd_file_t *ssrc, *sdst;
        sample buf[160];
        
        ssrc = sdst = NULL;

        if (argc == 3) {
                codec_g711_init();
                snd_read_open(&ssrc, argv[1]);
                snd_write_open(&sdst, argv[2], get_extension(argv[2]), 8000, 1);
                while(ssrc && sdst) {
                        snd_read_audio(&ssrc, buf, 160);
                        snd_write_audio(&sdst, buf, 160);
                }
                if (sdst) {
                        snd_write_close(&sdst);
                }
        }
        
        return TRUE;
}

#endif
