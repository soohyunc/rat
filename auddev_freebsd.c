/*
 * FILE:    auddev_freebsd.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins, Jim Lowe (james@cs.uwm.edu), Orion Hodson
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
/*
 * Changes made by Jim Lowe, University of Wisconsin - Milwaukee for
 * Voxware version 3.5 and the pca sound device.
 */


#ifdef FreeBSD

#include "config_unix.h"
#include "config_win32.h"
#include "util.h"
#include "audio.h"

#define	DEV_NONE    0				/* No audio device */
#define DEV_VOXWARE 1				/* Voxware audio device */
#define	DEV_PCA	    2				/* PCA device (the speaker) */
static int	    audio_device = DEV_NONE;	/* audio device we are using */
static audio_info_t dev_info;			/* For PCA device */
static int          can_read  = FALSE;
static int          can_write = FALSE;
static int          iport     = AUDIO_MICROPHONE;
static audio_format format;

/*
 * Voxware audio interface 
 */

#define vox_bat_to_device(x)  ((x) * 100 / MAX_AMP)
#define vox_device_to_bat(x)  ((x) * MAX_AMP / 100)

int     vox_audio_open(audio_format format);
void    vox_audio_close(int audio_fd);
void    vox_audio_drain(int audio_fd);
void    vox_audio_switch_out(int audio_fd, cushion_struct *cushion);      
void    vox_audio_switch_in(int audio_fd);
void    vox_audio_set_igain(int audio_fd, int gain);                       
int     vox_audio_get_igain(int audio_fd);
void    vox_audio_set_ogain(int audio_fd, int vol);
int     vox_audio_get_ogain(int audio_fd);
int     vox_audio_read(int audio_fd, sample *buf, int samples);
int     vox_audio_write(int audio_fd, sample *buf, int samples);
int     vox_audio_is_dry(int audio_fd);
void    vox_audio_non_block(int audio_fd);
void    vox_audio_block(int audio_fd);
int     vox_audio_requested(int audio_fd);
void    vox_audio_set_oport(int audio_fd, int port);
int     vox_audio_get_oport(int audio_fd);
int     vox_audio_next_oport(int audio_fd);
void    vox_audio_set_iport(int audio_fd, int port);
int     vox_audio_get_iport(int audio_fd);
int     vox_audio_next_iport(int audio_fd);
int     vox_audio_duplex(int audio_fd);

static int 
vox_audio_open_rw(char rw)
{
	int mode     = AFMT_S16_LE;	/* 16bit linear, little-endian */
	int stereo   = 0;		/* 0=mono, 1=stereo            */
	int speed    = 8000;
	int frag     = 0x7fff0007;	/* 128 bytes fragments         */
	int volume   = 100;
	int reclb    = 0;
	int audio_fd = -1;
	char buffer[2];			/* sigh. */

	switch (rw) {
	case O_RDONLY: can_read  = TRUE;
		can_write = FALSE;
		break;
	case O_WRONLY: can_read  = FALSE;
		can_write = TRUE;
		break;
	case O_RDWR  : can_read  = TRUE;
		can_write = TRUE;
		break;
	default      : abort();
	}

	audio_fd = open("/dev/audio", rw | O_NDELAY);
	if (audio_fd > 0) {
#if defined(SND_CTL_DSP_SETDUPLEX)
		if ((rw == O_RDWR) && ioctl(audio_fd, SNDCTL_DSP_SETDUPLEX, 0) == -1) {
			/* Cannot enable full-duplex mode. Oh well. */
			printf("ERROR: Cannot enable full-duplex mode!\n");
			printf("       RAT should automatically select half-duplex operation\n");
			printf("       in this case, so this error should never happen......\n");
			exit(1);
		}
#endif
		if (ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag) == -1) {
#ifdef DEBUG
			printf("Cannot set fragment size (ignored)\n");
#endif
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SETFMT, &mode) == -1) || (mode != AFMT_S16_LE)) { 
			printf("ERROR: Audio device doesn't support 16bit linear format!\n");
			return -1;
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1) || (stereo != (format.channels - 1))) {
			printf("ERROR: Audio device doesn't support %d channels!\n", format.channels);
			exit(1);
		}
		if ((ioctl(audio_fd, SNDCTL_DSP_SPEED, &speed) == -1) || (speed != format.sample_rate)) {
			printf("ERROR: Audio device doesn't support %d sampling rate!\n", format.sample_rate);
			exit(1);
		}

		/* Set global gain/volume to maximum values. This may fail on */
		/* some cards, but shouldn't cause any harm when it does..... */ 
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECLEV), &volume);
		/* Set the gain/volume properly. We use the controls for the  */
		/* specific mixer channel to do this, relative to the global  */
		/* maximum gain/volume we've just set...                      */
		vox_audio_set_igain(audio_fd, MAX_AMP / 2);
		vox_audio_set_ogain(audio_fd, MAX_AMP / 2);
		/* Select microphone input. We can't select output source...  */
		vox_audio_set_iport(audio_fd, iport);
		/* Turn off loopback from input to output... */
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);
		read(audio_fd, buffer, 2);	/* Device driver bug in linux-2.0.28: we must read some data before the ioctl */
		/* to tell us how much data is waiting works....                              */
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
vox_audio_open(audio_format fmt)
{
	format = fmt;
	if (vox_audio_duplex(-1)) {
		return vox_audio_open_rw(O_RDWR);
	} else {
		return vox_audio_open_rw(O_WRONLY);
	}
}

