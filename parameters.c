/*
 *	FILE:    parameters.c
 *	PROGRAM: RAT
 *	AUTHOR:	 O. Hodson
 *
 *	$Revision$
 *	$Date$
 *
 * Copyright (c) 1998 University College London
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

#include "config.h"
#include "rat_types.h"
#include "audio.h"
#include "util.h"
#include "math.h"
#include "transmit.h"
#include "session.h"
#include "parameters.h"

#define STEP	        16
#define SD_MAX_CHANNELS  5

u_int16 
avg_audio_energy(sample *buf, u_int32 samples, u_int32 channels)
{
        register u_int32 e1, e2;
        register sample *buf_end = buf + samples;

        assert (channels > 0);
        e1 = e2 = 0;
        switch (channels) {
        case 1:
                while(buf < buf_end) {
                        e1  += abs(*buf);
                        buf += STEP;
                }
                break;
        case 2:
                /* SIMD would improve this */
                while(buf < buf_end) {
                        e1 = abs(*buf++);
                        e2 = abs(*buf);
                        buf += STEP - 1;
                }
                e1 = max(e1, e2);
        }
        return (u_int16)(e1*STEP/samples);
}

/* ad hoc values - aesthetically better than 0.1, 0.01, 0.001, and so on */
#define DB_BIAS     (0.005f)
#define DB_BIAS_LOG (-2.3f)

int 
lin2db(u_int16 energy, double peak)
{
        float quasi_db;
        energy = (energy / 8) * 8; 
        quasi_db = ( -DB_BIAS_LOG + (float)log10(DB_BIAS+(float)energy/65535.0f)) / -DB_BIAS_LOG;
        return (int) (peak * quasi_db);
}

/* The silence detection algorithm is: 
 *
 *    Everytime someone adjusts volume, or starts talking, use
 *    a short parole period to calculate reasonable threshold.
 *
 *    This assumes that person is not talking as they adjust the
 *    volume, or click on start talking button.  This can be false
 *    when the source is music, or the speaker is a politician, 
 *    project leader, etc...
 *    
 */

/* snapshot in ms to adjust silence threshold */
#define SD_PAROLE_PERIOD 100
#define SD_LOWER_COUNT  3
#define SD_RAISE_COUNT  10

typedef struct s_sd {
        u_int32 parole;
        int32 tot, tot_sq;
        u_int32 thresh;
        u_int32 lt_cnt;
        u_int32 lt_max;
        u_int32 gt_cnt;
        u_int32 gt_min;
        u_int32 peak;
        u_int32 cnt;
} sd_t;

sd_t *
sd_init(u_int16 blk_dur, u_int16 freq)
{
	sd_t *s = (sd_t *)xmalloc(sizeof(sd_t));
        s->parole = SD_PAROLE_PERIOD * freq / (blk_dur*1000) + 1;
        sd_reset(s);
	return (s);
}

void
sd_reset(sd_t *s)
{
        s->cnt         = 0;
        s->tot         = 0;
        s->tot_sq      = 0;
        s->lt_cnt      = 0;
        s->lt_max      = 0;
        s->gt_cnt      = 0;
        s->gt_min      = 0xffff;
        s->thresh      = 0xffff;
}

void
sd_destroy(sd_t *s)
{
        xfree(s);
}

#define SD_RES 8

int
sd(sd_t *s, u_int16 energy)
{
        s->tot    += energy;
        s->tot_sq += (energy * energy);

        if (s->cnt == s->parole) {
                u_int32 m,stdd,trial_thresh;

                m    = s->tot / s->cnt;
                stdd = sqrt(abs(m * m - s->tot_sq / s->cnt));

                trial_thresh = m + 3 * stdd;
                if (trial_thresh < s->thresh) {
                        if ((s->thresh - trial_thresh) > trial_thresh) {
                                s->lt_max = max(s->lt_max, trial_thresh);
                                if (s->lt_cnt++ == SD_LOWER_COUNT) {
                                        s->thresh = s->lt_max;
                                        s->lt_cnt = 0;
                                        s->lt_max = 0;
                                }
                        }
                        s->gt_min = 0xffff;
                        s->gt_cnt = 0;
                } else if (trial_thresh > s->thresh) {
                        s->gt_min = min(s->gt_min, trial_thresh);
                        if (s->gt_cnt++ == SD_RAISE_COUNT) {
                                s->thresh = (3 * s->thresh + s->gt_min) / 4;
                                s->gt_min = 0xffff;
                                s->gt_cnt = 0;
                        } 
                        s->lt_cnt = 0;
                        s->lt_max = 0;
                } else {
                        s->gt_cnt = 0;
                        s->gt_min = 0xffff;
                        s->lt_cnt = 0;
                        s->lt_max = 0;
                }
                s->tot = s->tot_sq = 0;
                s->cnt = 0;
        }

        s->cnt++;
        return (energy < s->thresh);
}

typedef struct {
        u_char sig;
        u_char pre;
        u_char post;
} vad_limit_t;

typedef struct s_vad {
        /* limits */
        vad_limit_t limit[2];
        u_int32 tick;
        u_int32 spurt_cnt;
        /* state */
        u_char state;
        u_char sig_cnt;
        u_char post_cnt;
} vad_t;

vad_t *
vad_create(u_int16 blockdur, u_int16 freq)
{
        vad_t *v = (vad_t*)xmalloc(sizeof(vad_t));
        memset(v,0,sizeof(vad_t));
        vad_config(v, blockdur, freq);
        return v;
}

/* Duration of limits in ms */
#define VAD_SIG_LECT     60
#define VAD_SIG_CONF     60
#define VAD_PRE_LECT     60
#define VAD_PRE_CONF     20
#define VAD_POST_LECT   160
#define VAD_POST_CONF   160

