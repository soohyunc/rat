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
        int  (*audio_if_init)(void);                 /* Test and initialize audio interface (OPTIONAL)  */
        int  (*audio_if_free)(void);                 /* Free audio interface (OPTIONAL)                 */
        int  (*audio_if_dev_cnt)(void);              /* Device count for interface (REQUIRED) */
        const char* 
             (*audio_if_dev_name)(int);              /* Device name query (REQUIRED) */

        int  (*audio_if_open)(int, audio_format *ifmt, audio_format *ofmt); /* Open device with formats */
        void (*audio_if_close)(int);                 /* Close device (REQUIRED) */
        void (*audio_if_drain)(int);                 /* Drain device (REQUIRED) */
        int  (*audio_if_duplex)(int);                /* Device full duplex (REQUIRED) */

        int  (*audio_if_read) (int, u_char*, int);   /* Read samples (REQUIRED)  */
        int  (*audio_if_write)(int, u_char*, int);   /* Write samples (REQUIRED) */
        void (*audio_if_non_block)(int);             /* Set device non-blocking (REQUIRED) */
        void (*audio_if_block)(int);                 /* Set device blocking (REQUIRED)     */

        void (*audio_if_set_igain)(int,int);          /* Set input gain (REQUIRED)  */
        int  (*audio_if_get_igain)(int);              /* Get input gain (REQUIRED)  */
        void (*audio_if_set_ogain)(int,int);        /* Set output gain (REQUIRED) */
        int  (*audio_if_get_ogain)(int);            /* Get output gain (REQUIRED) */
        void (*audio_if_loopback)(int, int);         /* Enable hardware loopback (OPTIONAL) */

        void (*audio_if_set_oport)(int, int);        /* Set output port (REQUIRED)        */
        int  (*audio_if_get_oport)(int);             /* Get output port (REQUIRED)        */
        int  (*audio_if_next_oport)(int);            /* Go to next output port (REQUIRED) */
        void (*audio_if_set_iport)(int, int);        /* Set input port (REQUIRED)         */
        int  (*audio_if_get_iport)(int);             /* Get input port (REQUIRED)         */
        int  (*audio_if_next_iport)(int);            /* Go to next itput port (REQUIRED)  */
        int  (*audio_if_is_ready)(int);              /* Poll for audio availability (REQUIRED)   */
        void (*audio_if_wait_for)(int, int);         /* Wait until audio is available (REQUIRED) */
        int  (*audio_if_format_supported)(int, audio_format *);
} audio_if_t;

#include "auddev_luigi.h"
#include "auddev_null.h"
#include "auddev_osprey.h"
#include "auddev_oss.h"
#include "auddev_pca.h"
#include "auddev_sparc.h"
#include "auddev_sgi.h"
#include "auddev_win32.h"

