/*
 * FILE:        mix.c
 * PROGRAM:     RAT
 * AUTHOR:      Isidor Kouvelas 
 * MODIFIED BY: Orion Hodson + Colin Perkins 
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "mix.h"
#include "session.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "audio_fmt.h"
#include "timers.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "source.h"
#include "playout.h"
#include "debug.h"
#include "parameters.h"
#include "ui.h"

typedef struct s_mix_info {
	int	buf_len;        /* Length of circular buffer */
	int	head, tail;     /* Index to head and tail of buffer */
	ts_t	head_time;      /* Time of latest sample in buffer.
				 * In fact pad_time has to be taken into
				 * account to get the actual value. */
	ts_t	tail_time;	/* Current time */
	int	dist;		/* Distance between head and tail.
				 * We must make sure that this is kept
				 * equal to value of the device cushion
				 * unless there is no audio to mix. */
	sample	*mix_buffer;	/* The buffer containing mixed audio data. */
        int      channels;      /* number of channels being mixed. */
        int      rate;          /* Sampling frequency */
} mix_struct;

#define ROUND_UP(x, y)  (x) % (y) > 0 ? (x) - (x) % (y) : (x)

/*
 * Initialise the circular buffer that is used in mixing.
 * The buffer length should be as big as the largest possible
 * device cushion used (and maybe some more).
 * We allocate space three times the requested one so that we
 * dont have to copy everything when we hit the boundaries..
 */
mix_struct *
mix_create(session_struct * sp, int buffer_length)
{
	mix_struct	     *ms;
        codec_id_t            cid;
        const codec_format_t *cf;

        cid = codec_get_by_payload((u_char)sp->encodings[0]);
        assert(cid);
        cf = codec_get_format(cid);
	ms = (mix_struct *) xmalloc(sizeof(mix_struct));
	memset(ms, 0 , sizeof(mix_struct));
        ms->channels    = cf->format.channels;
        ms->rate        = cf->format.sample_rate;
        ms->buf_len     = buffer_length * ms->channels;
	ms->mix_bur  = (sample *)xmalloc(3 * ms->buf_len * BYTES_PER_SAMPLE);
	audio_zero(ms->mix_buffer, 3 * buffer_length , DEV_S16);
	ms->mix_buffer += ms->buf_len;
        ms->head_time = ms->tail_time = ts_map32(ms->rate, 0);
	return (ms);
}

void
mix_destroy(mix_struct *ms)
{
        xfree(ms->mix_buffer - ms->buf_len);
        xfree(ms);
}

static void
mix_audio(sample *dst, sample *src, int len)
{
	int tmp;
        sample *src_e;

        src_e = src + len;
        while (src != src_e) {
                tmp = *dst + *src++;
                if (tmp > 32767)
                        tmp = 32767;
                else if (tmp < -32768)
                        tmp = -32768;
                *dst++ = tmp;
        }
}

static void
mix_zero(mix_struct *ms, int offset, int len)
{
        assert(len < ms->buf_len);
	if (offset + len > ms->buf_len) {
		audio_zero(ms->mix_buffer + offset, ms->buf_len - offset, DEV_S16);
		audio_zero(ms->mix_buffer, offset + len-ms->buf_len, DEV_S16);
	} else {
		audio_zero(ms->mix_buffer + offset, len, DEV_S16);
	}
        xmemchk();
}

/*
 * This function takes a `unit' of data, up-/down-samples it as needed
 * to match the audio device, and mixes it ready for playout.
 *
 * This function assumes that packets come in order without missing or
 * duplicates! Starts of talkspurts should always be signaled.
 */

