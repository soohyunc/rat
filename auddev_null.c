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
#include "memory.h"
#include "debug.h"

static audio_format ifmt, ofmt;
static int audio_fd = -1;
static struct timeval last_read_time;

static int igain = 0, ogain = 0;

int
null_audio_open(audio_desc_t ad, audio_format *infmt, audio_format *outfmt)
{
        if (audio_fd != -1) {
                debug_msg("Warning device not closed before opening.\n");
                null_audio_close(ad);
        }

        memcpy(&ifmt, infmt,  sizeof(ifmt));
        memcpy(&ofmt, outfmt, sizeof(ofmt));

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
null_audio_set_igain(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        igain = gain;
	return;
}

/*
 * Get record gain.
 */
int
null_audio_get_igain(audio_desc_t ad)
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
null_audio_set_ogain(audio_desc_t ad, int vol)
{
        UNUSED(ad);
        ogain = vol;
	return;
}

/*
 * Get play gain.
 */
int
null_audio_get_ogain(audio_desc_t ad)
{
        UNUSED(ad);
	return ogain;
}

/*
 * Record audio data.
 */
int
null_audio_read(audio_desc_t ad, u_char *buf, int buf_bytes)
{
	int	                read_bytes;
	struct timeval          curr_time;
	static int              virgin = TRUE;

        UNUSED(ad);

	if (virgin) {
		gettimeofday(&last_read_time, NULL);
		virgin = FALSE;
	}

	gettimeofday(&curr_time, NULL);
	read_bytes  = (curr_time.tv_sec  - last_read_time.tv_sec) * 1000 + (curr_time.tv_usec - last_read_time.tv_usec) / 1000;
        /* diff from ms to samples */
        read_bytes *= (ifmt.bits_per_sample / 8 ) * (ifmt.sample_rate / 1000);

        if (read_bytes < ifmt.bytes_per_block) {
                return 0;
        }
        read_bytes = min(read_bytes, buf_bytes);

        memcpy(&last_read_time, &curr_time, sizeof(struct timeval));
        
        memset(buf, 0, read_bytes);
        xmemchk();

        return read_bytes;
}

/*
 * Playback audio data.
 */
int
null_audio_write(audio_desc_t ad, u_char *buf, int write_bytes)
{
        UNUSED(buf);
        UNUSED(ad);

	return write_bytes;
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

#define NULL_SPEAKER    0x0101
#define NULL_MICROPHONE 0x0201

static audio_port_details_t out_ports[] = {
        {NULL_SPEAKER, AUDIO_PORT_SPEAKER}
};

static audio_port_details_t in_ports[] = {
        {NULL_MICROPHONE, AUDIO_PORT_MICROPHONE}
};

/*
 * Set output port.
 */
void
null_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
        UNUSED(ad); UNUSED(port);

	return;
}

/*
 * Get output port.
 */

audio_port_t
null_audio_oport_get(audio_desc_t ad)
{
        UNUSED(ad);
	return out_ports[0].port;
}

int
null_audio_oport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return 1;
}

const audio_port_details_t*
null_audio_oport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        assert(idx == 0);
        return &out_ports[0];
}

/*
 * Set input port.
 */
void
null_audio_iport_set(audio_desc_t ad, int port)
{
        UNUSED(ad);
        UNUSED(port);
	return;
}

/*
 * Get input port.
 */
int
null_audio_iport_get(audio_desc_t ad)
{
        UNUSED(ad);
	return in_ports[0].port;
}

int
null_audio_iport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return 1;
}

const audio_port_details_t*
null_audio_iport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        assert(idx == 0);
        return &in_ports[0];
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
        diff *= (ifmt.bits_per_sample / 8) * ifmt.sample_rate / 1000;

        if (diff >= (unsigned)ifmt.bytes_per_block) return TRUE;
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

int
null_audio_device_count()
{
        return 1;
}

const char*
null_audio_device_name(audio_desc_t ad)
{
        UNUSED(ad);
        return "No Audio Device";
}

int
null_audio_supports(audio_desc_t ad, audio_format *fmt)
{
        UNUSED(ad);
        if ((!(fmt->sample_rate % 8000) || !(fmt->sample_rate % 11025)) && 
            (fmt->channels == 1 || fmt->channels == 2)) {
                return TRUE;
        }
        return FALSE;
}