audio_if_t audio_if_table[] = {
#ifdef IRIX
        {
                NULL, 
                NULL, 
                sgi_audio_device_count,
                sgi_audio_device_name,
                sgi_audio_open,
                sgi_audio_close,
                sgi_audio_drain,
                sgi_audio_duplex,
                sgi_audio_read,
                sgi_audio_write,
                sgi_audio_non_block,
                sgi_audio_block,
                sgi_audio_set_igain,
                sgi_audio_get_igain,
                sgi_audio_set_ogain,
                sgi_audio_get_ogain,
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
        },
#endif /* IRIX */
#ifdef Solaris
        {
                NULL,
                NULL,
                sparc_audio_device_count,
                sparc_audio_device_name,
                sparc_audio_open,
                sparc_audio_close,
                sparc_audio_drain,
                sparc_audio_duplex,
                sparc_audio_read,
                sparc_audio_write,
                sparc_audio_non_block,
                sparc_audio_block,
                sparc_audio_set_igain,
                sparc_audio_get_igain,
                sparc_audio_set_ogain,
                sparc_audio_get_ogain,
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
        },
#endif /* Solaris */
#ifdef HAVE_OSPREY
        {
                osprey_audio_init, 
                NULL, 
                osprey_audio_device_count,
                osprey_audio_device_name,
                osprey_audio_open,
                osprey_audio_close,
                osprey_audio_drain,
                osprey_audio_duplex,
                osprey_audio_read,
                osprey_audio_write,
                osprey_audio_non_block,
                osprey_audio_block,
                osprey_audio_set_igain,
                osprey_audio_get_igain,
                osprey_audio_set_ogain,
                osprey_audio_get_ogain,
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
        },
#endif /* HAVE_OSPREY */
#if defined(Linux)||defined(OSS)
        {
                oss_audio_query_devices, 
                NULL,
                oss_get_device_count,
                oss_get_device_name,
                oss_audio_open,
                oss_audio_close,
                oss_audio_drain,
                oss_audio_duplex,
                oss_audio_read,
                oss_audio_write,
                oss_audio_non_block,
                oss_audio_block,
                oss_audio_set_igain,
                oss_audio_get_igain,
                oss_audio_set_ogain,
                oss_audio_get_ogain,
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
        },

#endif /* Linux / OSS */

#if defined(WIN32)
        {
                w32sdk_audio_query_devices,
                NULL, 
                w32sdk_get_device_count,
                w32sdk_get_device_name,
                w32sdk_audio_open,
                w32sdk_audio_close,
                w32sdk_audio_drain,
                w32sdk_audio_duplex,
                w32sdk_audio_read,
                w32sdk_audio_write,
                w32sdk_audio_non_block,
                w32sdk_audio_block,
                w32sdk_audio_set_igain,
                w32sdk_audio_get_igain,
                w32sdk_audio_set_ogain,
                w32sdk_audio_get_ogain,
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
        },
#endif /* WIN32 */

#if defined(FreeBSD)
        {
                luigi_audio_query_devices,
                NULL,
                luigi_get_device_count,
                luigi_get_device_name,
                luigi_audio_open,
                luigi_audio_close,
                luigi_audio_drain,
                luigi_audio_duplex,
                luigi_audio_read,
                luigi_audio_write,
                luigi_audio_non_block,
                luigi_audio_block,
                luigi_audio_set_igain,
                luigi_audio_get_igain,
                luigi_audio_set_ogain,
                luigi_audio_get_ogain,
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
        },

#endif /* FreeBSD */

#if defined(HAVE_PCA)
        {
                pca_audio_init,
                NULL, 
                pca_audio_device_count,
                pca_audio_device_name,
                pca_audio_open,
                pca_audio_close,
                pca_audio_drain,
                pca_audio_duplex,
                pca_audio_read,
                pca_audio_write,
                pca_audio_non_block,
                pca_audio_block,
                pca_audio_set_igain,
                pca_audio_get_igain,
                pca_audio_set_ogain,
                pca_audio_get_ogain,
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
        },
#endif /* HAVE_PCA */
        {
                /* This is the null audio device - it should always go last so that
                 * audio_get_null_device works.  The idea being when we can't get hold
                 * of a real device we fake one.  Prevents lots of problems elsewhere.
                 */
                NULL,
                NULL, 
                null_audio_device_count,
                null_audio_device_name,
                null_audio_open,
                null_audio_close,
                null_audio_drain,
                null_audio_duplex,
                null_audio_read,
                null_audio_write,
                null_audio_non_block,
                null_audio_block,
                null_audio_set_igain,
                null_audio_get_igain,
                null_audio_set_ogain,
                null_audio_get_ogain,
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
        }
};

#define NUM_AUDIO_INTERFACES (sizeof(audio_if_table)/sizeof(audio_if_t))

/* Active interfaces is a table of entries pointing to entries in
 * audio interfaces table.  Audio open returns index to these */
static audio_desc_t active_device_desc[NUM_AUDIO_INTERFACES];
static int active_devices;
static int actual_devices;

#define MAX_ACTIVE_DEVICES   2

/* These are requested device formats.  */
#define AUDDEV_REQ_IFMT      0
#define AUDDEV_REQ_OFMT      1

/* These are actual device formats that are transparently converted 
 * into the required ones during reads and writes.  */
#define AUDDEV_ACT_IFMT      2
#define AUDDEV_ACT_OFMT      3

#define AUDDEV_NUM_FORMATS   4

static audio_format* fmts[MAX_ACTIVE_DEVICES][AUDDEV_NUM_FORMATS];
static sample*       convert_buf[MAX_ACTIVE_DEVICES]; /* used if conversions used */

/* Counters for samples read/written */
static u_int32 samples_read[MAX_ACTIVE_DEVICES], samples_written[MAX_ACTIVE_DEVICES];

/* We map indexes outside range for file descriptors so people don't attempt
 * to circumvent audio interface.  If something is missing it should be added
 * to the interfaces...
 */

