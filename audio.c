/*
 * FILE:     audio.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson / Isidor Kouvelas / Colin Perkins 
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
#include "debug.h"
#include "audio.h"
#include "session.h"
#include "transcoder.h"
#include "ui.h"
#include "transmit.h"
#include "mix.h"
#include "codec.h"
#include "channel.h"
#include "cushion.h"
#include "timers.h"
#include "receive.h"
#include "sndfile.h"

/* Zero buf used for writing zero chunks during cushion adaption */
static sample* audio_zero_buf;

#define C0 +0.46363718
#define C1 -0.92724705
#define C2 +0.46363718
#define D1 -1.9059465
#define D2 +0.9114024

#define IC0 +475
#define IC1 -950
#define IC2 +475
#define ID1 -1952
#define ID2 +933

typedef struct s_bias_ctl {
        /* for 8k pre-filtering */
        sample y1, y2;
        sample x1, x2;
        /* for rest */
        sample lta;
        u_char step;
        int    freq;
} bias_ctl;

static bias_ctl *
bias_ctl_create(int channels, int freq)
{
        bias_ctl *bc = (bias_ctl*)xmalloc(channels*sizeof(bias_ctl));
        memset(bc, 0, channels*sizeof(bias_ctl));
        bc->step = channels;
        bc->freq = freq;
        return bc;
}

__inline static void
prefilter(bias_ctl *pf, sample *buf, register int len, int step)
{
        register int y0, y1, y2;
        register int x0, x1, x2;

        y1 = pf->y1;
        y2 = pf->y2;
        x1 = pf->x1;
        x2 = pf->x2;
        
        while(len-- != 0) {
                x0 = *buf;
                y0 = (IC0 * x0 + IC1 * x1 + IC2 * x2 - ID1 * y1 - ID2 * y2) >> 10;
                *buf = y0 << 1;
                buf += step;                
                y2 = y1; y1 = y0;
                x2 = x1; x1 = x0;
        }

        pf->y1 = y1;
        pf->y2 = y2;
        pf->x1 = x1;
        pf->x2 = x2;
}

static void
remove_lta(bias_ctl *bc, sample *buf, register int len, int step)
{
        int  m, samples;
        m = 0;
        samples = len;

        while (len-- > 0) {
                m += *buf;
                *buf -= bc->lta;
                buf += step;
        }

        bc->lta -= (bc->lta - m / samples) >> 3;
}

static void
bias_ctl_destroy(bias_ctl *bc)
{
        xfree(bc);
}

void
audio_unbias(bias_ctl *bc, sample *buf, int len)
{
        if (bc->freq == 8000) {
                if (bc->step == 1) {
                        prefilter(bc, buf, len, 1);
                } else {
                        len /= bc->step;
                        prefilter(bc  , buf  , len, 2);
                        prefilter(bc+1, buf+1, len, 2);
                }
        } else {

                if (bc->step == 1) {
                        remove_lta(bc, buf, len, 1); 
                } else {
                        remove_lta(bc  , buf  , len / 2, 2);
                        remove_lta(bc+1, buf+1, len / 2, 2);
                }
        }
} 

void
audio_zero(sample *buf, int len, deve_e type)
{
	assert(len>=0);
	switch(type) {
	case DEV_PCMU:
		memset(buf, PCMU_AUDIO_ZERO, len);
		break;
	case DEV_S8:
		memset(buf, 0, len);
		break;
	case DEV_S16:
		memset(buf, 0, 2*len);
		break;
	default:
		fprintf(stderr, "%s:%d Type not recognized", __FILE__, __LINE__);
		break;
	}
}

int
audio_device_write(session_struct *sp, sample *buf, int dur)
{
        codec_t *cp = get_codec_by_pt(sp->encodings[0]);

	if (sp->audio_device) {
                const audio_format *ofmt = audio_get_ofmt(sp->audio_device);
                if (sp->out_file) {
                        snd_write_audio(&sp->out_file, buf, (u_int16)(dur * ofmt->channels));
                }
		if (sp->mode == TRANSCODER) {
			return transcoder_write(sp->audio_device, buf, dur*cp->channels);
		} else {
			return audio_write(sp->audio_device, buf, dur * ofmt->channels);
		}
        } else {
		return (dur * cp->channels);
        }
}