void
vad_config(vad_t *v, u_int16 blockdur, u_int16 freq)
{
        u_int32 time_ms;

        assert(blockdur != 0);
        assert(freq     != 0);

        time_ms = (blockdur * 1000) / freq;

        v->limit[VAD_MODE_LECT].sig  = (u_char)(VAD_SIG_LECT  / time_ms); 
        v->limit[VAD_MODE_LECT].pre  = (u_char)(VAD_PRE_LECT  / time_ms);
        v->limit[VAD_MODE_LECT].post = (u_char)(VAD_POST_LECT / time_ms);

        v->limit[VAD_MODE_CONF].sig  = (u_char)(VAD_SIG_CONF  / time_ms); 
        v->limit[VAD_MODE_CONF].pre  = (u_char)(VAD_PRE_CONF  / time_ms);
        v->limit[VAD_MODE_CONF].post = (u_char)(VAD_POST_CONF / time_ms);
}

void
vad_destroy(vad_t *v)
{
        assert (v != NULL);
        xfree(v);
}

#define VAD_SILENT        0
#define VAD_SPURT         1

u_int16
vad_to_get(vad_t *v, u_char silence, u_char mode)
{
        vad_limit_t *l = &v->limit[mode];

        assert(mode == VAD_MODE_LECT || mode == VAD_MODE_CONF);

        v->tick++;

        switch (v->state) {
        case VAD_SILENT:
                if (silence == FALSE) {
                        v->sig_cnt++;
                        if (v->sig_cnt == l->sig) {
                                v->state = VAD_SPURT;
                                v->spurt_cnt++;
                                v->post_cnt = 0;
                                v->sig_cnt  = 0;
                                return l->pre;
                        }
                } else {
                        v->sig_cnt = 0;
                }
                return 0;
                break;
        case VAD_SPURT:
                if (silence == FALSE) {
                        v->post_cnt = 0;
                        return 1;
                } else {
                        if (++v->post_cnt < l->post) {
                                return 1;
                        } else {
                                v->sig_cnt  = 0;
                                v->post_cnt = 0;
                                v->state = VAD_SILENT;
                                return 0;
                        }
                }
                break;
        }
        return 0; /* never arrives here */
}

u_int16
vad_max_could_get(vad_t *v)
{
        if (v->state == VAD_SILENT) {
                return v->limit[VAD_MODE_LECT].pre;
        } else {
                return 1;
        }
}

void
vad_reset(vad_t* v)
{
        v->state    = VAD_SILENT;
        v->sig_cnt  = 0;
        v->post_cnt = 0;
}

__inline u_char
vad_in_talkspurt(vad_t *v)
{
        return (v->state == VAD_SPURT) ? TRUE : FALSE;
}

__inline u_int32
vad_talkspurt_no(vad_t *v)
{
        return v->spurt_cnt;
}

void
vad_dump(vad_t *v)
{
        dprintf("vad tick %05d state %d sig %d post %d\n",
                v->tick,
                v->state,
                v->sig_cnt,
                v->post_cnt
                );
}

#define AGC_HISTORY_LEN  5
#define AGC_PEAK_LOWER    5000
#define AGC_PEAK_UPPER   14000

typedef struct s_agc {
        u_int16 peak;
        u_int16 cnt;
        u_int32 spurtno;
        u_char  new_gain;
        u_char  change;
        session_struct *sp; /* this is unpleasant to have and i wrote it! */
} agc_t;

agc_t *
agc_create(session_struct *sp)
{
        agc_t *a = (agc_t*)xmalloc(sizeof(agc_t));
        memset(a,0,sizeof(agc_t));
        a->spurtno = 0xff;
        a->sp      = sp;
        return a;
}

void
agc_destroy(agc_t *a)
{
        xfree(a);
}

void 
agc_reset(agc_t *a)
{
        a->peak    = 0;
        a->cnt     = 0;
        a->new_gain = 0;
        a->change  = FALSE;
}

/* This limit stops agc oscillating around close values, which cause 
 * silence suppression recallibration to occur too often [oth].
 */

#define AGC_GAIN_SIG 5

static void 
agc_consider(agc_t *a)
{
        int32 gain;

        a->change = FALSE;
        if (a->peak > AGC_PEAK_UPPER) {
                gain        = audio_get_gain(a->sp->audio_fd);
                a->new_gain = min(gain * AGC_PEAK_UPPER / a->peak, 99);
                if ((gain - a->new_gain) > AGC_GAIN_SIG) {
                        a->change   = TRUE;
                }
        } else if (a->peak < AGC_PEAK_LOWER) {
                gain        = audio_get_gain(a->sp->audio_fd);
                a->new_gain = min(gain * AGC_PEAK_LOWER / a->peak, 99);
                if ((a->new_gain - gain) > AGC_GAIN_SIG) {
                        a->change   = TRUE;
                }
        }
}

void
agc_update(agc_t *a, u_int16 energy, u_int32 spurtno)
{
        a->peak = max(a->peak, energy);
        if (a->spurtno != spurtno) {
                a->spurtno = spurtno;
                a->cnt++;
                if (a->cnt == AGC_HISTORY_LEN) {
                        agc_consider(a);
                        a->cnt = 0;
                        return;
                }
        }
}

u_char 
agc_apply_changes(agc_t *a)
{
        if (a->change == TRUE) {
                audio_set_gain(a->sp->audio_fd, a->new_gain);
                a->sp->input_gain = a->new_gain;
                tx_igain_update(a->sp);
                agc_reset(a);
                a->change = FALSE;
                return TRUE;
        }
        return FALSE;
}