#define AIF_GET_INTERFACE(x) ((((x) & 0x0f00) >> 8) - 1)
#define AIF_GET_DEVICE_NO(x) (((x) & 0x000f) - 1)
#define AIF_MAKE_DESC(iface,dev) (((iface + 1) << 8) | (dev + 1))

#define AIF_VALID_INTERFACE(id) ((id & 0x0f00))
#define AIF_VALID_DEVICE_NO(id) ((id & 0x000f))

/*****************************************************************************
 *
 * Code for working out how many devices are, what they are called, and what 
 * descriptor should be used to access them.
 *
 *****************************************************************************/

int
audio_get_device_count()
{
        return actual_devices;
}

int
audio_get_device_details(int idx, audio_device_details_t *add)
{
        int 		 iface = 0;
	int 		 devs  = 0;
        const char 	*name  = NULL;

        assert(idx < actual_devices && idx >= 0);
        assert(add != NULL);

        /* Find interface device number idx belongs to */
        while((devs = audio_if_table[iface].audio_if_dev_cnt()) && idx >= devs) {
                iface++;
                idx -= devs;
        }

        assert(devs != 0);

        add->descriptor = AIF_MAKE_DESC(iface, idx);
        assert(audio_if_table[iface].audio_if_dev_name != NULL);
        name = audio_if_table[iface].audio_if_dev_name(idx);
        assert(name != NULL);
        strncpy(add->name, name, AUDIO_DEVICE_NAME_LENGTH);
        return TRUE;
}

audio_desc_t
audio_get_null_device()
{
        audio_desc_t ad;

        /* Null audio device is only device on the last interface*/
        ad = AIF_MAKE_DESC((NUM_AUDIO_INTERFACES - 1), 0);

        return ad;
}

/*****************************************************************************
 *
 * Interface code.  Maps audio functions to audio devices.
 *
 *****************************************************************************/

static int
get_active_device_index(audio_desc_t ad)
{
        int i;
        
        for (i = 0; i < active_devices; i++) {
                if (active_device_desc[i] == ad) {
                        return i;
                }
        }

        return -1;
}

int
audio_device_is_open(audio_desc_t ad)
{
        int dev = get_active_device_index(ad);
        return (dev != -1);
}

int
audio_open(audio_desc_t ad, audio_format *ifmt, audio_format *ofmt)
{
        audio_format format;
        int iface, device, dev_idx;
        int success;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(ifmt != NULL);
        assert(ofmt != NULL);

        if ((ofmt->sample_rate != ifmt->sample_rate) ||
            (ofmt->encoding == DEV_S8 || ifmt->encoding == DEV_S8)) {
                /* Fail on things we don't support */
                debug_msg("Not supported\n");
                return 0;
        }

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        if (active_devices == MAX_ACTIVE_DEVICES) {
                debug_msg("Already have the maximum number of devices (%d) open.\n", MAX_ACTIVE_DEVICES);
                return FALSE;
        }

        dev_idx   = active_devices;

        assert(get_active_device_index(ad) == -1);
        assert(audio_if_table[iface].audio_if_open);

        if (audio_format_get_common(ifmt, ofmt, &format) == FALSE) {
                /* Input and output formats incompatible */
                return 0;
        }

        fmts[dev_idx][AUDDEV_ACT_IFMT] = audio_format_dup(&format);
        fmts[dev_idx][AUDDEV_ACT_OFMT] = audio_format_dup(&format);

        /* Formats can get changed in audio_if_open, but only sample
         * type, not the number of channels or freq 
         */
        success = audio_if_table[iface].audio_if_open(device, 
                                                          fmts[dev_idx][AUDDEV_ACT_IFMT], 
                                                          fmts[dev_idx][AUDDEV_ACT_OFMT]);

        if (success) {
                /* Add device to list of those active */
                active_device_desc[dev_idx] = ad;
                active_devices ++;

                if ((fmts[dev_idx][AUDDEV_ACT_IFMT]->sample_rate != format.sample_rate) ||
                    (fmts[dev_idx][AUDDEV_ACT_OFMT]->sample_rate != format.sample_rate) ||
                    (fmts[dev_idx][AUDDEV_ACT_IFMT]->channels    != format.channels)    ||
                    (fmts[dev_idx][AUDDEV_ACT_OFMT]->channels    != format.channels)) {
                        debug_msg("Device changed sample rate or channels - unsupported functionality.\n");
                        audio_close(ad);
                        return FALSE;
                }

                if (!audio_if_table[iface].audio_if_duplex(device)) {
                        printf("RAT v3.2.0 and later require a full duplex audio device, but \n");
                        printf("your device only supports half-duplex operation. Sorry.\n");
                        audio_close(ad);
                        return FALSE;
                }
                
                /* If we are going to need conversion between requested and 
                 * actual device formats store requested formats */
                if (!audio_format_match(ifmt, fmts[dev_idx][AUDDEV_ACT_IFMT])) {
                        fmts[dev_idx][AUDDEV_REQ_IFMT] = audio_format_dup(ifmt);
#ifdef DEBUG
                        {
                                char s[50];
                                audio_format_name(fmts[dev_idx][AUDDEV_REQ_IFMT], s, 50);
                                debug_msg("Requested Input: %s\n", s);
                                audio_format_name(fmts[dev_idx][AUDDEV_ACT_IFMT], s, 50);
                                debug_msg("Actual Input:    %s\n", s);
                        }
#endif
                }

                if (!audio_format_match(ofmt, fmts[dev_idx][AUDDEV_ACT_OFMT])) {
                        fmts[dev_idx][AUDDEV_REQ_OFMT] = audio_format_dup(ofmt);
#ifdef DEBUG
                        {
                                char s[50];
                                audio_format_name(fmts[dev_idx][AUDDEV_REQ_OFMT], s, 50);
                                debug_msg("Requested Output: %s\n", s);
                                audio_format_name(fmts[dev_idx][AUDDEV_ACT_OFMT], s, 50);
                                debug_msg("Actual Output:    %s\n", s);
                        }
#endif
                }

                if (fmts[dev_idx][AUDDEV_REQ_IFMT] || fmts[dev_idx][AUDDEV_REQ_OFMT]) {
                        convert_buf[dev_idx] = (sample*)xmalloc(DEVICE_REC_BUF); /* is this in samples or bytes ? */
                }

                samples_read[dev_idx]    = 0;
                samples_written[dev_idx] = 0;

                return TRUE;
        }

        audio_format_free(&fmts[dev_idx][AUDDEV_ACT_IFMT]);
        audio_format_free(&fmts[dev_idx][AUDDEV_ACT_OFMT]);

        return FALSE;
}