/* Close the audio device */
void
vox_audio_close(int audio_fd)
{
	if (audio_fd <0) return;
	vox_audio_drain(audio_fd);
	close(audio_fd);
}

/* Flush input buffer */
void
vox_audio_drain(int audio_fd)
{
#ifdef DEBUG
	printf("WARNING: audio_drain not yet implemented!\n");
#endif
}

int
vox_audio_duplex(int audio_fd)
{
	/* Find out if the device supports full-duplex operation. The device must
	 * be open to do this, so if we're passed -1 as a file-descriptor we open
	 * the device, do the ioctl, and then close it again...
	 */
	int options;
	if (audio_fd == -1) {
		audio_fd = vox_audio_open_rw(O_RDONLY);
		if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &options) == -1) {
			vox_audio_close(audio_fd);
			return FALSE;
		}
		vox_audio_close(audio_fd);
		return (options & DSP_CAP_DUPLEX);
	}
	/* Audio device already open */
	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &options) == -1 ) {
		return FALSE;
	}
	return (options & DSP_CAP_DUPLEX);
}

/* Gain and volume values are in the range 0 - MAX_AMP */
void
vox_audio_set_igain(int audio_fd, int gain)
{
	int volume = vox_bat_to_device(gain) << 8 | vox_bat_to_device(gain);

	if (audio_fd <= 0) {
		return;
	}
	switch (iport) {
	case AUDIO_MICROPHONE : if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_MIC), &volume) == -1) {
		perror("Setting gain");
	}
	return;
	case AUDIO_LINE_IN    : if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_LINE), &volume) == -1) {
		perror("Setting gain");
	}
	return;
	}
	printf("ERROR: Unknown iport in vox_audio_set_igain!\n");
	abort();
}

int
vox_audio_get_igain(int audio_fd)
{
	int volume;

	if (audio_fd <= 0) {
		return (0);
	}
	switch (iport) {
	case AUDIO_MICROPHONE : if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_MIC), &volume) == -1) {
		perror("Getting gain");
	}
	break;
	case AUDIO_LINE_IN    : if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_LINE), &volume) == -1) {
		perror("Getting gain");
	}
	break;
	default               : printf("ERROR: Unknown iport in vox_audio_set_igain!\n");
		abort();
	}
	return vox_device_to_bat(volume & 0xff);
}

void
vox_audio_set_ogain(int audio_fd, int vol)
{
	unsigned int volume;

	if (audio_fd <= 0) {
		return;
	}
	volume = vol << 8 | vol;
	if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Setting volume");
	}
}

int
vox_audio_get_ogain(int audio_fd)
{
	unsigned int volume;

	if (audio_fd <= 0) {
		return (0);
	}
	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_VOLUME), &volume) == -1) {
		perror("Getting volume");
	}
	return vox_device_to_bat(volume & 0x000000ff); /* Extract left channel volume */
}

