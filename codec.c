/*
 * FILE:    codec.c
 * PROGRAM: RAT
 * AUTHOR:  I.Kouvelas / O.Hodson
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

#include "config.h"
#include "codec.h"
#include "gsm.h"
#include "codec_adpcm.h"
#include "codec_g711.h"
#include "codec_lpc.h"
#include "codec_wbs.h"
#include "audio.h"
#include "util.h"
#include "session.h"
#include "receive.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "transmit.h"

#ifdef WIN32
#include "codec_acm.h"
#endif

typedef struct s_codec_state {
	struct s_codec_state *next;
	int	id;	/* Codec payload type */
	char	*s;	/* State */
} state_t;

static void
l16_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
  	int i;
	sample *d;

        UNUSED(s);

	d = (sample *)c->data;
	/* As this codec can deal with variable unit sizes
	 * let the table dictate what we do. */
  	for (i=0; i < cp->unit_len*cp->channels; i++) {
		*d++ = htons(*data);
		data++;
	}
}

static void
l16_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	int	i;
	sample	*p = (sample *)c->data;

        UNUSED(s);

	for (i = 0; i < cp->unit_len*cp->channels; i++) {
		*data++ = ntohs(*p);
		p++;
	}
}

static void
ulaw_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*p = c->data;

        UNUSED(s);

	for (i = 0; i < cp->unit_len; i++, p++, data++) {
                *p = s2u(*data);
        }
}

static void
ulaw_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*sc = (u_char *)c->data;

        UNUSED(s);

	for(i = 0; i < cp->unit_len; i++, sc++, data++) {
		*data = u2s(*sc);
	}
}

static void
alaw_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*p = c->data;

        UNUSED(s);

	for (i = 0; i < cp->unit_len; i++, p++, data++) {
		*p = s2a(*data);
        }
}

static void
alaw_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*sc = (u_char *)c->data;

        UNUSED(s);

	for(i = 0; i < cp->unit_len; i++, sc++, data++) {
		*data = a2s(*sc);
	}
}

static void
dvi_init(session_struct *sp, state_t *s, codec_t *c)
{
        UNUSED(sp);

	s->s = (char *) xmalloc(c->sent_state_sz);
	memset(s->s, 0, c->sent_state_sz);
}

static void
dvi_free(state_t *s)
{
        xfree(s->s);
}

static void
dvi_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
	memcpy(c->state, s->s, sizeof(struct adpcm_state));
	((struct adpcm_state*)c->state)->valprev = htons(((struct adpcm_state*)c->state)->valprev);
	adpcm_coder(data, c->data, cp->unit_len, (struct adpcm_state*)s->s);
}

static void
dvi_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	if (c->state_len > 0) {
		assert(c->state_len == cp->sent_state_sz);
		memcpy(s->s, c->state, sizeof(struct adpcm_state));
		((struct adpcm_state*)s->s)->valprev = ntohs(((struct adpcm_state*)s->s)->valprev);
	}
	adpcm_decoder(c->data, data, cp->unit_len, (struct adpcm_state*)s->s);
}

typedef struct s_wbs_state {
	wbs_state_struct	state;
	double			qmf_lo[16];
	double			qmf_hi[16];
	short			ns;		/* Noise shaping state */
} wbs_t;

static void
wbs_init(session_struct *sp, state_t *s, codec_t *c)
{
	wbs_t *wsp;

        UNUSED(sp);
        UNUSED(c);

	s->s = (char *) xmalloc(sizeof(wbs_t));
	wsp = (wbs_t *)s->s;
	wbs_state_init(&wsp->state, wsp->qmf_lo, wsp->qmf_hi, &wsp->ns);
}

static void
wbs_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
	subband_struct SubBandData;
	wbs_t *wsp;

        UNUSED(cp);

	wsp = (wbs_t *)s->s;
	memcpy(c->state, &wsp->state, WBS_STATE_SIZE);
	QMF(data, &SubBandData, wsp->qmf_lo, wsp->qmf_hi);
	LowEnc(SubBandData.Low, c->data, wsp->state.low, &wsp->ns);
	HighEnc(SubBandData.High, c->data, wsp->state.hi);
}

