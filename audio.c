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
#include "assert.h"
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
        int  m;
        m = 0;
        while (len-- > 0) {
                m += *buf;
                *buf -= bc->lta;
                buf += step;
        }
        bc->lta -= (bc->lta - m / len) >> 3;
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
                        remove_lta(bc  , buf  , len, 2);
                        remove_lta(bc+1, buf+1, len, 2);
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
	assert(sp->have_device);
	if (sp->mode == TRANSCODER) {
		return transcoder_read(sp->audio_fd, buf, samples);
	} else {
		return audio_read(sp->audio_fd, buf, samples);
	}
}

int
audio_device_write(session_struct *sp, sample *buf, int dur)
{
        codec_t *cp = get_codec_by_pt(sp->encodings[0]);

	if (sp->have_device) {
                u_int16 channels = audio_get_channels(sp->audio_fd);
                if (sp->out_file) {
                        snd_write_audio(&sp->out_file, buf, (u_int16)(dur * channels));
                }
		if (sp->mode == TRANSCODER) {
			return transcoder_write(sp->audio_fd, buf, dur*cp->channels);
		} else {
			return audio_write(sp->audio_fd, buf, dur * channels);
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

        if (sp->have_device) {
                return (TRUE);
        }

	cp = get_codec_by_pt(sp->encodings[0]);
	format.encoding        = DEV_L16;
	format.sample_rate     = cp->freq;
        format.bits_per_sample = 16;
	format.num_channels    = cp->channels;
        format.blocksize       = cp->unit_len * cp->channels;

	if (sp->mode == TRANSCODER) {
		if ((sp->audio_fd = transcoder_open()) == 0) {
                        return FALSE;
                }
		sp->have_device = TRUE;
	} else {
		/* XXX should pass a pointer to format ???!!! */
		if ((sp->audio_fd = audio_open(&format)) == 0) {
			return FALSE;
		}

		if (audio_duplex(sp->audio_fd) == FALSE) {
			printf("RAT v3.2.0 and later require a full duplex audio device, but \n");
			printf("your audio device only supports half-duplex operation. Sorry.\n");
			return FALSE;
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
	}
        
        if (audio_zero_buf == NULL) {
                audio_zero_buf = (sample*) xmalloc (format.blocksize * sizeof(sample));
                audio_zero(audio_zero_buf, format.blocksize, DEV_L16);
        }

        if ((sp->audio_fd != 0) && (sp->mode != TRANSCODER)) {
                audio_non_block(sp->audio_fd);
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
        return sp->have_device;
}

void
audio_device_give(session_struct *sp)
{
	gettimeofday(&sp->device_time, NULL);

	if (sp->have_device) {
                tx_stop(sp);
		if (sp->mode == TRANSCODER) {
			transcoder_close(sp->audio_fd);
		} else {
		        sp->input_mode = audio_get_iport(sp->audio_fd);
		        sp->output_mode = audio_get_oport(sp->audio_fd);
			audio_close(sp->audio_fd);
		}
		sp->audio_fd = 0;
		sp->have_device = FALSE;
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
        sp->encodings[0] = sp->next_encoding;
        if (audio_device_take(sp) == FALSE) {
                /* we failed, fallback */
                sp->encodings[0] = oldpt;
                audio_device_take(sp);
        }
        channel_set_coder(sp, sp->encodings[0]);
        sp->next_encoding = -1;
        ui_update(sp);
}


/* This function needs to be modified to return some indication of how well
 * or not we are doing.                                                    
 */
int
read_write_audio(session_struct *spi, session_struct *spo,  struct s_mix_info *ms)
{
        u_int32 cushion_size, read_dur;
        struct s_cushion_struct *c;
	int	trailing_silence, new_cushion, cushion_step, diff, channels;
	sample	*bufp;

        c = spi->cushion;
	/*
	 * The loop_delay stuff is meant to help in determining the pause
	 * length in the select in net.c for platforms where we cannot
	 * block on read availability of the audio device.
	 */
	if ((read_dur = tx_read_audio(spi)) <= 0) {
		if (spi->last_zero == FALSE) {
			spi->loop_estimate += 500;
		}
		spi->last_zero  = TRUE;
		spi->loop_delay = 1000;
		return 0;
	} else {
		if (read_dur > 160 && spi->loop_estimate > 5000) {
			spi->loop_estimate -= 250;
		}
		spi->last_zero  = FALSE;
		spi->loop_delay = spi->loop_estimate;
		assert(spi->loop_delay >= 0);
                if (spi->have_device == FALSE) {
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
        channels = audio_get_channels(spi->audio_fd);

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
                trailing_silence = mix_get_audio(ms, read_dur * channels, &bufp);
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

#ifdef BAR

#if  !defined(WIN32) && !defined(OTI_AUDIO)
int 
audio_is_ready(int audio_fd)
{
        struct timeval tv;
        fd_set afds;

        memset(&tv, 0, sizeof(struct timeval));
        FD_ZERO(&afds);
        FD_SET(audio_fd,&afds);
        select(audio_fd+1, &afds, NULL, NULL, &tv);
        return FD_ISSET(audio_fd, &afds);
}

#endif

void
audio_wait_for(session_struct *sp)
{
#ifdef WIN32
        DWORD   dwPeriod;
        codec_t *cp;
        
        cp = get_codec_by_pt(sp->encodings[0]);
        dwPeriod = cp->unit_len / 2  * 1000 / get_freq(sp->device_clock);
        /* The blocks we are passing to the audio interface are of duration dwPeriod.
         * dwPeriod is usually around 20ms (8kHz), but mmtask often doesn't give
         * us audio that often, more like every 40ms.
         */
        while (!audio_is_ready(sp->audio_fd)) {
                Sleep(dwPeriod);
        }

#elif defined(OTI_AUDIO)

        u_int32   period;
        codec_t *cp;
        
        cp = get_codec_by_pt(sp->encodings[0]);
        period = cp->unit_len / 2 * 1000 / get_freq(sp->device_clock);
        poll(NULL, 0, period);

#else /* UN*X with select on audio file descriptor */
        fd_set rfds;
        codec_t *cp;
        struct timeval tv;

        cp         = get_codec_by_pt(sp->encodings[0]);
        tv.tv_sec  = 0;
        tv.tv_usec = cp->unit_len / 2 * 1000 / get_freq(sp->device_clock);

        FD_ZERO(&rfds);
        FD_SET(sp->audio_fd,&rfds);
        select(sp->audio_fd+1, &rfds, NULL, NULL, &tv);
#endif
        return;
}

#endif /* BAR */

#define AUDIO_INTERFACE_NAME_LEN 32

typedef struct {
        char name[AUDIO_INTERFACE_NAME_LEN];

        int  (*audio_if_init)(void);               /* Test and initialize audio interface (OPTIONAL) */
        int  (*audio_if_free)(void);               /* Free audio interface (OPTIONAL)                */

        int  (*audio_if_open)(int, audio_format *f);       /* Open device with format (REQUIRED) */
        void (*audio_if_close)(int);               /* Close device (REQUIRED) */
        void (*audio_if_drain)(int);               /* Drain device (REQUIRED) */
        int  (*audio_if_duplex)(int);              /* Device full duplex (REQUIRED) */

        int  (*audio_if_read) (int, sample*, int); /* Read samples (REQUIRED)  */
        int  (*audio_if_write)(int, sample*, int); /* Write samples (REQUIRED) */
        void (*audio_if_non_block)(int);           /* Set device non-blocking (REQUIRED) */
        void (*audio_if_block)(int);               /* Set device blocking (REQUIRED)     */

        void (*audio_if_set_gain)(int,int);        /* Set input gain (REQUIRED)  */
        int  (*audio_if_get_gain)(int);            /* Get input gain (REQUIRED)  */
        void (*audio_if_set_volume)(int,int);      /* Set output gain (REQUIRED) */
        int  (*audio_if_get_volume)(int);          /* Get output gain (REQUIRED) */
        void (*audio_if_loopback)(int, int);       /* Enable hardware loopback (OPTIONAL) */

        void (*audio_if_set_oport)(int, int);      /* Set output port (REQUIRED)        */
        int  (*audio_if_get_oport)(int);           /* Get output port (REQUIRED)        */
        int  (*audio_if_next_oport)(int);          /* Go to next output port (REQUIRED) */
        void (*audio_if_set_iport)(int, int);      /* Set input port (REQUIRED)         */
        int  (*audio_if_get_iport)(int);           /* Get input port (REQUIRED)         */
        int  (*audio_if_next_iport)(int);          /* Go to next itput port (REQUIRED)  */

        int  (*audio_if_get_blocksize)(int);      /* Get audio device block size (REQUIRED) */ 
        int  (*audio_if_get_channels)(int);       /* Get audio device channels   (REQUIRED) */
        int  (*audio_if_get_freq)(int);           /* Get audio device frequency  (REQUIRED) */

        int  (*audio_if_is_ready)(int);            /* Poll for audio availability (REQUIRED)   */
        void (*audio_if_wait_for)(int, int);       /* Wait until audio is available (REQUIRED) */
} audio_if_t;

#define AUDIO_MAX_INTERFACES 5
/* These store available audio interfaces */
static audio_if_t audio_interfaces[AUDIO_MAX_INTERFACES];
static int num_interfaces = 0;

/* Active interfaces is a table of entries pointing to entries in
 * audio interfaces table.  Audio open returns index to these */
static audio_if_t *active_interfaces[AUDIO_MAX_INTERFACES];
static int num_active_interfaces = 0; 

/* This is the index of the next audio interface that audio_open */
static int selected_interface = 0;

/* We map indexes outside range for file descriptors so people don't attempt
 * to circumvent audio interface.  If something is missing it should be added
 * to the interfaces...
 */

#define AIF_IDX_TO_MAGIC(x) ((x) | 0xff00)
#define AIF_MAGIC_TO_IDX(x) ((x) & 0x00ff)

__inline static audio_if_t *
audio_get_active_interface(int idx)
{
        assert(idx < num_interfaces);
        assert(active_interfaces[idx] != NULL);

        return active_interfaces[idx];
}

int
audio_get_number_of_interfaces()
{
        return num_interfaces;
}

char*
audio_get_interface_name(int idx)
{
        if (idx < num_interfaces) {
                return audio_interfaces[idx].name;
        }
        return NULL;
}

void
audio_set_interface(int idx)
{
        if (idx < num_interfaces) {
                selected_interface = idx;
        }
}

int 
audio_get_interface()
{
        return selected_interface;
}

int 
audio_open(audio_format *format)
{
        audio_if_t *aif;
        int r; 

        aif = &audio_interfaces[selected_interface];
        assert(aif->audio_if_open);

        if (aif->audio_if_open(num_active_interfaces, format)) {
                /* Add selected interface to those active*/
                active_interfaces[num_active_interfaces] = &audio_interfaces[selected_interface];
                r = AIF_IDX_TO_MAGIC(num_active_interfaces);
                num_active_interfaces++;
                return r;
        }

        return 0;
}


void
audio_close(audio_desc_t ad)
{
        int i, j;
        audio_if_t *aif;

        ad = AIF_MAGIC_TO_IDX(ad);

        aif = audio_get_active_interface(ad);
        aif->audio_if_close(ad);

        i = j = 0;
        for(i = 0; i < num_active_interfaces; i++) {
                if (i != ad) {
                        active_interfaces[j] = active_interfaces[i];
                        j++;
                }
        }

        num_active_interfaces--;
}

void
audio_drain(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        aif->audio_if_drain(ad);
}

int
audio_duplex(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_duplex(ad);
}

int
audio_read(audio_desc_t ad, sample *buf, int len)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_read(ad, buf, len);
}

int
audio_write(audio_desc_t ad, sample *buf, int len)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_write(ad, buf, len);
}

void
audio_non_block(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_non_block(ad);
}

void
audio_block(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);
        
        aif->audio_if_block(ad);
}

void
audio_set_gain(audio_desc_t ad, int gain)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_gain(ad, gain);
}

int
audio_get_gain(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_get_gain(ad);
}

void
audio_set_volume(audio_desc_t ad, int volume)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_volume(ad, volume);
}

int
audio_get_volume(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return aif->audio_if_get_volume(ad);
}

void
audio_loopback(audio_desc_t ad, int gain)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        if (aif->audio_if_loopback) aif->audio_if_loopback(ad, gain);
}

void
audio_set_oport(audio_desc_t ad, int port)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_oport(ad, port);
}

int
audio_get_oport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_oport(ad));
}