int
vox_audio_read(int audio_fd, sample *buf, int samples)
{
	if (can_read) {
		int            len, read_len;
		audio_buf_info info;

		/* Figure out how many bytes we can read before blocking... */
		ioctl(audio_fd, SNDCTL_DSP_GETISPACE, &info);
		if (info.bytes > (samples * BYTES_PER_SAMPLE)) {
			read_len = (samples * BYTES_PER_SAMPLE);
		} else {
			read_len = info.bytes;
		}
		/* Read the data... */
		if ((len = read(audio_fd, (char *)buf, read_len)) < 0) {
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
vox_audio_write(int audio_fd, sample *buf, int samples)
{
	int  done, len;
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
		return samples;
	} else {
		return samples;
	}
}

/* Check if the audio output has run out of data */
int
vox_audio_is_dry(int audio_fd)
{
	return 0;
}

/* Set ops on audio device to be non-blocking */
void
vox_audio_non_block(int audio_fd)
{
	int  on = 1;

	if (audio_fd < 0) {
		return;
	}
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
#ifdef DEBUG
		fprintf(stderr, "Failed to set non-blocking mode on audio device!\n");
#endif
	}
}

/* Set ops on audio device to block */
void
vox_audio_block(int audio_fd)
{
	int  on = 0;

	if (audio_fd < 0) {
		return;
	}
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
#ifdef DEBUG
		fprintf(stderr, "Failed to set blocking mode on audio device!\n");
#endif
	}
}

/*
 * Check to see whether another application has requested the
 * audio device.
 */
int
vox_audio_requested(int audio_fd)
{
	return 0;
}

void
vox_audio_set_oport(int audio_fd, int port)
{
	/* There appears to be no-way to select this with OSS... */
	return;
}

int
vox_audio_get_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	return AUDIO_HEADPHONE;
}

int
vox_audio_next_oport(int audio_fd)
{
	/* There appears to be no-way to select this with OSS... */
	return AUDIO_HEADPHONE;
}

void
vox_audio_set_iport(int audio_fd, int port)
{
	int recmask;
	int recsrc;
	int gain;

	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_RECMASK), &recmask) == -1) {
		printf("WARNING: Unable to read recording mask!\n");
		return;
	}
	switch (port) {
	case AUDIO_MICROPHONE : if (recmask & SOUND_MASK_MIC) {
		recsrc = SOUND_MASK_MIC;
		if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_MIC)) {
			printf("WARNING: Unable to select recording source!\n");
			return;
		}
		gain = vox_audio_get_igain(audio_fd);
		iport = port;
		vox_audio_set_igain(audio_fd, gain);
	}
	break;
	case AUDIO_LINE_IN    : if (recmask & SOUND_MASK_LINE) {
		recsrc = SOUND_MASK_LINE;
		if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_LINE)){
			printf("WARNING: Unable to select recording source!\n");
			return;
		}
		gain = vox_audio_get_igain(audio_fd);
		iport = port;
		vox_audio_set_igain(audio_fd, gain);
	}
	break;
	default               : printf("audio_set_port: unknown port!\n");
		abort();
	};
	return;
}

int
vox_audio_get_iport(int audio_fd)
{
	return iport;
}

int
vox_audio_next_iport(int audio_fd)
{
	switch (iport) {
	case AUDIO_MICROPHONE : vox_audio_set_iport(audio_fd, AUDIO_LINE_IN);
		break;
	case AUDIO_LINE_IN    : vox_audio_set_iport(audio_fd, AUDIO_MICROPHONE);
		break;
	default               : printf("Unknown audio source!\n");
	}
	return iport;
}

void
vox_audio_switch_out(int audio_fd, cushion_struct *ap)
{
	if (!vox_audio_duplex(audio_fd) && !can_write) {
		vox_audio_close(audio_fd);
		vox_audio_open_rw(O_WRONLY);
	}
}

void
vox_audio_switch_in(int audio_fd)
{
	if (!vox_audio_duplex(audio_fd) && !can_read) {
		vox_audio_close(audio_fd);
		vox_audio_open_rw(O_RDONLY);
	}
}

/*
 * PCA speaker support for FreeBSD.
 */

#define pca_bat_to_device(x)	((x) * AUDIO_MAX_GAIN / MAX_AMP)
#define pca_device_to_bat(x)	((x) * MAX_AMP / AUDIO_MAX_GAIN)

