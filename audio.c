/*
 * FILE:     audio.c
 * PROGRAM:  RAT
 * AUTHOR:   Isidor Kouvelas
 * MODIFIED: V.J.Hardman + Colin Perkins
 *
 * Created parmaters.c by removing all silence detcetion etc.
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

#include "rat_types.h"
#include "audio.h"
#include "util.h"
#include "session.h"
#include "transcoder.h"
#include "ui.h"
#include "transmit.h"
#include "mix.h"
#include "codec.h"

/*
 * Check if buffer is filled with zero samples...
 */
int
is_audio_zero(sample *buf, int len, deve_e type)
{
	int	s;

	if (type == DEV_PCMU) {
		s = PCMU_AUDIO_ZERO;
	} else {
		s = 0;
	}

	for (; len > 0; len--, buf++) {
		if (*buf != s) {
			return FALSE;
		}
	}
	return TRUE;
}

typedef struct s_bias_ctl {
        int   bias;
        int   ccnt;
} bias_ctl;

void
audio_unbias(bias_ctl **bcp, sample *buf, int len)
{
	int		i;
        long		sum = 0;
	float		s1, s2;
	bias_ctl	*bc = *bcp;

	if (bc == NULL)
		return;

	for(i = 0; i < len; i++) {
		sum   += buf[i];
		buf[i] = buf[i] - bc->bias; 
        }
	/* Thsi could be overkill,but we use long term average
	 * with adaptive filter coefficients.
	 * The idea being that sum can only decrease
	 */
	sum = sum / len;
	s1 = (float) abs(sum)/32767.0;
	s2 = 1.0 - s1;
	bc->bias = (int)(s2 * (float)(bc->bias) + s1 * (float)sum);

	/* Auto switch off check */
	if (abs(sum) < BD_THRESHOLD) {
		bc->ccnt ++;
	} else {
		bc->ccnt = 0;
	}
        
	if (bc->ccnt == BD_CONSECUTIVE && bc->bias < BD_THRESHOLD) {
		xfree(bc);
		*bcp = NULL;
#ifdef DEBUG
		printf("Bias correction not necessary.\n");
#endif
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
	case DEV_L8:
		memset(buf, 0, len);
		break;
	case DEV_L16:
		memset(buf, 0, 2*len);
		break;
	default:
		fprintf(stderr, "%s:%d Type not recognized", __FILE__, __LINE__);
		break;
	}
}

int
audio_device_read(session_struct *sp, sample *buf, int samples)
{
	struct timeval	curr_time;
	int		diff;

	if (sp->have_device)
		if (sp->mode == TRANSCODER) {
			return (transcoder_read(sp->audio_fd, buf, samples));
		} else {
			return (audio_read(sp->audio_fd, buf, samples));
		}
	else {
		gettimeofday(&curr_time, NULL);
		diff = (((curr_time.tv_sec - sp->device_time.tv_sec) * 1e6)
			+ (curr_time.tv_usec - sp->device_time.tv_usec)) / 125;
		memcpy(&sp->device_time, &curr_time, sizeof(struct timeval));
		if (diff > samples)
			diff = samples;
		memset(buf, 0, samples * BYTES_PER_SAMPLE);
		return (diff);
	}
}

int
audio_device_write(session_struct *sp, sample *buf, int samples)
{
	if (sp->have_device)
		if (sp->mode == TRANSCODER) {
			return (transcoder_write(sp->audio_fd, buf, samples));
		} else {
			return (audio_write(sp->audio_fd, buf, samples));
		}
	else
		return (samples);
}