static void
wbs_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	subband_struct SubBandData;
	wbs_t	*wsp = (wbs_t *)s->s;

	if (c->state_len > 0)
		memcpy(&wsp->state, c->state, cp->sent_state_sz);

	LowDec(c->data, SubBandData.Low, wsp->state.low, &wsp->ns);
	HighDec(c->data, SubBandData.High, wsp->state.hi);
	deQMF(&SubBandData, data, wsp->qmf_lo, wsp->qmf_hi);
}

static void
lpc_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
        UNUSED(s);
        UNUSED(cp);

	lpc_analyze(data, (lpc_txstate_t *)c->data);
	((lpc_txstate_t *)c->data)->period = htons(((lpc_txstate_t *)c->data)->period);
}

static void
lpc_decode_init(session_struct *sp, state_t *s, codec_t *c)
{
        UNUSED(sp);
        UNUSED(c);

	s->s = (char*) xmalloc (sizeof(lpc_intstate_t));
	lpc_init((lpc_intstate_t*)s->s);
}

static void
lpc_decode_free(state_t *s)
{
        xfree(s->s);
}

static void
lpc_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
        UNUSED(cp);

	((lpc_txstate_t *)c->data)->period = htons(((lpc_txstate_t *)c->data)->period);
	lpc_synthesize(data, (lpc_txstate_t*)c->data, (lpc_intstate_t*)s->s);
}

static void
gsm_init(session_struct *sp, state_t *s, codec_t *c)
{
        UNUSED(sp);
        UNUSED(c);

	s->s = (char *)gsm_create();
}

static void
gsm_free(state_t *s)
{
        gsm_destroy((gsm)s->s);
}

static void
gsm_encoding(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
        UNUSED(cp);

	gsm_encode((gsm)s->s, data, (gsm_byte *)c->data); 
}

static void
gsm_decoding(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
        UNUSED(cp);

	gsm_decode((gsm)s->s, (gsm_byte*)c->data, (gsm_signal*)data);
}

#define DYNAMIC		-1

/* Quality Ratings are add hoc and only used to choose between which format 
 * to decode when redundancy is used.
 * Scales are:  8000Hz   0 - 19
 *             11025Hz  20 - 39
 *             16000Hz  40 - 59
 *             22050Hz  60 - 79
 *             32000Hz  80 - 99
 *             44100Hz 100 - 119
 *             48000Hz 120 - 139
 */

/* If you change the short_names of the dynamically detected codecs
 * here, update the files that add fn's to this table, i.e.
 * G723.1, G728.
 */