/*
 * Try to open the audio device.
 * Return: valid file descriptor if ok, -1 otherwise.
 */
int
pca_audio_open(audio_format fmt)
{
	int          audio_fd;
	audio_info_t tmp_info;

	audio_fd = open("/dev/pcaudio", O_WRONLY | O_NDELAY );

	if (audio_fd > 0) {
		format = fmt;
		AUDIO_INITINFO(&dev_info);
		dev_info.monitor_gain     = 0;
		dev_info.play.sample_rate = 8000;
		dev_info.play.channels    = 1;
		dev_info.play.precision   = 8;
		dev_info.play.gain	      = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
		dev_info.play.port	      = 0;
	
		switch(fmt.encoding) {
		case DEV_PCMU:
			assert(format.bits_per_sample == 8);
			dev_info.play.encoding  = AUDIO_ENCODING_ULAW;
			break;
		case DEV_S16:
			assert(format.bits_per_sample == 16);
			dev_info.play.encoding  = AUDIO_ENCODING_ULAW;
			break;
		case DEV_S8:
			assert(format.bits_per_sample == 8);
			dev_info.play.encoding  = AUDIO_ENCODING_RAW;
			break;
		default:
			printf("Unknown audio encoding in pca_audio_open: %x\n", format.encoding);
			abort();
		}
		memcpy(&tmp_info, &dev_info, sizeof(audio_info_t));
		if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&tmp_info) < 0) {
			perror("pca_audio_info: setting parameters");
			return -1;
		}
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
	return audio_fd;
}

/*
 * Shutdown.
 */
void
pca_audio_close(int audio_fd)
{
	if(audio_fd >= 0)
		(void)close(audio_fd);
	audio_fd = -1;
	return;
}

/*
 * Flush input buffer.
 */
void
pca_audio_drain(int audio_fd)
{
	return;
}

/*
 * Switch 1/2 duplex device to playback.
 */
void
pca_audio_switch_out(int audio_fd, cushion_struct *cuhsion)
{
	/* Just leave things well enough alone. */
	return;
}
/*
 * Switch 1/2 duplex device to record.
 */
void
pca_audio_switch_in(int audio_fd)
{
	/* A little difficult to record from the speaker. */
	return;
}

/*
 * Set record gain.
 */
void
pca_audio_set_igain(int audio_fd, int gain)
{
	return;
}

/*
 * Get record gain.
 */
int
pca_audio_get_igain(int audio_fd)
{
	return 0;
}

/*
 * Set play gain.
 */
void
pca_audio_set_ogain(int audio_fd, int vol)
{
	if (audio_fd >= 0) {
		AUDIO_INITINFO(&dev_info);
		dev_info.play.gain = pca_bat_to_device(vol);
		if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) 
			perror("pca_audio_set_ogain");
	}
	return;
}
/*
 * Get play gain.
 */
int
pca_audio_get_ogain(int audio_fd)
{
	if (audio_fd < 0)
		return 0;
	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("pca_audio_get_ogain");
	return pca_device_to_bat(dev_info.play.gain);
}

/*
 * Record audio data.
 */
int
pca_audio_read(int audio_fd, sample *buf, int samples)
{
	/*
	 * Reading data from internal PC speaker is a little difficult,
	 * so just return the time (in audio samples) since the last time called.
	 */
	int	                i;
	int	                diff;
	struct timeval        curr_time;
	static struct timeval last_time;
	static int            virgin = TRUE;

	if (virgin) {
		gettimeofday(&last_time, NULL);
		virgin = FALSE;
		for (i=0; i < 80; i++) {
			buf[i] = L16_AUDIO_ZERO;
		}
		return 80;
	}
	gettimeofday(&curr_time, NULL);
	diff = (((curr_time.tv_sec  - last_time.tv_sec) * 1e6) +
		curr_time.tv_usec - last_time.tv_usec) * format.sample_rate * 1e-6;
	if (diff > samples) diff = samples;	/* don't overrun the buffer */

	xmemchk();
	for (i=0; i < diff; i++) {
		buf[i] = L16_AUDIO_ZERO;
	}
	xmemchk();

	last_time = curr_time;

	return diff;  
}

/*
 * Playback audio data.
 */
