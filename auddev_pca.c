/*
 * FILE:    auddev_pca.c
 * PROGRAM: RAT
 * AUTHOR:  Jim Lowe (james@cs.uwm.edu) 
 * MODS: Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1996-98 University College London
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

/*
 * PCA speaker support for FreeBSD.
 *
 * This is an output only device so we pretend we read audio...
 *
 */

#include <pcaudioio.h>

#include "config_unix.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_pca.h"
#include "codec_g711.h"
#include "memory.h"
#include "debug.h"

static audio_info_t   dev_info;			/* For PCA device */
static int            audio_fd;
static struct timeval last_read_time;
static int            bytes_per_block;
static int            present;

#define pca_bat_to_device(x)	((x) * AUDIO_MAX_GAIN / MAX_AMP)
#define pca_device_to_bat(x)	((x) * MAX_AMP / AUDIO_MAX_GAIN)

int
pca_audio_device_count()
{
        return present;
}

const char*
pca_audio_device_name(audio_desc_t ad)
{
        UNUSED(ad);
        return "PCA Audio Device";
}

int
pca_audio_init()
{
        int audio_fd;
        if ((audio_fd = open("/dev/pcaudio", O_WRONLY | O_NDELAY)) != -1) {
                close(audio_fd);
                present = 1;
                return TRUE;
        }
        return FALSE;
}

/*
 * Try to open the audio device.
 * Return: valid file descriptor if ok, -1 otherwise.
 */

int
pca_audio_open(audio_desc_t ad, audio_format *ifmt, audio_format *ofmt)
{
	audio_info_t tmp_info;

        UNUSED(ofmt);
        assert(audio_format_match(ifmt, ofmt));

        if (ifmt->sample_rate != 8000 || ifmt->channels != 1) {
                return FALSE;
        }

	audio_fd = open("/dev/pcaudio", O_WRONLY | O_NDELAY );

	if (audio_fd > 0) {
		AUDIO_INITINFO(&dev_info);
		dev_info.monitor_gain     = 0;
		dev_info.play.sample_rate = ifmt->sample_rate;
		dev_info.play.channels    = ifmt->channels;
		dev_info.play.gain	      = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
		dev_info.play.port	      = 0;

                if (ifmt->encoding != DEV_PCMU) {
                        audio_format_change_encoding(ifmt, DEV_PCMU);
                }

                if (ofmt->encoding != DEV_PCMU) {
                        audio_format_change_encoding(ofmt, DEV_PCMU);
                }

                assert(ifmt->bits_per_sample == 8);
                dev_info.play.encoding  = AUDIO_ENCODING_ULAW;
                dev_info.play.precision   = 8;

                bytes_per_block = ofmt->bytes_per_block;

		memcpy(&tmp_info, &dev_info, sizeof(audio_info_t));

		if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&tmp_info) < 0) {
			perror("pca_audio_info: setting parameters");
                        pca_audio_close(ad);
			return FALSE;
		}

                return TRUE;
	} else {
		/* 
		 * Because we opened the device with O_NDELAY, the wait
		 * flag was not updaed so update it manually.
		 */
		audio_fd = open("/dev/pcaudioctl", O_WRONLY);
		if (audio_fd < 0) {
			AUDIO_INITINFO(&dev_info);
			dev_info.play.waiting = 1;
			(void)ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info);
			close(audio_fd);
		}
		audio_fd = -1;
	}
	return FALSE;
}

/*
 * Shutdown.
 */
void
pca_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
	if(audio_fd > 0)
		(void)close(audio_fd);
	audio_fd = -1;
	return;
}

/*
 * Flush input buffer.
 */
void
pca_audio_drain(audio_desc_t ad)
{
        UNUSED(ad);
	return;
}

/*
 * Set record gain.
 */
void
pca_audio_set_igain(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        UNUSED(gain);
	return;
}

/*
 * Get record gain.
 */
int
pca_audio_get_igain(audio_desc_t ad)
{
        UNUSED(ad);
	return 0;
}

int
pca_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad);
        /* LIE! LIE! LIE! LIE! 
         * But we really only support full duplex devices
         */
        return TRUE;
}

/*
 * Set play gain.
 */
void
pca_audio_set_ogain(audio_desc_t ad, int vol)
{
        UNUSED(ad);
        AUDIO_INITINFO(&dev_info);
        dev_info.play.gain = pca_bat_to_device(vol);
        if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) 
                perror("pca_audio_set_ogain");

	return;
}

/*
 * Get play gain.
 */
