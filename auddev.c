/*
 * FILE:     auddev.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson 
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "debug.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev.h"

typedef struct {
        char name[AUDIO_INTERFACE_NAME_LEN];

        int  (*audio_if_init)(void);               /* Test and initialize audio interface (OPTIONAL) */
        int  (*audio_if_free)(void);               /* Free audio interface (OPTIONAL)                */

        int  (*audio_if_open)(int, audio_format *ifmt, audio_format *ofmt); /* Open device with formats */
        void (*audio_if_close)(int);               /* Close device (REQUIRED) */
        void (*audio_if_drain)(int);               /* Drain device (REQUIRED) */
        int  (*audio_if_duplex)(int);              /* Device full duplex (REQUIRED) */

        int  (*audio_if_read) (int, u_char*, int);   /* Read samples (REQUIRED)  */
        int  (*audio_if_write)(int, u_char*, int);   /* Write samples (REQUIRED) */
        void (*audio_if_non_block)(int);           /* Set device non-blocking (REQUIRED) */
        void (*audio_if_block)(int);               /* Set device blocking (REQUIRED)     */

        void (*audio_if_set_gain)(int,int);        /* Set input gain (REQUIRED)  */
        int  (*audio_if_get_gain)(int);            /* Get input gain (REQUIRED)  */
        void (*audio_if_set_volume)(int,int);      /* Set output gain (REQUIRED) */
        int  (*audio_if_get_volume)(int);          /* Get output gain (REQUIRED) */
        void (*audio_if_loopback)(int, int);       /* Enable hardware loopback (OPTIONAL) */

        void (*audio_if_set_oport)(int, int);      /* Set output port (REQUIRED)        */
        int  (*audio_if_get_oport)(int);           /* Get output port (REQUIRED)        */
        int  (*audio_if_next_oport)(int);          /* Go to next output port (REQUIRED) */
        void (*audio_if_set_iport)(int, int);      /* Set input port (REQUIRED)         */
        int  (*audio_if_get_iport)(int);           /* Get input port (REQUIRED)         */
        int  (*audio_if_next_iport)(int);          /* Go to next itput port (REQUIRED)  */
        int  (*audio_if_is_ready)(int);            /* Poll for audio availability (REQUIRED)   */
        void (*audio_if_wait_for)(int, int);       /* Wait until audio is available (REQUIRED) */
        int  (*audio_if_format_supported)(int, audio_format *);
} audio_if_t;

#define AUDIO_MAX_INTERFACES 5
/* These store available audio interfaces */
static audio_if_t audio_interfaces[AUDIO_MAX_INTERFACES];
static int num_interfaces = 0;

/* These are requested device formats.  */
#define AUDDEV_REQ_IFMT      0
#define AUDDEV_REQ_OFMT      1

/* These are actual device formats that are transparently converted 
 * into the required ones during reads and writes.  */
#define AUDDEV_ACT_IFMT      2
#define AUDDEV_ACT_OFMT      3

#define AUDDEV_NUM_FORMATS   4

static audio_format* fmts[AUDIO_MAX_INTERFACES][AUDDEV_NUM_FORMATS];
static sample      * convert_buf[AUDIO_MAX_INTERFACES]; /* used if conversions used */

/* Active interfaces is a table of entries pointing to entries in
 * audio interfaces table.  Audio open returns index to these */
static audio_if_t *active_interfaces[AUDIO_MAX_INTERFACES];
static int num_active_interfaces = 0; 

/* This is the index of the next audio interface that audio_open */
static int sel_if = 0;

/* We map indexes outside range for file descriptors so people don't attempt
 * to circumvent audio interface.  If something is missing it should be added
 * to the interfaces...
 */

#define AIF_IDX_TO_MAGIC(x) ((x) | 0xff00)
#define AIF_MAGIC_TO_IDX(x) ((x) & 0x00ff)

__inline static audio_if_t *
audio_get_active_interface(int idx)
{
        assert(idx < num_interfaces);
        assert(active_interfaces[sel_if] != NULL);

        return active_interfaces[idx];
}

int
audio_get_number_of_interfaces()
{
        return num_interfaces;
}

char*
audio_get_interface_name(int idx)
{
        if (idx < num_interfaces) {
                return audio_interfaces[idx].name;
        }
        return NULL;
}