static codec_t codec_list[] = {
	{"LPC-8K-MONO", "LPC", 0, 7, 8000, 2, 1, 160, 
         0, LPCTXSIZE, 
         NULL, lpc_encode, NULL, 
         lpc_decode_init, lpc_decode, lpc_decode_free, NULL},
	{"GSM-8K-MONO", "GSM", 5, 3, 8000, 2, 1, 160, 
         0, 33, 
         gsm_init, gsm_encoding, gsm_free, 
         gsm_init, gsm_decoding, gsm_free, NULL},
        {"G723.1-8K-MONO", "G723.1(6.3kb/s)", 10, 4, 8000, 2, 1, 240, 
         0, 24, 
         NULL, NULL, NULL, 
         NULL, NULL, NULL, NULL},
        {"G723.1-8K-MONO", "G723.1(5.3kb/s)", 9, 4, 8000, 2, 1, 240, 
         0, 20, 
         NULL, NULL, NULL, 
         NULL, NULL, NULL, NULL},
        {"G728-8K-MONO", "G728(16kb/s)",11, 15, 8000, 2, 1, 160,
         0, 40,
         NULL, NULL, NULL,
         NULL, NULL, NULL, NULL},
        {"G729-8K-MONO", "G729(16kb/s)", 12, 18, 8000, 2, 1, 160,
         0, 20,
         NULL, NULL, NULL,
         NULL, NULL, NULL, NULL},
	{"DVI-8K-MONO", "DVI-ADPCM", 14, 5, 8000, 2, 1, 160, 
         sizeof(struct adpcm_state), 80, 
         dvi_init, dvi_encode, dvi_free, 
         dvi_init, dvi_decode, dvi_free, NULL},
        {"PCMA-8K-MONO", "A-Law", 15, 8, 8000, 2, 1, 160, 
         0, 160, 
         NULL, alaw_encode, NULL, 
         NULL, alaw_decode, NULL, NULL},
	{"PCMU-8K-MONO", "Mu-Law", 16, 0, 8000, 2, 1, 160, 
         0, 160, 
         NULL, ulaw_encode, NULL, 
         NULL, ulaw_decode, NULL, NULL},
	{"L16-8K-MONO", "Linear-16", 19, DYNAMIC, 8000, 2, 1, 160, 
         0, 320, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL},
	{"L16-8K-STEREO", "Linear-16", 19, DYNAMIC, 8000, 2, 2, 160, 
         0, 640, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL},
	{"DVI-16K-MONO", "DVI-ADPCM", 45, 6, 16000, 2, 1, 160, 
         sizeof(struct adpcm_state), 80, 
         dvi_init, dvi_encode, dvi_free, 
         dvi_init, dvi_decode, dvi_free, NULL},
	{"WBS-16K-MONO", "WB-ADPCM", 46, DYNAMIC, 16000, 2, 1, 160, 
         WBS_STATE_SIZE, WBS_UNIT_SIZE, 
         wbs_init, wbs_encode, NULL, 
         wbs_init, wbs_decode, NULL, NULL},
	{"L16-16K-MONO", "Linear-16", 59, DYNAMIC, 16000, 2, 1, 160, 
         0, 320, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL}, 
	{"L16-16K-STEREO", "Linear-16", 59, DYNAMIC, 16000, 2, 2, 160, 
         0, 640, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL}, 
	{"L16-32K-MONO", "Linear-16", 99, DYNAMIC, 32000, 2, 1, 160, 
         0, 320, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL},
	{"L16-32K-STEREO", "Linear-16", 99, DYNAMIC, 32000, 2, 2, 160, 
         0, 640, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL},
	{"L16-44K-MONO", "Linear-16", 119, 11, 44100, 2, 1, 160, 
         0, 320, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL},
	{"L16-44K-STEREO", "Linear-16", 119, 10, 44100, 2, 2, 160, 
         0, 640, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL}, 
	{"L16-48K-MONO", "Linear-16", 139, DYNAMIC, 48000, 2, 1, 160, 
         0, 320, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL}, 
	{"L16-48K-STEREO", "Linear-16", 139, DYNAMIC, 48000, 2, 2, 160, 
         0, 640, 
         NULL, l16_encode, NULL, 
         NULL, l16_decode, NULL, NULL}, 
	{""}
};

static codec_t *cd     = NULL;
static u_int32  codecs = 0;

typedef struct s_dpt {
	struct s_dpt *next;
	char *name;
	int pt;
} dpt;

void
set_dynamic_payload(dpt **dpt_list, char *name, int pt)
{
	dpt *p;

	for (p = *dpt_list; p; p = p->next) {
            if (strcmp(name, p->name) == 0) {
                printf("duplicate\n");
                break;
            }
	}
	if (p == 0) {
		p = (dpt *)xmalloc(sizeof(dpt));
		p->name = strcpy((char *) xmalloc(strlen(name) + 1), name);
		p->next = *dpt_list;
		*dpt_list = p;
#ifdef DEBUG_CODEC
                printf("added %s\n",name);
#endif
	} else {
#ifdef DEBUG_CODEC
		if (p->pt != pt) {
			printf("Reassigning dynamic payload type for encoding \"%s\". Old: %d New: %d\n", name, p->pt, pt);
		}
#endif
	}
	p->pt = pt;
}

int
get_dynamic_payload(dpt **dpt_list, char *name)
{
	dpt *p;

	for (p = *dpt_list; p; p = p->next) {
		if (strcmp(name, p->name) == 0)
			return (p->pt);
	}
#ifdef DEBUG_CODEC
	printf("get_dynamic_payload: payload_type for \"%s\" not specified.\n", name);
#endif
	return DYNAMIC;
}

void
codec_free_dynamic_payloads(dpt **dpt_list)
{
	dpt	*elem, *tmp_elem;

        elem = *dpt_list;
        while(elem != NULL) {
                tmp_elem = elem->next;
                xfree(elem->name);
                xfree(elem);
                elem = tmp_elem;
        }
}

