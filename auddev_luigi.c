/*
 * FILE:    auddev_luigi.c - Sound interface for Luigi Rizzo's FreeBSD driver
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1996,1997 University College London
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

#include "assert.h"
#include "config.h"
#include "audio.h"
#include "util.h"
#include <machine/soundcard.h>

static int can_read = FALSE;
static int can_write = FALSE;
static int iport = AUDIO_MICROPHONE;
static audio_format format;

#define BLOCKSIZE 320

#define bat_to_device(x) ((x) * 100 / MAX_AMP)
#define device_to_bat(x) ((x) * MAX_AMP / 100)

int 
audio_open_rw(char rw)
{
	int mode     = AFMT_S16_LE;	/* 16bit linear, little-endian */
	int stereo   = format.num_channels - 1;
	int speed    = format.sample_rate;
	int frag     = BLOCKSIZE;
	int volume, audio_fd;

	switch (rw) {
	case O_RDONLY:
		can_read  = TRUE;
		can_write = FALSE;
		break;
	case O_WRONLY:
		can_read  = FALSE;
		can_write = TRUE;
		break;
	case O_RDWR:
		can_read  = TRUE;
		can_write = TRUE;
		break;
	default:
		abort();
	}

	audio_fd = open("/dev/audio", rw /*| O_NDELAY*/);
	if (audio_fd > 0) {
		if (ioctl(audio_fd, SNDCTL_DSP_SETBLKSIZE, &frag) == -1) {
			printf("Cannot set fragment size (ignored)\n");
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SETFMT, &mode) == -1) || (mode != AFMT_S16_LE)) { 
			printf("ERROR: Audio device doesn't support 16bit linear format!\n");
			exit(1);
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1) || (stereo != (format.num_channels - 1))) {
			printf("ERROR: Audio device doesn't support %d channels!\n", format.num_channels);
			exit(1);
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SPEED, &speed) == -1) || (speed != format.sample_rate)) {
			printf("ERROR: Audio device doesn't support %d sampling rate!\n", format.sample_rate);
			exit(1);
		}

		/* Turn off loopback from input to output... */
		volume = 0;
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &volume);

		/* Zero all inputs */
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_CD), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_MIC), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_LINE), &volume);

		/* Zero all outputs */
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_SYNTH), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_OGAIN), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume);

		volume = 85 << 8 | 85;
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IGAIN), &volume);

		/* Select microphone input. We can't select output source... */
		audio_set_iport(audio_fd, iport);

		/* Set global gain/volume. */
		audio_set_volume(audio_fd, 75);
		audio_set_gain(audio_fd, 75);

		read(audio_fd, &speed, sizeof(speed));
		return (audio_fd);
	} else {
		close(audio_fd);
		can_read  = FALSE;
		can_write = FALSE;
		return (-1);
	}
}

int
audio_open(audio_format fmt)
{
	format = fmt;
	if (audio_duplex(-1)) {
		return audio_open_rw(O_RDWR);
	} else {
		return audio_open_rw(O_WRONLY);
	}
}

/* Close the audio device */
void
audio_close(int audio_fd)
{
	if (audio_fd < 0) return;
	audio_drain(audio_fd);
	close(audio_fd);
}

/* Flush input buffer */
void
audio_drain(int audio_fd)
{
#ifdef DEBUG
	printf("WARNING: audio_drain not yet implemented!\n");
#endif
}

int
audio_duplex(int audio_fd)
{
	return (TRUE);
}

int
audio_read(int audio_fd, sample *buf, int samples)
{
	if (can_read) {
		int l1, len, len0;
		char *base = (char *) buf;

		/* Figure out how many bytes we can read before blocking... */
		ioctl(audio_fd, FIONREAD, &len);
		if (len > (samples * BYTES_PER_SAMPLE))
			len = (samples * BYTES_PER_SAMPLE);

		/* Read the data... */
		for (len0 = len; len ; len -= l1, base += l1) {
			if ((l1 = read(audio_fd, base, len)) < 0) {
				return 0;
			}
		}

		return len0 / BYTES_PER_SAMPLE;
	} else {
		/* The value returned should indicate the time (in audio samples) */
		/* since the last time read was called.                           */
		int                   i;
		int                   diff;
		static struct timeval last_time;
		static struct timeval curr_time;
		static int            first_time = 0;

		if (first_time == 0) {
			gettimeofday(&last_time, NULL);
			first_time = 1;
		}
		gettimeofday(&curr_time, NULL);
		diff = (((curr_time.tv_sec - last_time.tv_sec) * 1e6) + (curr_time.tv_usec - last_time.tv_usec)) / 125;
		if (diff > samples) diff = samples;
		if (diff <      80) diff = 80;
		xmemchk();
		for (i=0; i<diff; i++) {
			buf[i] = L16_AUDIO_ZERO;
		}
		xmemchk();
		last_time = curr_time;
		return diff;
	}
}

