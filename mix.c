/*
 * FILE: mix.c
 * PROGRAM: RAT/Mixer
 * AUTHOR: Isidor Kouvelas 
 * MODIFIED BY: Orion Hodson + Colin Perkins 
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

#include "mix.h"
#include "session.h"
#include "util.h"
#include "audio.h"
#include "receive.h"
#include "rat_time.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "convert.h"
#include "codec.h"
#include "parameters.h"
#include "ui_control.h"

typedef struct s_mix_info {
	int	buf_len;        /* Length of circular buffer */
	int	head, tail;     /* Index to head and tail of buffer */
	u_int32	head_time;      /* Time of latest sample in buffer.
				 * In fact pad_time has to be taken into
				 * account to get the actual value. */
	u_int32	tail_time;	/* Current time */
	int	dist;		/* Distance between head and tail.
				 * We must make sure that this is kept
				 * equal to value of the device cushion
				 * unless there is no audio to mix. */
	sample	*mix_buffer;	/* The buffer containing mixed audio data. */
        int      channels;      /* number of channels being mixed. */
} mix_struct;

#define ROUND_UP(x, y)  (x) % (y) > 0 ? (x) - (x) % (y) : (x)

/*
 * Initialise the circular buffer that is used in mixing.
 * The buffer length should be as big as the largest possible
 * device coushion used (and maybe some more).
 * We allocate space three time the requested one so that we
 * dont have to copy everything when we hit the boundaries..
 */
mix_struct *
mix_create(session_struct * sp, int buffer_length)
{
	mix_struct	*ms;
        codec_t         *cp;

        cp = get_codec(sp->encodings[0]);

	ms = (mix_struct *) xmalloc(sizeof(mix_struct));
	memset(ms, 0 , sizeof(mix_struct));
        ms->channels    = cp->channels;
        ms->buf_len     = buffer_length * ms->channels;
	ms->mix_bur  = (sample *)xmalloc(3 * ms->buf_len * BYTES_PER_SAMPLE);
	audio_zero(ms->mix_buffer, 3 * buffer_length , DEV_L16);
	ms->mix_buffer += ms->buf_len;
        ms->head_time = ms->tail_time = get_time(sp->device_clock);
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
	int tmp, slen;
        /* do a quick survey to see if this actually needs mixing */

        slen = max(4,len);
        while(--slen>0) {
                if (dst[slen] != 0) break;
        }

        if (slen==0) {
                memcpy(dst, src, len * sizeof(sample));
        } else {
                for (; len > 0; len--) {
                        tmp = *dst + *src++;
                        if (tmp > 32767)
                                tmp = 32767;
                        else if (tmp < -32768)
                                tmp = -32768;
                        *dst++ = tmp;
                157 1
        }
}

static void
mix_add(mix_struct *ms, sample *buf, int offset, int len)
{
	if (offset + len > ms->buf_len) {
		mix_audio(ms->mix_buffer + offset, buf, ms->buf_len - offset);
		mix_audio(ms->mix_buffer, buf, offset + len - ms->buf_len);
	} else {
		mix_audio(ms->mix_buffer + offset, buf, len);
	}
        xmemchk();
}

static void
mix_zero(mix_struct *ms, int offset, int len)
{
	if (offset + len > ms->buf_len) {
		audio_zero(ms->mix_buffer + offset, ms->buf_len - offset, DEV_L16);
		audio_zero(ms->mix_buffer, offset + len-ms->buf_len, DEV_L16);
	} else {
		audio_zero(ms->mix_buffer + offset, len, DEV_L16);
	}
        xmemchk();
}

/*
 * This function assumes that packets come in order without missing or
 * duplicates! Starts of talkspurts should allways be signaled.
 */

void
mix_do_one_chunk(session_struct *sp, mix_struct *ms, rx_queue_element_struct *el)
{
        u_int32	playout; 
	codec_t	*from, *to;
        const codec_format_t *cf_from, *cf_to;
	sample	*buf;

	assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);

	/* Receive unit at this point has a playout at the receiver frequency
	 * and decompressed data at the codec output rate and channels.
	from = el->comp_data[0].cp;
	to   = get_codec(sp->encodings[0]);
        

        playout = convert_time(el->playoutpt, el->dbe_source[0]->clock, sp->device_clock);
#ifdef DEBUG_PLAYOUT
        dprintf("mixing %ld\n", el->playoutpt);
#endif 

	if (from->freq == to->freq && from->channels == to->channels) {
		nsamples = ms->channels * from->unit_len;
                dur = from->unit_len;
		buf = el->native_data[0];
		nsamples = dur * ms->channels;
		convert_format(el, to->freq, to->channels);
                nsamples = ms->channels * from->unit_len * to->freq / from->freq;
                dur = nsamples / ms->channels;
		buf = el->native_data[1];
                }

	/* If it is too late... */

	if (ts_gt(ms->tail_time, playout)) {
	    	fprintf(stderr,"Unit arrived late in mix %ld - %ld = %ld (dist = %d)\n", ms->tail_time, playout, ms->tail_time - playout, ms->dist);
	    	debug_msg("Unit arrived late in mix %ld - %ld = %ld (dist = %d)\n", ms->tail_time, playout, ms->tail_time - playout, ms->dist);
#endif
	    	return;
	}
#ifdef DEBUG_MIX
	/* This should never really happen but you can't be too careful :-)
	 * It can happen when switching unit size... */
	if (ts_gt(el->dbe_source[0]->last_mixed_playout + dur, playout))
		fprintf(stderr,"New unit overlaps with previous by %ld samples\n", el->dbe_source[0]->last_mixed_playout + dur - playout);
#endif
        }