void
audio_close(audio_desc_t ad)
{
        int i, j, k, iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        audio_if_table[iface].audio_if_close(device);

        /* Check device is open */
        assert(get_active_device_index(ad) != -1);

        i = j = 0;
        for(i = 0; i < active_devices; i++) {
                if (active_device_desc[i] == ad) {
                        for(k = 0; k < AUDDEV_NUM_FORMATS; k++) {
                                if (fmts[i][k] != NULL) audio_format_free(&fmts[i][k]);                                
                        }
                        if (convert_buf[i]) xfree(convert_buf[i]);
                        samples_written[i] = 0;
                        samples_read[i]    = 0;
                } else {
                        if (i != j) {
                                active_device_desc[j] = active_device_desc[i];
                                for(k = 0; k < AUDDEV_NUM_FORMATS; k++) {
                                        assert(fmts[j][k] == NULL);
                                        fmts[j][k] = fmts[i][k];
                                }
                                convert_buf[j]     = convert_buf[i];
                                samples_read[j]    = samples_read[i];
                                samples_written[j] = samples_written[i];
                        }
                        j++;
                }
        }

        active_devices --;
}

const audio_format*
audio_get_ifmt(audio_desc_t ad)
{
        int idx = get_active_device_index(ad);

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(idx >= 0 && idx < active_devices);

        if (fmts[idx][AUDDEV_REQ_IFMT]) {
                return fmts[idx][AUDDEV_REQ_IFMT];
        }

        return fmts[idx][AUDDEV_ACT_IFMT];
}

const audio_format*
audio_get_ofmt(audio_desc_t ad)
{
        int idx = get_active_device_index(ad);

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(idx >= 0 && idx < active_devices);

        if (fmts[idx][AUDDEV_REQ_OFMT]) {
                return fmts[idx][AUDDEV_REQ_OFMT];
        }

        return fmts[idx][AUDDEV_ACT_OFMT];
}

void
audio_drain(audio_desc_t ad)
{
        int device, iface;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));
        
        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);
        
        audio_if_table[iface].audio_if_drain(device);
}

int
audio_duplex(audio_desc_t ad)
{
        int device, iface;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return audio_if_table[iface].audio_if_duplex(device);
}

