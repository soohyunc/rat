/*
 * FILE:	audwin32.c
 *
 * Win32 audio interface for RAT.
 * Portions based on the VAT Win95 port by John Brezak.
 * Modifications by Isidor Kouvelas <I.Kouvelas@cs.ucl.ac.uk>
 * and Orion Hodson <O.Hodson@cs.ucl.ac.uk>.
 *
 * $Id$
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
 *
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Systems
 *      Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
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
 *
 */

#ifdef WIN32

#include <winsock.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "assert.h"

#include "rat_types.h"
#include "audio.h"
#include "util.h"

#define rat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_rat(x)	((x) * MAX_AMP / 255)

static int		error = 0;
static char		errorText[MAXERRORLENGTH];

#define WRITE_AHEAD	2560
#define ZBUF_LEN	WRITE_AHEAD
static sample zbuf[ZBUF_LEN];

#define	BUFFER_SIZE	160	/* Samples */
#define NUM_BUFFERS	50

static HWAVEOUT		shWaveOut;
static HWAVEIN		shWaveIn;

static WAVEFORMATEX	format;

static int		duplex;
static int		elapsed_time;

static WAVEHDR	read_bufs[NUM_BUFFERS], *read_curr;
static u_char	read_mem[NUM_BUFFERS * BUFFER_SIZE * 2];
static WAVEHDR	write_bufs[NUM_BUFFERS], *write_curr, *write_tail;
static u_char	write_mem[NUM_BUFFERS * BUFFER_SIZE * 2];
static u_int32  write_bufs_used;

typedef struct audMux_s {
	MIXERCONTROLDETAILS select_[8];
	MIXERCONTROLDETAILS vol_[8];
	u_char mcnt_;
	u_char vcnt_;
	char mmap_[8];
	char vmap_[8];
	u_char isOut_;
} audMux;

audMux		imux_, omux_;
static int	iports = 0;
static int	oports = 0;
static int	iport = 0;

static int	play_vol, rec_vol;

static int
mapName(audMux *mux, const char* name)
{

	return (0);
#ifdef NDEF
	return (mux->isOut_? StrToOPort(name) : StrToIPort(name));
#endif
}

static int
mapMixerPort(audMux *mux, const char* name)
{
	int i = mapName(mux, name);
	if (i < 0) {
		char nm[64];
		char* cp;
		strcpy(nm, name);
		while ((cp = strrchr(nm, ' ')) != 0) {
			*cp = 0;
			if ((i = mapName(mux, nm)) >= 0)
				break;
		}
	}
	return (i);
}

static void
getMixerDetails(MIXERLINE *ml, MIXERCONTROL *mc, audMux *mux)
{
	MIXERCONTROLDETAILS mcd;

	mcd.cbStruct = sizeof(mcd);
	mcd.dwControlID = mc->dwControlID;
	mcd.cChannels = ml->cChannels;
	mcd.cMultipleItems = mc->cMultipleItems;
	if (mcd.cMultipleItems) {
		u_int sts;
		MIXERCONTROLDETAILS_LISTTEXT mcdt[16];	
		mcd.cbDetails = sizeof(mcdt[0]);
		mcd.paDetails = mcdt;
		sts = mixerGetControlDetails(0, &mcd, MIXER_GETCONTROLDETAILSF_LISTTEXT);
		if (sts == 0) {
			u_int i;
			for (i = 0; i < mc->cMultipleItems; ++i) {
				u_int n, j;
				MIXERCONTROLDETAILS_BOOLEAN* mcdb;
				int port = mapMixerPort(mux, mcdt[i].szName);
				if (port >= 0)
					mux->mmap_[port] = i;

				n = mcd.cMultipleItems * mcd.cChannels;
				mcdb = (MIXERCONTROLDETAILS_BOOLEAN*)xmalloc(n * sizeof(MIXERCONTROLDETAILS_BOOLEAN));
				memset(mcdb, 0, n * sizeof(*mcdb));
				for (j = 0; j < mcd.cChannels; ++j)
					mcdb[j * mc->cMultipleItems+i].fValue = 1;
					
				mux->select_[i] = mcd;
				mux->select_[i].cbDetails = sizeof(*mcdb);
				mux->select_[i].paDetails = mcdb;
			}
			mux->mcnt_ = (u_char)mcd.cMultipleItems;
		}
	} else {
		MIXERCONTROLDETAILS_UNSIGNED* mcdu;
		int i = mux->vcnt_++;
		int port = mapMixerPort(mux, ml->szName);
		if (port >= 0)
			mux->vmap_[port] = i;

		mcdu = (MIXERCONTROLDETAILS_UNSIGNED*)xmalloc(mcd.cChannels * sizeof(MIXERCONTROLDETAILS_UNSIGNED));
		memset(mcdu, 0, mcd.cChannels * sizeof(*mcdu));
		mux->vol_[i] = mcd;
		mux->vol_[i].cbDetails = sizeof(*mcdu);
		mux->vol_[i].paDetails = mcdu;
	}
}

