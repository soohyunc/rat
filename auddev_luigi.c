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

#ifdef FreeBSD
#include "assert.h"
#include "config.h"
#include "audio.h"
#include "util.h"
#include <machine/soundcard.h>

static int can_read = FALSE;
static int can_write = FALSE;
static int iport = AUDIO_MICROPHONE;
static audio_format format;
static snd_chan_param pa;
#define BLOCKSIZE 320

#define bat_to_device(x) ((x) * 100 / MAX_AMP)
#define device_to_bat(x) ((x) * MAX_AMP / 100)

static int 
audio_open_rw(char rw)
{
    int             volume = 100;
    int             reclb = 0;
    int             audio_fd = -1;

    int             d = -1;	/* unit number for audio device */
    char            buf[64];
    char           *thedev;

    switch (rw) {
    case O_RDONLY:
	can_read = TRUE;
	can_write = FALSE;
	break;
    case O_WRONLY:
	can_read = FALSE;
	can_write = TRUE;
	break;
    case O_RDWR:
	can_read = TRUE;
	can_write = TRUE;
	break;
    default:
	abort();
    }

    thedev = getenv("AUDIODEV");
    if (thedev == NULL)
	thedev = "/dev/audio";
    else if (thedev[0] >= '0') {
	d = atoi(thedev);
	sprintf(buf, "/dev/audio%d", d);
	thedev = buf;
    }
    audio_fd = open(thedev, rw);
    if (audio_fd >= 0) {
	struct snd_size sz;
	snd_capabilities soundcaps;
	ioctl(audio_fd, AIOGCAP, &soundcaps);
        ioctl(audio_fd,SNDCTL_DSP_RESET,0);
	pa.play_rate   = pa.rec_rate   = format.sample_rate;
	pa.play_format = pa.rec_format = AFMT_S16_LE;
	sz.play_size   = sz.rec_size   = format.blocksize;

	switch (soundcaps.formats & (AFMT_FULLDUPLEX | AFMT_WEIRD)) {
	case AFMT_FULLDUPLEX:
	    /*
	     * this entry for cards with decent full duplex.
	     */
	    break;
	case AFMT_FULLDUPLEX | AFMT_WEIRD:
	    /* this is the sb16... */
	    pa.play_format = AFMT_S8;
	    sz.play_size = format.blocksize / 2;
	    break;
	default:		/* no full duplex... */
	    if (rw == O_RDWR) {
		fprintf(stderr, "sorry no full duplex support here\n");
		close(audio_fd);
		return -1;
	    }
	}
	ioctl(audio_fd, AIOSFMT, &pa);
	ioctl(audio_fd, AIOSSIZE, &sz);

	/* Set global gain/volume to maximum values. This may fail on */
	/* some cards, but shouldn't cause any harm when it does..... */
	ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume);
	ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IGAIN), &volume);
	/* Set the gain/volume properly. We use the controls for the  */
	/* specific mixer channel to do this, relative to the global  */
	/* maximum gain/volume we've just set...                      */
	audio_set_gain(audio_fd, MAX_AMP / 2);
	audio_set_volume(audio_fd, MAX_AMP / 2);
	/* Select microphone input. We can't select output source...  */
	audio_set_iport(audio_fd, iport);
	/* Turn off loopback from input to output... */
	ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);
	{
	    char            buf[64];
	    read(audio_fd, buf, 64);
	}			/* start... */
	return audio_fd;
    } else {
	perror("audio_open");
	close(audio_fd);
	return -1;
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
	UNUSED(audio_fd);
	dprintf("WARNING: audio_drain not yet implemented!\n");
}

int
audio_duplex(int audio_fd)
{
    /*
     * Find out if the device supports full-duplex operation. The device must
     * be open to do this, so if we're passed -1 as a file-descriptor we open
     * the device, do the ioctl, and then close it again...
     */
    snd_capabilities soundcaps;
    int             fd = audio_fd;

    if (audio_fd == -1)
	fd = audio_open_rw(O_RDWR);
    ioctl(fd, AIOGCAP, &soundcaps);
    if (audio_fd == -1)
	close(fd);
    return (soundcaps.formats & AFMT_FULLDUPLEX) ? 1 : 0;

}