int
audio_read(audio_desc_t ad, sample *buf, int samples)
{
        /* Samples is the number of samples to read * number of channels */
        int read_len;
        int sample_size;
        int device, iface;
        int idx = get_active_device_index(ad);

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));        
        assert(idx >= 0 && idx < active_devices);
        assert(buf != NULL);

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        xmemchk();

        if (fmts[idx][AUDDEV_REQ_IFMT] == NULL) {
                /* No conversion necessary as input format and real format are
                 * the same. [Input format only allocated if different from
                 * real format].
                 */
                sample_size = fmts[idx][AUDDEV_ACT_IFMT]->bits_per_sample / 8;
                read_len    = audio_if_table[iface].audio_if_read(device, 
                                                                       (u_char*)buf, 
                                                                       samples * sample_size);
                samples_read[idx] += read_len / (sample_size * fmts[idx][AUDDEV_ACT_IFMT]->channels);
        } else {
                assert(fmts[idx][AUDDEV_ACT_IFMT] != NULL);
                sample_size = fmts[idx][AUDDEV_ACT_IFMT]->bits_per_sample / 8;
                read_len    = audio_if_table[iface].audio_if_read(device, 
                                                                       (u_char*)convert_buf[idx], 
                                                                       samples * sample_size);
                read_len    = audio_format_buffer_convert(fmts[idx][AUDDEV_REQ_IFMT], 
                                                          (u_char*) convert_buf[idx], 
                                                          read_len, 
                                                          fmts[idx][AUDDEV_ACT_IFMT], 
                                                          (u_char*) buf,
                                                          DEVICE_REC_BUF);
                sample_size = fmts[idx][AUDDEV_REQ_IFMT]->bits_per_sample / 8;
                samples_read[idx] += read_len / (sample_size * fmts[idx][AUDDEV_REQ_IFMT]->channels);
        }

        xmemchk();
        return read_len / sample_size;
}

int
audio_write(audio_desc_t ad, sample *buf, int len)
{
        int write_len ,sample_size;
        int iface, device;
        int idx = get_active_device_index(ad);
        
        assert(idx >= 0 && idx < active_devices);
        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);
        
        xmemchk();

        if (fmts[idx][AUDDEV_REQ_OFMT] == NULL) {
                /* No conversion necessary as output format and real format are
                 * the same. [Output format only allocated if different from
                 * real format].
                 */
                sample_size = fmts[idx][AUDDEV_ACT_OFMT]->bits_per_sample / 8;
                write_len   = audio_if_table[iface].audio_if_write(device, (u_char*)buf, len * sample_size);
                samples_written[idx] += write_len / (sample_size * fmts[idx][AUDDEV_ACT_OFMT]->channels);
        } else {
                write_len = audio_format_buffer_convert(fmts[idx][AUDDEV_REQ_OFMT],
                                                        (u_char*)buf,
                                                        len,
                                                        fmts[idx][AUDDEV_ACT_OFMT],
                                                        (u_char*) convert_buf[idx],
                                                        DEVICE_REC_BUF);
                audio_if_table[iface].audio_if_write(device, (u_char*)convert_buf[idx], write_len);
                sample_size = fmts[idx][AUDDEV_ACT_OFMT]->bits_per_sample / 8;
                samples_written[idx] += write_len / (sample_size * fmts[idx][AUDDEV_REQ_OFMT]->channels);
        }

        xmemchk();
        return write_len / sample_size;
}

void
audio_non_block(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        audio_if_table[iface].audio_if_non_block(device);
}

void
audio_block(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);
        
        audio_if_table[iface].audio_if_block(device);
}

void
audio_set_igain(audio_desc_t ad, int gain)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        audio_if_table[iface].audio_if_set_igain(device, gain);
}

int
audio_get_igain(audio_desc_t ad)
{
        int gain;
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        gain = audio_if_table[iface].audio_if_get_igain(device);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        return gain;
}

void
audio_set_ogain(audio_desc_t ad, int volume)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        assert(volume >= 0);
        assert(volume <= MAX_AMP);

        audio_if_table[iface].audio_if_set_ogain(device, volume);
}

int
audio_get_ogain(audio_desc_t ad)
{
        int volume;
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);
        
        volume = audio_if_table[iface].audio_if_get_ogain(device);
        assert(volume >= 0);
        assert(volume <= MAX_AMP);

        return volume;
}