static void
getMixerCtrls(MIXERLINE *ml, audMux *mux)
{
	MIXERLINECONTROLS mlc;
	MIXERCONTROL mc[16];
	u_int i;

	memset(&mlc, 0, sizeof(mlc));
	memset(mc, 0, sizeof(mc));
	mlc.cbStruct = sizeof(mlc);
	mlc.cbmxctrl = sizeof(mc[0]);
	mlc.pamxctrl = &mc[0];
	mlc.dwLineID = ml->dwLineID;
	mlc.cControls = ml->cControls;
	mixerGetLineControls(0, &mlc, MIXER_GETLINECONTROLSF_ALL);
	for (i = 0; i < mlc.cControls; ++i) {
		switch (mc[i].dwControlType) {

		case MIXERCONTROL_CONTROLTYPE_MUX:
		case MIXERCONTROL_CONTROLTYPE_MIXER:
		case MIXERCONTROL_CONTROLTYPE_VOLUME:
			getMixerDetails(ml, &(mc[i]), mux);
			break;
		}
	}
	/*
	 * if there are multiple source lines for this line,
	 * get their controls
	 */
	for (i = 0; i < ml->cConnections; ++i) {
		MIXERLINE src;
		memset(&src, 0, sizeof(src));
		src.cbStruct = sizeof(src);
		src.dwSource = i;
		src.dwDestination = ml->dwDestination;
		if (mixerGetLineInfo(0, &src, MIXER_GETLINEINFOF_SOURCE) == 0)
			getMixerCtrls(&src, mux);
	}
}

