/*
 * FILE:        mix.c
 * PROGRAM:     RAT
 * AUTHOR:      Isidor Kouvelas 
 * MODIFIED BY: Orion Hodson + Colin Perkins 
 *
 * Copyright (c) 1995-2000 University College London
 * All rights reserved.
 */
 
#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
        "$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "session.h"
#include "codec_types.h"
#include "codec.h"
#include "audio_util.h"
#include "audio_fmt.h"
#include "timers.h"
#include "channel_types.h"
#include "pdb.h"
#include "source.h"
#include "mix.h"
#include "playout.h"
#include "debug.h"
#include "parameters.h"
#include "ui_send_audio.h"

#define MIX_MAGIC 0x81654620

struct s_mixer {
        int   buf_len;        /* Length of circular buffer               */
        int   head, tail;     /* Index to head and tail of buffer        */
        ts_t  head_time;      /* Time of latest sample in buffer.        */
                              /* In fact pad_time has to be taken into   */
                              /* account to get the actual value.        */
        ts_t tail_time;       /* Current time                            */
        int  dist;            /* Distance between head and tail.         */
                              /* We must make sure that this is kept     */ 
                              /* equal to value of the device cushion    */
                              /* unless there is no audio to mix.        */
        sample  *mix_buffer;  /* The buffer containing mixed audio data. */
        mixer_info_t info;      
        uint32_t     magic;   /* Debug check value                       */
};

typedef void (*mix_f)(sample *buf, sample *incoming, int len);
static mix_f audio_mix_fn;

static void
mix_verify(mixer_t *ms) 
{
#ifdef DEBUG
        ts_t delta;
        int  dist;

        assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);

        assert(!ts_gt(ms->tail_time, ms->head_time));

        assert(ms->dist <= ms->buf_len);

        delta = ts_sub(ms->head_time, ms->tail_time);

        dist = delta.ticks * ms->info.channels * ms->info.sample_rate / ts_get_freq(delta);
        assert(abs((int)ms->dist - (int)dist) <= 1);

        if (ts_eq(ms->head_time, ms->tail_time)) {
                assert(ms->head == ms->tail);
        }
#endif 
        assert(ms->magic == MIX_MAGIC);
}

/*
 * Initialise the circular buffer that is used in mixing.
 * The buffer length should be as big as the largest possible
 * device cushion used (and maybe some more).
 * We allocate space three times the requested one so that we
 * dont have to copy everything when we hit the boundaries..
 */
int
mix_create(mixer_t            **ppms, 
           const mixer_info_t  *pmi)
{
        mixer_t *pms;

        pms = (mixer_t *) xmalloc(sizeof(mixer_t));
        if (pms) {
                memset(pms, 0 , sizeof(mixer_t));
                pms->magic       = MIX_MAGIC;
                memcpy(&pms->info, pmi, sizeof(mixer_info_t));
                pms->buf_len     = pms->info.buffer_length * pms->info.channels;
                pms->mix_buffer  = (sample *)xmalloc(3 * pms->buf_len * BYTES_PER_SAMPLE);
                audio_zero(pms->mix_buffer, 3 * pms->info.buffer_length , DEV_S16);
                pms->mix_buffer += pms->buf_len;
                pms->head_time = pms->tail_time = ts_map32(pms->info.sample_rate, 0);
                *ppms = pms;

                audio_mix_fn = audio_mix;
#ifdef WIN32
                if (mmx_present()) {
                        audio_mix_fn = audio_mix_mmx;
                }
#endif /* WIN32 */
                mix_verify(pms);
                return TRUE;
        }
        return FALSE;
}

void
mix_destroy(mixer_t **ppms)
{
        mixer_t *pms;

        assert(ppms);
        pms = *ppms;
        assert(pms);
        mix_verify(pms);
        xfree(pms->mix_buffer - pms->buf_len); /* yuk! ouch! splat! */
        xfree(pms);
        *ppms = NULL;
}

static void
mix_zero(mixer_t *ms, int offset, int len)
{
        assert(len <= ms->buf_len);
        if (offset + len > ms->buf_len) {
                audio_zero(ms->mix_buffer + offset, ms->buf_len - offset, DEV_S16);
                audio_zero(ms->mix_buffer, offset + len-ms->buf_len, DEV_S16);
        } else {
                audio_zero(ms->mix_buffer + offset, len, DEV_S16);
        }
        xmemchk();
}


/* mix_put_audio mixes a single audio frame into mix buffer.  It returns
 * TRUE if incoming audio frame is compatible with mix, FALSE
 * otherwise.  */