int     
audio_next_oport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_next_oport(ad));
}

void
audio_set_iport(audio_desc_t ad, int port)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_set_oport(ad, port);
}

int
audio_get_iport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_iport(ad));
}

int
audio_next_iport(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_next_iport(ad));
}

int
audio_get_blocksize(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_blocksize(ad));
}

int
audio_get_channels(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_channels(ad));
}

int
audio_get_freq(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_get_freq(ad));
}

int
audio_is_ready(audio_desc_t ad)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        return (aif->audio_if_is_ready(ad));
}

void
audio_wait_for(audio_desc_t ad, int delay_ms)
{
        audio_if_t *aif;
        
        ad = AIF_MAGIC_TO_IDX(ad);
        aif = audio_get_active_interface(ad);

        aif->audio_if_wait_for(ad, delay_ms);
}

/* Code for adding/initialising/removing audio interfaces */

static int
audio_add_interface(audio_if_t *aif_new)
{
        if ((aif_new->audio_if_init == NULL || aif_new->audio_if_init()) &&
            num_interfaces < AUDIO_MAX_INTERFACES) {
                memcpy(audio_interfaces + num_interfaces, aif_new, sizeof(audio_if_t));
                debug_msg("Audio interface added %s\n", aif_new->name);
                num_interfaces++;
                return TRUE;
        }
        return FALSE;
}