void
codec_init(session_struct *sp)
{
	int	pt;
        u_int32 idx;

	if (cd) {
		return;
	}

        codecs = sizeof(codec_list) / sizeof(codec_t);
        cd     = (codec_t*)xmalloc(sizeof(codec_list));
        memcpy(cd, codec_list, sizeof(codec_list));
        
	for (idx = 0; idx < codecs; idx++) {
		if (cd[idx].pt == DYNAMIC) {
			if ((pt = get_dynamic_payload(&sp->dpt_list, cd[idx].name)) == DYNAMIC) {
                                debug_msg("Dynamic payload for %s not found.\n", cd[idx].name);
                                cd[idx].encode = NULL; /* Do not encode without payload number */
                        }
                        cd[idx].pt = pt;
		}
	}
#ifdef WIN32
        acmInit();
#endif
        codec_g711_init();
}

codec_t *
get_codec_by_pt(int pt)
{
        /* This code just went inefficient as G.723.1 has different
         * codings for same payload type.  We used to have a big table
         * indexed by payload but can't do this now.  
         */
        
        u_int32 i = 0;

        while(i < codecs) {
                if (cd[i].pt == pt) return (cd + i);
                i++;
        }

        return NULL;
}

codec_t *
get_codec_by_index(u_int32 idx)
{
        assert(idx < codecs);
        return cd + idx;
}

codec_t *
get_codec_by_name(char *name, session_struct *sp)
{
	u_int32 i;

        UNUSED(sp);

	for (i = 0; i < codecs; i++) {
            if (cd[i].name && strcasecmp(name, cd[i].name) == 0)
                return (&cd[i]);
	}

	debug_msg("codec \"%s\" not found.\n", name);

	return NULL;
}

u_int32 get_codec_count()
{
        return codecs;
}

enum co_e {
	ENCODE,
	DECODE
};

static state_t *
get_codec_state(session_struct *sp, state_t **lp, int pt, enum co_e ed)
{
	state_t *stp;
	codec_t *cp;

	for (stp = *lp; stp; stp = stp->next)
		if (stp->id == pt)
			break;

	if (stp == 0) {
		stp = (state_t *)xmalloc(sizeof(state_t));
		memset(stp, 0, sizeof(state_t));
		cp = get_codec_by_pt(pt);
		stp->id = pt;

		switch(ed) {
		case ENCODE:
		        if (cp->enc_init)
				cp->enc_init(sp,stp, cp);
			break;
		case DECODE:
			if (cp->dec_init)
				cp->dec_init(sp,stp, cp);
			break;
		default:
			fprintf(stderr, "get_codec_state: unknown op\n");
			exit(1);
		}

		stp->next = *lp;
		*lp = stp;
	}

	return (stp);
}

static void
clear_state_list(state_t **list, enum co_e ed)
{
        state_t *stp;
        codec_t *cp;

        while(*list != NULL) {
                stp = *list;
                *list = (*list)->next;
                cp = get_codec_by_pt(stp->id);
                switch(ed) {
                case ENCODE:
                        if (cp->enc_free) {
                                cp->enc_free(stp);
                        }
                        break;
                case DECODE:
                        if (cp->dec_free) {
                                cp->dec_free(stp);
                        }
                        break;
                }
                xfree(stp);
        }
        *list = NULL;
}

void
clear_encoder_states(state_t **list)
{
        clear_state_list(list, ENCODE);
}

void
clear_decoder_states(state_t **list)
{
        clear_state_list(list, DECODE);
}


void 
encoder(session_struct *sp, sample *data, int coding, coded_unit *c)
{
	codec_t	*cp;
	state_t *stp;

	cp = get_codec_by_pt(coding);
	c->cp = cp;
	stp = get_codec_state(sp, &sp->state_list, cp->pt, ENCODE);

	c->state = cp->sent_state_sz > 0 ? (u_char *) block_alloc(cp->sent_state_sz) : (u_char *) NULL;
	c->state_len = cp->sent_state_sz;
	c->data = (u_char *) block_alloc(cp->max_unit_sz);
	c->data_len = cp->max_unit_sz;

	cp->encode(data, c, stp, cp);
}

