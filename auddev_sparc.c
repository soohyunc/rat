/*
 * FILE:     auddev_sparc.c
 * PROGRAM:  RAT
 * AUTHOR:   Isidor Kouvelas
 * MODIFIED: Colin Perkins / Orion Hodson
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
#include "assert.h"
#include "debug.h"
#include "audio_types.h"
#include "auddev_sparc.h"
#include "codec_g711.h"
#include "cushion.h"

typedef Audio_hdr* audio_header_pointer;

static audio_info_t	dev_info;
static int 		mulaw_device = FALSE;	/* TRUE if the hardware can only do 8bit mulaw sampling */
static int              blocksize = 0;

static int audio_fd = -1; 

#define bat_to_device(x)	((x) * AUDIO_MAX_GAIN / MAX_AMP)
#define device_to_bat(x)	((x) * MAX_AMP / AUDIO_MAX_GAIN)

/* Try to open the audio device.                        */
/* Returns TRUE if ok, 0 otherwise. */

int
sparc_audio_open(audio_desc_t ad, audio_format* format)
{
	audio_info_t	tmp_info;

        if (audio_fd != -1) {
                debug_msg("Device already open!");
                sparc_audio_close(ad);
                return FALSE;
        }

	audio_fd = open("/dev/audio", O_RDWR | O_NDELAY);

	if (audio_fd > 0) {
		AUDIO_INITINFO(&dev_info);
		dev_info.monitor_gain       = 0;
		dev_info.output_muted       = 0; /* 0==not muted */
		dev_info.play.sample_rate   = format->sample_rate;
		dev_info.record.sample_rate = format->sample_rate;
		dev_info.play.channels      = format->num_channels;
		dev_info.record.channels    = format->num_channels;
		dev_info.play.precision     = format->bits_per_sample;
		dev_info.record.precision   = format->bits_per_sample;
		dev_info.play.gain          = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
		dev_info.record.gain        = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
		dev_info.play.port          = AUDIO_HEADPHONE;
		dev_info.record.port        = AUDIO_MICROPHONE;
		dev_info.play.balance       = AUDIO_MID_BALANCE;
		dev_info.record.balance     = AUDIO_MID_BALANCE;
#ifdef Solaris
		dev_info.play.buffer_size   = DEVICE_BUF_UNIT * (format->sample_rate / 8000) * (format->bits_per_sample / 8);
		dev_info.record.buffer_size = DEVICE_BUF_UNIT * (format->sample_rate / 8000) * (format->bits_per_sample / 8);
#ifdef DEBUG
		debug_msg("Setting device buffer_size to %d\n", dev_info.play.buffer_size);
#endif /* DEBUG */
#endif /* Solaris */
                blocksize = format->blocksize;
                switch (format->encoding) {
		case DEV_PCMU:
			dev_info.record.encoding = AUDIO_ENCODING_ULAW;
			dev_info.play.encoding   = AUDIO_ENCODING_ULAW;
			break;
		case DEV_L8:
			assert(format->bits_per_sample == 8);
			dev_info.record.encoding = AUDIO_ENCODING_LINEAR;
			dev_info.play.encoding   = AUDIO_ENCODING_LINEAR;
			break;
		case DEV_L16:
			assert(format->bits_per_sample == 16);
			dev_info.record.encoding = AUDIO_ENCODING_LINEAR;
			dev_info.play.encoding   = AUDIO_ENCODING_LINEAR;
			break;
		default:
			debug_msg("ERROR: Unknown audio encoding in audio_open!\n");
			abort();
                }

		memcpy(&tmp_info, &dev_info, sizeof(audio_info_t));
		if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&tmp_info) < 0) {
			if (format->encoding == DEV_L16) {
#ifdef DEBUG
				debug_msg("Old hardware detected: can't do 16 bit audio, trying 8 bit...\n");
#endif
				dev_info.play.precision = 8;
				dev_info.record.precision = 8;
				dev_info.record.encoding = AUDIO_ENCODING_ULAW;
				dev_info.play.encoding = AUDIO_ENCODING_ULAW;
				if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) {
					perror("Setting MULAW audio paramterts");
					return FALSE;
				}
				mulaw_device = TRUE;
			} else {
				perror("Setting audio paramterts");
				return FALSE;
			}
		}
		return audio_fd;
	} else {
		/* Because we opened the device with O_NDELAY
		 * the waiting flag was not updated so update
		 * it manually using the audioctl device...
		 */
		audio_fd = open("/dev/audioctl", O_RDWR);
		AUDIO_INITINFO(&dev_info);
		dev_info.play.waiting = 1;
		if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) {
#ifdef DEBUG
			perror("Setting requests");
#endif
		}
                if (audio_fd > 0) {
                        sparc_audio_close(audio_fd);
                }
		return FALSE;
	}
}

/* Close the audio device */
void
sparc_audio_close(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	if (audio_fd <= 0) {
                debug_msg("Invalid desc");
		return;
        }

	close(audio_fd);
	audio_fd = -1;
}

/* Flush input buffer */
void
sparc_audio_drain(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	ioctl(audio_fd, I_FLUSH, (caddr_t)FLUSHR);
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
sparc_audio_set_gain(audio_desc_t ad, int gain)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	dev_info.record.gain = bat_to_device(gain);
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting gain");
}

