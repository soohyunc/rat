/*
 * FILE:    transmit.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Isidor Kouvelas
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
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "codec_state.h"
#include "playout.h"
#include "channel_types.h"
#include "channel.h"
#include "session.h"
#include "audio.h"
#include "audio_util.h"
#include "sndfile.h"
#include "converter.h"
#include "parameters.h"
#include "pdb.h"
#include "ui_send_rtp.h"
#include "ui_send_audio.h"
#include "ui_send_prefs.h"
#include "rtp.h"
#include "transmit.h"
#include "util.h"

/* All this code can be greatly simplified and reduced by making
 * better use of the playout buffer structure in playout.h.
 */

typedef struct s_tx_unit {
        sample   *data;             /* pointer to raw data in read_buf */
        uint32_t  dur_used;         /* number of time intervals filled */
        uint16_t  energy;
        u_char    silence;          /* First pass */
        u_char    send;             /* Silence second pass */
} tx_unit;

typedef struct s_tx_buffer {
        struct s_session     *sp;
        struct s_vad         *vad;
        struct s_agc         *agc;
        struct s_bias_ctl    *bc;
        struct s_pb          *media_buffer; 
        struct s_pb          *channel_buffer; 
        struct s_pb          *audio_buffer; /* Audio buffer and it's iterators */
        struct s_pb_iterator *reading;      /* Current read point iterator     */
        struct s_pb_iterator *silence;      /* Silence classification iterator */
        struct s_pb_iterator *transmit;     /* Transmission point iterator     */
        struct s_codec_state_store *state_store;    /* Encoder states        */
        uint32_t              sending_audio:1;
	uint16_t              sample_rate;
        uint16_t              channels;
        uint16_t              unit_dur; /* dur. in sampling intervals (excludes channels) */

        /* Statistics log */
        double          mean_read_dur;
        /* These are a hack because we use playout buffer
         * which expects time units of type ts_t so we need
         * to be able to map to and from 32 bit no for
         * packet timestamp */
        ts_sequencer    down_seq;  /* used for 32 -> ts_t */
        ts_sequencer    up_seq;    /* used for ts_t -> 32 */

        /* place for the samples */
        sample samples[DEVICE_REC_BUF];
        int    last_sample; /* Stores the index of the last read buffer */

        /* bandwidth estimate parameters */
        int    bps_bytes_sent;
        ts_t   bps_last_update;
} tx_buffer;

static sample dummy_buf[DEVICE_REC_BUF];
static void tx_read_sndfile(session_t *sp, uint16_t tx_freq, uint16_t tx_channels, tx_unit *u);

static int
tx_unit_create(tx_buffer *tb, tx_unit  **ptu, int n_samples)
{
        tx_unit *tu;
        tu = block_alloc(sizeof(tx_unit));
        if (tu) {
                memset(tu, 0, sizeof(tx_unit));
                *ptu = tu;
                /* Position sample pointer */
                if (tb->last_sample + n_samples > DEVICE_REC_BUF) {
                        tb->last_sample = 0;
                }
                tu->data = tb->samples + tb->last_sample;
                tu->energy = 555;
                tb->last_sample += n_samples;
                return TRUE;
        }
        debug_msg("Failed to allocate tx_unit\n");
        return FALSE;
}

static void
tx_unit_destroy(tx_unit **ptu, uint32_t len)
{
        tx_unit *tu = *ptu;
        assert(tu != NULL);
        assert(len == sizeof(tx_unit));

        block_free(tu, sizeof(tx_unit));
        *ptu = NULL;
}