void
audio_set_interface(int idx)
{
        if (idx < num_interfaces) {
                sel_if = idx;
        }
}

int
audio_get_null_interface()
{
        /* Null audio interface is always the last */
        return num_interfaces - 1;
}

int 
audio_get_interface()
{
        return sel_if;
}

/* Audio Interface actions ***************************************************/

audio_desc_t
audio_open(audio_format *ifmt, audio_format *ofmt)
{
        audio_if_t *aif;
        audio_format format;
        int success;
        int r; 

        assert(ifmt != NULL);
        assert(ofmt != NULL);

        if ((ofmt->sample_rate != ifmt->sample_rate) ||
            (ofmt->encoding == DEV_S8 || ifmt->encoding == DEV_S8)) {
                /* Fail on things we don't support */
                debug_msg("Not supported\n");
                return 0;
        }

        aif = &audio_interfaces[sel_if];
        assert(aif->audio_if_open);

        if (audio_format_get_common(ifmt, ofmt, &format) == FALSE) {
                /* Input and output formats incompatible */
                return 0;
        }

        fmts[sel_if][AUDDEV_ACT_IFMT] = audio_format_dup(&format);
        fmts[sel_if][AUDDEV_ACT_OFMT] = audio_format_dup(&format);

        /* Formats can get changed in audio_if_open, but only sample
         * type, not the number of channels or freq 
         */
        success = aif->audio_if_open(sel_if, 
                                     fmts[sel_if][AUDDEV_ACT_IFMT], 
                                     fmts[sel_if][AUDDEV_ACT_OFMT]);

        if (success) {
                if ((fmts[sel_if][AUDDEV_ACT_IFMT]->sample_rate != format.sample_rate) ||
                    (fmts[sel_if][AUDDEV_ACT_OFMT]->sample_rate != format.sample_rate) ||
                    (fmts[sel_if][AUDDEV_ACT_IFMT]->channels    != format.channels)    ||
                    (fmts[sel_if][AUDDEV_ACT_OFMT]->channels    != format.channels)) {
                        debug_msg("Device changed sample rate or channels - unsupported functionality.\n");
                        aif->audio_if_close(sel_if);
                        audio_format_free(&fmts[sel_if][AUDDEV_ACT_IFMT]);
                        audio_format_free(&fmts[sel_if][AUDDEV_ACT_OFMT]);
                }

                if (!aif->audio_if_duplex(AIF_IDX_TO_MAGIC(sel_if))) {
                        printf("RAT v3.2.0 and later require a full duplex audio device, but \n");
                        printf("your %s only supports half-duplex operation. Sorry.\n", aif->name);
                        aif->audio_if_close(AIF_IDX_TO_MAGIC(sel_if));
                        return 0;
                }
                
                active_interfaces[sel_if] = &audio_interfaces[sel_if];
                /* If we are going to need conversion between requested and 
                 * actual device formats store requested formats */
                if (!audio_format_match(ifmt, fmts[sel_if][AUDDEV_ACT_IFMT])) {
                        fmts[sel_if][AUDDEV_REQ_IFMT] = audio_format_dup(ifmt);
#ifdef DEBUG
                        {
                                char s[50];
                                audio_format_name(fmts[sel_if][AUDDEV_REQ_IFMT], s, 50);
                                debug_msg("Requested Input: %s\n", s);
                                audio_format_name(fmts[sel_if][AUDDEV_ACT_IFMT], s, 50);
                                debug_msg("Actual Input:    %s\n", s);
                        }
#endif
                }

                if (!audio_format_match(ofmt, fmts[sel_if][AUDDEV_ACT_OFMT])) {
                        fmts[sel_if][AUDDEV_REQ_OFMT] = audio_format_dup(ofmt);
#ifdef DEBUG
                        {
                                char s[50];
                                audio_format_name(fmts[sel_if][AUDDEV_REQ_OFMT], s, 50);
                                debug_msg("Requested Output: %s\n", s);
                                audio_format_name(fmts[sel_if][AUDDEV_ACT_OFMT], s, 50);
                                debug_msg("Actual Output:    %s\n", s);
                        }
#endif
                }

                if (fmts[sel_if][AUDDEV_REQ_IFMT] || fmts[sel_if][AUDDEV_REQ_OFMT]) {
                        convert_buf[sel_if] = (sample*)xmalloc(DEVICE_REC_BUF); /* is this in samples or bytes ? */
                }

                r = AIF_IDX_TO_MAGIC(sel_if);
                num_active_interfaces++;
                return r;
        }

        audio_format_free(&fmts[sel_if][AUDDEV_ACT_IFMT]);
        audio_format_free(&fmts[sel_if][AUDDEV_ACT_OFMT]);

        return 0;
}

