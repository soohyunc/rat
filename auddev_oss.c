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
#include "debug.h"
#include "memory.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_oss.h"
#include "util.h"

int	iport     = AUDIO_MICROPHONE;
int	bytes_per_block;

/* Magic to get device names from OSS */

#define OSS_MAX_DEVICES 3
#define OSS_MAX_NAME_LEN 64

static int ndev;
char   dev_name[OSS_MAX_DEVICES][OSS_MAX_NAME_LEN];

static char the_dev[] = "/dev/audioX";
static int audio_fd[OSS_MAX_DEVICES];


#define OSS_MAX_SUPPORTED_FORMATS 8
/* == 8k,16k,32,48k * mono, stereo */
static u_char       have_probed[OSS_MAX_DEVICES];
static audio_format af_sup[OSS_MAX_DEVICES][OSS_MAX_SUPPORTED_FORMATS];
static int          n_af_sup[OSS_MAX_DEVICES];

static oss_probe_formats(audio_desc_t);

audio_format format;

#define bat_to_device(x)  ((x) * 100 / MAX_AMP)
#define device_to_bat(x)  ((x) * MAX_AMP / 100)

static int
deve2oss(deve_e encoding)
{
        switch(encoding) {
        case DEV_PCMU: return AFMT_MU_LAW;
        case DEV_S8:   return AFMT_S8;
        case DEV_S16:  return AFMT_S16_LE;
        }
        abort();
}

/* Try to open the audio device.              */
/* Return TRUE if successful FALSE otherwise. */
int
oss_audio_open(audio_desc_t ad, audio_format *ifmt, audio_format *ofmt)
{
	int  mode, stereo, speed;
	int  volume   = (100<<8)|100;
	int  frag     = 0x7fff0000; 			/* unlimited number of fragments */
	int  reclb    = 0;
	char buffer[128];				/* sigh. */

        if (ad <0 || ad>OSS_MAX_DEVICES) {
                debug_msg("Invalid audio descriptor (%d)", ad);
                return FALSE;
        }

        sprintf(the_dev, "/dev/audio%d", ad);

	audio_fd[ad] = open(the_dev, O_RDWR | O_NDELAY);
	if (audio_fd[ad] > 0) {
		/* Note: The order in which the device is set up is important! Don't */
		/*       reorder this code unless you really know what you're doing! */
		if (ioctl(audio_fd[ad], SNDCTL_DSP_SETDUPLEX, 0) == -1) {
			fprintf(stderr, "ERROR: Cannot enable full-duplex mode!\n");
                        return FALSE;
		}

                mode = deve2oss(ifmt->encoding);
                
		if ((ioctl(audio_fd[ad], SNDCTL_DSP_SETFMT, &mode) == -1)) {
                        if (ifmt->encoding == DEV_S16) {
                                audio_format_change_encoding(ifmt, DEV_PCMU);
                                audio_format_change_encoding(ofmt, DEV_PCMU);
                                if ((ioctl(audio_fd[ad], SNDCTL_DSP_SETFMT, &mode) == -1)) {
                                        oss_audio_close(ad);
                                        return FALSE;
                                }
                                debug_msg("Using mu-law\n");
                        }
		}

                if (!have_probed[ad]) {
                        oss_probe_formats(ad);
                        have_probed[ad] = TRUE;
                }

                /* 20 ms blocksize - only modulates read sizes */
                bytes_per_block = 20 * (ifmt->sample_rate / 1000) * (ifmt->bits_per_sample / 8);
                /* Round to the nearest legal frag size (next power of two lower...) */
                frag |= (int) (log(bytes_per_block)/log(2));
                debug_msg("frag=%x bytes_per_block=%d\n", frag, bytes_per_block);
		if ((ioctl(audio_fd[ad], SNDCTL_DSP_SETFRAGMENT, &frag) == -1)) {
			fprintf(stderr, "ERROR: Cannot set the fragement size\n");
		}

                stereo = ifmt->channels - 1; 
                assert(stereo == 0 || stereo == 1);
		if ((ioctl(audio_fd[ad], SNDCTL_DSP_STEREO, &stereo) == -1) || (stereo != (ifmt->channels - 1))) {
			printf("ERROR: Audio device doesn't support %d channels!\n", ifmt->channels);
                        oss_audio_close(ad);
                        return FALSE;
		}

                speed = ifmt->sample_rate;
		if ((ioctl(audio_fd[ad], SNDCTL_DSP_SPEED, &speed) == -1) || (speed != ifmt->sample_rate)) {
			printf("ERROR: Audio device doesn't support %d sampling rate!\n", ifmt->sample_rate);
                        oss_audio_close(ad);
                        return FALSE;
		}

		/* Set global gain/volume to maximum values. This may fail on */
		/* some cards, but shouldn't cause any harm when it does..... */ 
		ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_VOLUME), &volume);
		ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_RECLEV), &volume);
		/* Select microphone input. We can't select output source...  */
		oss_audio_set_iport(ad, iport);
		/* Turn off loopback from input to output... This only works  */
		/* on a few cards, but shouldn't cause problems on the others */
		ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);
		/* Device driver bug: we must read some data before the ioctl */
		/* to tell us how much data is waiting works....              */
		read(audio_fd[ad], buffer, 128);	
		return audio_fd[ad];
	} else {
		oss_audio_close(ad);
		return -1;
	}
}