int
tx_create(tx_buffer **ntb, 
          session_t  *sp,
	  uint16_t    sample_rate,     
          uint16_t    channels,
          uint16_t    unit_dur)
{
        tx_buffer *tb;

        tb = (tx_buffer*)xmalloc(sizeof(tx_buffer));
        if (tb) {
                memset(tb, 0, sizeof(tx_buffer));
                debug_msg("Unit duration %d channels %d\n", unit_dur, channels);
                tb->sp          = sp;
		tb->sample_rate = sample_rate;
                tb->channels    = channels;
                tb->unit_dur    = unit_dur;
		tb->mean_read_dur = unit_dur;
                tb->bc          = bias_ctl_create(channels, sample_rate);
                tb->vad         = vad_create(unit_dur, sample_rate);
                tb->agc         = agc_create(sp);
                sp->auto_sd     = sd_init(unit_dur, sample_rate);
                sp->manual_sd   = manual_sd_init(unit_dur, sample_rate, sp->manual_sd_thresh);
                
                pb_create(&tb->audio_buffer, (playoutfreeproc)tx_unit_destroy);
                pb_create(&tb->media_buffer, (playoutfreeproc)media_data_destroy);
                pb_create(&tb->channel_buffer, (playoutfreeproc)channel_data_destroy);

                *ntb = tb;
                return TRUE;
        }
        return FALSE;
}

void
tx_destroy(tx_buffer **ptb)
{
        tx_buffer *tb;

        assert(ptb != NULL);
        tb = *ptb;
        assert(tb != NULL);

        bias_ctl_destroy(tb->bc);
        sd_destroy(tb->sp->auto_sd);
        manual_sd_destroy(tb->sp->manual_sd);
        vad_destroy(tb->vad);
        agc_destroy(tb->agc);

        pb_destroy(&tb->audio_buffer);
        pb_destroy(&tb->media_buffer);
        pb_destroy(&tb->channel_buffer);

        xfree(tb);
        *ptb = NULL;
}

/* These routines are called when the button on the interface is toggled */
void
tx_start(tx_buffer *tb)
{
        tx_unit *tu_new;
        ts_t     unit_start;

        /* Not sure why this is here (?) */
        if (tb->sending_audio) {
                debug_msg("Why? Fix");
                abort();
                return;
        }

        tb->sending_audio = TRUE;

        /* Turn off auto lecture */
        tb->sp->auto_lecture = 1;       

        /* Reset signal classification and auto-scaling */
        sd_reset(tb->sp->auto_sd);
        vad_reset(tb->vad);
        agc_reset(tb->agc);

        /* Attach iterator for silence classification */
        pb_iterator_create(tb->audio_buffer, &tb->transmit);
        pb_iterator_create(tb->audio_buffer, &tb->silence);
        pb_iterator_create(tb->audio_buffer, &tb->reading);
        assert(pb_iterator_count(tb->audio_buffer) == 3);

        /* Add one unit to media buffer to kick off audio reading */
        unit_start = tb->sp->cur_ts;
        tx_unit_create(tb, &tu_new, tb->unit_dur * tb->channels);
        assert(ts_valid(unit_start));
        pb_add(tb->audio_buffer, 
               (u_char*)tu_new,
               sizeof(tx_unit),
               unit_start);

        /* And then put reading iterator on it */
        pb_iterator_advance(tb->reading);

        assert(tb->state_store == NULL);
        codec_state_store_create(&tb->state_store, ENCODER);
        tx_update_ui(tb);
/*
        if (tb->sp->silence_detection != SILENCE_DETECTION_OFF) {
                ui_send_rtp_active(tb->sp, tb->sp->mbus_ui_addr, rtp_my_ssrc(tb->sp->rtp_session[0]));
        }
        */
        tb->bps_last_update = tb->sp->cur_ts;
}

void
tx_stop(tx_buffer *tb)
{
        struct timeval tv;

        if (tb->sending_audio == FALSE) {
                return;
        }

        gettimeofday(&tv, NULL);
        tb->sp->auto_lecture  = tv.tv_sec;
        codec_state_store_destroy(&tb->state_store);
        channel_encoder_reset(tb->sp->channel_coder);
        ui_send_audio_input_powermeter(tb->sp, tb->sp->mbus_ui_addr, 0);
        tb->sending_audio = FALSE;
        tx_update_ui(tb);
        /* Detach iterators      */
        assert(pb_iterator_count(tb->audio_buffer) == 3);
        pb_iterator_destroy(tb->audio_buffer, &tb->transmit);
        pb_iterator_destroy(tb->audio_buffer, &tb->silence);
        pb_iterator_destroy(tb->audio_buffer, &tb->reading);
        assert(pb_iterator_count(tb->audio_buffer) == 0);

        /* Drain playout buffers */
        pb_flush(tb->audio_buffer);
        pb_flush(tb->media_buffer);
        pb_flush(tb->channel_buffer);

        tb->bps_bytes_sent = 0;
}