void
mix_process(mix_struct          *ms,
            rtcp_dbentry        *dbe,
            coded_unit          *frame,
            ts_t                 playout)
{
        sample  *samples;
        u_int32  nticks, nsamples, pos;
        u_int16  channels, rate;
        ts_t     frame_period, expected_playout, delta, new_head_time;

	assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);

        codec_get_native_info(frame->id, &rate, &channels);

        assert(rate     == (u_int32)ms->rate);
        assert(channels == (u_int32)ms->channels);

        nticks          = frame->data_len / (sizeof(sample) * channels);
        frame_period    = ts_map32(rate, nticks);

        if (dbe->first_mix) {
                debug_msg("New mix\n");
                dbe->last_mixed = ts_sub(playout, frame_period);
                dbe->first_mix  = 0;
        }

        assert(ts_gt(playout, dbe->last_mixed));

        samples  = (sample*)frame->data;
        nsamples = frame->data_len / sizeof(sample);
                
        /* Check for overlap in decoded frames */
        expected_playout = ts_add(dbe->last_mixed, frame_period);
        if (!ts_eq(expected_playout, playout)) {
                if (ts_gt(expected_playout, playout)) {
                        delta = ts_sub(expected_playout, playout);
                        debug_msg("Overlapping units\n");
                        if (ts_gt(frame_period, delta)) {
                                u_int32  trim = delta.ticks * ms->channels;
                                samples  += trim;
                                nsamples -= trim;
                                debug_msg("Trimmed %d samples\n", trim);
                        } else {
                                debug_msg("Skipped unit\n");
                        }
                } else {
                        debug_msg("Gap between units %d %d\n", expected_playout.ticks, playout.ticks);
                }
        }

        /* Zero ahead if necessary */

        new_head_time = ts_add(playout, ts_map32(ms->rate, nsamples / ms->channels));
        if (ts_eq(ms->head_time, ms->tail_time)) {
                ms->head_time = ms->tail_time = playout;
        }

        if (ts_gt(new_head_time, ms->head_time))  {
                int zeros;
                delta = ts_sub(new_head_time, ms->head_time);
                zeros = delta.ticks * ms->channels;
                mix_zero(ms, ms->head, zeros);
                ms->dist += zeros;
                ms->head += zeros;
                ms->head %= ms->buf_len;
                ms->head_time = ts_add(ms->head_time, delta);
        }
        assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);                                          
        assert(!ts_gt(playout, ms->head_time));

        /* Work out where to write the data */
        delta = ts_sub(ms->head_time, playout);
        pos   = (ms->head - delta.ticks*ms->channels) % ms->buf_len;
        if (pos > 0x7fffffff) {
                debug_msg("mix wrap\n");
                pos += ms->buf_len;
                assert(pos < (u_int32)ms->buf_len);
        }
        
        if (pos + nsamples > (u_int32)ms->buf_len) { 
                mix_audio(ms->mix_buffer + pos, 
                          samples, 
                          ms->buf_len - pos); 
                xmemchk();
                mix_audio(ms->mix_buffer, 
                          samples + (ms->buf_len - pos) * ms->channels, 
                          pos + nsamples - ms->buf_len); 
                xmemchk();
        } else { 
                mix_audio(ms->mix_buffer + pos, 
                          samples, 
                          nsamples); 
                xmemchk();
        } 
        dbe->last_mixed = playout;

        return;
}

/*
 * The mix_get_audio function returns a pointer to "amount" samples of mixed 
 * audio data, suitable for playout (ie: you can do audio_device_write() with
 * the returned data).
 *
 * This function was modified so that it returns the amount of
 * silence at the end of the buffer returned so that the cushion
 * adjustment functions can use it to decrease the cushion.
 *
 * Note: amount is number of samples to get and not sampling intervals!
 */