void
audio_close(audio_desc_t ad)
{
        int i, j;
        audio_if_t *aif;

        ad = AIF_MAGIC_TO_IDX(ad);

        aif = audio_get_active_interface(ad);
        aif->audio_if_close(ad);

        i = j = 0;
        for(i = 0; i < num_active_interfaces; i++) {
                if (i != ad) {
                        active_interfaces[j] = active_interfaces[i];
                        j++;
                }
        }

        for(i = 0; i < AUDDEV_NUM_FORMATS; i++) {
                if (fmts[ad][i] != NULL) audio_format_free(&fmts[ad][i]);
        }

        if (convert_buf[ad]) {
                xfree(convert_buf[ad]);
                convert_buf[ad] = NULL;
        }

        num_active_interfaces--;
}

const audio_format*
audio_get_ifmt(audio_desc_t ad)
{
        ad = AIF_MAGIC_TO_IDX(ad);

        if (fmts[ad][AUDDEV_REQ_IFMT]) {
                return fmts[ad][AUDDEV_REQ_IFMT];
        }

        return fmts[ad][AUDDEV_ACT_IFMT];
}

const audio_format*
audio_get_ofmt(audio_desc_t ad)
{
        ad = AIF_MAGIC_TO_IDX(ad);

        if (fmts[ad][AUDDEV_REQ_OFMT]) {
                return fmts[ad][AUDDEV_REQ_OFMT];
        }

        return fmts[ad][AUDDEV_ACT_OFMT];
}

void
audio_drain(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        aif->audio_if_drain(ad);
}

int
audio_duplex(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_duplex(ad);
}

int
audio_read(audio_desc_t ad, sample *buf, int samples)
{
        /* Samples is the number of samples to read * number of channels */
        
        audio_if_t *aif;
        int read_len;
        int sample_size;
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        xmemchk();

        if (fmts[ad][AUDDEV_REQ_IFMT] == NULL) {
                /* No conversion necessary as input format and real format are
                 * the same. [Input format only allocated if different from
                 * real format].
                 */
                sample_size = fmts[ad][AUDDEV_ACT_IFMT]->bits_per_sample / 8;
                read_len    = aif->audio_if_read(ad, (u_char*)buf, samples * sample_size);
        } else {
                assert(fmts[ad][AUDDEV_ACT_IFMT] != NULL);
                sample_size = fmts[ad][AUDDEV_ACT_IFMT]->bits_per_sample / 8;
                read_len    = aif->audio_if_read(ad, (u_char*)convert_buf[ad], samples * sample_size);
                read_len    = audio_format_buffer_convert(fmts[ad][AUDDEV_REQ_IFMT], 
                                                          (u_char*) convert_buf[ad], 
                                                          read_len, 
                                                          fmts[ad][AUDDEV_ACT_IFMT], 
                                                          (u_char*) buf,
                                                          DEVICE_REC_BUF);
                sample_size = fmts[ad][AUDDEV_REQ_IFMT]->bits_per_sample / 8;
        }
        xmemchk();
        return read_len / sample_size;
}