/* Close the audio device */
void
oss_audio_close(audio_desc_t ad)
{
        assert(audio_fd[ad] > 0);
	oss_audio_drain(ad);
	close(audio_fd[ad]);
        audio_fd[ad] = -1;
}

/* Flush input buffer */
void
oss_audio_drain(audio_desc_t ad)
{
        u_char buf[160];

        assert(ad < OSS_MAX_DEVICES);
        assert(audio_fd[ad] > 0);

        while(oss_audio_read(ad, buf, 160) == 160);
}

int
oss_audio_duplex(audio_desc_t ad)
{
        /* We don't open device if not full duplex. */
        UNUSED(ad);
        return TRUE;
}

/* Gain and volume values are in the range 0 - MAX_AMP */
void
oss_audio_set_gain(audio_desc_t ad, int gain)
{

	int volume = bat_to_device(gain) << 8 | bat_to_device(gain);

        assert(ad < OSS_MAX_DEVICES);
        assert(audio_fd[ad] > 0);

	switch (iport) {
	case AUDIO_MICROPHONE : 
		if (ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_MIC), &volume) == -1) {
			perror("Setting gain");
		}
		return;
	case AUDIO_LINE_IN : 
		if (ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_LINE), &volume) == -1) {
			perror("Setting gain");
		}
		return;
	case AUDIO_CD:
		if (ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_CD), &volume) < 0) {
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

        UNUSED(ad); assert(audio_fd[ad] > 0); assert(ad < OSS_MAX_DEVICES);

	switch (iport) {
	case AUDIO_MICROPHONE : 
		if (ioctl(audio_fd[ad], MIXER_READ(SOUND_MIXER_MIC), &volume) == -1) {
			perror("Getting gain");
		}
		break;
	case AUDIO_LINE_IN : 
		if (ioctl(audio_fd[ad], MIXER_READ(SOUND_MIXER_LINE), &volume) == -1) {
			perror("Getting gain");
		}
		break;
	case AUDIO_CD:
		if (ioctl(audio_fd[ad], MIXER_READ(SOUND_MIXER_CD), &volume) < 0) {
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

        UNUSED(ad); assert(audio_fd[ad] > 0);

	volume = vol << 8 | vol;
	if (ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Setting volume");
	}
}

void
oss_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad); assert(audio_fd[ad] > 0);

        gain = gain << 8 | gain;
        if (ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_IMIX), &gain) == -1) {
                perror("loopback");
        }
}