int
pca_audio_write(int audio_fd, sample *buf, int samples)
{
	int	 nbytes;
	int    len;
	u_char *p;
	u_char play_buf[DEVICE_REC_BUF];

	if (audio_fd < 0)
		return 0;

	for (nbytes = 0; nbytes < samples; nbytes++)
		if(format.encoding == DEV_S16) 
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
int
pca_audio_is_dry(audio_fd)
{
	return 0;
}

/*
 * Set options on audio device to be non-blocking.
 */
void
pca_audio_non_block(int audio_fd)
{
	int on = 1;

	if (audio_fd < 0)
		return;
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		perror("pca_audio_non_block");
 
	return;
}

/*
 * Set options on audio device to be blocking.
 */
void
pca_audio_block(int audio_fd)
{
	int on = 0;

	if (audio_fd < 0)
		return;
	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0)
		perror("pca_audio_block");
	return;
}
int
pca_audio_requested(int audio_fd)
{
	return 0;
}

/*
 * Set output port.
 */
void
pca_audio_set_oport(int audio_fd, int port)
{
	/* There is only one port... */
	return;
}

/*
 * Get output port.
 */
int
pca_audio_get_oport(int audio_fd)
{
	/* There is only one port... */
	return 0;
}

/*
 * Set next output port.
 */
int
pca_audio_next_oport(int audio_fd)
{
	/* There is only one port... */
	return 0;
}

/*
 * Set input port.
 */
void
pca_audio_set_iport(int audio_fd, int port)
{
	/* Hmmm.... */
	return;
}

/*
 * Get input port.
 */
int
pca_audio_get_iport(int audio_fd)
{
	/* Hmm...hack attack */
	return AUDIO_MICROPHONE;
}
/*
 * Get next input port...
 */
int
pca_audio_next_iport(int audio_fd)
{
	/* Hmm... */
	return AUDIO_MICROPHONE;
}
/*
 * Return 1 if full duplex, 0 if half.
 */
int
pca_audio_duplex(int audio_fd)
{
	/* Speaker is half duplex, then only half of that again :-) */
	return 0;
}

/*
 * FreeBSD audio support.
 */

int
audio_open(audio_format fmt)
{
	int audio_fd;
  
	audio_device = DEV_NONE;
	/*
	 * First we look for a voxware sound card,
	 * then try the PCA device,
	 * if we can't find one of those, then give up.
	 */
	if ((audio_fd=vox_audio_open(fmt)) != -1) {
		audio_device = DEV_VOXWARE;
	}
	if(audio_fd == -1 && (audio_fd=pca_audio_open(fmt)) != -1){
		audio_device = DEV_PCA;
	}
	return audio_fd;
}

void
audio_close(int audio_fd)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_close(audio_fd);
		break;
	case DEV_PCA:
		pca_audio_close(audio_fd);
		break;
	default:
		break;
	}
	audio_device = DEV_NONE;
	return;
}

void
audio_drain(int audio_fd)
{
        sample buf[160];
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_drain(audio_fd);
		break;
	case DEV_PCA:
		pca_audio_drain(audio_fd);
		break;
	default:
		break;
	}
        
        while(audio_read(audio_fd, buf, 160) == 160);

	return;
}