#include "auddev_sparc.h"
#include "auddev_oss.h"
#include "auddev_win32.h"

#ifdef HAVE_PCA
#include "auddev_pca.h"
#endif /* HAVE_PCA */

#ifdef HAVE_OSPREY
#include "auddev_oti.h"
#endif /* HAVE_OSPREY */

int
audio_init_interfaces()
{
        int i, n;
#ifdef Solaris
        {
                audio_if_t aif_sparc = {
                        "Sun Audio Device",
                        NULL, 
                        NULL, 
                        sparc_audio_open,
                        sparc_audio_close,
                        sparc_audio_drain,
                        sparc_audio_duplex,
                        sparc_audio_read,
                        sparc_audio_write,
                        sparc_audio_non_block,
                        sparc_audio_block,
                        sparc_audio_set_gain,
                        sparc_audio_get_gain,
                        sparc_audio_set_volume,
                        sparc_audio_get_volume,
                        sparc_audio_loopback,
                        sparc_audio_set_oport,
                        sparc_audio_get_oport,
                        sparc_audio_next_oport,
                        sparc_audio_set_iport,
                        sparc_audio_get_iport,
                        sparc_audio_next_iport,
                        sparc_audio_get_blocksize,
                        sparc_audio_get_channels,
                        sparc_audio_get_freq,
                        sparc_audio_is_ready,
                        sparc_audio_wait_for,
                };
                audio_add_interface(&aif_sparc);
        }
#endif Solaris

#ifdef HAVE_OSPREY
        {
                audio_if_t aif_oti = {
                        "Osprey Audio Device",
                        NULL, 
                        NULL, 
                        oti_audio_open,
                        oti_audio_close,
                        oti_audio_drain,
                        oti_audio_duplex,
                        oti_audio_read,
                        oti_audio_write,
                        oti_audio_non_block,
                        oti_audio_block,
                        oti_audio_set_gain,
                        oti_audio_get_gain,
                        oti_audio_set_volume,
                        oti_audio_get_volume,
                        oti_audio_loopback,
                        oti_audio_set_oport,
                        oti_audio_get_oport,
                        oti_audio_next_oport,
                        oti_audio_set_iport,
                        oti_audio_get_iport,
                        oti_audio_next_iport,
                        oti_audio_get_blocksize,
                        oti_audio_get_channels,
                        oti_audio_get_freq,
                        oti_audio_is_ready,
                        oti_audio_wait_for,
                };
                audio_add_interface(&aif_oti);
        }
#endif /* HAVE_OSPREY */

#if defined(Linux)||defined(OSS)
        oss_audio_query_devices();
        n = oss_get_device_count();

        for(i = 0; i < n; i++) {
                audio_if_t aif_oss = {
                        "OSS Audio Device",
                        NULL, 
                        NULL, 
                        oss_audio_open,
                        oss_audio_close,
                        oss_audio_drain,
                        oss_audio_duplex,
                        oss_audio_read,
                        oss_audio_write,
                        oss_audio_non_block,
                        oss_audio_block,
                        oss_audio_set_gain,
                        oss_audio_get_gain,
                        oss_audio_set_volume,
                        oss_audio_get_volume,
                        oss_audio_loopback,
                        oss_audio_set_oport,
                        oss_audio_get_oport,
                        oss_audio_next_oport,
                        oss_audio_set_iport,
                        oss_audio_get_iport,
                        oss_audio_next_iport,
                        oss_audio_get_blocksize,
                        oss_audio_get_channels,
                        oss_audio_get_freq,
                        oss_audio_is_ready,
                        oss_audio_wait_for,
                };
                strcpy(aif_oss.name, oss_get_device_name(i));
                audio_add_interface(&aif_oss);
        }

#endif /* Linux / OSS */

#if defined(WIN32)
        w32sdk_audio_query_devices();
        n = w32sdk_get_device_count();
        for(i = 0; i < n; i++) {
                audio_if_t aif_w32sdk = {
                        "W32SDK Audio Device",
                        NULL, 
                        NULL, 
                        w32sdk_audio_open,
                        w32sdk_audio_close,
                        w32sdk_audio_drain,
                        w32sdk_audio_duplex,
                        w32sdk_audio_read,
                        w32sdk_audio_write,
                        w32sdk_audio_non_block,
                        w32sdk_audio_block,
                        w32sdk_audio_set_gain,
                        w32sdk_audio_get_gain,
                        w32sdk_audio_set_volume,
                        w32sdk_audio_get_volume,
                        w32sdk_audio_loopback,
                        w32sdk_audio_set_oport,
                        w32sdk_audio_get_oport,
                        w32sdk_audio_next_oport,
                        w32sdk_audio_set_iport,
                        w32sdk_audio_get_iport,
                        w32sdk_audio_next_iport,
                        w32sdk_audio_get_blocksize,
                        w32sdk_audio_get_channels,
                        w32sdk_audio_get_freq,
                        w32sdk_audio_is_ready,
                        w32sdk_audio_wait_for,
                };
                strcpy(aif_w32sdk.name, w32sdk_get_device_name(i));
                audio_add_interface(&aif_w32sdk);
        }
#endif /* WIN32 */

#if defined(FreeBSD)
        {
                audio_if_t aif_luigi = {
                        "Default Audio Device",
                        NULL,
                        NULL, 
                        luigi_audio_open,
                        luigi_audio_close,
                        luigi_audio_drain,
                        luigi_audio_duplex,
                        luigi_audio_read,
                        luigi_audio_write,
                        luigi_audio_non_block,
                        luigi_audio_block,
                        luigi_audio_set_gain,
                        luigi_audio_get_gain,
                        luigi_audio_set_volume,
                        luigi_audio_get_volume,
                        luigi_audio_loopback,
                        luigi_audio_set_oport,
                        luigi_audio_get_oport,
                        luigi_audio_next_oport,
                        luigi_audio_set_iport,
                        luigi_audio_get_iport,
                        luigi_audio_next_iport,
                        luigi_audio_get_blocksize,
                        luigi_audio_get_channels,
                        luigi_audio_get_freq,
                        luigi_audio_is_ready,
                        luigi_audio_wait_for,
                };
        }
#endif /* FreeBSD */

#if defined(HAVE_PCA)
        {
                audio_if_t aif_pca = {
                        "PCA Audio Device",
                        pca_audio_init,
                        NULL, 
                        pca_audio_open,
                        pca_audio_close,
                        pca_audio_drain,
                        pca_audio_duplex,
                        pca_audio_read,
                        pca_audio_write,
                        pca_audio_non_block,
                        pca_audio_block,
                        pca_audio_set_gain,
                        pca_audio_get_gain,
                        pca_audio_set_volume,
                        pca_audio_get_volume,
                        pca_audio_loopback,
                        pca_audio_set_oport,
                        pca_audio_get_oport,
                        pca_audio_next_oport,
                        pca_audio_set_iport,
                        pca_audio_get_iport,
                        pca_audio_next_iport,
                        pca_audio_get_blocksize,
                        pca_audio_get_channels,
                        pca_audio_get_freq,
                        pca_audio_is_ready,
                        pca_audio_wait_for,
                };
        }
#endif /* HAVE_PCA */

        UNUSED(i); /* Some if def combinations may mean that these do not get used */
        UNUSED(n);

        return 0;
}

int
audio_free_interfaces(void)
{
        return TRUE;
}