static void
setupMux(audMux *mux, DWORD ctype)
{
	MIXERLINE l;
	int s;
	memset(&l, 0, sizeof(l));
	l.cbStruct = sizeof(l);
	l.dwComponentType = ctype;
	s = mixerGetLineInfo(0, &l, MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (s == 0)
		getMixerCtrls(&l, mux);
}

static int
audio_open_out()
{
	int		i;
	WAVEHDR		*whp;
	u_char		*bp;

	if (shWaveOut)
		return (TRUE);

	error = waveOutOpen(&shWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
	if (error) {
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		fprintf(stderr, "OpenOut: unable to waveOutOpen: %s\n", errorText);
#endif
		return (FALSE);
	}
	audio_set_volume(0, play_vol);

	memset(write_bufs, 0, sizeof(write_bufs));
	for (i = 0, whp = write_bufs, bp = write_mem; i < NUM_BUFFERS; i++, whp++, bp += BUFFER_SIZE * 2) {
		whp->dwFlags = 0;
		whp->dwBufferLength = BUFFER_SIZE * 2;
		whp->lpData = bp;
		error = waveOutPrepareHeader(shWaveOut, whp, sizeof(WAVEHDR));
		if (error) {
			waveOutGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "Win32Audio: waveOutPrepareHeader: %s\n", errorText);
			exit(1);
		}
	}
	write_tail = write_curr = write_bufs;

	return (TRUE);
}

static unsigned char audio_ready = 0;

unsigned char
is_audio_ready()
{
#ifdef DEBUG
        if (audio_ready>10) {
                printf("Lots of audio available (%d blocks)\n", audio_ready);
        }
#endif

	return (audio_ready>0) ? TRUE : FALSE;
}

static void CALLBACK
waveInProc(HWAVEIN hwi,
		   UINT    uMsg,
		   DWORD   dwInstance,
		   DWORD   dwParam1,
		   DWORD   dwParam2)
{
	switch(uMsg) {
	case WIM_DATA:
		audio_ready++;
		break;
	}
	
	return;
}

static int
audio_open_in()
{
	WAVEHDR		*whp;
	int		l;
	u_char		*bp;

	if (shWaveIn)
		return (TRUE);

	error = waveInOpen(&shWaveIn, 
					   WAVE_MAPPER, 
					   &format, 
					   waveInProc, 
					   0, 
					   CALLBACK_FUNCTION);
	if (error != MMSYSERR_NOERROR) {
		waveInGetErrorText(error, errorText, sizeof(errorText));
		//fprintf(stderr, "Win32Audio: waveInOpen Error: (%d) %s\n", error, errorText);
		return (FALSE);
	}

	/* Provide buffers for reading */
	for (l = 0, whp = read_bufs, bp = read_mem; l < NUM_BUFFERS; l++, whp++, bp += BUFFER_SIZE * 2) {
		whp->lpData = bp;
		whp->dwBufferLength = BUFFER_SIZE * 2;
		whp->dwFlags = 0;
		error = waveInPrepareHeader(shWaveIn, whp, sizeof(WAVEHDR));
		if (error) {
			waveInGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "waveInPrepareHeader: (%d) %s\n", error, errorText);
			exit(1);
		}
		error = waveInAddBuffer(shWaveIn, whp, sizeof(WAVEHDR));
		if (error) {
			waveInGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "waveInAddBuffer: (%d) %s\n", error, errorText);
			exit(1);
		}
	}
	read_curr = read_bufs;

	audio_set_gain(20, rec_vol);

	error = waveInStart(shWaveIn);
	if (error) {
		waveInGetErrorText(error, errorText, sizeof(errorText));
		fprintf(stderr, "Win32Audio: waveInStart: (%d) %s\n", error, errorText);
		exit(1);
	}

	return (TRUE);
}

extern int thread_pri;

int audio_get_channels()
{
	return format.nChannels;
}

int
audio_open(audio_format fmt)
{
	play_vol = rec_vol = 50;
	memset(zbuf, 0, ZBUF_LEN);

	format.wFormatTag      = WAVE_FORMAT_PCM;
	format.nChannels       = 1;
	format.nSamplesPerSec  = 8000;
	format.wBitsPerSample  = 16;
	format.nAvgBytesPerSec = format.nChannels * format.nSamplesPerSec * format.wBitsPerSample / 8;
	format.nBlockAlign     = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize          = 0;

	if (audio_open_in() == FALSE)
		return -1;
	duplex = audio_open_out();

	if (mixerGetNumDevs()) {
		int i;
		/* set up the mixer controls for input & out select & gain */
		memset(&imux_, 0, sizeof(imux_));
		memset(&imux_.mmap_, -1, sizeof(imux_.mmap_));
		setupMux(&imux_, MIXERLINE_COMPONENTTYPE_DST_WAVEIN);
		for (i = 0; i < sizeof(imux_.mmap_); ++i)
			if (iports < imux_.mmap_[i])
				iports = imux_.mmap_[i];

		memset(&omux_, 0, sizeof(omux_));
		memset(&omux_.mmap_, -1, sizeof(omux_.mmap_));
		omux_.isOut_ = 1;
		setupMux(&omux_, MIXERLINE_COMPONENTTYPE_DST_SPEAKERS);
		for (i = 0; i < sizeof(omux_.mmap_); ++i)
			if (oports < omux_.mmap_[i])
				oports = omux_.mmap_[i];
	}

	switch(thread_pri) {
	case 1:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		break;
	case 2:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		break;
	case 3:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		break;
	default:
		break;
	}

	return 1;
}