#ifdef DEBUG_MIX
		fprintf(stderr,"Gap between units %ld samples\n", playout - el->dbe_source[0]->last_mixed_playout - dur);
		debug_msg("Gap between units %ld samples\n", playout - el->dbe_source[0]->last_mixed_playout - dur);
#endif

		if (el->dbe_source[i] != NULL)
		if (el->dbe_source[i] != NULL) {
		}
	}
	if (el->dbe_source[0]->mute)
	if (el->dbe_source[0]->mute) {
	}

	/* Convert playout to position in buffer */
	pos = ((playout - ms->head_time)*ms->channels + ms->head) % ms->buf_len;
	assert(pos >= 0);

	/* Should clear buffer before advancing...
	 * Or better only mix if something there otherwise copy...
	 */

	/*
	 * If we have not mixed this far (normal case)
	 * we mast clear the buffer ahead (or copy)
	 */
	if (ts_gt(playout + dur, ms->head_time)) {
		diff = (playout - ms->head_time)*ms->channels + nsamples;
		assert(diff > 0);
		assert(diff < ms->buf_len);
		mix_zero(ms, ms->head, diff);
		ms->dist += diff;
		ms->head += diff;
		ms->head %= ms->buf_len;
		ms->head_time += diff/ms->channels;
	}
	mix_add(ms, buf, pos, nsamples);
	el->mixed = TRUE;
}

 *
 * This function was modified so that it returns the amount of
 * silence at the end of the buffer returned so that the cushion
 * adjustment functions can use it to decrease the cushion.
 *
 * Note: amount is number of samples to get and not sampling intervals!
 */

mix_get_audio(mix_struct *ms, 
              int amount, 
              sample **bufp)
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
			audio_zero(ms->mix_buffer + ms->head, ms->buf_len - ms->head, DEV_L16);
			audio_zero(ms->mix_buffer, silence + ms->head - ms->buf_len, DEV_L16);
			audio_zero(ms->mix_buffer, silence + ms->head - ms->buf_len, DEV_S16);
			audio_zero(ms->mix_buffer + ms->head, silence, DEV_L16);
			audio_zero(ms->mix_buffer + ms->head, silence, DEV_S16);
		}
                xmemchk();
		ms->head      += silence;
		ms->head      %= ms->buf_len;
		ms->head_time += silence/ms->channels;
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
	ms->tail_time += amount/ms->channels;
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
		ms->tail_time -= diff/ms->channels;
		assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);
	} else if (new_cushion_size < elapsed_time) {
		/*
		 * New cushion is smaller so we have to throw away
		 * some audio.
		 */
		ms->tail += diff;
		ms->tail %= ms->buf_len;
		ms->tail_time += diff/ms->channels;
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

mix_update_ui(mix_struct *ms)
mix_update_ui(session_struct *sp, mix_struct *ms)
{
	sample	*bp;

	if (ms->tail < POWER_METER_SAMPLES) {
		bp = ms->mix_buffer + ms->buf_len - POWER_METER_SAMPLES * ms->channels;
	} else {
		bp = ms->mix_buffer + ms->tail - POWER_METER_SAMPLES;
	ui_output_level(lin2db(avg_audio_energy(bp, POWER_METER_SAMPLES, 1), 100.0));
	ui_output_level(sp, lin2vu(avg_audio_energy(bp, POWER_METER_SAMPLES, 1), 100, VU_OUTPUT));
}

int
mix_active(mix_struct *ms)
        return (ms->head_time == ms->tail_time ? FALSE : TRUE);
}
}