int
audio_write(int audio_fd, sample *buf, int samples)
{
	int    done, len;
	char  *p;

	if (can_write) {
		p   = (char *) buf;
		len = samples * BYTES_PER_SAMPLE;
		while (1) {
			if ((done = write(audio_fd, p, len)) == len) {
				break;
			}
			if (errno != EINTR) {
				perror("Error writing device");
				return samples - ((len - done) / BYTES_PER_SAMPLE);
			}
			len -= done;
			p   += done;
		}
		return (samples);
	} else {
		return (samples);
	}
}

/* Set ops on audio device to be non-blocking */
void
audio_non_block(int audio_fd)
{
	int  on = 1;

	if (audio_fd < 0)
		return;
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
#ifdef DEBUG
		fprintf(stderr, "Failed to set non-blocking mode on audio device!\n");
#endif
	}
}

/* Set ops on audio device to block */
void
audio_block(int audio_fd)
{
	int  on = 0;

	if (audio_fd < 0)
		return;
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
#ifdef DEBUG
		fprintf(stderr, "Failed to set blocking mode on audio device!\n");
#endif
	}
}


/* Gain and volume values are in the range 0 - MAX_AMP */
void
audio_set_volume(int audio_fd, int vol)
{
	int volume;

	if (audio_fd < 0)
		return;
	volume = vol << 8 | vol;
	if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Setting volume");
	}
}

int
audio_get_volume(int audio_fd)
{
	int volume;

	if (audio_fd < 0)
		return (0);
	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Getting volume");
	}

	return device_to_bat(volume & 0xff); /* Extract left channel volume */
}

void
audio_set_oport(int audio_fd, int port)
{
	/* There appears to be no-way to select this with OSS... */
	return;
}

int
audio_get_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	return AUDIO_HEADPHONE;
}

int
audio_next_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	return AUDIO_HEADPHONE;
}

void
audio_set_gain(int audio_fd, int gain)
{
	int volume = bat_to_device(gain) << 8 | bat_to_device(gain);

	if (audio_fd < 0) {
		return;
	}
	switch (iport) {
	case AUDIO_MICROPHONE:
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_MIC), &volume) < 0)
			perror("Setting gain");
		break;
	case AUDIO_LINE_IN:
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_LINE), &volume) < 0)
			perror("Setting gain");
		break;
	}
	return;
}

int
audio_get_gain(int audio_fd)
{
	int volume;

	if (audio_fd < 0) {
		return (0);
	}
	switch (iport) {
	case AUDIO_MICROPHONE:
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_MIC), &volume) < 0)
			perror("Getting gain");
		break;
	case AUDIO_LINE_IN:
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_LINE), &volume) < 0)
			perror("Getting gain");
		break;
	}
	return (device_to_bat(volume & 0xff));
}

void
audio_set_iport(int audio_fd, int port)
{
	int recmask, gain, src;

	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_RECMASK), &recmask) == -1) {
		perror("Unable to read recording mask");
		return;
	}

	switch (port) {
	case AUDIO_MICROPHONE:
		src = SOUND_MASK_MIC;
		break;
	case AUDIO_LINE_IN:
		src = SOUND_MASK_LINE;
		break;
	}

	gain = audio_get_gain(audio_fd);
	audio_set_gain(audio_fd, 0);
	if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &src) < 0)) {
		perror("Unable to select recording source");
		return;
	}
	iport = port;
	audio_set_gain(audio_fd, gain);
}

int
audio_get_iport(int audio_fd)
{
	return iport;
}

int
audio_next_iport(int audio_fd)
{
	switch (iport) {
	case AUDIO_MICROPHONE:
		audio_set_iport(audio_fd, AUDIO_LINE_IN);
		break;
	case AUDIO_LINE_IN:
		audio_set_iport(audio_fd, AUDIO_MICROPHONE);
		break;
	default:
		printf("Unknown audio source!\n");
	}
	return (iport);
}

void
audio_switch_out(int audio_fd, cushion_struct *ap)
{
	if (!audio_duplex(audio_fd) && !can_write) {
		audio_close(audio_fd);
		audio_open_rw(O_WRONLY);
	}
}

void
audio_switch_in(int audio_fd)
{
	if (!audio_duplex(audio_fd) && !can_read) {
		audio_close(audio_fd);
		audio_open_rw(O_RDONLY);
	}
}