static void
audio_close_in()
{
	int		i;
	WAVEHDR		*whp;


	if (shWaveIn == 0)
		return;

	waveInStop(shWaveIn);
	waveInReset(shWaveIn);

	for (i = 0, whp = read_bufs; i < NUM_BUFFERS; i++, whp++)
		if (whp->dwFlags & WHDR_PREPARED)
			waveInUnprepareHeader(shWaveIn, whp, sizeof(WAVEHDR));

	waveInClose(shWaveIn);
	shWaveIn = 0;
}

static void
audio_close_out()
{
	int	i;
	WAVEHDR		*whp;


	if (shWaveOut == 0)
		return;

	waveOutReset(shWaveOut);

	for (i = 0, whp = write_bufs; i < NUM_BUFFERS; i++, whp++)
		if (whp->dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(shWaveOut, whp, sizeof(WAVEHDR));

	(void) waveOutClose(shWaveOut);
	shWaveOut = 0;
}

void
audio_close(int audio_fd)
{
	audio_close_in();
	audio_close_out();
}

int
audio_duplex(int audio_fd)
{
	return (duplex);
}


void
audio_drain(int audio_fd)
{
}

int
audio_read(int audio_fd, sample *buf, int samples)
{
	int		len = 0;

	if (shWaveIn == 0) {
		assert(shWaveOut);
		for (len = 0; write_tail->dwFlags & WHDR_DONE;) {
			if (len + write_tail->dwBufferLength / 2 > (unsigned)samples)
				break;
			else
				len += write_tail->dwBufferLength / 2;

			write_tail->dwFlags &=~ WHDR_DONE;
			write_tail++;
			write_bufs_used--;
			if (write_tail >= write_bufs + NUM_BUFFERS)
				write_tail = write_bufs;
		}

		if (write_curr == write_tail && len + 160 <= samples)
			len += 160;

		memset(buf, PCMU_AUDIO_ZERO, len);
		return (len);
	} else if (duplex) {
		while (write_tail->dwFlags & WHDR_DONE) {
			write_tail->dwFlags &= ~WHDR_DONE;
			write_tail++;
			write_bufs_used--;
			if (write_tail >= write_bufs + NUM_BUFFERS)
				write_tail = write_bufs;
		}

	}

	while ( (read_curr->dwFlags & WHDR_DONE) &&
			(len < samples) ) {
		memcpy(buf, read_curr->lpData, BUFFER_SIZE * BYTES_PER_SAMPLE);
		buf += BUFFER_SIZE;
		len += BUFFER_SIZE;
		if (len + BUFFER_SIZE < samples) {
		read_curr->dwFlags &=~ WHDR_DONE;
		error = waveInAddBuffer(shWaveIn, read_curr, sizeof(WAVEHDR));
		if (error) {
			waveInGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "waveInAddBuffer: (%d) %s\n", error, errorText);
			exit(1);
		}
		read_curr++;
		if (read_curr >= read_bufs + NUM_BUFFERS)
			read_curr = read_bufs;
			if (audio_ready > 0) {
				audio_ready--;
	}
		} else { 
			OutputDebugString("Read buffer too small - cheating\n");
		}		
	}

	return (len);
}

#define WRITE_ERROR_STILL_PLAYING 33

