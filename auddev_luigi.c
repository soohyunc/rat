/*
 * FILE:    auddev_luigi.c - Sound interface for Luigi Rizzo's FreeBSD driver
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

#ifdef FreeBSD

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_luigi.h"
#include "memory.h"
#include "debug.h"

static int iport = AUDIO_MICROPHONE;
static snd_chan_param pa;
static int audio_fd = -1;

#define BLOCKSIZE 320

#define RAT_TO_DEVICE(x) ((x) * 100 / MAX_AMP)
#define DEVICE_TO_RAT(x) ((x) * MAX_AMP / 100)

#define LUIGI_AUDIO_IOCTL(fd, cmd, val) if (ioctl((fd), (cmd), (val)) < 0) { \
                                            debug_msg("Failed %s\n",#cmd); \
                                            luigi_error = __LINE__; \
                                               }

#define LUIGI_MAX_AUDIO_NAME_LEN 32
#define LUIGI_MAX_AUDIO_DEVICES  3

static int dev_ids[LUIGI_MAX_AUDIO_DEVICES];
static char names[LUIGI_MAX_AUDIO_DEVICES][LUIGI_MAX_AUDIO_NAME_LEN];
static int ndev = 0;
static int luigi_error;

int 
luigi_audio_open(audio_desc_t ad, audio_format *ifmt, audio_format *ofmt)
{
        int             volume = 100;
        int             reclb = 0;
        
        char            thedev[64];
        
        assert(ad >= 0 && ad < ndev); 
	sprintf(thedev, "/dev/audio%d", dev_ids[ad]);

        audio_fd = open(thedev, O_RDWR);
        if (audio_fd >= 0) {
                struct snd_size sz;
                snd_capabilities soundcaps;
                /* Ignore any earlier errors */
                luigi_error = 0;

                LUIGI_AUDIO_IOCTL(audio_fd, AIOGCAP, &soundcaps);
                LUIGI_AUDIO_IOCTL(audio_fd,SNDCTL_DSP_RESET,0);

                switch (soundcaps.formats & (AFMT_FULLDUPLEX | AFMT_WEIRD)) {
                case AFMT_FULLDUPLEX:
                        /*
                         * this entry for cards with decent full duplex.
                         */
                        break;
                case AFMT_FULLDUPLEX | AFMT_WEIRD:
                        /* this is the sb16... */
                        debug_msg("Weird Hardware\n");
                        audio_format_change_encoding(ofmt, DEV_S8);
                        break;
                default:		/* no full duplex... */
                        fprintf(stderr, "Sorry driver does support full duplex for this soundcard\n");
                        luigi_audio_close(ad);
                        return FALSE;
                }
                
                assert(ofmt->channels == ifmt->channels);
                
                if (ifmt->channels == 2 && !(soundcaps.formats & AFMT_STEREO)) {
                        fprintf(stderr,"Driver does not support stereo for this soundcard\n");
                        luigi_audio_close(ad);
                        return FALSE;
                }

                switch(ifmt->encoding) {
                case DEV_PCMU: pa.rec_format = AFMT_MU_LAW; break;
                case DEV_S8:   pa.rec_format = AFMT_S8;     break;
                case DEV_S16:  pa.rec_format = AFMT_S16_LE; break;
                }
                pa.rec_rate = ifmt->sample_rate;
                sz.rec_size = ifmt->bytes_per_block;

                switch(ofmt->encoding) {
                case DEV_PCMU: pa.play_format = AFMT_MU_LAW; break;
                case DEV_S8:   pa.play_format = AFMT_S8;     break;
                case DEV_S16:  pa.play_format = AFMT_S16_LE; break;
                }
                pa.play_rate = ofmt->sample_rate;
                sz.play_size = ofmt->bytes_per_block;

                LUIGI_AUDIO_IOCTL(audio_fd, AIOSFMT, &pa);
                LUIGI_AUDIO_IOCTL(audio_fd, AIOSSIZE, &sz);
                
                /* Set global gain/volume to maximum values. This may fail on */
                /* some cards, but shouldn't cause any harm when it does..... */
                LUIGI_AUDIO_IOCTL(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume);
                LUIGI_AUDIO_IOCTL(audio_fd, MIXER_WRITE(SOUND_MIXER_IGAIN), &volume);
                /* Set the gain/volume properly. We use the controls for the  */
                /* specific mixer channel to do this, relative to the global  */
                /* maximum gain/volume we've just set...                      */
                luigi_audio_set_gain(audio_fd, MAX_AMP / 2);
                luigi_audio_set_volume(audio_fd, MAX_AMP / 2);
                /* Select microphone input. We can't select output source...  */
                luigi_audio_set_iport(audio_fd, iport);
                /* Turn off loopback from input to output... */
                LUIGI_AUDIO_IOCTL(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &reclb);

                if (luigi_error != 0) {
                        /* Failed somewhere in initialization - reset error and exit*/
                        luigi_audio_close(ad);
                        luigi_error = 0;
                        return FALSE;
                }
                
                read(audio_fd, thedev, 64);
                return TRUE;
        } else {
                perror("audio_open");
                luigi_audio_close(ad);
                return FALSE;
        }
}

