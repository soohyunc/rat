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
#include <machine/pcaudioio.h>

#include "config_unix.h"
#include "audio_types.h"
#include "auddev_pca.h"
#include "codec_g711.h"
#include "assert.h"
#include "memory.h"
#include "debug.h"

static audio_info_t dev_info;			/* For PCA device */
static audio_format format;
static int          audio_fd;
static struct timeval last_read_time;

#define pca_bat_to_device(x)	((x) * AUDIO_MAX_GAIN / MAX_AMP)
#define pca_device_to_bat(x)	((x) * MAX_AMP / AUDIO_MAX_GAIN)

int
pca_audio_init()
{
        int audio_fd;
        if ((audio_fd = open("/dev/pcaudio", O_WRONLY | O_NDELAY)) != -1) {
                close(audio_fd);
                return TRUE;
        }
        return FALSE;
}

/*
 * Try to open the audio device.
 * Return: valid file descriptor if ok, -1 otherwise.
 */

int
pca_audio_open(audio_desc_t ad, audio_format *fmt)
{
	audio_info_t tmp_info;

        if (fmt->sample_rate != 8000 || fmt->num_channels != 1) {
                return FALSE;
        }

	audio_fd = open("/dev/pcaudio", O_WRONLY | O_NDELAY );

	if (audio_fd > 0) {
		memcpy(&format, fmt, sizeof(format));
		AUDIO_INITINFO(&dev_info);
		dev_info.monitor_gain     = 0;
		dev_info.play.sample_rate = 8000;
		dev_info.play.channels    = 1;
		dev_info.play.precision   = 8;
		dev_info.play.gain	      = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
		dev_info.play.port	      = 0;
		switch(fmt->encoding) {
		case DEV_PCMU:
			assert(format.bits_per_sample == 8);
			dev_info.play.encoding  = AUDIO_ENCODING_ULAW;
			break;
		case DEV_L16:
			assert(format.bits_per_sample == 16);
			dev_info.play.encoding  = AUDIO_ENCODING_ULAW;
			break;
		case DEV_L8:
			assert(format.bits_per_sample == 8);
			dev_info.play.encoding  = AUDIO_ENCODING_RAW;
			break;
		default:
			printf("Unknown audio encoding in pca_audio_open: %x\n", format.encoding);
                        pca_audio_close(ad);
                        return FALSE;
		}
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
pca_audio_set_gain(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        UNUSED(gain);
	return;
}

/*
 * Get record gain.
 */
int
pca_audio_get_gain(audio_desc_t ad)
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
pca_audio_set_volume(audio_desc_t ad, int vol)
{
        UNUSED(ad);
        AUDIO_INITINFO(&dev_info);
        dev_info.play.gain = pca_bat_to_device(vol);
        if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) 
                perror("pca_audio_set_volume");

	return;
}

/*
 * Get play gain.
 */
int
pca_audio_get_volume(audio_desc_t ad)
{
        UNUSED(ad);
	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("pca_audio_get_volume");
	return pca_device_to_bat(dev_info.play.gain);
}

/*
 * Record audio data.
 */
int
pca_audio_read(audio_desc_t ad, sample *buf, int samples)
{
	/*
	 * Reading data from internal PC speaker is a little difficult,
	 * so just return the time (in audio samples) since the last time called.
	 */
	int	                i;
	int	                diff;
        int                     excess_audio;
	struct timeval          curr_time;
	static int              virgin = TRUE;

        UNUSED(ad);

	if (virgin) {
		gettimeofday(&last_read_time, NULL);
		virgin = FALSE;
		for (i=0; i < 80; i++) {
			buf[i] = L16_AUDIO_ZERO;
		}
		return 80;
	}
	gettimeofday(&curr_time, NULL);
	diff = (((curr_time.tv_sec  - last_read_time.tv_sec) * 1e6) +
		curr_time.tv_usec - last_read_time.tv_usec) * format.sample_rate * 1e-6;
        if (diff < format.blocksize) {
                /* Not enough time has elapsed for 1 block to accumulate
                 * so bail.
                 */
                return 0;
        }

        if (diff > samples) {
                /* Another read will be necessary because we have not been
                 * keeping up.
                 */
                excess_audio = TRUE;
        } else {
                excess_audio = FALSE;
        }
                
        xmemchk();
        memset(buf, 0, sizeof(sample)*samples);
	xmemchk();

        if (excess_audio == FALSE) {
                /* We have read all of the audio we were supposed to */
                last_read_time = curr_time;
        }

	return samples;  
}

/*
 * Playback audio data.
 */
int
pca_audio_write(audio_desc_t ad, sample *buf, int samples)
{
	int	 nbytes;
	int    len;
	u_char *p;
	u_char play_buf[DEVICE_REC_BUF];

        UNUSED(ad);

	for (nbytes = 0; nbytes < samples; nbytes++)
		if(format.encoding == DEV_L16) 
			play_buf[nbytes] = lintomulaw[(unsigned short)buf[nbytes]];
		else
			play_buf[nbytes] = buf[nbytes];

	p = play_buf;
	len = samples;
	while (TRUE) {
		if ((nbytes = write(audio_fd, p, len)) == len)
			break;
		if (errno == EWOULDBLOCK) {	/* XXX */
			return 0;
		}
		if (errno != EINTR) {
			perror("pca_audio_write");
			return (samples - len);
		}
		len -= nbytes;
		p += nbytes;
	} 
    
	return samples;
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

/*
 * Set output port.
 */
void
pca_audio_set_oport(audio_desc_t ad, int port)
{
	/* There is only one port... */
        UNUSED(ad); UNUSED(port);

	return;
}

/*
 * Get output port.
 */
int
pca_audio_get_oport(audio_desc_t ad)
{
	/* There is only one port... */
        UNUSED(ad);

	return AUDIO_SPEAKER;
}

/*
 * Set next output port.
 */
int
pca_audio_next_oport(audio_desc_t ad)
{
	/* There is only one port... */
        UNUSED(ad);
	return AUDIO_SPEAKER;
}

/*
 * Set input port.
 */
void
pca_audio_set_iport(audio_desc_t ad, int port)
{
	/* Hmmm.... */
        UNUSED(ad);
        UNUSED(port);
	return;
}

/*
 * Get input port.
 */
int
pca_audio_get_iport(audio_desc_t ad)
{
	/* Hmm...hack attack */
        UNUSED(ad);
	return AUDIO_MICROPHONE;
}

/*
 * Get next input port...
 */
int
pca_audio_next_iport(audio_desc_t ad)
{
	/* Hmm... */
        UNUSED(ad);
	return AUDIO_MICROPHONE;
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
 * Get device blocksize
 */
int
pca_audio_get_blocksize(audio_desc_t ad)
{
        UNUSED(ad);
        return format.blocksize;
}

/*
 * Get device channels
 */
int
pca_audio_get_channels(audio_desc_t ad)
{
        UNUSED(ad);
        return format.num_channels;
}

/*
 * Get device blocksize
 */
int
pca_audio_get_freq(audio_desc_t ad)
{
        UNUSED(ad);
        return format.sample_rate;
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
	diff = (((now.tv_sec  - last_read_time.tv_sec) * 1e6) +
		now.tv_usec - last_read_time.tv_usec) * format.sample_rate * 1e-6;

        if (diff >= (unsigned)format.blocksize) return diff;
        return FALSE;
}

void
pca_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        if (pca_audio_is_ready(ad)) {
                return;
        } else {
                struct timeval delay;
                delay.tv_sec = 0;
                delay.tv_usec = delay_ms * 1000;
                select(0,NULL,NULL,NULL,&delay);
        }
}