int
tx_read_audio(tx_buffer *tb)
{
        session_t *sp;
        tx_unit   *u;
	ts_t       u_ts;
        uint32_t   read_dur = 0, this_read, ulen;

        assert(tb->channels > 0 && tb->channels <= 2);

        sp = tb->sp;
        if (tb->sending_audio) {
                int filled_unit;
                assert(pb_iterator_count(tb->audio_buffer) == 3);
                do {
                        if (pb_iterator_get_at(tb->reading, (u_char**)&u, &ulen, &u_ts) == FALSE) {
                                debug_msg("Reading iterator failed to get unit!\n");
                        }
                        assert(u != NULL);

                        this_read = audio_read(sp->audio_device, 
                                               u->data + u->dur_used * tb->channels,
                                               (tb->unit_dur - u->dur_used) * tb->channels) / tb->channels;
                        assert(this_read <= tb->unit_dur - u->dur_used);

                        filled_unit = FALSE;
                        u->dur_used += this_read;
                        if (u->dur_used == tb->unit_dur) {
                                read_dur += tb->unit_dur;
                                if (sp->in_file) {
                                        tx_read_sndfile(sp, tb->sample_rate, tb->channels, u);
                                }
				sp->cur_ts = ts_add(sp->cur_ts, ts_map32(tb->sample_rate, tb->unit_dur));
                                u_ts       = sp->cur_ts;
				tx_unit_create(tb, &u, tb->unit_dur * tb->channels);
                                pb_add(tb->audio_buffer, (u_char*)u, ulen, u_ts);
                                pb_iterator_advance(tb->reading);
                                filled_unit = TRUE;
                        } 
                } while (filled_unit == TRUE);
                assert(pb_iterator_count(tb->audio_buffer) == 3);
        } else {
                int this_read = 0;
                /* We're not sending, but have access to the audio device. 
                 * Read the audio anyway to get exact values, and then 
                 * throw the data we've just read away...    
                 */
                do {
                        this_read = audio_read(sp->audio_device, dummy_buf, DEVICE_REC_BUF / 4) / sp->tb->channels;
                        read_dur += this_read;
                } while (this_read > 0);
		sp->cur_ts = ts_add(sp->cur_ts, ts_map32(tb->sample_rate, read_dur));
        }

        if (read_dur >= (uint32_t)(DEVICE_REC_BUF / (4 * tb->channels))) {
                debug_msg("Read a lot of audio %d\n", read_dur);
                if (tb->sending_audio) {
                        debug_msg("Resetting transmitter\n");
                        tx_stop(tb);
                        tx_start(tb);
                }
        }
        
        if (read_dur) {
                sp->tb->mean_read_dur += ((double)read_dur - sp->tb->mean_read_dur) / 8.0;
        }

        assert(read_dur < 0x7fffffff);
        
        return read_dur;
}