void
audio_loopback(audio_desc_t ad, int gain)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        assert(gain >= 0);
        assert(gain <= MAX_AMP);

        if (audio_if_table[iface].audio_if_loopback) audio_if_table[iface].audio_if_loopback(device, gain);
}

void
audio_set_oport(audio_desc_t ad, int port)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);
        
        audio_if_table[iface].audio_if_set_oport(device, port);
}

int
audio_get_oport(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return (audio_if_table[iface].audio_if_get_oport(device));
}

int     
audio_next_oport(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return (audio_if_table[iface].audio_if_next_oport(device));
}

void
audio_set_iport(audio_desc_t ad, int port)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        audio_if_table[iface].audio_if_set_iport(device, port);
}

int
audio_get_iport(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return (audio_if_table[iface].audio_if_get_iport(device));
}

int
audio_next_iport(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return (audio_if_table[iface].audio_if_next_iport(device));
}

int
audio_is_ready(audio_desc_t ad)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        return (audio_if_table[iface].audio_if_is_ready(device));
}

void
audio_wait_for(audio_desc_t ad, int delay_ms)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        audio_if_table[iface].audio_if_wait_for(device, delay_ms);
}

/* Code for adding/initialising/removing audio ifaces */

int
audio_device_supports(audio_desc_t ad, u_int16 rate, u_int16 channels)
{
        int iface, device;

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(audio_device_is_open(ad));

        iface  = AIF_GET_INTERFACE(ad);
        device = AIF_GET_DEVICE_NO(ad);

        if (rate % 8000 || channels > 2) {
                debug_msg("Invalid combo %d Hz %d channels\n", rate, channels);
                return FALSE;
        }

        if (audio_if_table[iface].audio_if_format_supported) {
                audio_format tfmt;
                tfmt.encoding    = DEV_S16;
                tfmt.sample_rate = rate;
                tfmt.channels    = channels;
                return audio_if_table[iface].audio_if_format_supported(device, &tfmt);
        }

        debug_msg("Format support query function not implemented! Lying about supported formats.\n");

        return TRUE;
}

u_int32
audio_get_device_time(audio_desc_t ad)
{
        audio_format *fmt;
        u_int32       samples_per_block;
        int dev = get_active_device_index(ad);

        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(dev >= 0 && dev < active_devices);

        if (fmts[dev][AUDDEV_REQ_IFMT]) {
                fmt = fmts[dev][AUDDEV_REQ_IFMT]; 
        } else {
                fmt = fmts[dev][AUDDEV_ACT_IFMT]; 
        }
        
        samples_per_block = fmt->bytes_per_block * 8 / (fmt->channels * fmt->bits_per_sample);
        
        return (samples_read[dev]/samples_per_block) * samples_per_block;
}

u_int32
audio_get_samples_read(audio_desc_t ad)
{
        int dev = get_active_device_index(ad);
        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(dev >= 0 && dev < active_devices);
        return samples_read[dev];
}

u_int32
audio_get_samples_written(audio_desc_t ad)
{
        int dev = get_active_device_index(ad);
        assert(AIF_VALID_INTERFACE(ad) && AIF_VALID_DEVICE_NO(ad));
        assert(dev >= 0 && dev < active_devices);
        return samples_written[dev];
}

int
audio_init_interfaces(void)
{
        u_int32 i, j;
	int	c;

        actual_devices = 0;

        for(i = 0; i < NUM_AUDIO_INTERFACES; i++) {
                if (audio_if_table[i].audio_if_init) {
                        audio_if_table[i].audio_if_init(); 
                }
                assert(audio_if_table[i].audio_if_dev_cnt);
		c = audio_if_table[i].audio_if_dev_cnt();
		if (c == 0) {
			/* audio_if_table[i] has no devices (eg: a linux box where */
			/* the kernel has been compiled without sound support). We */
			/* must remove this interface from the system...     [csp] */
			debug_msg("Removing interface %d\n", i);
			for (j = i + 1; j < NUM_AUDIO_INTERFACES; j++) {
				memcpy(&(audio_if_table[j-1]), &(audio_if_table[j]), sizeof(audio_if_t));
			}
		}
                actual_devices += c;
        }

        return TRUE;
}

int
audio_free_interfaces(void)
{
        u_int32 i;

        for(i = 0; i < NUM_AUDIO_INTERFACES; i++) {
                if (audio_if_table[i].audio_if_free) {
                        audio_if_table[i].audio_if_free(); 
                }
        }

        return TRUE;
}