int
audio_write(int audio_fd, sample *cp, int remain)
{
	int		error, len, ret;

	if (shWaveOut == 0)
		return (remain);

	ret = remain;
	if (write_bufs_used > NUM_BUFFERS - 10) {
		char errmsg[80];
		sprintf(errmsg, 
				"Running out of write buffers %d left\n",
				write_bufs_used);
		OutputDebugString(errmsg);
	}


	for (; remain > 0; remain -= len) {
		if (write_curr->dwFlags & WHDR_DONE) {
			/* Have overdone it! */
			char msg[80];
			sprintf(msg,
				"audio_write, reached end of buffer (%06d bytes remain)\n",
				remain);
			OutputDebugString(msg);
			return (ret - remain);
		}

		len = remain > BUFFER_SIZE? BUFFER_SIZE: remain;

		memcpy(write_curr->lpData, cp, len * BYTES_PER_SAMPLE);
		cp += len;

		error = waveOutWrite(shWaveOut, write_curr, sizeof(WAVEHDR));
		
		if (error == WRITE_ERROR_STILL_PLAYING) { /* We've filled device buffer ? */
				char msg[80];
				sprintf(msg,
						"Win32Audio - device filled. Discarding %d bytes.\n",
						ret - remain);
				OutputDebugString(msg);
					/* we return as if we wrote everything out
					 * to give buffer a little breathing room
					 */

				return ret;
		} else if (error) {
			waveOutGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, 
					"Win32Audio: waveOutWrite (%d): %s\n", 
					error,
					errorText);
			return (ret - remain);
		}

		write_curr++;
		write_bufs_used++;
		if (write_curr >= write_bufs + NUM_BUFFERS)
			write_curr = write_bufs;
	}
	return (ret);
}

void
audio_non_block(int audio_fd)
{
}

void
audio_set_gain(int audio_fd, int level)
{
	MIXERCONTROLDETAILS* mcd;
	u_int i;

	rec_vol = level;

	if (shWaveIn == 0)
		return;

	level = rat_to_device(level);

	if (level > 255)
		level = 255;
	level <<= 8;
	mcd = &imux_.vol_[imux_.vmap_[iport]];
	for (i = 0; i < mcd->cChannels; ++i)
	   ((MIXERCONTROLDETAILS_UNSIGNED*)mcd->paDetails + i)->dwValue = level;
	mixerSetControlDetails(0, mcd, MIXER_SETCONTROLDETAILSF_VALUE);
}

int
audio_get_gain(int audio_fd)
{
	return (rec_vol);
}

void
audio_set_volume(int audio_fd, int level)
{
	DWORD	vol;

	play_vol = level;

	if (shWaveOut == 0)
		return;

	level = rat_to_device(level);
	if (level >= 255)
		level = (short)-1;
	else
		level <<= 8;
	vol = level | (level << 16);

	error = waveOutSetVolume(shWaveOut, vol);
	if (error) {
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		fprintf(stderr, "Win32Audio: waveOutSetVolume: %s\n", errorText);
#endif
	}
}

int
audio_get_volume(int audio_fd)
{
	DWORD	vol;

	if (shWaveOut == 0)
		return (play_vol);

	error = waveOutGetVolume(shWaveOut, &vol);
	if (error) {
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		fprintf(stderr, "Win32Audio: waveOutGetVolume Error: %s\n", errorText);
#endif
		return (0);
	} else
		return (device_to_rat(vol & 0xff));
}

void
audio_set_oport(int audio_fd, int port)
{
	UNUSED(audio_fd);
	UNUSED(port);
}

/* Return selected output port */
int audio_get_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

/* Select next available output port */
int
audio_next_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

void 
audio_set_iport(int audio_fd, int port)
{
	UNUSED(audio_fd);
	UNUSED(port);
}

/* Return selected input port */
int
audio_get_iport(int audio_fd)
{
	return (AUDIO_MICROPHONE);
}

/* Select next available input port */
int
audio_next_iport(int audio_fd)
{
	return (AUDIO_MICROPHONE);
}

#endif /* WIN32 */

