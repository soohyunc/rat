/*
 * FILE:    auddev_sgi.c
 * PROGRAM: RAT
 * AUTHORS:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996 University College London
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

#include <assert.h>

#include "config_unix.h"
#include "config_win32.h"
#include "auddev_sgi.h"
#include "debug.h"

#if defined(IRIX)

#define QSIZE		16000		/* Two seconds for now... */
#define AVG_SIZE	20000
#define SAMSIG		7500.0
#define BASEOFF         0x0a7f

#define RAT_TO_SGI_DEVICE(x)	((x) * 255 / MAX_AMP)
#define SGI_DEVICE_TO_RAT(x)	((x) * MAX_AMP / 255)

static int      audio_fd = -1;
static ALport	rp = NULL, wp = NULL;	/* Read and write ports */
static int	iport = AUDIO_MICROPHONE;
static audio_format format;
/*
 * Try to open the audio device.
 * Return TRUE if successfull FALSE otherwise.
 */
int
sgi_audio_open(audio_desc_t ad, audio_format* fmt)
{
	ALconfig	c;
	long		cmd[8];

        if (audio_fd != -1) {
                sgi_audio_close(ad);
        }

        memcpy(&format, fmt, sizeof(audio_format));

	if ((c = ALnewconfig()) == NULL) {
		fprintf(stderr, "ALnewconfig error\n");
		exit(1);
	}

        switch(fmt->channels) {
        case 1:
                ALsetchannels(c, AL_MONO); break;
        case 2:
                ALsetchannels(c, AL_STEREO); break;
        default:
                sgi_audio_close(ad);
        }

	ALsetwidth(c, AL_SAMPLE_16);
	ALsetqueuesize(c, QSIZE);
	ALsetsampfmt(c, AL_SAMPFMT_TWOSCOMP);

	if ((wp = ALopenport("RAT write", "w", c)) == NULL) {
		fprintf(stderr, "ALopenport (write) error\n");
                sgi_audio_close(ad);
                return FALSE;
        }

	if ((rp = ALopenport("RAT read", "r", c)) == NULL) {
		fprintf(stderr, "ALopenport (read) error\n");
                sgi_audio_close(ad);
                return FALSE;
        }

	cmd[0] = AL_OUTPUT_RATE;
	cmd[1] = format.sample_rate;
	cmd[2] = AL_INPUT_SOURCE;
	cmd[3] = AL_INPUT_MIC;
	cmd[4] = AL_INPUT_RATE;
	cmd[5] = format.sample_rate;
	cmd[6] = AL_MONITOR_CTL;
	cmd[7] = AL_MONITOR_OFF;

	if (ALsetparams(AL_DEFAULT_DEVICE, cmd, 8L) == -1) {
		fprintf(stderr, "audio_open/ALsetparams error\n");
                sgi_audio_close(ad);
        }

	/* Get the file descriptor to use in select */
	audio_fd = ALgetfd(rp);

	if (ALsetfillpoint(rp, format.bytes_per_block) < 0) {
                debug_msg("ALsetfillpoint failed (%d samples)\n", format.bytes_per_block);
        }
        
	/* We probably should free the config here... */
        
	return TRUE;
}

/* Close the audio device */
void
sgi_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
	ALcloseport(rp);
	ALcloseport(wp);
        audio_fd = -1;
}

/* Flush input buffer */
void
sgi_audio_drain(audio_desc_t ad)
{
	sample	buf[QSIZE];

        UNUSED(ad); assert(audio_fd > 0);

	while(sgi_audio_read(audio_fd, buf, QSIZE) == QSIZE);
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
sgi_audio_set_gain(audio_desc_t ad, int gain)
{
	long	cmd[4];

        UNUSED(ad); assert(audio_fd > 0);

	cmd[0] = AL_LEFT_INPUT_ATTEN;
	cmd[1] = 255 - RAT_TO_SGI_DEVICE(gain);
	cmd[2] = AL_RIGHT_INPUT_ATTEN;
	cmd[3] = cmd[1];
	ALsetparams(AL_DEFAULT_DEVICE, cmd, 4L);
}

int
sgi_audio_get_gain(audio_desc_t ad)
{
	long	cmd[2];

        UNUSED(ad); assert(audio_fd > 0);

	cmd[0] = AL_LEFT_INPUT_ATTEN;
	ALgetparams(AL_DEFAULT_DEVICE, cmd, 2L);
	return (255 - SGI_DEVICE_TO_RAT(cmd[1]));
}

void
sgi_audio_set_volume(audio_desc_t ad, int vol)
{
	long	cmd[4];

        UNUSED(ad); assert(audio_fd > 0);

	cmd[0] = AL_LEFT_SPEAKER_GAIN;
	cmd[1] = RAT_TO_SGI_DEVICE(vol);
	cmd[2] = AL_RIGHT_SPEAKER_GAIN;
	cmd[3] = cmd[1];
	ALsetparams(AL_DEFAULT_DEVICE, cmd, 4L);
}

int
sgi_audio_get_volume(audio_desc_t ad)
{
	long	cmd[2];

        UNUSED(ad); assert(audio_fd > 0);

	cmd[0] = AL_LEFT_SPEAKER_GAIN;
	ALgetparams(AL_DEFAULT_DEVICE, cmd, 2L);
	return (SGI_DEVICE_TO_RAT(cmd[1]));
}

static int non_block = 1;	/* Initialise to non blocking */

int
sgi_audio_read(audio_desc_t ad, sample *buf, int samples)
{
	long   		len;

        UNUSED(ad); assert(audio_fd > 0);
        
	if (non_block) {
		if ((len = ALgetfilled(rp)) < format.bytes_per_block)
			return (0);
                len = min(format.bytes_per_block, samples);
	} else {
		len = (long)samples;
        }

	if (len > QSIZE) {
		fprintf(stderr, "audio_read: too big!\n");
		len = QSIZE;
	}

	ALreadsamps(rp, buf, len);

	return ((int)len);
}

int
sgi_audio_write(audio_desc_t ad, sample *buf, int samples)
{
        UNUSED(ad); assert(audio_fd > 0);

	if (samples > QSIZE) {
		fprintf(stderr, "audio_write: too big!\n");
		samples = QSIZE;
	}

	/* Will block */
	ALwritesamps(wp, buf, (long)samples);
	return (samples);
}

/* Set ops on audio device to be non-blocking */
void
sgi_audio_non_block(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	non_block = 1;
}

/* Set ops on audio device to block */
void
sgi_audio_block(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	non_block = 0;
}

void
sgi_audio_set_oport(audio_desc_t ad, int port)
{
        UNUSED(ad); assert(audio_fd > 0);
}

int
sgi_audio_get_oport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	return (AUDIO_SPEAKER);
}

