/*
 * FILE:    codec.c
 * PROGRAM: RAT
 * AUTHOR:  I.Kouvelas
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

#include "codec.h"
#include "gsm.h"
#include "codec_adpcm.h"
#include "codec_lpc.h"
#include "codec_wbs.h"
#include "audio.h"
#include "util.h"
#include "session.h"
#include "receive.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "transmit.h"

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

	for (i = 0; i < cp->unit_len; i++)
		*p++ = lintomulaw[(unsigned short)*data++];
}

static void
ulaw_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*sc = (u_char *)c->data;

        UNUSED(s);

	for(i = 0; i < cp->unit_len; i++) {
		*data++ = u2s(*sc);
		sc++;
	}
}

static void
alaw_encode(sample *data, coded_unit *c, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*p = c->data;

        UNUSED(s);

	for (i = 0; i < cp->unit_len; i++,data++)
		*p++ = s2a(*data);
}

static void
alaw_decode(coded_unit *c, sample *data, state_t *s, codec_t *cp)
{
	int	i;
	u_char	*sc = (u_char *)c->data;

        UNUSED(s);

	for(i = 0; i < cp->unit_len; i++, sc++) {
		*data++ = a2s(*sc);
	}
}

static void
dvi_init(session_struct *sp, state_t *s, codec_t *c)
{
        UNUSED(sp);

	s->s = xmalloc(c->sent_state_sz);
	memset(s->s, 0, c->sent_state_sz);
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

	s->s = xmalloc(sizeof(wbs_t));
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

static codec_t codec_list[] = {
	{"LPC-8K-MONO",     2,       7, 8000, 2, 1, 0, 160, LPCTXSIZE, NULL, lpc_encode, lpc_decode_init, lpc_decode},
	{"GSM-8K-MONO",     4,       3, 8000, 2, 1, 0, 160, 33, gsm_init, gsm_encoding, gsm_init, gsm_decoding},
        {"PCMA-8K-MONO",    8,       8, 8000, 2, 1, 0, 160, 160, NULL, alaw_encode, NULL, alaw_decode},
	{"PCMU-8K-MONO",    9,       0, 8000, 2, 1, 0, 160, 160, NULL, ulaw_encode, NULL, ulaw_decode},
	{"DVI-8K-MONO",     7,       5, 8000, 2, 1, sizeof(struct adpcm_state), 160, 80, dvi_init, dvi_encode, dvi_init, dvi_decode},
	{"DVI-11K-MONO",   10,      16,11025, 2, 1, sizeof(struct adpcm_state), 160, 80, dvi_init, dvi_encode, dvi_init, dvi_decode},
	{"DVI-16K-MONO",   11,       6,16000, 2, 1, sizeof(struct adpcm_state), 160, 80, dvi_init, dvi_encode, dvi_init, dvi_decode},
	{"DVI-22K-MONO",   12,      17,22050, 2, 1, sizeof(struct adpcm_state), 160, 80, dvi_init, dvi_encode, dvi_init, dvi_decode},
	{"WBS-16K-MONO",   10, DYNAMIC,16000, 2, 1, WBS_STATE_SIZE, 160, WBS_UNIT_SIZE, wbs_init, wbs_encode, wbs_init, wbs_decode},
	{"L16-8K-MONO",     9, DYNAMIC, 8000, 2, 1, 0, 160, 320, NULL, l16_encode, NULL, l16_decode},
	{"L16-8K-STEREO",   9, DYNAMIC, 8000, 2, 2, 0, 160, 640, NULL, l16_encode, NULL, l16_decode},
	{"L16-16K-MONO",   11, DYNAMIC,16000, 2, 1, 0, 160, 320, NULL, l16_encode, NULL, l16_decode}, 
	{"L16-16K-STEREO", 11, DYNAMIC,16000, 2, 2, 0, 160, 640, NULL, l16_encode, NULL, l16_decode}, 
	{"L16-32K-MONO",   12, DYNAMIC,32000, 2, 1, 0, 160, 320, NULL, l16_encode, NULL, l16_decode},
	{"L16-32K-STEREO", 12, DYNAMIC,32000, 2, 2, 0, 160, 640, NULL, l16_encode, NULL, l16_decode},
	{"L16-44K-MONO",   13,      11,44100, 2, 1, 0, 160, 320, NULL, l16_encode, NULL, l16_decode},
	{"L16-44K-STEREO", 13,      10,44100, 2, 2, 0, 160, 640, NULL, l16_encode, NULL, l16_decode}, 
	{"L16-48K-MONO",   14, DYNAMIC,48000, 2, 1, 0, 160, 320, NULL, l16_encode, NULL, l16_decode}, 
	{"L16-48K-STEREO", 14, DYNAMIC,48000, 2, 2, 0, 160, 640, NULL, l16_encode, NULL, l16_decode}, 
	{""}
};

static codec_t cd[MAX_CODEC];

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
		p->name = strsave(name);
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
codec_init(session_struct *sp)
{
	static	inited = 0;
	codec_t	*cp;
	int	pt;

	if (inited) {
		return;
	}
	inited = 1;

	memset(&cd, 0, MAX_CODEC * sizeof(codec_t));

	for (cp = codec_list; *cp->name != 0; cp++) {
		if (cp->pt == DYNAMIC) {
			if ((pt = get_dynamic_payload(&sp->dpt_list, cp->name)) == DYNAMIC)
				continue;
		} else {
			pt = cp->pt;
		}
		memcpy(&cd[pt], cp, sizeof(codec_t));
		cd[pt].pt = pt;
	}
}

codec_t *
get_codec(int pt)
{
	assert(pt >= 0 && pt < MAX_CODEC);
	if (cd[pt].name != 0) {
		return &cd[pt];
	} else {
		return NULL;
	}
}

codec_t *
get_codec_byname(char *name, session_struct *sp)
{
	int i;

        UNUSED(sp);

	for (i = 0; i < MAX_CODEC; i++) {
            if (cd[i].name && strcasecmp(name, cd[i].name) == 0)
                return (&cd[i]);
	}
#ifdef DEBUG_CODEC
	printf("get_codec_byname: codec \"%s\" not found.\n", name);
#endif
	return NULL;
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
		cp = get_codec(pt);
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

void
encoder(session_struct *sp, sample *data, int coding, coded_unit *c)
{
	codec_t	*cp;
	state_t *stp;

	cp = get_codec(coding);
	c->cp = cp;
	stp = get_codec_state(sp, &sp->state_list, cp->pt, ENCODE);

	c->state = cp->sent_state_sz > 0? block_alloc(cp->sent_state_sz) : NULL;
	c->state_len = cp->sent_state_sz;
	c->data = block_alloc(cp->max_unit_sz);
	c->data_len = cp->max_unit_sz;
	cp->encode(data, c, stp, cp);
}

void
reset_encoder(session_struct *sp, int coding)
{
        UNUSED(sp);
        UNUSED(coding);
        /* should write this soon! */

        dprintf("reset encoder called and no code exists here!");
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
		u->native_data[u->native_count] = (sample*)xmalloc(cp->channels * cp->unit_len * cp->sample_size);
		u->native_count++;
	}
	assert(cp->decode!=NULL);
	cp->decode(&u->comp_data[0], u->native_data[u->native_count-1], stp, cp);
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
codec_compatible(codec_t *c1, codec_t *c2)
{
	return ((c1->freq == c2->freq)                  &&
                (c1->channels == c2->channels)          &&
		(c1->unit_len == c2->unit_len)          &&
		(c1->sample_size == c2->sample_size));
}