int
tx_process_audio(tx_buffer *tb)
{
        session_t       *sp;
        struct s_pb_iterator *marker;
        tx_unit              *u;
        uint32_t               u_len;
        ts_t                  u_ts;
        int                   to_send;
        
        assert(tb->sending_audio);
        
        sp = tb->sp;
	session_validate(sp);

        /* Do signal classification up until read point, that
         * is not a complete audio frame so cannot be done 
         */
        assert(pb_iterator_count(tb->audio_buffer) == 3);
        pb_iterator_get_at(tb->silence, (u_char**)&u, &u_len, &u_ts);
        while (pb_iterators_equal(tb->silence, tb->reading) == FALSE) {
                bias_remove(tb->bc, u->data, u->dur_used * tb->channels);
                u->energy = audio_avg_energy(u->data, u->dur_used * tb->channels, tb->channels);
                u->send   = FALSE;
                
                /* Silence classification on this block */
                switch(tb->sp->silence_detection) {
                case SILENCE_DETECTION_AUTO:
                        u->silence = sd(tb->sp->auto_sd, (uint16_t)u->energy);
                        break;
                case SILENCE_DETECTION_MANUAL:
                        u->silence = manual_sd(tb->sp->manual_sd, 
                                               (uint16_t)u->energy, 
                                               audio_abs_max(u->data, u->dur_used * tb->channels));
                        break;
                }
                                               
                /* Pass decision to voice activity detector (damps transients, etc) */
                to_send = vad_to_get(tb->vad, 
                                     (u_char)u->silence, 
                                     (u_char)((sp->lecture) ? VAD_MODE_LECT : VAD_MODE_CONF));           
                agc_update(tb->agc, (uint16_t)u->energy, vad_talkspurt_no(tb->vad));
                
                if (sp->silence_detection != SILENCE_DETECTION_OFF) {
                        if (to_send != 0) {
                                pb_iterator_dup(&marker, tb->silence);
                                while(u != NULL && to_send != 0) {
                                        u->send = TRUE;
                                        to_send --;
                                        pb_iterator_retreat(marker);
                                        pb_iterator_get_at(marker, (u_char**)&u, &u_len, &u_ts);
                                }
                                pb_iterator_destroy(tb->audio_buffer, &marker);
                        }
                        assert(pb_iterator_count(tb->audio_buffer) == 3);
                } else {
                        u->silence = FALSE;
                        u->send    = TRUE;
                }
                pb_iterator_advance(tb->silence);
                pb_iterator_get_at(tb->silence, (u_char**)&u, &u_len, &u_ts);
        }

        if (sp->agc_on == TRUE && 
            agc_apply_changes(tb->agc) == TRUE) {
                ui_send_audio_input_gain(sp, sp->mbus_ui_addr);
        }

        return TRUE;
}

static int
tx_encode(struct s_codec_state_store *css, 
          sample     *buf, 
          uint32_t     dur_used,
          uint32_t     encoding,
          u_char     *payloads, 
          coded_unit **coded)
{
        codec_id_t id;
        uint32_t    i;

        id = codec_get_by_payload(payloads[encoding]);
        assert(id);

        /* Look to see if we have already coded this unit,
         * i.e. we are using redundancy.  Don't want to code
         * twice since it screws up encoder state.
         */
        
        for (i = 0; i < encoding; i++) {
                if (coded[i]->id == id) {
                        break;
                }
        }

        if (i == encoding) {
                const codec_format_t *cf;
                coded_unit native;
                codec_state *cs;
                
                /* Unit does not exist already */
                cf = codec_get_format(id);
                
                /* native is a temporary coded_unit that we use to pass to
                 * codec_encode since this take a 'native' (raw) coded unit as
                 * input and fills in coded with the transformed data.
                 */
                native.id        = codec_get_native_coding((uint16_t)cf->format.sample_rate, 
                                                           (uint16_t)cf->format.channels);
                native.state     = NULL;
                native.state_len = 0;
                native.data      = (u_char*)buf;
                native.data_len  = (uint16_t)(dur_used * sizeof(sample) * cf->format.channels);
                
                /* Get codec state from those stored for us */
                cs = codec_state_store_get(css, id);
                return codec_encode(cs, &native, coded[encoding]);
        } else {
                /* duplicate coded unit */
                return coded_unit_dup(coded[encoding], coded[i]);
        }
}