int
pca_audio_get_ogain(audio_desc_t ad)
{
        UNUSED(ad);
	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("pca_audio_get_ogain");
	return pca_device_to_bat(dev_info.play.gain);
}

/*
 * Record audio data.
 */
int
pca_audio_read(audio_desc_t ad, u_char *buf, int read_bytes)
{
	/*
	 * Reading data from internal PC speaker is a little difficult,
	 * so just return the time (in audio samples) since the last time called.
	 */
	int	                diff;
	struct timeval          curr_time;
	static int              virgin = TRUE;

        UNUSED(ad);

	if (virgin) {
		gettimeofday(&last_read_time, NULL);
		virgin = FALSE;
	}

	gettimeofday(&curr_time, NULL);
	diff = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000 + (curr_time.tv_usec - last_read_time.tv_usec) / 1000;
        /* diff from ms to samples */
        
        diff *= dev_info.play.sample_rate / 1000;
        read_bytes = min(read_bytes, diff);

        memcpy(&last_read_time, &curr_time, sizeof(struct timeval));
        memset(buf, 0, read_bytes);
        xmemchk();

        return read_bytes;
}

/*
 * Playback audio data.
 */
int
pca_audio_write(audio_desc_t ad, u_char *buf, int write_bytes)
{
	int	 nbytes;

        UNUSED(ad);

        if ((nbytes = write(audio_fd, buf, write_bytes)) != write_bytes) {
		if (errno == EWOULDBLOCK) {	/* XXX */
                        perror("pca_audio_write");
			return 0;
		}
		if (errno != EINTR) {
			perror("pca_audio_write");
			return (write_bytes - nbytes);
		}
	} 
    
	return write_bytes;
}

/*
 * Set options on audio device to be non-blocking.
 */
void
pca_audio_non_block(audio_desc_t ad)
{
	int on = 1;

        UNUSED(ad);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		perror("pca_audio_non_block");
 
	return;
}

/*
 * Set options on audio device to be blocking.
 */
void
pca_audio_block(audio_desc_t ad)
{
	int on = 0;

        UNUSED(ad);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		perror("pca_audio_block");
	return;
}

#define PCA_SPEAKER    0x0101
#define PCA_MICROPHONE 0x0201

static audio_port_details_t in_ports[] = {
        { PCA_MICROPHONE, AUDIO_PORT_MICROPHONE}
};

static audio_port_details_t out_ports[] = {
        { PCA_SPEAKER,    AUDIO_PORT_SPEAKER}
};

/*
 * Set output port.
 */
void
pca_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
	/* There is only one port... */
        UNUSED(ad); UNUSED(port);

        assert(port == PCA_SPEAKER);

	return;
}

/*
 * Get output port.
 */
audio_port_t
pca_audio_oport_get(audio_desc_t ad)
{
	/* There is only one port... */
        UNUSED(ad);

	return out_ports[0].port;
}

int
pca_audio_oport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return 1;
}

const audio_port_details_t*
pca_audio_oport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        UNUSED(idx);
        assert(idx == 0);
        return &out_ports[0];
}

/*
 * Set input port.
 */
void
pca_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
	/* Hmmm.... */
        UNUSED(ad);
        UNUSED(port);
	return;
}

/*
 * Get input port.
 */
audio_port_t
pca_audio_iport_get(audio_desc_t ad)
{
	/* Hmm...hack attack */
        UNUSED(ad);
	return in_ports[0].port;
}

int
pca_audio_iport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return 1;
}

const audio_port_details_t*
pca_audio_iport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        UNUSED(idx);
        assert(idx == 0);
        return &in_ports[0];
}

/*
 * Enable hardware loopback
 */
void 
pca_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        UNUSED(gain);
        /* Nothing doing... */
}

/*
 * For external purposes this function returns non-zero
 * if audio is ready.
 */
int
pca_audio_is_ready(audio_desc_t ad)
{
        struct timeval now;
        u_int32 diff;

        UNUSED(ad);

        gettimeofday(&now,NULL);
	diff = (now.tv_sec  - last_read_time.tv_sec) * 1000 + (now.tv_usec - last_read_time.tv_usec)/1000;
        diff *= 8; /* Only runs at 8k */

        if (diff >= (unsigned)bytes_per_block) return TRUE;
        return FALSE;
}

void
pca_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        if (pca_audio_is_ready(ad)) {
                return;
        } else {
                usleep(delay_ms * 1000);
        }
}

int
pca_audio_supports(audio_desc_t ad, audio_format *fmt)
{
        UNUSED(ad);
        if (fmt->channels == 1 && fmt->sample_rate == 8000) return TRUE;
        return FALSE;
}