int
audio_device_take(session_struct *sp)
{
	codec_t		*cp;
	audio_format    format;

        if (sp->audio_device) {
                return (TRUE);
        }

	cp = get_codec_by_pt(sp->encodings[0]);
	format.encoding        = DEV_S16;
	format.sample_rate     = cp->freq;
        format.bits_per_sample = 16;
	format.channels    = cp->channels;
        format.bytes_per_block       = cp->unit_len * cp->channels * BYTES_PER_SAMPLE;

	if (sp->mode == TRANSCODER) {
		if ((sp->audio_device = transcoder_open()) == 0) {
                        return FALSE;
                }
	} else {
		sp->audio_device = audio_open(&format, &format);

                if (!sp->audio_device) {
                        /* Maybe we cannot support this format. Try minimal
                         * case - ulaw 8k.
                         */
                        audio_format fallback;
                        cp = get_codec_by_name("PCMU-8K-MONO");
                        assert(cp); /* in case someone changes codec name */

                        sp->encodings[0] = cp->pt;

                        fallback.encoding        = DEV_S16;
                        fallback.sample_rate     = cp->freq;
                        fallback.bits_per_sample = 16;
                        fallback.channels    = cp->channels;
                        fallback.bytes_per_block       = cp->unit_len * cp->channels * BYTES_PER_SAMPLE;
                        sp->audio_device = audio_open(&fallback, &fallback);
                        
                        if (sp->audio_device) {
                                /* Make format consistent just in case. */
                                memcpy(&format, &fallback, sizeof(audio_format));
                                debug_msg("Could use requested format, but could use mulaw 8k\n");
                        }
                }

                if (!sp->audio_device) {
                        /* Both original request and fallback request to
                         * device have failed.  This probably means device is in use.
                         * So now use null device with original format requested.
                         */
                        audio_set_interface(audio_get_null_interface());
                        sp->audio_device = audio_open(&format, &format);
                        debug_msg("Using null audio device\n");
                        assert(sp->audio_device != 0);
                }

		audio_drain(sp->audio_device);
	
		if (sp->input_mode!=AUDIO_NO_DEVICE) {
			audio_set_iport(sp->audio_device, sp->input_mode);
			audio_set_gain(sp->audio_device, sp->input_gain);
		} else {
			sp->input_mode=audio_get_iport(sp->audio_device);
			sp->input_gain=audio_get_gain(sp->audio_device);
		}
		if (sp->output_mode!=AUDIO_NO_DEVICE) {
			audio_set_oport(sp->audio_device, sp->output_mode);
			audio_set_volume(sp->audio_device, sp->output_gain);
		} else {
			sp->output_mode=audio_get_oport(sp->audio_device);
			sp->output_gain=audio_get_volume(sp->audio_device);
		}
	}
        
        if (audio_zero_buf == NULL) {
                audio_zero_buf = (sample*) xmalloc (format.bytes_per_block * sizeof(sample));
                audio_zero(audio_zero_buf, format.bytes_per_block, DEV_S16);
        }

        if ((sp->audio_device != 0) && (sp->mode != TRANSCODER)) {
                audio_non_block(sp->audio_device);
        }

        /* We initialize the pieces above the audio device here since their parameters
         * depend on what is set here
         */
        if (sp->device_clock) xfree(sp->device_clock);
        sp->device_clock = new_time(sp->clock, cp->freq);
        sp->meter_period = cp->freq / 15;
        sp->bc           = bias_ctl_create(cp->channels, cp->freq);
        sp->tb           = tx_create(sp, (u_int16)cp->unit_len, (u_int16)cp->channels);
        sp->ms           = mix_create(sp, 32640);
        cushion_create(&sp->cushion, cp->unit_len);
        tx_igain_update(sp);
        ui_update(sp);
        return sp->audio_device;
}

void
audio_device_give(session_struct *sp)
{
	gettimeofday(&sp->device_time, NULL);

	if (sp->audio_device) {
                tx_stop(sp);
		if (sp->mode == TRANSCODER) {
			transcoder_close(sp->audio_device);
		} else {
		        sp->input_mode = audio_get_iport(sp->audio_device);
		        sp->output_mode = audio_get_oport(sp->audio_device);
			audio_close(sp->audio_device);
		}
		sp->audio_device = 0;
	} else {
                return;
        }

        if (audio_zero_buf) {
                xfree(audio_zero_buf);
                audio_zero_buf = NULL;
        }
        
        bias_ctl_destroy(sp->bc);
        sp->bc = NULL;

        cushion_destroy(sp->cushion);
        mix_destroy(sp->ms);
        tx_destroy(sp);
        playout_buffers_destroy(sp, &sp->playout_buf_list);
}