int
sgi_audio_next_oport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	return (AUDIO_SPEAKER);
}

void
sgi_audio_set_iport(audio_desc_t ad, int port)
{
	long pvbuf[2];

        UNUSED(ad); assert(audio_fd > 0);

        switch(port) {
        case AUDIO_MICROPHONE: 
                pvbuf[0] = AL_INPUT_SOURCE;
                pvbuf[1] = AL_INPUT_MIC;
                ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
                iport = AUDIO_MICROPHONE;
                break;
        case AUDIO_LINE_IN: 
                pvbuf[0] = AL_INPUT_SOURCE;
                pvbuf[1] = AL_INPUT_LINE;
                ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
                iport = AUDIO_LINE_IN;
                break;
        default:
                printf("Illegal input port!\n");
                abort();
        }
}

int
sgi_audio_get_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	return iport;
}

int
sgi_audio_next_iport(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        switch (iport) {
        case AUDIO_MICROPHONE:
                sgi_audio_set_iport(ad, AUDIO_LINE_IN);
                break;
        case AUDIO_LINE_IN:
                sgi_audio_set_iport(ad, AUDIO_MICROPHONE);
                break;
        default:
                printf("Unknown audio source!\n");
        }
        return iport;
}

void 
sgi_audio_loopback(audio_desc_t ad, int gain)
{
        long pvbuf[4];
        int  pvcnt;

        UNUSED(ad); assert(audio_fd > 0);

        pvcnt = 2;
        pvbuf[0] = AL_MONITOR_CTL;
        pvbuf[1] = AL_MONITOR_OFF;

        if (gain) {
                pvcnt = 6;
                pvbuf[1] = AL_MONITOR_ON;
                pvbuf[2] = AL_LEFT_MONITOR_ATTEN;
                pvbuf[3] = 255 - RAT_TO_SGI_DEVICE(gain);
                pvbuf[4] = AL_RIGHT_MONITOR_ATTEN;
                pvbuf[5] = pvbuf[3];
        }
        
        if (ALsetparams(AL_DEFAULT_DEVICE, pvbuf, pvcnt) != 0) {
                debug_msg("loopback failed\n");
        }
}

int
sgi_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        return 1;
}

int
sgi_audio_get_channels(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
 	return ALgetchannels(ALgetconfig(rp));
}

int
sgi_audio_get_bytes_per_block(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        return format.bytes_per_block; /* Could use ALgetfillpoint */
}

int
sgi_audio_get_freq(audio_desc_t ad)
{
        long pvbuf[2];
        UNUSED(ad); assert(audio_fd > 0);
        
        pvbuf[0] = AL_INPUT_RATE;

        if (ALqueryparams(AL_DEFAULT_DEVICE, pvbuf, 2) == -1) {
                debug_msg("Could not get freq");
        }
        return (int)pvbuf[1];
}

int  
sgi_audio_is_ready(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        
        if (ALgetfilled(rp) >= format.bytes_per_block) {
                return TRUE;
        } else {
                return FALSE;
        }
}

void 
sgi_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        struct timeval tv;
        fd_set rfds;
        UNUSED(ad); assert(audio_fd > 0);

        tv.tv_sec  = 0;
        tv.tv_usec = delay_ms * 1000;

        FD_ZERO(&rfds);
        FD_SET(audio_fd, &rfds);
        
        select(audio_fd + 1, &rfds, NULL, NULL, &tv);
}

#endif