void
reset_encoder(session_struct *sp, int coding)
{
        UNUSED(sp);
        UNUSED(coding);
        /* should write this soon! */

}

void
decode_unit(rx_queue_element_struct *u)
{
	codec_t *cp;
	state_t *stp;

	if (u->comp_count == 0)
		return;
	cp = u->comp_data[0].cp;
	assert(u->native_count<MAX_NATIVE);
	stp = get_codec_state(NULL, &u->dbe_source[0]->state_list, cp->pt, DECODE);

	/* XXX dummy packets are not denoted explicitly, dummies have non-zero
	 * native_counts and zero comp_counts when dummy data is calculated  */ 
	if (u->native_count == 0) {
                u->native_size[u->native_count] = cp->channels * cp->unit_len * cp->sample_size;
		u->native_data[u->native_count] = (sample*)block_alloc(u->native_size[u->native_count]);
		u->native_count++;
	}
	assert(cp->decode!=NULL);
	cp->decode(&u->comp_data[0], u->native_data[u->native_count-1], stp, cp);
}

u_int32
get_codec_frame_size(char *data, codec_t *cp)
{
        if (cp->get_frame_size) {
                return cp->get_frame_size(data);
        } else {
                return cp->max_unit_sz;
        }
}

void
clear_coded_unit(coded_unit *u)
{
        if (u->state_len)
                block_free(u->state, u->state_len);
        assert(u->data_len);
        block_free(u->data, u->data_len);
        memset(u, 0, sizeof(coded_unit));
}

int
codec_loosely_compatible(codec_t *c1, codec_t *c2)
{
	return ((c1->freq == c2->freq)                  &&
                (c1->channels == c2->channels));
}

int
codec_compatible(codec_t *c1, codec_t *c2)
{
	return ((c1->freq == c2->freq)                  &&
                (c1->channels == c2->channels)          &&
		(c1->unit_len == c2->unit_len)          &&
		(c1->sample_size == c2->sample_size));
}

int
codec_first_with(int freq, int channels)
{
        u_int32 idx;
        for(idx = 0; idx < codecs; idx++) {
                if (cd[idx].name && cd[idx].freq == freq && cd[idx].channels == channels) {
                        return cd[idx].pt;
                }
        }
        return -1;
}

int
codec_matching(char *short_name, int freq, int channels)
{
	/* This has been changed to try really hard to find a matching codec.  */
	/* The reason is that it's now called as part of the command-line      */
	/* parsing, and so has to cope with user entered codec names. Also, it */
	/* should recognise the names sdr gives the codecs, for compatibility  */
	/* with rat-v3.0.                                                [csp] */
	/* This is not quite as inefficient as it looks, since stage 1 will    */
	/* almost always find a match.                                         */
        u_int32	 idx;
	char	*long_name;

	/* Stage 1: Try the designated short names... */
        for(idx = 0; idx < codecs; idx++) {
                if (cd[idx].freq == freq && cd[idx].channels == channels && !strcasecmp(short_name, cd[idx].short_name)) {
			assert(cd[idx].name != NULL);
                        return cd[idx].pt;
                }
        }

	/* Stage 2: Try to generate a matching name... */
	long_name = (char *) xmalloc(strlen(short_name) + 12);
	sprintf(long_name, "%s-%dK-%s", short_name, freq/1000, channels==1?"MONO":"STEREO");
        for(idx = 0; idx < codecs; idx++) {
                if (cd[idx].freq == freq && cd[idx].channels == channels && !strcasecmp(long_name, cd[idx].name)) {
                        return cd[idx].pt;
                }
        }

	/* Stage 3: Nasty hack... PCM->PCMU for compatibility with sdr and old rat versions */
	if (strcasecmp(short_name, "pcm") == 0) {
		sprintf(long_name, "PCMU-%dK-%s", freq/1000, channels==1?"MONO":"STEREO");
		for(idx = 0; idx < codecs; idx++) {
			if (cd[idx].freq == freq && cd[idx].channels == channels && !strcasecmp(long_name, cd[idx].name)) {
				return cd[idx].pt;
			}
		}
	}

	debug_msg("Unable to find codec \"%s\" at rate %d channels %d\n", short_name, freq, channels);
        return -1;
}