int
audio_write(audio_desc_t ad, sample *buf, int len)
{
        audio_if_t *aif;
        int write_len ,sample_size;

        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        
        xmemchk();

        if (fmts[ad][AUDDEV_REQ_OFMT] == NULL) {
                /* No conversion necessary as output format and real format are
                 * the same. [Output format only allocated if different from
                 * real format].
                 */
                sample_size = fmts[ad][AUDDEV_ACT_OFMT]->bits_per_sample / 8;
                write_len = aif->audio_if_write(ad, (u_char*)buf, len * sample_size);
        } else {
                write_len = audio_format_buffer_convert(fmts[ad][AUDDEV_REQ_OFMT],
                                                        (u_char*)buf,
                                                        len,
                                                        fmts[ad][AUDDEV_ACT_OFMT],
                                                        (u_char*) convert_buf[ad],
                                                        DEVICE_REC_BUF);
                aif->audio_if_write(ad, (u_char*)convert_buf[ad], write_len);
                sample_size = fmts[ad][AUDDEV_ACT_OFMT]->bits_per_sample / 8;
        }

        xmemchk();
        return write_len / sample_size;
}

void
audio_non_block(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_non_block(ad);
}

void
audio_block(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        
        aif->audio_if_block(ad);
}

void
audio_set_gain(audio_desc_t ad, int gain)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        aif->audio_if_set_gain(ad, gain);
}

int
audio_get_gain(audio_desc_t ad)
{
        audio_if_t *aif;
        int gain;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        
        gain = aif->audio_if_get_gain(ad);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        return gain;
}

void
audio_set_volume(audio_desc_t ad, int volume)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        assert(volume >= 0);
        assert(volume <= MAX_AMP);

        aif->audio_if_set_volume(ad, volume);
}

int
audio_get_volume(audio_desc_t ad)
{
        audio_if_t *aif;
        int volume;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        volume = aif->audio_if_get_volume(ad);
        assert(volume >= 0);
        assert(volume <= MAX_AMP);

        return volume;
}

void
audio_loopback(audio_desc_t ad, int gain)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        if (aif->audio_if_loopback) aif->audio_if_loopback(ad, gain);
}

void
audio_set_oport(audio_desc_t ad, int port)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_oport(ad, port);
}

int
audio_get_oport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_oport(ad));
}

int     
audio_next_oport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_next_oport(ad));
}

void
audio_set_iport(audio_desc_t ad, int port)
{
        audio_if_t *aif;
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_iport(ad, port);
}

int
audio_get_iport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_iport(ad));
}

int
audio_next_iport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_next_iport(ad));
}

int
audio_is_ready(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_is_ready(ad));
}

void
audio_wait_for(audio_desc_t ad, int delay_ms)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_wait_for(ad, delay_ms);
}

/* Code for adding/initialising/removing audio interfaces */

int
audio_device_supports(audio_desc_t ad, u_int16 rate, u_int16 channels)
{
        audio_if_t *aif;
 
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        if (rate % 8000 || channels > 2) {
                debug_msg("Invalid combo %d Hz %d channels\n", rate, channels);
                return FALSE;
        }

        if (aif->audio_if_format_supported) {
                audio_format tfmt;
                tfmt.encoding    = DEV_S16;
                tfmt.sample_rate = rate;
                tfmt.channels    = channels;
                return aif->audio_if_format_supported(ad, &tfmt);
        }

        debug_msg("Format support query function not implemented! Lying about supported formats.\n");

        return TRUE;
}

static int
audio_add_interface(audio_if_t *aif_new)
{
        if ((aif_new->audio_if_init == NULL || aif_new->audio_if_init()) &&
            num_interfaces < AUDIO_MAX_INTERFACES) {
                memcpy(audio_interfaces + num_interfaces, aif_new, sizeof(audio_if_t));
                debug_msg("Audio interface added %s\n", aif_new->name);
                num_interfaces++;
                return TRUE;
        }
        return FALSE;
}

#include "auddev_luigi.h"
#include "auddev_null.h"
#include "auddev_osprey.h"
#include "auddev_oss.h"
#include "auddev_pca.h"
#include "auddev_sparc.h"
#include "auddev_sgi.h"
#include "auddev_win32.h"

