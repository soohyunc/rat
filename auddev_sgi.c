/*
 * FILE:    sgi.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins
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

#include "audio.h"

#if defined(IRIX)

#define READ_UNIT	80
#define QSIZE		16000		/* Two seconds for now... */
#define AVG_SIZE	20000
#define SAMSIG		7500.0
#define BASEOFF         0x0a7f

#define bat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_bat(x)	((x) * MAX_AMP / 255)

static ALport	rp, wp;		/* Read and write ports */
static int	iport = AUDIO_MICROPHONE;

/*
 * Try to open the audio device.
 * Return TRUE if successfull FALSE otherwise.
 */
int
audio_open(audio_format format)
{
	int 		audio_fd = -1;
	ALconfig	c;
	long		cmd[8];

	if ((c = ALnewconfig()) == NULL) {
		fprintf(stderr, "ALnewconfig error\n");
		exit(1);
	}

	ALsetchannels(c, AL_MONO);
	ALsetwidth(c, AL_SAMPLE_16);
	ALsetqueuesize(c, QSIZE);
	ALsetsampfmt(c, AL_SAMPFMT_TWOSCOMP);
	if ((wp = ALopenport("RAT write", "w", c)) == NULL)
		fprintf(stderr, "ALopenport (write) error\n");
	if ((rp = ALopenport("RAT read", "r", c)) == NULL)
		fprintf(stderr, "ALopenport (read) error\n");

	cmd[0] = AL_OUTPUT_RATE;
	cmd[1] = format.sample_rate;
	cmd[2] = AL_INPUT_SOURCE;
	cmd[3] = AL_INPUT_MIC;
	cmd[4] = AL_INPUT_RATE;
	cmd[5] = format.sample_rate;
	cmd[6] = AL_MONITOR_CTL;
	cmd[7] = AL_MONITOR_OFF;
	if (ALsetparams(AL_DEFAULT_DEVICE, cmd, 8L) == -1)
		fprintf(stderr, "audio_open/ALsetparams error\n");

	/* Get the file descriptor to use in select */
	audio_fd = ALgetfd(rp);

	ALsetfillpoint(rp, READ_UNIT);

	/* We probably should free the config here... */

	return audio_fd;
}

/* Close the audio device */
void
audio_close(int audio_fd)
{
	ALcloseport(rp);
	ALcloseport(wp);
}

/* Flush input buffer */
void
audio_drain(int audio_fd)
{
	sample	buf[QSIZE];
	while(audio_read(audio_fd, buf, QSIZE) == QSIZE);
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
audio_set_gain(int audio_fd, int gain)
{
	long	cmd[4];

	if (audio_fd == 0)
		return;

	cmd[0] = AL_LEFT_INPUT_ATTEN;
	cmd[1] = 255 - bat_to_device(gain);
	cmd[2] = AL_RIGHT_INPUT_ATTEN;
	cmd[3] = cmd[1];
	ALsetparams(AL_DEFAULT_DEVICE, cmd, 4L);
}

int
audio_get_gain(int audio_fd)
{
	long	cmd[2];

	if (audio_fd == 0)
		return 50;

	cmd[0] = AL_LEFT_INPUT_ATTEN;
	ALgetparams(AL_DEFAULT_DEVICE, cmd, 2L);
	return (255 - device_to_bat(cmd[1]));
}

void
audio_set_volume(int audio_fd, int vol)
{
	long	cmd[4];

	if (audio_fd == 0)
		return;

	cmd[0] = AL_LEFT_SPEAKER_GAIN;
	cmd[1] = bat_to_device(vol);
	cmd[2] = AL_RIGHT_SPEAKER_GAIN;
	cmd[3] = cmd[1];
	ALsetparams(AL_DEFAULT_DEVICE, cmd, 4L);
}

int
audio_get_volume(int audio_fd)
{
	long	cmd[2];

	if (audio_fd == 0)
		return 50;

	cmd[0] = AL_LEFT_SPEAKER_GAIN;
	ALgetparams(AL_DEFAULT_DEVICE, cmd, 2L);
	return (device_to_bat(cmd[1]));
}

static int non_block = 1;	/* Initialise to non blocking */

int
audio_read(int audio_fd, sample *buf, int samples)
{
	long   		len;
        
	if (non_block) {
		if ((len = ALgetfilled(rp)) <= 0)
			return (0);
		len -= len % READ_UNIT;
		if (len <= 0)
			len = READ_UNIT;
		if (len > samples)
			len = samples;
	} else
		len = (long)samples;

	if (len > QSIZE) {
		fprintf(stderr, "audio_read: too big!\n");
		len = QSIZE;
	}

	ALreadsamps(rp, buf, len);

	return ((int)len);
}

int
audio_write(int audio_fd, sample *buf, int samples)
{
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
audio_non_block(int audio_fd)
{
	non_block = 1;
}

/* Set ops on audio device to block */
void
audio_block(int audio_fd)
{
	non_block = 0;
}

void
audio_set_oport(int audio_fd, int port)
{
	/* Not possible? */
}

int
audio_get_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

int
audio_next_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

void
audio_set_iport(int audio_fd, int port)
{
	long pvbuf[2];

	if (audio_fd > 0) {
		switch(port) {
		case AUDIO_MICROPHONE : pvbuf[0] = AL_INPUT_SOURCE;
					pvbuf[1] = AL_INPUT_MIC;
					ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
					iport = AUDIO_MICROPHONE;
					break;
		case AUDIO_LINE_IN    : pvbuf[0] = AL_INPUT_SOURCE;
					pvbuf[1] = AL_INPUT_LINE;
					ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
					iport = AUDIO_LINE_IN;
					break;
		default	              :	printf("Illegal input port!\n");
					abort();
		}
	}
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
    case AUDIO_MICROPHONE : audio_set_iport(audio_fd, AUDIO_LINE_IN);
                            break;
    case AUDIO_LINE_IN    : audio_set_iport(audio_fd, AUDIO_MICROPHONE);
                            break;
    default               : printf("Unknown audio source!\n");
  }
  return iport;
}

void
audio_switch_out(int audio_fd, struct s_cushion_struct *ap)
{
	/* Full duplex device: do nothing! */
}
   
void
audio_switch_in(int audio_fd)
{
	/* Full duplex device: do nothing! */
}

int
audio_duplex(int audio_fd)
{
  return 1;
}

int
audio_get_channels(void)
{
 	return ALgetchannels(ALgetconfig(rp));
}

#endif