void
tx_send(tx_buffer *tb)
{
        struct s_pb_iterator    *cpos;
        channel_data            *cd;
        channel_unit            *cu;
        session_t 		*sp;
        tx_unit        		*u;
        ts_t            	 u_ts, u_sil_ts, delta;
        ts_t            	 time_ts;
        uint32_t         	 time_32, cd_len;
        uint32_t         	 u_len, units, i, j, k, n, send, encoding;
        int 			 success;
        
        assert(pb_iterator_count(tb->audio_buffer) == 3);

        if (pb_iterators_equal(tb->silence, tb->transmit)) {
                /* Nothing to do */
                debug_msg("Nothing to do\n");
                return;
        } 

#ifndef NDEBUG
        {
                struct s_pb *buf;
                buf = pb_iterator_get_playout_buffer(tb->transmit);
                assert(pb_iterator_count(buf) == 3);
        }
#endif /* NDEBUG */

        pb_iterator_get_at(tb->silence,  (u_char**)&u, &u_len, &u_sil_ts);
        pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);

        assert(ts_gt(u_sil_ts, u_ts));

        delta = ts_sub(u_sil_ts, u_ts);
        n = delta.ticks / tb->unit_dur;

        sp = tb->sp;
	session_validate(sp);
        units = channel_encoder_get_units_per_packet(sp->channel_coder);
        
        while(n > units) {
                send = FALSE;

                /* Check whether we want to send this group of units */
                for (i = 0; i < units; i++) {
                        pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);
                        if (u->send) {
                                send = TRUE;
                                break;
                        }
                        pb_iterator_advance(tb->transmit);
                }

                /* Rewind transmit point to where it was before we did
                 * last check */
                while(i > 0) {
                        pb_iterator_retreat(tb->transmit);
                        i--;
                }
                
                for (i = 0;i < units; i++) {
                        media_data *m;
                        success = pb_iterator_get_at(tb->transmit, (u_char**)&u, &u_len, &u_ts);
                        assert(success);
                        if (send) {
                                media_data_create(&m, sp->num_encodings);
                                for(encoding = 0; encoding < (uint32_t)sp->num_encodings; encoding ++) {
                                        tx_encode(tb->state_store, 
                                                  u->data, 
                                                  u->dur_used,
                                                  encoding,
                                                  sp->encodings, 
                                                  m->rep);
                                }
                        } else {
                                media_data_create(&m, 0);
                        }
                        assert(m != NULL);
                        success = pb_add(tb->media_buffer, 
                                         (u_char*)m, 
                                         sizeof(media_data), 
                                         u_ts);
                        assert(success);
                        success = pb_iterator_advance(tb->transmit);
                        assert(success);
                }
                n -= units;
        }

        channel_encoder_encode(sp->channel_coder, tb->media_buffer, tb->channel_buffer);

        pb_iterator_create(tb->channel_buffer, &cpos);
        pb_iterator_advance(cpos);
        while(pb_iterator_detach_at(cpos, (u_char**)&cd, &cd_len, &time_ts)) {
                uint32_t csrc[16];
                char *data, pt;
                int   data_len, done;
                int  marker;

                /* Set up fields for RTP header */
                cu = cd->elem[0];
                pt = channel_coder_get_payload(sp->channel_coder, cu->pt);
                time_32 = ts_seq32_out(&tb->up_seq, tb->sample_rate, time_ts);
                if (time_32 - sp->last_depart_ts != units * tb->unit_dur) {
                        marker = 1;
                        debug_msg("new talkspurt %d %d != %d\n", time_32, sp->last_depart_ts, units * tb->unit_dur);
                } else {
                        marker = 0;
                }   
                
                /* layer loop starts here */
                for(j = 0; j < (uint32_t)sp->layers; j++) {
                        data_len = 0;
                        /* determine data length for packet.  This is a   */  
                        /* little over complicated because of layering... */
                        for(i = j, k=0; i < cd->nelem; i += sp->layers) {
                                data_len += (int) cd->elem[i]->data_len;
                                k++;
                        }

                        /* Copy all out going data into one block (no scatter) */
                        data = (char*)block_alloc(data_len);
                        done = 0;
                        for(i = j; i < cd->nelem; i += sp->layers) {
                                memcpy(data + done, cd->elem[i]->data, cd->elem[i]->data_len);
                                done += cd->elem[i]->data_len;
                        }
                        rtp_send_data(sp->rtp_session[j], time_32, pt, marker, 0, csrc, data, data_len, NULL, 0);
                        block_free(data, data_len);
                        tb->bps_bytes_sent += data_len;
                }
                /* layer loop ends here */
                
                sp->last_depart_ts  = time_32;
                channel_data_destroy(&cd, sizeof(channel_data));
        }
        pb_iterator_destroy(tb->channel_buffer, &cpos);

        /* Drain tb->audio, remove every older than silence position
         * by two packets worth of audio.  Note tb->media is drained
         * by the channel encoding stage and tb->channel is drained
         * in the act of transmission with pbi_detach_at call.
         */
        u_ts = ts_map32(tb->sample_rate, 2 * units * tb->unit_dur);

        {
                struct s_pb *buf;
                buf = pb_iterator_get_playout_buffer(tb->transmit);
                assert(pb_iterator_count(buf) == 3);
        }

        assert(pb_iterator_count(tb->audio_buffer) == 3);        
        n = pb_iterator_audit(tb->transmit, u_ts);
}

