/*
 * FILE:    auddev_oss.c - Open Sound System audio device driver
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * From revision 1.25 of auddev_linux.c
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

#if defined(OSS)||defined(Linux)

#include "assert.h"
#include "config.h"
#include "audio.h"
#include "util.h"

#include <sys/soundcard.h>

int	can_read  = FALSE;
int	can_write = FALSE;
int	iport     = AUDIO_MICROPHONE;
int	is_duplex = FALSE;
int	done_test = FALSE;
int	blocksize;

audio_format format;

#define bat_to_device(x)  ((x) * 100 / MAX_AMP)
#define device_to_bat(x)  ((x) * MAX_AMP / 100)

static int 
audio_open_rw(char rw)
{
	int  mode     = AFMT_S16_LE;			/* 16bit linear, little-endian */
	int  stereo   = format.num_channels - 1;	/* 0=mono, 1=stereo            */
	int  speed    = format.sample_rate;
	int  volume   = (100<<8)|100;
	int  frag     = 0x7fff0000; 			/* unlimited number of fragments */
	int  reclb    = 0;
	int  audio_fd = -1;
	char buffer[128];				/* sigh. */

	/* Calculate the size of the fragments we want... 20ms worth of audio data... */
	blocksize = DEVICE_BUF_UNIT * (format.sample_rate / 8000) * (format.bits_per_sample / 8);
	/* Round to the nearest legal frag size (next power of two lower...) */
	frag |= (int) (log(blocksize)/log(2));
	debug_msg("frag=%x blocksize=%d\n", frag, blocksize);

	switch (rw) {
	case O_RDONLY: 
		can_read  = TRUE;
		can_write = FALSE;
		break;
	case O_WRONLY: 
		can_read  = FALSE;
		can_write = TRUE;
		break;
	case O_RDWR  : 
		can_read  = TRUE;
		can_write = TRUE;
		break;
	default : 
		printf("Unknown r/w mode!\n");
		abort();
	}

	audio_fd = open("/dev/audio", rw);
	if (audio_fd > 0) {
		/* Note: The order in which the device is set up is important! Don't */
		/*       reorder this code unless you really know what you're doing! */
		if ((rw == O_RDWR) && ioctl(audio_fd, SNDCTL_DSP_SETDUPLEX, 0) == -1) {
			printf("ERROR: Cannot enable full-duplex mode!\n");
			printf("       RAT should automatically select half-duplex operation\n");
			printf("       in this case, so this error should never happen......\n");
			abort();
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag) == -1)) {
			printf("ERROR: Cannot set the fragement size\n");
			abort();
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SETFMT, &mode) == -1) || (mode != AFMT_S16_LE)) { 
			printf("ERROR: Audio device doesn't support 16bit linear format!\n");
			abort();
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1) || (stereo != (format.num_channels - 1))) {
			printf("ERROR: Audio device doesn't support %d channels!\n", format.num_channels);
			abort();
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SPEED, &speed) == -1) || (speed != format.sample_rate)) {
			printf("ERROR: Audio device doesn't support %d sampling rate!\n", format.sample_rate);
			abort();
		}
		/* Set global gain/volume to maximum values. This may fail on */
		/* some cards, but shouldn't cause any harm when it does..... */ 
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECLEV), &volume);
		/* Select microphone input. We can't select output source...  */
		audio_set_iport(audio_fd, iport);
		/* Turn off loopback from input to output... This only works  */
		/* on a few cards, but shouldn't cause problems on the others */
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);
		/* Device driver bug: we must read some data before the ioctl */
		/* to tell us how much data is waiting works....              */
		read(audio_fd, buffer, 128);	
		/* Okay, now we're done...                                    */
		return audio_fd;
	} else {
		close(audio_fd);
		can_read  = FALSE;
		can_write = FALSE;
		return -1;
	}
}

/* Try to open the audio device.              */
/* Return TRUE if successful FALSE otherwise. */
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
}

int
audio_duplex(int audio_fd)
{
	/* Find out if the device supports full-duplex operation. The device must
	 * be open to do this, so if we're passed -1 as a file-descriptor we open
	 * the device, do the ioctl, and then close it again...
	 */
	int info;
	int did_open = FALSE;

	if (done_test) return is_duplex;

	if (audio_fd == -1) {
		audio_fd = open("/dev/audio", O_RDWR | O_NDELAY);
		did_open = TRUE;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_SETDUPLEX, 0) == -1) {
		if (did_open) close(audio_fd);
		is_duplex = FALSE;
		done_test = TRUE;
		return FALSE;
	}
	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &info) == -1) {
		if (did_open) close(audio_fd);
		is_duplex = FALSE;
		done_test = TRUE;
		return FALSE;
	}

	if (did_open) {
		close(audio_fd);
	}

	is_duplex = info & DSP_CAP_DUPLEX;
	done_test = TRUE;
	debug_msg("%s duplex audio\n", is_duplex?"Full":"Half");
	return is_duplex;
}

