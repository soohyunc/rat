/*
 * FILE:    auddev_oss.c - Open Sound System audio device driver
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 * MODS:    Orion Hodson
 *
 * From revision 1.25 of auddev_linux.c
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1996-98 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted,` for non-commercial use only, provided
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

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "debug.h"
#include "memory.h"
#include "audio_types.h"
#include "auddev_oss.h"

int	can_read  = FALSE;
int	can_write = FALSE;
int	iport     = AUDIO_MICROPHONE;
int	is_duplex = FALSE;
int	done_test = FALSE;
int	blocksize;

/* Magic to get device names from OSS */

#define OSS_MAX_DEVICES 3
#define OSS_MAX_NAME_LEN 32
static int ndev;
char   dev_name[OSS_MAX_DEVICES][OSS_MAX_NAME_LEN];

static char the_dev[] = "/dev/audioX";
static int audio_fd = -1;

audio_format format;

#define bat_to_device(x)  ((x) * 100 / MAX_AMP)
#define device_to_bat(x)  ((x) * MAX_AMP / 100)

static int 
oss_audio_open_rw(audio_desc_t ad, char rw)
{
	int  mode     = AFMT_S16_LE;			/* 16bit linear, little-endian */
	int  stereo   = format.num_channels - 1;	/* 0=mono, 1=stereo            */
	int  speed    = format.sample_rate;
	int  volume   = (100<<8)|100;
	int  frag     = 0x7fff0000; 			/* unlimited number of fragments */
	int  reclb    = 0;
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

	audio_fd = open(the_dev, rw);
	if (audio_fd > 0) {
		/* Note: The order in which the device is set up is important! Don't */
		/*       reorder this code unless you really know what you're doing! */
		if ((rw == O_RDWR) && ioctl(audio_fd, SNDCTL_DSP_SETDUPLEX, 0) == -1) {
			printf("ERROR: Cannot enable full-duplex mode!\n");
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
		oss_audio_set_iport(audio_fd, iport);
		/* Turn off loopback from input to output... This only works  */
		/* on a few cards, but shouldn't cause problems on the others */
		ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);
		/* Device driver bug: we must read some data before the ioctl */
		/* to tell us how much data is waiting works....              */
		read(audio_fd, buffer, 128);	
		/* Okay, now we're done...                                    */
                UNUSED(ad);
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
oss_audio_open(audio_desc_t ad, audio_format *fmt)
{
        int mode;

        UNUSED(ad); 

        memcpy(&format, fmt, sizeof(audio_format));

        if (ad <0 || ad>OSS_MAX_DEVICES) {
                debug_msg("Invalid audio descriptor (%d)", ad);
                return FALSE;
        }

        sprintf(the_dev, "/dev/audio%d", ad);

	if (oss_audio_duplex(-1)) {
                mode = O_RDWR;
        } else {
                perror("Half duplex cards not supported\n");
                exit(-1);
                mode = O_WRONLY;
	}
        audio_fd = oss_audio_open_rw(ad, mode);
        return (audio_fd > 0) ? TRUE : FALSE;
}

/* Close the audio device */
void
oss_audio_close(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	oss_audirain(audio_fd);
	close(audio_fd);
        audio_fd = -1;
}

/* Flush input buffer */
void
oss_audio_drain(audio_desc_t ad)
{
        sample buf[160];

        UNUSED(ad); assert(audio_fd > 0);

        while(oss_audio_read(audio_fd, buf, 160) == 160);
}

int
oss_audio_duplex(audio_desc_t ad)
{
	/* Find out if the device supports full-duplex operation. The device must
	 * be open to do this, so if we're passed -1 as a file-descriptor we open
	 * the device, do the ioctl, and then close it again...
	 */
	int info;
	int did_open = FALSE;

        UNUSED(ad); 

	if (done_test) return is_duplex;

	if (ad == -1) {
		audio_fd = open(the_dev, O_RDWR | O_NDELAY);
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
oss_audio_set_gain(audio_desc_t ad, int gain)
{
	int volume = bat_to_device(gain) << 8 | bat_to_device(gain);

        UNUSED(ad); assert(audio_fd > 0);

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
oss_audio_get_gain(audio_desc_t ad)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

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
oss_audio_set_volume(audio_desc_t ad, int vol)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

	volume = vol << 8 | vol;
	if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Setting volume");
	}
}

void
oss_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad); assert(audio_fd > 0);

        gain = gain << 8 | gain;
        if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &gain) == -1) {
                perror("loopback");
        }
}