int
audio_device_take(session_struct *sp)
{
	codec_t		*cp;
	audio_format    format;

        if (sp->have_device) {
                return (TRUE);
        }

	cp = get_codec(sp->encodings[0]);

	format.encoding        = DEV_L16;
	format.sample_rate     = cp->freq;
        format.bits_per_sample = 16;
	format.num_channels    = cp->channels;

	if (sp->mode == TRANSCODER) {
		if ((sp->audio_fd = transcoder_open()) == -1) {
                        return FALSE;
                }
		sp->have_device = TRUE;
		return TRUE;
	} else {
		/* XXX should pass a pointer to format ???!!! */
		if ((sp->audio_fd = audio_open(format)) == -1) {
			return (FALSE);
		}
		audio_drain(sp->audio_fd);
		sp->have_device = TRUE;
	
		if (sp->input_mode!=AUDIO_NO_DEVICE) {
			audio_set_iport(sp->audio_fd, sp->input_mode);
			audio_set_gain(sp->audio_fd, sp->input_gain);
		} else {
			sp->input_mode=audio_get_iport(sp->audio_fd);
			sp->input_gain=audio_get_gain(sp->audio_fd);
		}
		if (sp->output_mode!=AUDIO_NO_DEVICE) {
			audio_set_oport(sp->audio_fd, sp->output_mode);
			audio_set_volume(sp->audio_fd, sp->output_gain);
		} else {
			sp->output_mode=audio_get_oport(sp->audio_fd);
			sp->output_gain=audio_get_volume(sp->audio_fd);
		}

		if (sp->ui_on) {
			ui_hide_audio_busy(sp);
			ui_update(sp);
		}
		return TRUE;
	}
}

void
audio_device_give(session_struct *sp)
{
	gettimeofday(&sp->device_time, NULL);

	if (sp->have_device && sp->keep_device == FALSE) {
		if (sp->ui_on) {
			ui_show_audio_busy(sp);
		}
		if (sp->mode == TRANSCODER) {
			transcoder_close(sp->audio_fd);
		} else {
		        sp->input_mode = audio_get_iport(sp->audio_fd);
		        sp->output_mode = audio_get_oport(sp->audio_fd);
			audio_close(sp->audio_fd);
		}
		sp->audio_fd = -1;
		sp->have_device = FALSE;
	}
}

void
read_write_init(cushion_struct *cushion, session_struct *sp)
{
	audio_zero(cushion->audio_zero_buf, MAX_CUSHION, DEV_L16);
        if ((sp->audio_fd != -1) && (sp->mode != TRANSCODER)) {
                audio_non_block(sp->audio_fd);
        }
	sp->loop_delay = sp->loop_estimate = 20000;

	/* 160 = 20ms @ 8000KHz and 1 channel */
	sp->rb = read_device_init(sp, 160);
}

/*
 * SAFETY is how safe we want to be with the device going dry. If we want to
 * cover for larger future jumps in workstation delay then SAFETY should be
 * larger. Sensible values are between 0.9 and 1
 */
#define SAFETY		0.96
#ifdef SunOS_4
#define CUSHION_STEP	160
#else
#define CUSHION_STEP	80
#endif
#define HISTOGRAM_SIZE	(DEVICE_REC_BUF / CUSHION_STEP)
#define HISTORY_SIZE	500
#define COVER		((float)HISTORY_SIZE * SAFETY)

void 
audio_init(session_struct *sp, cushion_struct *c)
{
	int i, *ip;

	c->cushion_size     = 0;
	c->cushion_estimate = 160;
	c->cushion_step = CUSHION_STEP;
	c->read_history = (int *)xmalloc(HISTORY_SIZE * sizeof(int));
	for (i = 0, ip = c->read_history; i < HISTORY_SIZE; i++, ip++)
		*ip = 4;
	c->histogram = (int *)xmalloc(HISTOGRAM_SIZE * sizeof(int));
	memset(c->histogram, 0, HISTOGRAM_SIZE * sizeof(int));
	c->histogram[4] = HISTORY_SIZE;
	c->hi = 0;
        sp->input_mode = AUDIO_NO_DEVICE;
        sp->output_mode = AUDIO_NO_DEVICE;

	sp->bc = (bias_ctl*)xmalloc(sizeof(bias_ctl));
	memset(sp->bc, 0 , sizeof(bias_ctl));
}

/* This function needs to be modified to return some indication of how well
 * or not we are doing.                                                    
 */