int
mix_get_audio(mix_struct *ms, int amount, sample **bufp)
{
	int	silence;

        xmemchk();
	assert(amount < ms->buf_len);
	if (amount > ms->dist) {
		/*
 		 * If we dont have enough to give one of two things
		 * must have happened.
		 * a) There was silence :-)
		 * b) There wasn't enough time to decode the stuff...
		 * In either case we will have to return silence for
		 * now so zero the rest of the buffer and move the head.
		 */
		silence = amount - ms->dist;
		if (ms->head + silence > ms->buf_len) {
#ifdef DEBUG_MIX
			fprintf(stderr,"Insufficient audio: zeroing end of mix buffer %d %d\n", ms->buf_len - ms->head, silence + ms->head - ms->buf_len);
#endif
			audio_zero(ms->mix_buffer + ms->head, ms->buf_len - ms->head, DEV_S16);
			audio_zero(ms->mix_buffer, silence + ms->head - ms->buf_len, DEV_S16);
		} else {
			audio_zero(ms->mix_buffer + ms->head, silence, DEV_S16);
		}
                xmemchk();
		ms->head      += silence;
		ms->head      %= ms->buf_len;
		ms->head_time  = ts_add(ms->head_time,
                                        ts_map32(ms->rate, silence/ms->channels));
		ms->dist       = amount;
		assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);
	} else {
		silence = 0;
	}

	if (ms->tail + amount > ms->buf_len) {
		/*
		 * We have run into the end of the buffer so we will
		 * have to copy stuff before we return it.
		 * The space after the 'end' of the buffer is used
		 * for this purpose as the space before is used to
		 * hold silence that is returned in case the cushion
		 * grows too much.
		 * Of course we could use both here (depending on which
		 * direction involves less copying) and copy actual
		 * voice data in the case a cushion grows into it.
		 * The problem is that in that case we are probably in
		 * trouble and want to avoid doing too much...
		 *
		 * Also if the device is working in similar boundaries
		 * to our chunk sizes and we are a bit careful about the
		 * possible cushion sizes this case can be avoided.
		 */
                xmemchk();
		memcpy(ms->mix_buffer + ms->buf_len, ms->mix_buffer, BYTES_PER_SAMPLE*(ms->tail + amount - ms->buf_len));
                xmemchk();
#ifdef DEBUG_MIX
		fprintf(stderr,"Copying start of mix len: %d\n", ms->tail + amount - ms->buf_len);
#endif
	}
	*bufp = ms->mix_buffer + ms->tail;
	ms->tail_time = ts_add(ms->tail_time, 
                               ts_map32(ms->rate, amount/ms->channels));
	ms->tail      += amount;
	ms->tail      %= ms->buf_len;
	ms->dist      -= amount;
	assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);

	return silence;
}

/*
 * We need the amount of time we went dry so that we can make a time
 * adjustment to keep in sync with the receive buffer etc...
 *
 */
void
mix_get_new_cushion(mix_struct *ms, int last_cushion_size, int new_cushion_size,
			int dry_time, sample **bufp)
{
	int	diff, elapsed_time;

#ifdef DEBUG_MIX
	fprintf(stderr, "Getting new cushion %d old %d\n", new_cushion_size, last_cushion_size);
#endif

	elapsed_time = (last_cushion_size + dry_time);
	diff = abs(new_cushion_size - elapsed_time) * ms->channels;
#ifdef DEBUG_MIX
        fprintf(stderr,"new cushion size %d\n",new_cushion_size);
#endif
	if (new_cushion_size > elapsed_time) {
		/*
		 * New cushion is larger so move tail back to get
		 * the right amount and end up at the correct time.
		 * The effect of moving the tail is that some old
		 * audio and/or silence will be replayed. We do not
		 * care to much as we are right after an underflow.
		 */
		ms->tail -= diff;
		if (ms->tail < 0) {
			ms->tail += ms->buf_len;
		}
		ms->dist += diff;
		assert(ms->dist <= ms->buf_len);
		ms->tail_time = ts_sub(ms->tail_time,
                                       ts_map32(ms->rate, diff/ms->channels));
		assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);
	} else if (new_cushion_size < elapsed_time) {
		/*
		 * New cushion is smaller so we have to throw away
		 * some audio.
		 */
		ms->tail += diff;
		ms->tail %= ms->buf_len;
		ms->tail_time = ts_add(ms->tail_time,
                                       ts_map32(ms->rate, diff/ms->channels));
		if (diff > ms->dist) {
			ms->head = ms->tail;
			ms->head_time = ms->tail_time;
			ms->dist = 0;
		} else {
			ms->dist -= diff;
		}
		assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);
	}
	mix_get_audio(ms, new_cushion_size * ms->channels, bufp);
}

#define POWER_METER_SAMPLES 160

void
mix_update_ui(session_struct *sp, mix_struct *ms)
{
	sample	*bp;

	if (ms->tail < POWER_METER_SAMPLES) {
		bp = ms->mix_buffer + ms->buf_len - POWER_METER_SAMPLES * ms->channels;
	} else {
		bp = ms->mix_buffer + ms->tail - POWER_METER_SAMPLES;
	}
	ui_output_level(sp, lin2vu(avg_audio_energy(bp, POWER_METER_SAMPLES, 1), 100, VU_OUTPUT));
}

int
mix_active(mix_struct *ms)
{
        return !ts_eq(ms->head_time, ms->tail_time);
}