int
oss_audio_get_volume(audio_desc_t ad)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, MIXER_READ(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Getting volume");
	}
	return device_to_bat(volume & 0x000000ff); /* Extract left channel volume */
}

int
oss_audio_read(audio_desc_t ad, sample *buf, int samples)
{
        UNUSED(ad); assert(audio_fd > 0);

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
oss_audio_write(audio_desc_t ad, sample *buf, int samples)
{
        UNUSED(ad); assert(audio_fd > 0);
	
        if (can_write) {
		int    		 done, len;
		char  		*p;

		p   = (char *) buf;
		len = samples * BYTES_PER_SAMPLE;
		while (1) {
			if ((done = write(audio_fd, p, len)) = len) {
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
oss_audio_non_block(audio_desc_t ad)
{
	int  on = 1;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set non-blocking mode on audio device!\n");
	}
}

/* Set ops on audio device to block */
void
oss_audio_block(audio_desc_t ad)
{
	int  on = 0;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set blocking mode on audio device!\n");
	}
}

void
oss_audio_set_oport(audio_desc_t ad, int port)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd > 0);
	UNUSED(port);
	return;
}

int
oss_audio_get_oport(audio_desc_t ad)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd > 0);
	return AUDIO_HEADPHONE;
}

int
oss_audio_next_oport(audio_desc_t ad)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd > 0);
	return AUDIO_HEADPHONE;
}

void
oss_audio_set_iport(audio_desc_t ad, int port)
{
	int recmask;
	int recsrc;
	int gain;

        UNUSED(ad); assert(audio_fd > 0);

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
			gain = oss_audio_get_gain(audio_fd);
			iport = port;
			oss_audio_set_gain(audio_fd, gain);
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
			gain = oss_audio_get_gain(audio_fd);
			iport = port;
			oss_audio_set_gain(audio_fd, gain);
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
			gain = oss_audio_get_gain(audio_fd);
			iport = port;
			oss_audio_set_gain(audio_fd, gain);
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
oss_audio_get_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return iport;
}

int
oss_audio_next_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        switch (iport) {
	case AUDIO_MICROPHONE : 
		oss_audio_set_iport(audio_fd, AUDIO_LINE_IN);
		break;
	case AUDIO_LINE_IN : 
		oss_audio_set_iport(audio_fd, AUDIO_CD);
		break;
	case AUDIO_CD : 
		oss_audio_set_iport(audio_fd, AUDIO_MICROPHONE);
		break;
	default : 
		debug_msg("Unknown audio source!\n");
	}

	return iport;
}

int
oss_audio_get_blocksize(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return blocksize;
}

int
oss_audio_get_channels(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return format.num_channels;
}

int
oss_audio_get_freq(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return format.sample_rate;
}

static int
oss_audio_select(audio_desc_t ad, int delay_us)
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
oss_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        oss_audio_select(ad, delay_ms * 1000);
}

int 
oss_audio_is_ready(audio_desc_t ad)
{
        return oss_audio_select(ad, 0);
}

void
oss_audio_query_devices()
{
        FILE *f;
        char buf[OSS_MAX_NAME_LEN], *name_start;
        int  found_devices = FALSE;
        
        char devices_tag[] = "Audio devices:";
        int len = strlen(devices_tag);

        f = fopen("/dev/sndstat", "r");
        
        if (f) {
                while(!feof(f)) {
                        fgets(buf, OSS_MAX_NAME_LEN, f);

                        if (!strncmp(buf, devices_tag, len)) {
                                found_devices = TRUE;
                                debug_msg("Found devices entry\n");
                                continue;
                        }
                        if (found_devices) {
                                if ((name_start = strstr(buf, ":")) && ndev < OSS_MAX_DEVICES) {
                                        name_start += 2; /* pass colon plus space */
                                        strcpy(dev_name[ndev], name_start);
                                        ndev++;
                                        debug_msg("OSS device found (%s)\n", name_start);
                                } else {
                                        break;
                                }
                        }
                }
                fclose(f);
        }
}

int
oss_get_device_count()
{
        return ndev;
}

char *
oss_get_device_name(int idx)
{
        if (idx >=0 && idx < ndev) {
                return dev_name[idx];
        }
        debug_msg("Invalid index\n");
        return NULL;
}


#endif /* OSS */