int
read_write_audio(session_struct *spi, session_struct *spo, cushion_struct *c, struct s_mix_info *ms)
{
	int	read_len, trailing_silence, new_cushion, diff, sh, sl, ih, il;
	int	cel, ceh, i;
	sample	*bufp;

	/*
	 * The loop_delay stuff is meant to help in determining the pause
	 * length in the select in net.c for platforms where we cannot
	 * block on read availability of the audio device.
	 */
	if ((read_len = read_device(spi)) <= 0) {
		if (spi->last_zero == FALSE) {
			spi->loop_estimate += 500;
		}
		spi->last_zero  = TRUE;
		spi->loop_delay = 1000;
		return (0);
	} else {
		if (read_len > 160 && spi->loop_estimate > 5000) {
			spi->loop_estimate -= 250;
		}
		spi->last_zero  = FALSE;
		spi->loop_delay = spi->loop_estimate;
		assert(spi->loop_delay >= 0);
	}

	/* read_len now reflects the amount of real time it took us to get
	 * through the last cycle of processing. */

	/* Calculate a new value for the desired cushion on every cycle. */
	c->histogram[c->read_history[c->hi]]--;
	i = read_len / c->cushion_step;
	c->read_history[c->hi] = i;
	c->histogram[i]++;
	c->hi++;
	if (c->hi == HISTORY_SIZE)
		c->hi = 0;
	sl = sh = il = ih = 0;
	while (ih < HISTOGRAM_SIZE && sh < HISTORY_SIZE) {
		sh += c->histogram[ih];
		if (sl < COVER) {
			sl += c->histogram[ih];
			il = ih;
		}
		ih++;
	}

	/* cel ignores outliers but adds 10:3 to the cushion
	 * ceh doesn't ignore outliers and sets the cushion directly
	 * with no unusual events, ceh < cel and delay is lower */
	if (spo->lecture == TRUE && spo->auto_lecture == 0) {
		cel = (il + 10) * c->cushion_step;
		ceh = (ih + 10) * c->cushion_step;
	} else {
		cel = (il + 3) * c->cushion_step;
		ceh = ih * c->cushion_step;
	}

	c->cushion_estimate = min(cel, ceh);
	if (c->cushion_estimate < 2 * c->cushion_step)
		c->cushion_estimate = 2 * c->cushion_step;

        /* Ignore first read from the device after startup */
	if (c->cushion_size == 0)
		c->cushion_estimate = c->cushion_step;

	/* Following code will try to achieve new cushion size without
	 * messing up the audio...
	 * First case is when we are in trouble and the output has gone dry.
	 * In this case we write out a complete new cushion with the desired
	 * size. We do not care how much of it is going to be real audio and
	 * how much silence so long as the silence is at the head of the new
	 * cushion. If the silence was at the end we would be creating
	 * another silence gap...
	 */
	if (read_len > c->cushion_size) {
		/* Use a step for the cushion to keep things nicely rounded   */
		/* in the mixing. Round it up.                                */
		new_cushion = c->cushion_estimate + c->cushion_step - c->cushion_estimate % c->cushion_step;
		/* The mix routine also needs to know for how long the output */
		/* went dry so that it can adjust the time.                   */
		mix_get_new_cushion(ms, c->cushion_size, new_cushion, read_len - c->cushion_size, &bufp);
		audio_device_write(spo, bufp, new_cushion);
		if ((spo->out_file) && (spo->flake_go == 0)) {
			fwrite(bufp, BYTES_PER_SAMPLE, new_cushion, spo->out_file);
		}
		c->cushion_size = new_cushion;
	} else {
		trailing_silence = mix_get_audio(ms, read_len, &bufp);
		if (trailing_silence > c->cushion_step) {
			/* Check whether we need to adjust the cushion */
			diff = c->cushion_estimate - c->cushion_size;
			if (abs(diff) < c->cushion_step) {
				diff = 0;
			}
		} else {
			diff = 0;
		}

		/* If diff is less than zero then we must decrease the */
		/* cushion so loose some of the trailing silence.      */
		if (diff < 0) {
			read_len        -= c->cushion_step;
			c->cushion_size -= c->cushion_step;
		}
		audio_device_write(spo, bufp, read_len);
		if (spo->out_file && (spo->flake_go == 0)) {
			fwrite(bufp, BYTES_PER_SAMPLE, read_len, spo->out_file);
                        spo->flake_os-=read_len;
		}
		/*
		 * If diff is greater than zero then we must increase the
		 * cushion so increase the amount of trailing silence.
		 */
		if (diff > 0) {
			audio_device_write(spo, c->audio_zero_buf, c->cushion_step);
			if ((spo->out_file) && (spo->flake_go == 0)){
				fwrite(c->audio_zero_buf, BYTES_PER_SAMPLE, c->cushion_step, spo->out_file);
                                spo->flake_os -= c->cushion_step;
			}
			c->cushion_size += c->cushion_step;
		}
	}
	return (read_len);
}