int
oss_audio_get_volume(audio_desc_t ad)
{
	int volume;

        UNUSED(ad); assert(audio_fd[ad] > 0);

	if (ioctl(audio_fd[ad], MIXER_READ(SOUND_MIXER_PCM), &volume) == -1) {
		perror("Getting volume");
	}
	return device_to_bat(volume & 0x000000ff); /* Extract left channel volume */
}

int
oss_audio_read(audio_desc_t ad, u_char *buf, int read_bytes)
{
        int read_len;
        audio_buf_info info;

        assert(audio_fd[ad] > 0);        

        /* Figure out how many bytes we can read before blocking... */
        ioctl(audio_fd[ad], SNDCTL_DSP_GETISPACE, &info);

        read_len = min(info.bytes, read_bytes);
        if ((read_len = read(audio_fd[ad], (char *)buf, read_len)) < 0) {
                perror("audio_read");
			return 0;
        }

        return read_len;
}

int
oss_audio_write(audio_desc_t ad, u_char *buf, int write_bytes)
{
        int    		 done, len;
        char  		*p;

        assert(audio_fd[ad] > 0);
        
        p   = (char *) buf;
        len = write_bytes;
        while (1) {
                if ((done = write(audio_fd[ad], p, len)) == len) {
                        break;
                }
                if (errno != EINTR) {
				perror("Error writing device");
				return write_bytes - (len - done);
                }
                len -= done;
                p   += done;
        }
        return write_bytes;
}

/* Set ops on audio device to be non-blocking */
void
oss_audio_non_block(audio_desc_t ad)
{
	int  on = 1;

        UNUSED(ad); assert(audio_fd[ad] > 0);

	if (ioctl(audio_fd[ad], FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set non-blocking mode on audio device!\n");
	}
}

/* Set ops on audio device to block */
void
oss_audio_block(audio_desc_t ad)
{
	int  on = 0;

        UNUSED(ad); assert(audio_fd[ad] > 0);

	if (ioctl(audio_fd[ad], FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set blocking mode on audio device!\n");
	}
}

void
oss_audio_set_oport(audio_desc_t ad, int port)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd[ad] > 0);
	UNUSED(port);
	return;
}

int
oss_audio_get_oport(audio_desc_t ad)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd[ad] > 0);
	return AUDIO_HEADPHONE;
}

int
oss_audio_next_oport(audio_desc_t ad)
{
	/* There appears to be no-way to select this with OSS... */
        UNUSED(ad); assert(audio_fd[ad] > 0);
	return AUDIO_HEADPHONE;
}