int
audio_read(int audio_fd, sample *buf, int samples)
{
	if (can_read) {
		int             l1, len0;
		unsigned int	len;
		char           *base = (char *) buf;
		/* Figure out how many bytes we can read before blocking... */
		ioctl(audio_fd, FIONREAD, &len);
		if (len > (samples * BYTES_PER_SAMPLE))
			len = (samples * BYTES_PER_SAMPLE);
		/* Read the data... */
		for (len0 = len; len; len -= l1, base += l1) {
			if ((l1 = read(audio_fd, base, len)) < 0) {
				return 0;
			}
		}
		return len0 / BYTES_PER_SAMPLE;
	} else {
		/* The value returned should indicate the time (in audio samples) */
		/* since the last time read was called.                           */
		int             i;
		int             diff;
		static struct timeval last_time;
		static struct timeval curr_time;
		static int      first_time = 0;
		
		if (first_time == 0) {
			gettimeofday(&last_time, NULL);
			first_time = 1;
		}
		gettimeofday(&curr_time, NULL);
		diff = (((curr_time.tv_sec - last_time.tv_sec) * 1e6) + (curr_time.tv_usec - last_time.tv_usec)) / 125;
		if (diff > samples)
			diff = samples;
		if (diff < 80)
			diff = 80;
		xmemchk();
		for (i = 0; i < diff; i++) {
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
	int             done, len, slen;
	char           *p;
	
	if (can_write) {
		p = (char *) buf;
		slen = BYTES_PER_SAMPLE;
		len = samples * BYTES_PER_SAMPLE;
		if (pa.play_format != AFMT_S16_LE) {
			/* soundblaster... S16 -> S8 */
			int             i;
			short          *src = (short *) buf;
			char           *dst = (char *) buf;
			for (i = 0; i < samples; i++)
				dst[i] = src[i] >> 8;
			len = samples;
			slen = 1;
		}
		while (1) {
			if ((done = write(audio_fd, p, len)) == len) {
				break;
			}
			if (errno != EINTR) {
				perror("Error writing device");
				return samples - ((len - done) / slen);
			}
			len -= done;
			p += done;
		}
		return samples;
	} else {
		return samples;
	}
}

/* Set ops on audio device to be non-blocking */
void
audio_non_block(int audio_fd)
{
	int             frag = 1;
#ifdef DEBUG
	fprintf(stderr, "called audio_non_block\n");
#endif
	if (ioctl(audio_fd, SNDCTL_DSP_NONBLOCK, &frag) == -1) {
		perror("Setting non blocking i/o");
	}
}

/* Set ops on audio device to block */
void
audio_block(int audio_fd)
{
	int             frag = 0;
#ifdef DEBUG
	fprintf(stderr, "called audio_block\n");
#endif
	if (ioctl(audio_fd, SNDCTL_DSP_NONBLOCK, &frag) == -1) {
		perror("Setting blocking i/o");
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
	UNUSED(audio_fd);
	UNUSED(port);
	return;
}

int
audio_get_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	UNUSED(audio_fd);
	return AUDIO_HEADPHONE;
}

int
audio_next_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	UNUSED(audio_fd);
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
	case AUDIO_CD:
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_CD), &volume) < 0)
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
	case AUDIO_CD:
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_CD), &volume) < 0)
			perror("Getting gain");
		break;
	default:
		printf("ERROR: Unknown iport in audio_set_gain!\n");
		abort();
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
	case AUDIO_CD:
		src = SOUND_MASK_CD;
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
	UNUSED(audio_fd);
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
		audio_set_iport(audio_fd, AUDIO_CD);
		break;
	case AUDIO_CD:
		audio_set_iport(audio_fd, AUDIO_MICROPHONE);
		break;
	default:
		printf("Unknown audio source!\n");
	}
	return (iport);
}

void
audio_switch_out(int audio_fd, struct s_cushion_struct *ap)
{
	UNUSED(ap);
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

int
audio_get_blocksize()
{
        return format.blocksize;
}

int
audio_get_channels()
{
        return format.num_channels;
}

int
audio_get_freq()
{
        return format.sample_rate;
}

#endif
