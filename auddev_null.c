/*
 * FILE:    auddev_null.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson 
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

/*
 * NULL audio device. 
 */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "auddev_null.h"
#include "assert.h"
#include "memory.h"
#include "debug.h"

static audio_format format;
static int audio_fd = -1;
static struct timeval last_read_time;

static int igain = 0, ogain = 0;

int
null_audio_open(audio_desc_t ad, audio_format *fmt)
{
        if (audio_fd != -1) {
                debug_msg("Warning device not closed before opening.\n");
                null_audio_close(ad);
        }

        memcpy(&format, fmt, sizeof(format));
	return TRUE;
}

/*
 * Shutdown.
 */
void
null_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
	if (audio_fd > 0)
                audio_fd = -1;
	return;
}

/*
 * Flush input buffer.
 */
void
null_audio_drain(audio_desc_t ad)
{
        UNUSED(ad);
	return;
}

/*
 * Set record gain.
 */
void
null_audio_set_gain(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        igain = gain;
	return;
}

/*
 * Get record gain.
 */
int
null_audio_get_gain(audio_desc_t ad)
{
        UNUSED(ad);
	return igain;
}

int
null_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad);
        return TRUE;
}

/*
 * Set play gain.
 */
void
null_audio_set_volume(audio_desc_t ad, int vol)
{
        UNUSED(ad);
        ogain = vol;
	return;
}

/*
 * Get play gain.
 */
int
null_audio_get_volume(audio_desc_t ad)
{
        UNUSED(ad);
	return ogain;
}

/*
 * Record audio data.
 */
int
null_audio_read(audio_desc_t ad, sample *buf, int samples)
{
	int	                diff;
	struct timeval          curr_time;
	static int              virgin = TRUE;

        UNUSED(ad);

	if (virgin) {
		gettimeofday(&last_read_time, NULL);
		virgin = FALSE;
	}

	gettimeofday(&curr_time, NULL);
	diff = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000 + (curr_time.tv_usec - last_read_time.tv_usec) / 1000;
        /* diff from ms to samples */
        diff *= format.sample_rate / 1000;

        if (diff < format.blocksize) {
                return 0;
        }
        samples = format.blocksize;
        last_read_time.tv_usec += samples * 1000 / (format.sample_rate/1000);
        if (last_read_time.tv_usec >= 1000000) {
                last_read_time.tv_sec += last_read_time.tv_usec / 1000000;
                last_read_time.tv_usec = last_read_time.tv_usec % 1000000;
        }
        
        memset(buf, 0, sizeof(sample)*samples);
        xmemchk();

        return samples;
}

/*
 * Playback audio data.
 */
int
null_audio_write(audio_desc_t ad, sample *buf, int samples)
{
        UNUSED(buf);
        UNUSED(ad);

	return samples;
}

/*
 * Set options on audio device to be non-blocking.
 */
void
null_audio_non_block(audio_desc_t ad)
{
        UNUSED(ad);
        debug_msg("null_audio_non_block");
}

/*
 * Set options on audio device to be blocking.
 */
void
null_audio_block(audio_desc_t ad)
{
        UNUSED(ad);
	debug_msg("null_audio_block");
}

/*
 * Set output port.
 */
void
null_audio_set_oport(audio_desc_t ad, int port)
{
        UNUSED(ad); UNUSED(port);

	return;
}

/*
 * Get output port.
 */
int
null_audio_get_oport(audio_desc_t ad)
{
        UNUSED(ad);
	return AUDIO_SPEAKER;
}

/*
 * Set next output port.
 */
int
null_audio_next_oport(audio_desc_t ad)
{
        UNUSED(ad);
	return AUDIO_SPEAKER;
}

/*
 * Set input port.
 */
void
null_audio_set_iport(audio_desc_t ad, int port)
{
        UNUSED(ad);
        UNUSED(port);
	return;
}

/*
 * Get input port.
 */
int
null_audio_get_iport(audio_desc_t ad)
{
        UNUSED(ad);
	return AUDIO_MICROPHONE;
}

/*
 * Get next input port...
 */
int
null_audio_next_iport(audio_desc_t ad)
{
        UNUSED(ad);
	return AUDIO_MICROPHONE;
}

/*
 * Enable hardware loopback
 */
void 
null_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        UNUSED(gain);
        /* Nothing doing... */
}

/*
 * Get device blocksize
 */
int
null_audio_get_blocksize(audio_desc_t ad)
{
        UNUSED(ad);
        return format.blocksize;
}

/*
 * Get device channels
 */
int
null_audio_get_channels(audio_desc_t ad)
{
        UNUSED(ad);
        return format.num_channels;
}

/*
 * Get device blocksize
 */
int
null_audio_get_freq(audio_desc_t ad)
{
        UNUSED(ad);
        return format.sample_rate;
}

/*
 * For external purposes this function returns non-zero
 * if audio is ready.
 */
int
null_audio_is_ready(audio_desc_t ad)
{
        struct timeval now;
        u_int32 diff;

        UNUSED(ad);

        gettimeofday(&now,NULL);
	diff = (now.tv_sec  - last_read_time.tv_sec) * 1000 + (now.tv_usec - last_read_time.tv_usec)/1000;
        diff *= format.sample_rate / 1000;

        if (diff >= (unsigned)format.blocksize) return diff;
        return FALSE;
}

void
null_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        if (null_audio_is_ready(ad)) {
                return;
        } else {
                usleep(delay_ms * 1000);
        }
}