/* Close the audio device */
void
luigi_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
	if (audio_fd < 0) {
                debug_msg("Device already closed!\n");
                return;
        }
	luigi_audio_drain(audio_fd);
	close(audio_fd);
        audio_fd = -1;
}

/* Flush input buffer */
void
luigi_audio_drain(audio_desc_t ad)
{
        u_char buf[160];
        
        assert(audio_fd > 0);
        UNUSED(ad);
        while(luigi_audio_read(audio_fd, buf, 160) == 160);
}

int
luigi_audio_duplex(audio_desc_t ad)
{
        /* We only ever open device full duplex! */
        UNUSED(ad);
        return TRUE;
}

int
luigi_audio_read(audio_desc_t ad, u_char *buf, int read_bytes)
{
        int done, this_read;
        int len;
        /* Figure out how many bytes we can read before blocking... */

        UNUSED(ad); assert(audio_fd > 0);

        LUIGI_AUDIO_IOCTL(audio_fd, FIONREAD, &len);

        len = min(len, read_bytes);

        /* Read the data... */
        done = 0;
        while(done < len) {
                this_read = read(audio_fd, (void*)buf, len - done);
                done += this_read;
                buf  += this_read;
        }
        return done;
}

int
luigi_audio_write(audio_desc_t ad, u_char *buf, int write_bytes)
{
	int done;

        UNUSED(ad); assert(audio_fd > 0);

        done = write(audio_fd, (void*)buf, write_bytes);
        if (done != write_bytes && errno != EINTR) {
                perror("Error writing device");
                return (write_bytes - done);
        }

        return write_bytes;
}

/* Set ops on audio device to be non-blocking */
void
luigi_audio_non_block(audio_desc_t ad)
{
	int             frag = 1;

	UNUSED(ad); assert(audio_fd != -1);

        LUIGI_AUDIO_IOCTL(audio_fd, SNDCTL_DSP_NONBLOCK, &frag);
}

/* Set ops on audio device to be blocking */
void
luigi_audio_block(audio_desc_t ad)
{
  	int             frag = 0;
        
        UNUSED(ad); assert(audio_fd > 0);
        
        LUIGI_AUDIO_IOCTL(audio_fd, SNDCTL_DSP_NONBLOCK, &frag);
} 

/* Gain and volume values are in the range 0 - MAX_AMP */
void
luigi_audio_set_volume(audio_desc_t ad, int vol)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

	volume = vol << 8 | vol;
	LUIGI_AUDIO_IOCTL(audio_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume);
}

int
luigi_audio_get_volume(audio_desc_t ad)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

	LUIGI_AUDIO_IOCTL(audio_fd, MIXER_READ(SOUND_MIXER_PCM), &volume);

	return DEVICE_TO_RAT(volume & 0xff); /* Extract left channel volume */
}

void
luigi_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad); assert(audio_fd > 0);

        gain = gain << 8 | gain;

        LUIGI_AUDIO_IOCTL(audio_fd, MIXER_WRITE(SOUND_MIXER_IMIX), &gain);
}

void
luigi_audio_set_oport(audio_desc_t ad, int port)
{
	UNUSED(ad); assert(audio_fd > 0);
	UNUSED(port);

	return;
}

int
luigi_audio_get_oport(audio_desc_t ad)
{
	UNUSED(ad); assert(audio_fd > 0);
	return AUDIO_HEADPHONE;
}

int
luigi_audio_next_oport(audio_desc_t ad)
{
	UNUSED(ad); assert(audio_fd > 0);
	return AUDIO_HEADPHONE;
}