int
mix_put_audio(mixer_t     *ms,
              pdb_entry_t *pdbe,
              coded_unit  *frame,
              ts_t         playout)
{
        static int hits;
        sample  *samples;

        uint32_t nticks, nsamples, pos, original_head;
        uint16_t channels, rate;
        ts_t     frame_period, expected_playout, delta, pot_head_time;
        ts_t     orig_head_time;

        int zero_start = -1, zero_len = -1;

        mix_verify(ms);
        hits++;
        codec_get_native_info(frame->id, &rate, &channels);

        orig_head_time = ms->head_time;
        original_head  = ms->head;
        if (rate != ms->info.sample_rate || channels != ms->info.channels) {
                /* This should only occur if source changes sample rate
                 * mid-stream and before buffering runs dry in end host.
                 * This should be a very rare event.
                 */
                debug_msg("Unit (%d, %d) not compitible with mix (%d, %d).\n",
                          rate,
                          channels,
                          ms->info.sample_rate,
                          ms->info.channels);
                return FALSE;
        }

        assert(rate     == (uint32_t)ms->info.sample_rate);
        assert(channels == (uint32_t)ms->info.channels);
	
        nticks          = frame->data_len / (sizeof(sample) * channels);
        frame_period    = ts_map32(rate, nticks);
	
	/* Map frame period to mixer time base, otherwise we can get truncation
	 * errors in verification of mixer when sample rate conversion is active.
	 */
	frame_period    = ts_convert(ms->info.sample_rate, frame_period);
	
        if (pdbe->first_mix) {
                debug_msg("New mix\n");
                pdbe->last_mixed = ts_sub(playout, frame_period);
                pdbe->first_mix  = 0;
        }

        mix_verify(ms);

        samples  = (sample*)frame->data;
        nsamples = frame->data_len / sizeof(sample);

        mix_verify(ms);

        /* Potential time for head */
        pot_head_time = ts_add(playout, 
                               ts_map32(ms->info.sample_rate, nsamples / ms->info.channels));

        /* Check for overlap in decoded frames */
        expected_playout = ts_add(pdbe->last_mixed, frame_period);
        if (!ts_eq(expected_playout, playout)) {
                if (ts_gt(expected_playout, playout)) {
                        delta = ts_sub(expected_playout, playout);
                        if (ts_gt(frame_period, delta)) {
                                uint32_t  trim;
				delta = ts_convert(ms->info.sample_rate, delta);
				trim = delta.ticks * ms->info.channels;
                                samples  += trim;
                                assert(nsamples > trim);
				nsamples -= trim;
				debug_msg("Mixer trimmed %d samples (%d)\n", trim, playout.ticks);
                        } else {
                                debug_msg("Skipped unit\n");
                        }
                } else {
                        if (expected_playout.ticks - playout.ticks != 0) {
                                debug_msg("Gap between units %d %d\n", 
                                          expected_playout.ticks, 
                                          playout.ticks);
                        }
                }
        }

        /* If mixer has been out of use, fire it up */
        if (ts_eq(ms->head_time, ms->tail_time)) {
                mix_verify(ms);
                ms->head_time = ms->tail_time = playout;
                assert(ms->head == ms->tail);
                ms->head = ms->tail;
                ms->dist = 0;
                mix_verify(ms);
        }

        /* Zero ahead if new potential head time is greater than existing */
        if (ts_gt(pot_head_time, ms->head_time))  {
                int zeros;
                delta = ts_sub(pot_head_time, ms->head_time);
                zeros = delta.ticks * ms->info.channels * ms->info.sample_rate / ts_get_freq(delta);
                mix_verify(ms);
                mix_zero(ms, ms->head, zeros);
                zero_start = ms->head;
                zero_len   = zeros;
                ms->dist += zeros;
                ms->head += zeros;
                ms->head %= ms->buf_len;
                ms->head_time = pot_head_time;
                mix_verify(ms);
        }

        assert(!ts_gt(playout, ms->head_time));

        /* Work out where to write the data */
        pos = ms->head - nsamples;
        if ((uint32_t)ms->head < nsamples) {
                /* Head has just wrapped.  Want to start from before */
                /* the wrap...                                       */
                pos += ms->buf_len;
        }
        
        if (pos + nsamples > (uint32_t)ms->buf_len) { 
            xmemchk();    
				audio_mix_fn(ms->mix_buffer + pos, 
                             samples, 
                             ms->buf_len - pos); 
                xmemchk();
                audio_mix_fn(ms->mix_buffer, 
                             samples + (ms->buf_len - pos), 
                             pos + nsamples - ms->buf_len); 
                xmemchk();
        } else { 
                audio_mix_fn(ms->mix_buffer + pos, 
                             samples, 
                             nsamples); 
                xmemchk();
        } 
        pdbe->last_mixed = playout;

        return TRUE;
}