void
tx_update_ui(tx_buffer *tb)
{
        session_t	*sp           = tb->sp;

        if (sp->meter && tb->sending_audio) {
                struct s_pb_iterator *prev;  
                tx_unit              *u;
                uint32_t               u_len;
                ts_t                  u_ts;

                /* Silence point should be upto read point here so use last
                 * completely read unit.
                 */
                assert(pb_iterator_count(tb->audio_buffer) == 3);
                pb_iterator_dup(&prev, tb->silence);
                pb_iterator_retreat(prev);
                if (pb_iterators_equal(tb->silence, prev)) {
                        pb_iterator_destroy(tb->audio_buffer, &prev);
                        return;
                }
                if (pb_iterator_get_at(prev, (u_char**)&u, &u_len, &u_ts) &&
                    (vad_in_talkspurt(sp->tb->vad) == TRUE || sp->silence_detection == SILENCE_DETECTION_OFF)) {
                        ui_send_audio_input_powermeter(sp, sp->mbus_ui_addr, lin2vu(u->energy, 100, VU_INPUT));
                } else {
                        ui_send_audio_input_powermeter(sp, sp->mbus_ui_addr, 0);
                }
                pb_iterator_destroy(tb->audio_buffer, &prev);
                assert(pb_iterator_count(tb->audio_buffer) == 3);
        }
	/* This next routine is really inefficient - we only need do ui_info_activate() */
	/* when the state changes, else we flood the mbus with redundant messages.      */
        if (sp->silence_detection != SILENCE_DETECTION_OFF) {
                if (vad_in_talkspurt(sp->tb->vad) == TRUE) {
                        if (sp->ui_activated == FALSE) {
                                ui_send_rtp_active(sp, sp->mbus_ui_addr, rtp_my_ssrc(sp->rtp_session[0]));
                                sp->ui_activated = TRUE;
                        }
                } else if (sp->ui_activated == TRUE) {
                        ui_send_rtp_inactive(sp, sp->mbus_ui_addr, rtp_my_ssrc(sp->rtp_session[0]));
                        sp->ui_activated = FALSE;
                }
		if (sp->lecture) {
			sp->lecture = FALSE;
			ui_send_lecture_mode(sp, sp->mbus_ui_addr);
		}
        } else if (sp->silence_detection == SILENCE_DETECTION_OFF) {
                if (tb->sending_audio == TRUE && sp->ui_activated == FALSE) {
                        ui_send_rtp_active(sp, sp->mbus_ui_addr, rtp_my_ssrc(sp->rtp_session[0]));
                        sp->ui_activated = TRUE;
                }
        }
        if (tb->sending_audio == FALSE && sp->ui_activated == TRUE) {
                ui_send_rtp_inactive(sp, sp->mbus_ui_addr, rtp_my_ssrc(sp->rtp_session[0]));
                sp->ui_activated = FALSE;
        }
}