void
luigi_audio_set_gain(audio_desc_t ad, int gain)
{
	int volume = RAT_TO_DEVICE(gain) << 8 | RAT_TO_DEVICE(gain);

        UNUSED(ad); assert(audio_fd > 0);

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
		if (ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_CD), &volume) < 0) {
			perror("Setting gain");
                }
		break;
	}
	return;
}

int
luigi_audio_get_gain(audio_desc_t ad)
{
	int volume;

        UNUSED(ad); assert(audio_fd > 0);

	switch (iport) {
	case AUDIO_MICROPHONE:
		LUIGI_AUDIO_IOCTL(audio_fd, MIXER_READ(SOUND_MIXER_MIC), &volume);
		break;
	case AUDIO_LINE_IN:
		LUIGI_AUDIO_IOCTL(audio_fd, MIXER_READ(SOUND_MIXER_LINE), &volume);
		break;
	case AUDIO_CD:
		LUIGI_AUDIO_IOCTL(audio_fd, MIXER_READ(SOUND_MIXER_CD), &volume);
		break;
	default:
		debug_msg("ERROR: Unknown iport in audio_set_gain!\n");
	}
	return (DEVICE_TO_RAT(volume & 0xff));
}

void
luigi_audio_set_iport(audio_desc_t ad, int port)
{
	int recmask, gain, src;

        UNUSED(ad); assert(audio_fd > 0);

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

	gain = luigi_audio_get_gain(ad);
	luigi_audio_set_gain(ad, 0);

	if ((ioctl(audio_fd, MIXER_WRITE(SOUND_MIXER_RECSRC), &src) < 0)) {
		return;
	}

	iport = port;
	luigi_audio_set_gain(ad, gain);
}

int
luigi_audio_get_iport(audio_desc_t ad)
{
	UNUSED(ad); assert(audio_fd > 0);
	return iport;
}

int
luigi_audio_next_iport(audio_desc_t ad)
{
        int target;
        UNUSED(ad); assert(audio_fd > 0);

	switch (iport) {
	case AUDIO_MICROPHONE:
                target = AUDIO_LINE_IN;
		break;
	case AUDIO_LINE_IN:
                target = AUDIO_CD;
		break;
	case AUDIO_CD:
		target = AUDIO_MICROPHONE;
		break;
	default:
		printf("Unknown audio source!\n");
	}

        luigi_audio_set_iport(ad, target);
        if (iport != target) {
                /* We may not have line or cd but should always have mic */
                luigi_audio_set_iport(ad, AUDIO_MICROPHONE);
        }

	return (iport);
}

static int
luigi_audio_select(audio_desc_t ad, int delay_us)
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
luigi_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        luigi_audio_select(ad, delay_ms * 1000);
}

int 
luigi_audio_is_ready(audio_desc_t ad)
{
        return luigi_audio_select(ad, 0);
}

int 
luigi_audio_supports(audio_desc_t ad, audio_format *fmt)
{
        snd_capabilities s;

        UNUSED(ad);

        if (luigi_error) debug_msg("Device error!");
        luigi_error = 0;
        LUIGI_AUDIO_IOCTL(audio_fd, AIOGCAP, &s);
        if (!luigi_error) {
                if ((unsigned)fmt->sample_rate < s.rate_min || (unsigned)fmt->sample_rate > s.rate_max) return FALSE;
                if (fmt->channels == 1) return TRUE;                    /* Always supports mono */
                assert(fmt->channels == 2);
                if (s.formats & AFMT_STEREO) return TRUE;
        }
        return FALSE;
}

void
luigi_audio_query_devices()
{
        FILE *f;
        char buf[128], *p;
        int n;

        f = fopen("/dev/sndstat", "r");
        if (f) {
                while (!feof(f) && ndev < LUIGI_MAX_AUDIO_DEVICES) {
                        p = fgets(buf, 128, f);
                        n = sscanf(buf, "pcm%d: <%[A-z0-9 ]>", dev_ids + ndev, names[ndev]);
                        if (p && n == 2) {
                                debug_msg("dev (%d) name (%s)\n", dev_ids[ndev], names[ndev]);
                                ndev++;
                        }
                }
                fclose(f);
        }
}

int
luigi_get_device_count()
{
        return ndev;
}

char *
luigi_get_device_name(int idx)
{
        if (idx >=0 && idx < ndev) {
                return names[idx];
        }
        return NULL;
}

#endif /* FreeBSD */