void
audio_device_reconfigure(session_struct *sp)
{
        u_int16 oldpt    = sp->encodings[0];
        audio_device_give(sp);
        tx_stop(sp);
        
        if (sp->next_selected_device != -1) {
                audio_set_interface(sp->next_selected_device);
                sp->next_selected_device = -1;
        } 
        
        if (sp->next_encoding != -1) {
                /* Changing encoding */
                sp->encodings[0] = sp->next_encoding;
                if (audio_device_take(sp) == FALSE) {
                        /* we failed, fallback */
                        sp->encodings[0] = oldpt;
                        audio_device_take(sp);
                }
                channel_set_coder(sp, sp->encodings[0]);
                sp->next_encoding = -1;
        } else {
                /* Just changing device */
                audio_device_take(sp);
                channel_set_coder(sp, sp->encodings[0]);
        }
}


/* This function needs to be modified to return some indication of how well
 * or not we are doing.                                                    
 */
int
read_write_audio(session_struct *spi, session_struct *spo,  struct s_mix_info *ms)
{
        u_int32 cushion_size, read_dur;
        struct s_cushion_struct *c;
	int	trailing_silence, new_cushion, cushion_step, diff;
        const audio_format* ofmt;
	sample	*bufp;

        c = spi->cushion;
	if ((read_dur = tx_read_audio(spi)) <= 0) {
		return 0;
	} else {
                if (!spi->audio_device) {
                        /* no device means no cushion */
                        return read_dur;
                }
	}

	/* read_dur now reflects the amount of real time it took us to get
	 * through the last cycle of processing. 
         */

	if (spo->lecture == TRUE && spo->auto_lecture == 0) {
                cushion_update(c, read_dur, CUSHION_MODE_LECTURE);
	} else {
                cushion_update(c, read_dur, CUSHION_MODE_CONFERENCE);
	}

	/* Following code will try to achieve new cushion size without
	 * messing up the audio...
	 * First case is when we are in trouble and the output has gone dry.
	 * In this case we write out a complete new cushion with the desired
	 * size. We do not care how much of it is going to be real audio and
	 * how much silence so long as the silence is at the head of the new
	 * cushion. If the silence was at the end we would be creating
	 * another silence gap...
	 */
        cushion_size = cushion_get_size(c);
        ofmt         = audio_get_ofmt(spi->audio_device);

	if ( cushion_size < read_dur ) {
		/* Use a step for the cushion to keep things nicely rounded   */
                /* in the mixing. Round it up.                                */
                new_cushion = cushion_use_estimate(c);
                /* The mix routine also needs to know for how long the output */
                /* went dry so that it can adjust the time.                   */
                mix_get_new_cushion(ms, 
                                    cushion_size, 
                                    new_cushion, 
                                    (read_dur - cushion_size), 
                                    &bufp);
                audio_device_write(spo, bufp, new_cushion);
                debug_msg("catch up! read_dur(%d) > cushion_size(%d)\n",
                        read_dur,
                        cushion_size);
                cushion_size = new_cushion;
        } else {
                trailing_silence = mix_get_audio(ms, read_dur * ofmt->channels, &bufp);
                cushion_step = cushion_get_step(c);
                diff  = 0;

                if (trailing_silence > cushion_step) {
                        /* Check whether we need to adjust the cushion */
                        diff = cushion_diff_estimate_size(c);
                        if (abs(diff) < cushion_step) {
                                diff = 0;
                        }
                } 
                
                /* If diff is less than zero then we must decrease the */
                /* cushion so loose some of the trailing silence.      */
                if (diff < 0 && mix_active(ms) == FALSE && spi->playout_buf_list == NULL) {
                        /* Only decrease cushion if not playing anything out */
                        read_dur -= cushion_step;
                        cushion_step_down(c);
                        debug_msg("Decreasing cushion\n");
                }
                audio_device_write(spo, bufp, read_dur);
                /*
                 * If diff is greater than zero then we must increase the
                 * cushion so increase the amount of trailing silence.
                 */
                if (diff > 0) {
                        audio_device_write(spo, audio_zero_buf, cushion_step);
                        cushion_step_up(c);
                        debug_msg("Increasing cushion.\n");
                }
        }
        return (read_dur);
}