/*
 * The mix_get_audio function returns a pointer to "request" samples of mixed 
 * audio data, suitable for playout (ie: you can do audio_device_write() with
 * the returned data).
 *
 * This function was modified so that it returns the amount of
 * silence at the end of the buffer returned so that the cushion
 * adjustment functions can use it to decrease the cushion.
 *
 * Note: "request" is number of samples to get and not sampling intervals!
 */

int
mix_get_audio(mixer_t *ms, int request, sample **bufp)
{
        int        silence, amount;
        ts_t    delta;

        xmemchk();
        mix_verify(ms);
        amount = request;
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
                        debug_msg("Insufficient audio: zeroing end of mix buffer %d %d\n", ms->buf_len - ms->head, silence + ms->head - ms->buf_len);
#endif
                        audio_zero(ms->mix_buffer + ms->head, ms->buf_len - ms->head, DEV_S16);
                        audio_zero(ms->mix_buffer, silence + ms->head - ms->buf_len, DEV_S16);
                } else {
                        audio_zero(ms->mix_buffer + ms->head, silence, DEV_S16);
                }
                xmemchk();
                mix_verify(ms);
                ms->head      += silence;
                ms->head      %= ms->buf_len;
                ms->head_time  = ts_add(ms->head_time,
                                        ts_map32(ms->info.sample_rate, silence/ms->info.channels));
                ms->dist       = amount;
                mix_verify(ms);
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
                debug_msg("Copying start of mix len: %d\n", ms->tail + amount - ms->buf_len);

#endif /* DEBUG_MIX */
        }

#ifdef DEBUG_MIX
/*
        debug_msg("Head %u (%d), tail %u (%d)\n", (uint32_t) ms->head, 
                  ms->head_time.ticks, 
                  (uint32_t)ms->tail,
                  ms->tail_time.ticks);
                  */
#endif /* DEBUG_MIX */

        mix_verify(ms);

        *bufp = ms->mix_buffer + ms->tail;
        delta = ts_map32(ms->info.sample_rate, amount/ms->info.channels);
        ms->tail_time = ts_add(ms->tail_time, delta);
                               
        ms->tail      += amount;
        ms->tail      %= ms->buf_len;
        ms->dist      -= amount;
        mix_verify(ms);

        return silence;
}

/*
 * We need the amount of time we went dry so that we can make a time
 * adjustment to keep in sync with the receive buffer etc...
 */
void
mix_new_cushion(mixer_t *ms, 
                int      last_cushion_size, 
                int      new_cushion_size, 
                int      dry_time, 
                sample **bufp)
{
        int diff, elapsed_time;

        mix_verify(ms);
#ifdef DEBUG_MIX
        debug_msg("Getting new cushion %d old %d\n", new_cushion_size, last_cushion_size);
#endif

        elapsed_time = (last_cushion_size + dry_time);
        diff = abs(new_cushion_size - elapsed_time) * ms->info.channels;
#ifdef DEBUG_MIX
        debug_msg("new cushion size %d\n",new_cushion_size);
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

                ms->tail_time = ts_sub(ms->tail_time,
                                       ts_map32(ms->info.sample_rate, diff/ms->info.channels));
                mix_verify(ms);
        } else if (new_cushion_size < elapsed_time) {
                /*
                 * New cushion is smaller so we have to throw away
                 * some audio.
                 */
                ms->tail += diff;
                ms->tail %= ms->buf_len;
                ms->tail_time = ts_add(ms->tail_time,
                                       ts_map32(ms->info.sample_rate, diff/ms->info.channels));
                if (diff > ms->dist) {
                        ms->head = ms->tail;
                        ms->head_time = ms->tail_time;
                        ms->dist = 0;
                } else {
                        ms->dist -= diff;
                }
                mix_verify(ms);
        }
        mix_verify(ms);
        mix_get_audio(ms, new_cushion_size * ms->info.channels, bufp);
        mix_verify(ms);
}

uint16_t
mix_get_energy(mixer_t *ms, uint16_t samples)
{
        sample        *bp;

        if (ms->tail < samples) {
                bp = ms->mix_buffer + ms->buf_len - samples * ms->info.channels;
        } else {
                bp = ms->mix_buffer + ms->tail - samples;
        }

        return audio_avg_energy(bp, samples, 1);
}

int
mix_active(mixer_t *ms)
{
        mix_verify(ms);
        return !ts_eq(ms->head_time, ms->tail_time);
}

const mixer_info_t *
mix_query(const mixer_t *ms)
{
        if (ms == NULL) {
                return FALSE;
        }
        return &ms->info;
}