void
oss_audio_set_iport(audio_desc_t ad, int port)
{
	int recmask;
	int recsrc;
	int gain;

        UNUSED(ad); assert(audio_fd[ad] > 0);

	if (ioctl(audio_fd[ad], MIXER_READ(SOUND_MIXER_RECMASK), &recmask) == -1) {
		debug_msg("WARNING: Unable to read recording mask!\n");
		return;
	}
	switch (port) {
	case AUDIO_MICROPHONE : 
		if (recmask & SOUND_MASK_MIC) {
			recsrc = SOUND_MASK_MIC;
			if ((ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_MIC)) {
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = oss_audio_get_gain(ad);
			iport = port;
			oss_audio_set_gain(ad, gain);
		} else {
			debug_msg("Audio device doesn't support recording from microphone\n");
		}
		break;
	case AUDIO_LINE_IN : 
		if (recmask & SOUND_MASK_LINE) {
			recsrc = SOUND_MASK_LINE;
			if ((ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_LINE)){
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = oss_audio_get_gain(ad);
			iport = port;
			oss_audio_set_gain(ad, gain);
		} else {
			debug_msg("Audio device doesn't support recording from line-input\n");
		}
		break;
	case AUDIO_CD:
		if (recmask & SOUND_MASK_CD) {
			recsrc = SOUND_MASK_CD;
			if ((ioctl(audio_fd[ad], MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc) == -1) && !(recsrc & SOUND_MASK_LINE)){
				debug_msg("WARNING: Unable to select recording source!\n");
				return;
			}
			gain = oss_audio_get_gain(ad);
			iport = port;
			oss_audio_set_gain(ad, gain);
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
        UNUSED(ad); assert(audio_fd[ad] > 0);
	return iport;
}

int
oss_audio_next_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd[ad] > 0);

        switch (iport) {
	case AUDIO_MICROPHONE : 
		oss_audio_set_iport(ad, AUDIO_LINE_IN);
		break;
	case AUDIO_LINE_IN : 
		oss_audio_set_iport(ad, AUDIO_CD);
		break;
	case AUDIO_CD : 
		oss_audio_set_iport(ad, AUDIO_MICROPHONE);
		break;
	default : 
		debug_msg("Unknown audio source!\n");
	}

	return iport;
}

static int
oss_audio_select(audio_desc_t ad, int delay_us)
{
        fd_set rfds;
        struct timeval tv;

        UNUSED(ad); assert(audio_fd[ad] > 0);
        
        tv.tv_sec = 0;
        tv.tv_usec = delay_us;

        FD_ZERO(&rfds);
        FD_SET(audio_fd[ad], &rfds);

        select(audio_fd[ad]+1, &rfds, NULL, NULL, &tv);

        return FD_ISSET(audio_fd[ad], &rfds);
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

static int
oss_set_mode(audio_desc_t ad, int speed, int stereo)
{
        int sp, st, success;
        
        ioctl(audio_fd[ad], SNDCTL_DSP_RESET, 0);
        success = TRUE;
        
        /* We might want to set sample type here */

        st = stereo;
        if (ioctl(audio_fd[ad], SNDCTL_DSP_STEREO, &st) == -1 || st != stereo) {
                success = FALSE;
        }

        sp = speed;
        if (ioctl(audio_fd[ad], SNDCTL_DSP_SPEED, &sp) == -1 || sp != speed) {
                success = FALSE;
        }
        
        return success;
}

static int
oss_probe_formats(audio_desc_t ad)
{
        int speed, stereo;
        for (speed = 8000; speed <= 48000; speed += 8000) {
                if (speed == 24000 || speed == 40000) continue;
                for (stereo = 0; stereo < 2; stereo++) {
                        if (!oss_set_mode(ad, speed, stereo)) continue; /* Failed */
                        af_sup[ad][n_af_sup[ad]].sample_rate = speed;
                        af_sup[ad][n_af_sup[ad]].channels    = stereo + 1;
                        n_af_sup[ad]++;
                }
        }
        return TRUE;
}

int
oss_audio_supports(audio_desc_t ad, audio_format *fmt)
{
        int i;

        for(i = 0; i < n_af_sup[ad]; i++) {
                if (af_sup[ad][i].channels    == fmt->channels &&
                    af_sup[ad][i].sample_rate == fmt->sample_rate) return TRUE;
        }
        return FALSE;
}

int
oss_audio_query_devices(void)
{
        FILE 	*f;
        char 	 buf[OSS_MAX_NAME_LEN];
	char	*name_start;
        int  	 found_devices = FALSE;
        char 	 devices_tag[] = "Audio devices:";
        int 	 len           = strlen(devices_tag);

	ndev = 0;

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
                                        strncpy(dev_name[ndev], name_start, OSS_MAX_NAME_LEN);
                                        purge_chars(dev_name[ndev],"\n");
                                        ndev++;
                                        debug_msg("OSS device found (%s)\n", name_start);
                                } else {
                                        break;
                                }
                        }
                }
                fclose(f);
        } else {
		debug_msg("Cannot open /dev/sndstat\n");
	}
	return ndev;
}

int
oss_get_device_count()
{
        return ndev;
}

const char *
oss_get_device_name(audio_desc_t idx)
{
        if (idx >=0 && idx < ndev) {
                return dev_name[idx];
        }
        debug_msg("Invalid index\n");
        return NULL;
}


#endif /* OSS */