void
tx_igain_update(tx_buffer *tb)
{
        sd_reset(tb->sp->auto_sd);
        agc_reset(tb->agc);
}

int
tx_is_sending(tx_buffer *tb)
{
        return tb->sending_audio;
}

double
tx_get_bps(tx_buffer *tb)
{
        if (tb->bps_bytes_sent == 0) {
                return 0.0;
        } else {
                uint32_t dms;
                double  bps;
                ts_t delta = ts_abs_diff(tb->bps_last_update, tb->sp->cur_ts);
                dms        = ts_to_us(delta);
                bps        = tb->bps_bytes_sent * 8e6 / (double)dms;
                tb->bps_bytes_sent  = 0;
                tb->bps_last_update = tb->sp->cur_ts;
                return bps;
        }
}

static void 
tx_read_sndfile(session_t *sp, uint16_t tx_freq, uint16_t tx_channels, tx_unit *u)
{
        sndfile_fmt_t sfmt;
        int samples_read, dst_samples;

        snd_get_format(sp->in_file, &sfmt);
        if (sfmt.channels != tx_channels || sfmt.sample_rate != tx_freq) {
                converter_fmt_t target;
                const converter_fmt_t *actual;
                coded_unit in, out;

                target.src_channels = (uint16_t)sfmt.channels;
                target.src_freq     = (uint16_t)sfmt.sample_rate;
                target.dst_channels = (uint16_t)tx_channels;
                target.dst_freq     = tx_freq;

                /* Check if existing converter exists and whether valid */
                if (sp->in_file_converter != NULL) {
                        actual = converter_get_format(sp->in_file_converter);
                        if (memcmp(actual, &target, sizeof(converter_fmt_t)) != 0) {
                                converter_destroy(&sp->in_file_converter);
                        }
                }
                /* Create relevent converter if necessary */
                if (sp->in_file_converter == NULL) {
                        const converter_details_t *details = NULL;
                        uint32_t i, n;
                        /* We iterate through available converters
                         * since they have different capabilities,
                         * specifically MS-ACM does m*8:n*11025 and
                         * the RAT ones don't at time of writing.
                         */
                        n = converter_get_count();
                        for(i = 0; i < n; i++) {
                                details = converter_get_details(i);
                                if (converter_create(details->id, &target, &sp->in_file_converter)) {
                                        debug_msg("Created converter %s for sound file conversion\n", details->name);
                                        break;
                                }
                        }
                        if (i == n) {
                                debug_msg("Could not create suitable converter for sound file\n");
                                snd_read_close(&sp->in_file);
                                return;
                        }
                }

                dst_samples = u->dur_used * tx_channels;

                /* Prepare block to read audio into */
                in.id        = codec_get_native_coding(target.src_freq, target.src_channels);
                in.state     = NULL;
                in.state_len = 0;
                in.data_len  = sizeof(sample) * dst_samples * 
                        (target.src_freq * target.src_channels) /
                        (target.dst_freq * target.dst_channels);
                        
                in.data      = (u_char*)block_alloc(in.data_len);

                /* Get the sound from file */
                samples_read = snd_read_audio(&sp->in_file,
                                              (sample*)in.data,
                                              (uint16_t)(in.data_len / sizeof(sample)));

                if (samples_read == 0) {
                        /* File is paused */
                        codec_clear_coded_unit(&in);
                        return;
                }

                /* Prepare output block */
                memset(&out, 0, sizeof(out));
                converter_process(sp->in_file_converter,
                                  &in,
                                  &out);
                assert((uint32_t)dst_samples == out.data_len / sizeof(sample));
                memcpy(u->data,
                       out.data,
                       dst_samples * sizeof(sample));
                /* Tidy up */
                codec_clear_coded_unit(&in);
                codec_clear_coded_unit(&out);
        } else {
                snd_read_audio(&sp->in_file,
                               u->data,
                               (uint16_t)(u->dur_used * tx_channels));
        }
}