void
audio_switch_out(int audio_fd, cushion_struct *cushion)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_switch_out(audio_fd, cushion);
		break;
	case DEV_PCA:
		pca_audio_switch_out(audio_fd, cushion);
		break;
	default:
		break;
	}
	return;
}
void
audio_switch_in(int audio_fd)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_switch_in(audio_fd);
		break;
	case DEV_PCA:
		pca_audio_switch_in(audio_fd);
		break;
	default:
		break;
	}
	return;
}
void
audio_set_igain(int audio_fd, int gain)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_set_igain(audio_fd, gain);
		break;
	case DEV_PCA:
		pca_audio_set_igain(audio_fd, gain);
		break;
	default:
		break;
	}
	return;
}
int
audio_get_igain(int audio_fd)
{
	int	gain;

	gain = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		gain = vox_audio_get_igain(audio_fd);
		break;
	case DEV_PCA:
		gain = pca_audio_get_igain(audio_fd);
		break;
	default:
		break;
	}
	return gain;
}
void
audio_set_ogain(int audio_fd, int vol)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_set_ogain(audio_fd, vol);
		break;
	case DEV_PCA:
		pca_audio_set_ogain(audio_fd, vol);
		break;
	default:
		break;
	}
	return;
}
int
audio_get_ogain(int audio_fd)
{
	int volume;

	volume = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		volume = vox_audio_get_ogain(audio_fd);
		break;
	case DEV_PCA:
		volume = pca_audio_get_ogain(audio_fd);
		break;
	default:
		break;
	}
	return volume;
}
int
audio_read(int audio_fd, sample *buf, int samples)
{
	int nbytes;

	nbytes = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		nbytes = vox_audio_read(audio_fd, buf, samples);
		break;
	case DEV_PCA:
		nbytes = pca_audio_read(audio_fd, buf, samples);
		break;
	default:
		break;
	}
	return nbytes;
}
int
audio_write(int audio_fd, sample *buf, int samples)
{
	int nbytes;

	nbytes = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		nbytes = vox_audio_write(audio_fd, buf, samples);
		break;
	case DEV_PCA:
		nbytes = pca_audio_write(audio_fd, buf, samples);
		break;
	default:
		break;
	}
	return nbytes;
}
int
audio_is_dry(audio_fd)
{
	int nbytes;

	nbytes = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		nbytes = vox_audio_is_dry(audio_fd);
		break;
	case DEV_PCA:
		nbytes = pca_audio_is_dry(audio_fd);
		break;
	default:
		break;
	}
	return nbytes;
}
void
audio_non_block(int audio_fd)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_non_block(audio_fd);
		break;
	case DEV_PCA:
		pca_audio_non_block(audio_fd);
		break;
	default:
		break;
	}
	return;
}
void
audio_block(int audio_fd)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_block(audio_fd);
		break;
	case DEV_PCA:
		pca_audio_block(audio_fd);
		break;
	default:
		break;
	}
	return;
}
int
audio_requested(int audio_fd)
{
	int ok;

	ok = FALSE;
	switch(audio_device){
	case DEV_VOXWARE:
		ok = vox_audio_requested(audio_fd);
		break;
	case DEV_PCA:
		ok = pca_audio_requested(audio_fd);
		break;
	default:
		break;
	}
	return ok;
}
void
audio_set_oport(int audio_fd, int port)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_set_oport(audio_fd, port);
		break;
	case DEV_PCA:
		pca_audio_set_oport(audio_fd, port);
		break;
	default:
		break;
	}
	return;
}
int
audio_get_oport(int audio_fd)
{
	int port;
  
	port = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		port = vox_audio_get_oport(audio_fd);
		break;
	case DEV_PCA:
		port = pca_audio_get_oport(audio_fd);
		break;
	default:
		break;
	}
	return port;
}
int
audio_next_oport(int audio_fd)
{
	int port;

	port = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		port = vox_audio_next_oport(audio_fd);
		break;
	case DEV_PCA:
		port = pca_audio_next_oport(audio_fd);
		break;
	default:
		break;
	}
	return port;
}
void
audio_set_iport(int audio_fd, int port)
{
	switch(audio_device){
	case DEV_VOXWARE:
		vox_audio_set_iport(audio_fd, port);
		break;
	case DEV_PCA:
		pca_audio_set_iport(audio_fd, port);
		break;
	default:
		break;
	}
	return;
}
int
audio_get_iport(int audio_fd)
{
	int port;

	port = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		port = vox_audio_get_iport(audio_fd);
		break;
	case DEV_PCA:
		port = pca_audio_get_iport(audio_fd);
		break;
	default:
		break;
	}
	return port;
}
int
audio_next_iport(int audio_fd)
{
	int port;

	port = 0;
	switch(audio_device){
	case DEV_VOXWARE:
		port = vox_audio_next_iport(audio_fd);
		break;
	case DEV_PCA:
		port = pca_audio_next_iport(audio_fd);
		break;
	default:
		break;
	}
	return port;
}
int
audio_duplex(int audio_fd)
{
	int duplex;

	duplex = FALSE;
	switch(audio_device){
	case DEV_VOXWARE:
		duplex = vox_audio_duplex(audio_fd);
		break;
	case DEV_PCA:
		duplex = pca_audio_duplex(audio_fd);
		break;
	default:
		break;
	}
	return duplex;
}
#endif /* FreeBSD */