int
sparc_audio_get_gain(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting gain");
	return (device_to_bat(dev_info.record.gain));
}

void
sparc_audio_set_volume(audio_desc_t ad, int vol)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	dev_info.play.gain = bat_to_device(vol);
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting volume");
}

int
sparc_audio_get_volume(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting gain");
	return (device_to_bat(dev_info.play.gain));
}

void
sparc_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad); assert(audio_fd > 0);

        /* Nasty bug on Ultra 30's a loopback gain of anything above
         * 90 on my machine is unstable.  On the earlier Ultra's
         * anything below 90 produces no perceptible loopback.
         */
        gain = gain * 85/100;

        AUDIO_INITINFO(&dev_info);
	dev_info.monitor_gain = bat_to_device(gain);
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting loopback");
}

int
sparc_audio_read(audio_desc_t ad, sample *buf, int samples)
{
	int	i, len;
	static u_char mulaw_buf[DEVICE_REC_BUF];
	u_char	*p;

        UNUSED(ad); assert(audio_fd > 0);

	if (mulaw_device) {
		if ((len = read(audio_fd, mulaw_buf, samples)) < 0) {
			return 0;
		} else {
			p = mulaw_buf;
			for (i = 0; i < len; i++) {
				*buf++ = u2s((unsigned)*p);
				p++;
			}
			return (len);
		}
	} else {
		if ((len = read(audio_fd, (char *)buf, samples * BYTES_PER_SAMPLE)) < 0) {
			return 0;
		} else {
			return (len / BYTES_PER_SAMPLE);
		}
	}
}

int
sparc_audio_write(audio_desc_t ad, sample *buf, int samples)
{
	int		i, done, len, bps;
	unsigned char	*p, *q;
	static u_char mulaw_buf[DEVICE_REC_BUF];

        UNUSED(ad); assert(audio_fd > 0);

	if (mulaw_device) {
		p = mulaw_buf;
		for (i = 0; i < samples; i++)
			*p++ = lintomulaw[(unsigned short)*buf++];
		p = mulaw_buf;
		len = samples;
		bps = 1;
	} else {
		p = (char *)buf;
		len = samples * BYTES_PER_SAMPLE;
		bps = BYTES_PER_SAMPLE;
	}

	q = p;
	while (1) {
		if ((done = write(audio_fd, p, len)) == len)
			break;
		if (errno != EINTR)
			return (samples - ((len - done) / bps));
		len -= done;
		p += done;
	}

	return (samples);
}

/* Set ops on audio device to be non-blocking */
void
sparc_audio_non_block(audio_desc_t ad)
{
	int	on = 1;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		fprintf(stderr, "Failed to set non blocking mode on audio device!\n");
}

/* Set ops on audio device to block */
void
sparc_audio_block(audio_desc_t ad)
{
	int	on = 0;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		fprintf(stderr, "Failed to set blocking mode on audio device!\n");
}

void
sparc_audio_set_oport(audio_desc_t ad, int port)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	/* AUDIO_SPEAKER or AUDIO_HEADPHONE */
	dev_info.play.port = port;
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting port");
}

int
sparc_audio_get_oport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting port");
	return (dev_info.play.port);
}

int
sparc_audio_next_oport(audio_desc_t ad)
{
	int	port;

        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting port");
	
	port = dev_info.play.port;
	port <<= 1;

	/* It is either wrong on some machines or i got something wrong! */
	if (dev_info.play.avail_ports < 3)
		dev_info.play.avail_ports = 3;

	if ((port & dev_info.play.avail_ports) == 0)
		port = 1;

	AUDIO_INITINFO(&dev_info);
	dev_info.play.port = port;
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting port");

	return (port);
}

void
sparc_audio_set_iport(audio_desc_t ad, int port)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	dev_info.record.port = port;
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting port");
}

int
sparc_audio_get_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting port");
	return (dev_info.record.port);
}

int
sparc_audio_next_iport(audio_desc_t ad)
{
	int	port;

        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting port");

	port = dev_info.record.port;
	port <<= 1;

	if (dev_info.record.avail_ports > 3)
		dev_info.record.avail_ports = 3;

	/* Hack to fix Sparc 5 SOLARIS bug */
	if ((port & dev_info.record.avail_ports) == 0)
		port = 1;

	AUDIO_INITINFO(&dev_info);
	dev_info.record.port = port;
	if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0)
		perror("Setting port");

	return (port);
}

int
sparc_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return 1;
}

int 
sparc_audio_get_blocksize(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return blocksize;
}

int
sparc_audio_get_channels(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return dev_info.play.channels;
}

int
sparc_audio_get_freq(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return dev_info.play.sample_rate;
}

static int
sparc_audio_select(audio_desc_t ad, int delay_us)
{
        fd_set rfds;
        struct timeval tv;

        UNUSED(ad); assert(audio_fd > 0);
        
        tv.tv_sec = 0;
        tv.tv_usec = delay_us;

        FD_ZERO(&rfds);
        FD_SET(audio_fd, &rfds);

        select(audio_fd+1, &rfds, NULL, NULL, &tv);

        return FD_ISSET(audio_fd, &rfds);
}

void
sparc_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        UNUSED(ad); assert(audio_fd > 0);
        sparc_audio_select(ad, delay_ms * 1000);
}

int 
sparc_audio_is_ready(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        return sparc_audio_select(ad, 0);
}