int
audio_init_interfaces()
{
        int i, n;

#ifdef IRIX
        {
                audio_if_t aif_sgi = {
                        "SGI Audio Device",
                        NULL, 
                        NULL, 
                        sgi_audio_open,
                        sgi_audio_close,
                        sgi_audio_drain,
                        sgi_audio_duplex,
                        sgi_audio_read,
                        sgi_audio_write,
                        sgi_audio_non_block,
                        sgi_audio_block,
                        sgi_audio_set_gain,
                        sgi_audio_get_gain,
                        sgi_audio_set_volume,
                        sgi_audio_get_volume,
                        sgi_audio_loopback,
                        sgi_audio_set_oport,
                        sgi_audio_get_oport,
                        sgi_audio_next_oport,
                        sgi_audio_set_iport,
                        sgi_audio_get_iport,
                        sgi_audio_next_iport,
                        sgi_audio_is_ready,
                        sgi_audio_wait_for,
                        NULL
                };
                audio_add_interface(&aif_sgi);
        }
#endif /* IRIX */

#ifdef Solaris
        {
                audio_if_t aif_sparc = {
                        "Sun Audio Device",
                        NULL, 
                        NULL, 
                        sparc_audio_open,
                        sparc_audio_close,
                        sparc_audio_drain,
                        sparc_audio_duplex,
                        sparc_audio_read,
                        sparc_audio_write,
                        sparc_audio_non_block,
                        sparc_audio_block,
                        sparc_audio_set_gain,
                        sparc_audio_get_gain,
                        sparc_audio_set_volume,
                        sparc_audio_get_volume,
                        sparc_audio_loopback,
                        sparc_audio_set_oport,
                        sparc_audio_get_oport,
                        sparc_audio_next_oport,
                        sparc_audio_set_iport,
                        sparc_audio_get_iport,
                        sparc_audio_next_iport,
                        sparc_audio_is_ready,
                        sparc_audio_wait_for,
                        NULL
                };
                audio_add_interface(&aif_sparc);
        }
#endif /* Solaris */

#ifdef HAVE_OSPREY
        {
                audio_if_t aif_osprey = {
                        "Osprey Audio Device",
                        osprey_audio_init, 
                        NULL, 
                        osprey_audio_open,
                        osprey_audio_close,
                        osprey_audio_drain,
                        osprey_audio_duplex,
                        osprey_audio_read,
                        osprey_audio_write,
                        osprey_audio_non_block,
                        osprey_audio_block,
                        osprey_audio_set_gain,
                        osprey_audio_get_gain,
                        osprey_audio_set_volume,
                        osprey_audio_get_volume,
                        osprey_audio_loopback,
                        osprey_audio_set_oport,
                        osprey_audio_get_oport,
                        osprey_audio_next_oport,
                        osprey_audio_set_iport,
                        osprey_audio_get_iport,
                        osprey_audio_next_iport,
                        osprey_audio_is_ready,
                        osprey_audio_wait_for,
                        NULL
                };
                audio_add_interface(&aif_osprey);
        }
#endif /* HAVE_OSPREY */

#if defined(Linux)||defined(OSS)
        oss_audio_query_devices();
        n = oss_get_device_count();

        for(i = 0; i < n; i++) {
                audio_if_t aif_oss = {
                        "OSS Audio Device",
                        NULL, 
                        NULL, 
                        oss_audio_open,
                        oss_audio_close,
                        oss_audio_drain,
                        oss_audio_duplex,
                        oss_audio_read,
                        oss_audio_write,
                        oss_audio_non_block,
                        oss_audio_block,
                        oss_audio_set_gain,
                        oss_audio_get_gain,
                        oss_audio_set_volume,
                        oss_audio_get_volume,
                        oss_audio_loopback,
                        oss_audio_set_oport,
                        oss_audio_get_oport,
                        oss_audio_next_oport,
                        oss_audio_set_iport,
                        oss_audio_get_iport,
                        oss_audio_next_iport,
                        oss_audio_is_ready,
                        oss_audio_wait_for,
                        oss_audio_supports
                };
                strncpy(aif_oss.name, oss_get_device_name(i), AUDIO_INTERFACE_NAME_LEN - 1);
                audio_add_interface(&aif_oss);
        }

#endif /* Linux / OSS */

#if defined(WIN32)
        w32sdk_audio_query_devices();
        n = w32sdk_get_device_count();
        for(i = 0; i < n; i++) {
                audio_if_t aif_w32sdk = {
                        "W32SDK Audio Device",
                        NULL, 
                        NULL, 
                        w32sdk_audio_open,
                        w32sdk_audio_close,
                        w32sdk_audio_drain,
                        w32sdk_audio_duplex,
                        w32sdk_audio_read,
                        w32sdk_audio_write,
                        w32sdk_audio_non_block,
                        w32sdk_audio_block,
                        w32sdk_audio_set_gain,
                        w32sdk_audio_get_gain,
                        w32sdk_audio_set_volume,
                        w32sdk_audio_get_volume,
                        w32sdk_audio_loopback,
                        w32sdk_audio_set_oport,
                        w32sdk_audio_get_oport,
                        w32sdk_audio_next_oport,
                        w32sdk_audio_set_iport,
                        w32sdk_audio_get_iport,
                        w32sdk_audio_next_iport,
                        w32sdk_audio_is_ready,
                        w32sdk_audio_wait_for,
                        w32sdk_audio_supports
                };
                strcpy(aif_w32sdk.name, w32sdk_get_device_name(i));
                audio_add_interface(&aif_w32sdk);
        }
#endif /* WIN32 */

#if defined(FreeBSD)
        luigi_audio_query_devices();
        n = luigi_get_device_count();
        for (i = 0; i < n; i++) {
                audio_if_t aif_luigi = {
                        "Default Audio Device",
                        NULL,
                        NULL, 
                        luigi_audio_open,
                        luigi_audio_close,
                        luigi_audio_drain,
                        luigi_audio_duplex,
                        luigi_audio_read,
                        luigi_audio_write,
                        luigi_audio_non_block,
                        luigi_audio_block,
                        luigi_audio_set_gain,
                        luigi_audio_get_gain,
                        luigi_audio_set_volume,
                        luigi_audio_get_volume,
                        luigi_audio_loopback,
                        luigi_audio_set_oport,
                        luigi_audio_get_oport,
                        luigi_audio_next_oport,
                        luigi_audio_set_iport,
                        luigi_audio_get_iport,
                        luigi_audio_next_iport,
                        luigi_audio_is_ready,
                        luigi_audio_wait_for,
                        luigi_audio_supports
                };
                strcpy(aif_luigi.name, luigi_get_device_name(i));
                audio_add_interface(&aif_luigi);
        }
#endif /* FreeBSD */

#if defined(HAVE_PCA)
        {
                audio_if_t aif_pca = {
                        "PCA Audio Device",
                        pca_audio_init,
                        NULL, 
                        pca_audio_open,
                        pca_audio_close,
                        pca_audio_drain,
                        pca_audio_duplex,
                        pca_audio_read,
                        pca_audio_write,
                        pca_audio_non_block,
                        pca_audio_block,
                        pca_audio_set_gain,
                        pca_audio_get_gain,
                        pca_audio_set_volume,
                        pca_audio_get_volume,
                        pca_audio_loopback,
                        pca_audio_set_oport,
                        pca_audio_get_oport,
                        pca_audio_next_oport,
                        pca_audio_set_iport,
                        pca_audio_get_iport,
                        pca_audio_next_iport,
                        pca_audio_is_ready,
                        pca_audio_wait_for,
                        pca_audio_supports
                };
                audio_add_interface(&aif_pca);                
        }
#endif /* HAVE_PCA */
        {
                /* This is the null audio device - it should always go last so that
                 * audio_get_null_interface works.  The idea being when we can't get hold
                 * of a real device we fake one.  Prevents lots of problems elsewhere.
                 */
                audio_if_t aif_null = {
                        "No Audio Device",
                        NULL,
                        NULL, 
                        null_audio_open,
                        null_audio_close,
                        null_audio_drain,
                        null_audio_duplex,
                        null_audio_read,
                        null_audio_write,
                        null_audio_non_block,
                        null_audio_block,
                        null_audio_set_gain,
                        null_audio_get_gain,
                        null_audio_set_volume,
                        null_audio_get_volume,
                        null_audio_loopback,
                        null_audio_set_oport,
                        null_audio_get_oport,
                        null_audio_next_oport,
                        null_audio_set_iport,
                        null_audio_get_iport,
                        null_audio_next_iport,
                        null_audio_is_ready,
                        null_audio_wait_for,
                        NULL
                };
                audio_add_interface(&aif_null);                
        }

        UNUSED(i); /* Some if def combinations may mean that these do not get used */
        UNUSED(n);

        return 0;
}

int
audio_free_interfaces(void)
{
        return TRUE;
}