/* Gain and volume values are in the range 0 - MAX_AMP */
void
audio_set_gain(int audio_fd, int gain)
{
	int volume = bat_to_device(gain) << 8 | bat_to_device(gain);

	if (audio_fd < 0) {
		return;
	}
	switch (iport) {
	case AUDIO_MICROPHONE : 
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_MIC), &volume) == -1) {
			perror("Setting gain");
		}
		return;
	case AUDIO_LINE_IN : 
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_LINE), &volume) == -1) {
			perror("Setting gain");
		}
		return;
	case AUDIO_CD:
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_CD), &volume) < 0) {
			perror("Setting gain");
		}
		return;
	}
	printf("ERROR: Unknown iport in audio_set_gain!\n");
	abort();
}

int
audio_get_gain(int audio_fd)
{
	int volume;

	if (audio_fd < 0) {
		return (0);
	}
	switch (iport) {
	case AUDIO_MICROPHONE : 
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_MIC), &volume) == -1) {
			perror("Getting gain");
		}
		break;
	case AUDIO_LINE_IN : 
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_LINE), &volume) == -1) {
			perror("Getting gain");
		}
		break;
	case AUDIO_CD:
		if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_CD), &volume) < 0) {
			perror("Getting gain");
		}
		break;
	default : 
		printf("ERROR: Unknown iport in audio_set_gain!\n");
		abort();
	}
	return device_to_bat(volume & 0xff);
}

void
audio_set_volume(int audio_fd, int vol)
{
	int volume;

	if (audio_fd < 0) {
		return;
	}
	volume = vol << 8 | vol;
if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Setting volume");
	}
}

int
audio_get_volume(int audio_fd)
{
	int volume;

	if (audio_fd < 0) {
		return (0);
	}
	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Getting volume");
	}
	return device_to_bat(volume & 0x000000ff); /* Extract left channel volume */
}

int
audio_read(int audio_fd, sample *buf, int samples)
{
	if (can_read) {
		int            len, read_len;
		audio_buf_info info;

		/* Figure out how many bytes we can read before blocking... */
		ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info);
		if (info.bytes > (int) (samples * BYTES_PER_SAMPLE)) {
			read_len = (samples * BYTES_PER_SAMPLE);
		} else {
			read_len = info.bytes;
		}
		if ((len = read(audio_fd, (char *)buf, read_len)) < 0) {
			perror("audio_read");
			return 0;
		}
		return len / BYTES_PER_SAMPLE;
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
	if (can_write) {
		int    		 done, len;
		char  		*p;

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
		return samples;
	} else {
		return samples;
	}
}

/* Set ops on audio device to be non-blocking */
void
audio_non_block(int audio_fd)
{
	int  on = 1;

	if (audio_fd < 0) {
		return;
	}
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set non-blocking mode on audio device!\n");
	}
}

/* Set ops on audio device to block */
void
audio_block(int audio_fd)
{
	int  on = 0;

	if (audio_fd < 0) {
		return;
	}
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set blocking mode on audio device!\n");
	}
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
audio_set_iport(int audio_fd, int port)
{
	int recmask;
	int recsrc;
	int gain;

	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_RECMASK), &recmask) == -1) {
		debug_msg("WARNING: Unable to read recording mask!\n");
		return;
	}
	switch (port) {
	case AUDIO_MICROPHONE : 
		if (recmask & SOUND_MASK_MIC) {
			recsrc = SOUND_MASK_MIC;
			if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_MIC)) {
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = audio_get_gain(audio_fd);
			iport = port;
			audio_set_gain(audio_fd, gain);
		} else {
			debug_msg("Audio device doesn't support recording from microphone\n");
		}
		break;
	case AUDIO_LINE_IN : 
		if (recmask & SOUND_MASK_LINE) {
			recsrc = SOUND_MASK_LINE;
			if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_LINE)){
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = audio_get_gain(audio_fd);
			iport = port;
			audio_set_gain(audio_fd, gain);
		} else {
			debug_msg("Audio device doesn't support recording from line-input\n");
		}
		break;
	case AUDIO_CD:
		if (recmask & SOUND_MASK_CD) {
			recsrc = SOUND_MASK_CD;
			if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_LINE)){
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = audio_get_gain(audio_fd);
			iport = port;
			audio_set_gain(audio_fd, gain);
		} else {
			debug_msg("Audio device doesn't support recording from CD\n");
		}
		break;
	default : 
		debug_msg("audio_set_port: unknown port!\n");
		abort();
	};
	return;
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
	case AUDIO_MICROPHONE : 
		audio_set_iport(audio_fd, AUDIO_LINE_IN);
		break;
	case AUDIO_LINE_IN : 
		audio_set_iport(audio_fd, AUDIO_CD);
		break;
	case AUDIO_CD : 
		audio_set_iport(audio_fd, AUDIO_MICROPHONE);
		break;
	default : 
		debug_msg("Unknown audio source!\n");
	}
	return iport;
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
audio_get_blocksize(void)
{
	return blocksize;
}

int
audio_get_channels()
{
	return format.num_channels;
}

#endif /* OSS */
