/*
 * FILE: cc_red.c
 * PROGRAM: RAT / redundancy
 * AUTHOR: Orion Hodson
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-97 University College London
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
#include "debug.h"
#include "memory.h"
#include "util.h"
#include "session.h"
#include "audio_fmt.h"
#include "codec.h"
#include "channel.h"
#include "receive.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "cc_red.h"

#define MAX_RED_LAYERS  4
/* 3 is excessive, and the ui lets folks set 2, 4 is some margin for decoder */
#define MAX_RED_OFFSET  40

#define RED_F(x)  ((x)&0x80000000)
#define RED_PT(x) (((x)>>24)&0x7f)
#define RED_OFF(x) (((x)>>10)&0x3fff)
#define RED_LEN(x) ((x)&0x3ff)

typedef struct s_red_coder {
        int         offset[MAX_RED_LAYERS];            /* Relative to primary    */
        int         coding[MAX_RED_LAYERS];            /* Coding of each layer   */
        int         nlayers;
        cc_unit    *c[MAX_RED_OFFSET][MAX_RED_LAYERS]; /* Circular buffer        */
        int         n[MAX_RED_OFFSET];                 /* Number of encs present */
        int         head;
        int         tail;
        int         len;
        cc_unit     last;
        int         last_hdr_idx;
} red_coder_t;

__inline static void 
red_clear_row(red_coder_t *r, int row)
{
        int i;
        for(i=0;i<r->n[row];i++) {
                clear_cc_unit(r->c[row][i], 0);
                block_free(r->c[row][i], sizeof(cc_unit));
        }
        r->n[row] = 0;
}

__inline static void
red_clear_last_hdrs(red_coder_t *r)
{
        while(r->last_hdr_idx > 0) {
                r->last_hdr_idx--;
                if (r->last.iov[r->last_hdr_idx].iov_base != NULL) {
                        block_free(r->last.iov[r->last_hdr_idx].iov_base, 
                                   r->last.iov[r->last_hdr_idx].iov_len);
                        r->last.iov[r->last_hdr_idx].iov_base = NULL; 
                        r->last.iov[r->last_hdr_idx].iov_len  = 0;
                }
        }

        r->last.iovc = 0;
}

void
red_flush(red_coder_t *r)
{
        int i;
        red_clear_last_hdrs(r);
        for(i=0;i<MAX_RED_OFFSET;i++) {
                red_clear_row(r, i);
        }
        r->head = r->tail = r->len = 0;
        debug_msg("flushed\n");
}

red_coder_t *
red_enc_create(void)
{
        red_coder_t *r = (red_coder_t*)xmalloc(sizeof(red_coder_t));
        memset(r,0,sizeof(red_coder_t));
        return r;
}

void
red_enc_destroy(red_coder_t *r)
{
        red_flush(r);
        xfree(r);
}

/* expects string with "codec1/offset1/codec2/offset2..." 
 * nb we don't save samples so if coding changes we take our chances.
 * No attempt is made to recode to new format as this takes an 
 * unspecified amount of cpu.  */

int
red_config(session_struct *sp, red_coder_t *r, char *cmd)
{
        char *s;
        codec_id_t id;
        int   i;

        UNUSED(sp);
        red_flush(r);

        r->offset[0] = 0;
        r->nlayers = 0;

        s = strtok(cmd, "/");
        do {
                id = codec_get_by_name(s);
                if (!id) {
                        debug_msg("Codec not recognized.\n");
                        abort();
                }
                r->coding[r->nlayers] = codec_get_payload(id);
                s = strtok(NULL,"/");
                r->offset[r->nlayers] = atoi(s);

                if (r->offset[r->nlayers]<0 || r->offset[r->nlayers]>MAX_RED_OFFSET) {
                        debug_msg("Offset of < 0 or > MAX_RED_OFFSET.\n");
                        abort();
                }

                for(i=0;i<r->nlayers;i++) {
                        if (r->offset[i]>=r->offset[r->nlayers]) {
                                debug_msg("Successing redundant encodings must have bigger offsets %d <= %d\n", r->offset[i], r->offset[r->nlayers]);
                                abort();
                        }
                }

                if (r->nlayers>0) {
                        codec_id_t id0, id1;
                        id0 = codec_get_by_payload(r->coding[0]);
                        id1 = codec_get_by_payload(r->coding[r->nlayers]);
                        assert(id0); assert(id1);
                        assert(codec_audio_formats_compatible(id0,id1));
                }
                r->nlayers++;
        } while((s=strtok(NULL,"/")) && r->nlayers<MAX_RED_LAYERS);

        r->offset[0]     = 0;

        return TRUE;
}

#define RED_FRAG_SZ 32

void 
red_qconfig(session_struct    *sp, 
            red_coder_t *r,
            char *buf, int blen) 
{
        codec_id_t            id;
        const codec_format_t *cf;
        int i,fraglen,len;
        char fragbuf[RED_FRAG_SZ];

        UNUSED(sp);

        len = 0;
        for(i=0;i<r->nlayers;i++) {
                id = codec_get_by_payload(r->coding[i]);
                assert(id);
                cf = codec_get_format(id);
                sprintf(fragbuf,"%s/%d/", cf->long_name, r->offset[i]);
                fraglen = strlen(fragbuf);
                
                if ((fraglen + len) < blen) {
                        sprintf(buf + len, "%s", fragbuf);
                        len += fraglen;
                } else {
                        debug_msg("Buffer too small!");
                        break;
                }
        }
        /* zap trailing slash */
        if (len>0) {
                buf[len-1] = '\0';
        }
}

__inline static void
red_pack_hdr(char *h, char more, char pt, short offset, short len)
{
        assert(((~0 <<  7) & pt)     == 0);
        assert(((~0 << 14) & offset) == 0);
        assert(((~0 << 10) & len)    == 0);
        assert(codec_get_by_payload((u_char)pt));
        if (more) {
                u_int32 *hdr = (u_int32*)h;
                (*hdr)       = 0;
                (*hdr)       = (0x80|pt)<<24;
                (*hdr)      |= offset<<10;
                (*hdr)      |= len;
                (*hdr)       = htonl((*hdr));
        } else {
                (*h) = pt;
        }
}

__inline static cc_unit*
red_get_unit(red_coder_t *r, int unit_off, int pt)
{
        int i,pos;

        assert (pt < 128);

        if (unit_off > r->len) return NULL;

        pos = r->head - unit_off;

        if (pos < 0) pos += MAX_RED_OFFSET;

        if (r->head > r->tail) {
                if (pos > r->head || pos < r->tail) return NULL;
        } else if (r->head < r->tail) {
                if (pos > r->head && pos < r->tail) return NULL;
        }

        for(i = 0; i<r->n[pos]; i++) {
                 if (r->c[pos][i]->pt == pt) return r->c[pos][i];
        }

        return NULL;
}

__inline static int
red_available(red_coder_t *r) {
        int i;
        cc_unit *u;

        i = 0;
        while (((u = red_get_unit(r, r->offset[i], r->coding[i])) != NULL) && 
               i < r->nlayers)
                i++;

        return i;
}


int 
red_encode(session_struct *sp, cc_unit **coded, int num_coded, cc_unit **out, red_coder_t *r) 
{
        int i, avail, new_ts;
        cc_unit *u;
        codec_id_t id;
        u_int16 samples_per_frame;

        r->head = (r->head + 1) % MAX_RED_OFFSET;
        r->len++;
        
        if (r->head == r->tail) {
                r->tail = (r->tail + 1) % MAX_RED_OFFSET;
                r->len--;
        }

        /* check if new talkspurt */
        if (r->last_hdr_idx != 0) {
                new_ts = 0;
        } else {
                new_ts = CC_NEW_TS;
        }

        /* get rid of old data */
        red_clear_row(r, r->head);
        red_clear_last_hdrs(r);

        if (num_coded == 0) {
                /* NB above steps are necessary to keep 
                 * stored data in correct temportal order
                 * when no data incoming.
                 */
                *out = NULL;
                return CC_NOT_READY;
        }

        /* transfer new data into head */

        for(i = 0; i < num_coded; i++) {
                r->c[r->head][i] = coded[i];
                assert(r->c[r->head][i]->pt < 128);
                r->n[r->head]++;
        }

        avail = red_available(r);
        assert(avail <= r->nlayers);

        id = codec_get_by_payload((u_char)r->coding[0]);
        assert(id);
        samples_per_frame = codec_get_samples_per_frame(id);
        if (avail != r->nlayers) {
                /* Not enough redundancy available, so advertise max offset 
                 * to help receivers*/
                r->last.iov[0].iov_base = (caddr_t)block_alloc(sizeof(u_int32));
                r->last.iov[0].iov_len  = sizeof(u_int32);
                red_pack_hdr((char*)r->last.iov[0].iov_base, 
                             1, 
                             (char)r->coding[r->nlayers-1], 
                             (short)(r->offset[r->nlayers-1] * collator_get_units(sp->collator) * samples_per_frame),
                             0);
                r->last_hdr_idx = 1;
                r->last.iovc    = 1;
        }

        /* pack rest of headers */
        i = avail;
        while(i-- > 0) {
                cc_unit *tmp;
                int sz = (i != 0) ? sizeof(u_int32) : sizeof(u_char);

                tmp = red_get_unit(r, r->offset[i], r->coding[i]);
                assert(tmp != NULL);

                r->last.iov[r->last.iovc].iov_base = (caddr_t)block_alloc(sz);
                r->last.iov[r->last.iovc].iov_len  = sz;
                red_pack_hdr((char*)r->last.iov[r->last.iovc].iov_base,
                             (char)i,
                             (char)r->coding[i],
                             (short)(r->offset[r->nlayers-1] * collator_get_units(sp->collator) * samples_per_frame),
                             (short)get_bytes(tmp)
                             );
                r->last.iovc++;
                r->last_hdr_idx++;
        }
        assert(r->last_hdr_idx <= r->nlayers);

        /* pack data */
        while(avail-- > 0) {
                u = red_get_unit(r, r->offset[avail], r->coding[avail]);
                memmove(r->last.iov + r->last.iovc, u->iov, sizeof(struct iovec) * u->iovc);
                r->last.iovc += u->iovc;
        }

        *out = &r->last;
        return CC_READY | new_ts;
}

static int
red_max_offset(cc_unit *ccu)
{
        int i       = 0;
	int off     = 0;
	int max_off = 0;

        for(i = 0; i < ccu->iovc && ccu->iov[i].iov_len == 4; i++) {
                off = RED_OFF(ntohl(*((u_int32*)ccu->iov[i].iov_base)));
                max_off = max(max_off, off);
        }
        if (i > MAX_RED_OFFSET || i == ccu->iovc) {
		return -1;
	}
        return max_off;
}

#define RED_MAX_RECV 5
typedef struct s_red_dec_state {
        int n;
        char encs[RED_MAX_RECV];
} red_dec_state;

red_dec_state *
red_dec_create()
{
        red_dec_state *r = (red_dec_state*)xmalloc(sizeof(red_dec_state));
        r->n = 0;
        return r;
}

void
red_dec_destroy(red_dec_state *r)
{
        xfree(r);
}

void
red_decode(session_struct *sp, rx_queue_element_struct *u, red_dec_state *r)
{
        /* Second system 1 Old system 0 */
        rx_queue_element_struct *su;
        int i, max_off, hdr_idx, data_idx, off, len, update_req;
        u_int32 red_hdr, samples_per_frame;
        codec_id_t            id;
        
        cc_unit *cu = u->ccu[0];

        hdr_idx  = cu->hdr_idx;
        data_idx = cu->data_idx;

        if ((cu->iov[hdr_idx].iov_len != 4) || (max_off = red_max_offset(cu)) == -1) {
                return;
        }
        
        update_req = FALSE;

        do {
                red_hdr = ntohl(*((u_int32*)cu->iov[hdr_idx].iov_base));
                id      = codec_get_by_payload(RED_PT(red_hdr));
                len     = RED_LEN(red_hdr);
                if (id && len != 0) {
                        off = RED_OFF(red_hdr);
                        samples_per_frame = codec_get_samples_per_frame(id);
                        su = get_rx_unit((max_off - off) / samples_per_frame, u->ccu[0]->pt, u);
                        data_idx += fragment_spread(id, len, &cu->iov[data_idx], cu->iovc - data_idx, su);
                } else {
                        if (!id) debug_msg("pt %d not decodable\n", RED_PT(red_hdr));
                }
                if ((hdr_idx >= 0          && 
                     hdr_idx<RED_MAX_RECV) && 
                    id                     &&
                    codec_get_payload(id) != r->encs[hdr_idx]) {
                        r->encs[hdr_idx] = codec_get_payload(id);
                        r->n = hdr_idx+1;
                        update_req = TRUE;
                }
                hdr_idx++;
        } while (cu->iov[hdr_idx].iov_len != 1);

        id = codec_get_by_payload(*((char*)cu->iov[hdr_idx].iov_base)&0x7f);
        assert(id);
        len = 0;
        i = data_idx;
        while (i < cu->iovc) 
                len += cu->iov[i++].iov_len;
        samples_per_frame = codec_get_samples_per_frame(id);
        su = get_rx_unit(max_off / samples_per_frame, u->cc_pt, u);
        data_idx += fragment_spread(id, len, &cu->iov[data_idx], cu->iovc - data_idx, su);
        assert(data_idx == cu->iovc);

        if ((hdr_idx >= 0          && 
             hdr_idx<RED_MAX_RECV) && 
            id                     &&
            codec_get_payload(id) != r->encs[hdr_idx]) {
                r->encs[hdr_idx] = codec_get_payload(id);
                update_req = TRUE;
        }

        if (update_req) {
                /* this is not nice, running out of time... */
                const codec_format_t *cf;
                char fmt[100];
                sprintf(fmt, "REDUNDANCY(");
                len = 11;
                r->n = hdr_idx + 1;
                for(i = 0; i < r->n; i++) {
                        id = codec_get_by_payload(r->encs[r->n - 1 - i]);
                        cf = codec_get_format(id);
                        if (id) {
                                sprintf(fmt+len, "%s,", cf->short_name);
                                len += strlen(cf->short_name) + 1;
                        } else {
                                debug_msg("pt %d not recognized\n", r->encs[r->n - 1 - i]);
                        }
                } 
                sprintf(fmt+len-1,")");
                rtcp_set_encoder_format(sp, u->dbe_source[0], fmt);
        }
}    

int
red_bps(session_struct *sp, red_coder_t *r)
{
        int i, b, ups, upp;
        codec_id_t id;
        const codec_format_t *cf;
        b = 0;
        upp = collator_get_units(sp->collator);
        id  = codec_get_by_payload(r->coding[0]);
        cf  = codec_get_format(id);
        ups = cf->format.sample_rate / codec_get_samples_per_frame(id);
        for(i=0;i<r->nlayers;i++) {
                id = codec_get_by_payload(r->coding[i]);
                cf  = codec_get_format(id);
                b += cf->mean_coded_frame_size * upp + cf->mean_per_packet_state_size + 4;
        }
        b += 1 + (r->nlayers-1)*4 + 12; /* headers */
        return (8*b*ups/upp);
}

/* this ultra paranoid function works out the correct allocation
 * of memory blocks for a redundantly encoded block, sets the number 
 * of trailing units needed to prod the channel coder and returns 
 * the number units per packet of the primary encoding.
 */

int
red_valsplit(char *blk, unsigned int blen, cc_unit *cu, int *trailing, int *inter_pkt_gap) {
<<<<<<< cc_red.c
        UNUSED(blk);
        UNUSED(blen);
        UNUSED(cu);
        UNUSED(trailing);
        UNUSED(inter_pkt_gap);
=======
        int 	 tlen, n, todo;
        int 	 hdr_idx; 
        u_int32  red_hdr, max_off;
        codec_t *cp;
        char	*media;

        assert(!cu->iovc);
        max_off  = 0;
        hdr_idx  = 0;
        cu->iovc = MAX_RED_LAYERS;
        todo     = blen;
        
	/* This next block finds the start of the media data. When it completes, "media" */
	/* will point to the first byte of media data after the redundancy header.       */
        media = blk;
        while (RED_F((red_hdr = ntohl(*(u_int32*)media)))) {
                media += 4;
        }
        media += 1;

	/* Now work our way through the redundant data blocks, setting up the iov pointers */
	/* in the cc_unit... */
        while(RED_F((red_hdr=ntohl(*((u_int32*)blk)))) && todo >0) {
                cu->iov[hdr_idx++].iov_len = 4;
                todo -= 4;
                blk  += 4;
                cp      = get_codec_by_pt(RED_PT(red_hdr));
                max_off = max(max_off, RED_OFF(red_hdr));
                tlen    = RED_LEN(red_hdr);
                /* we do not discard packet if we cannot decode redundant data */
                if (cp && tlen && fragment_sizes(cp, media, tlen, cu->iov, &cu->iovc, CC_UNITS) < 0) {
                        debug_msg("frg sz");
                        goto fail;
                }
                todo  -= tlen;
                media += tlen;
                assert(hdr_idx <= MAX_RED_LAYERS);
        }
        
	/* Next do the same thing, but with the primary encoding... */
        if (hdr_idx >= MAX_RED_LAYERS || todo <= 0) {
                debug_msg("hdr ovr\n");
                goto fail;
        }
        cp = get_codec_by_pt((*blk)&0x7f);
        if (cp == NULL) {
                debug_msg("Cannot decode primary, discarding entire packet...");
                goto fail;
        }
        cu->iov[hdr_idx++].iov_len = 1;
        todo -= 1;

        if (hdr_idx >= MAX_RED_LAYERS || todo <= 0) {
                debug_msg("Too many redundant encodings, discarding packet...\n");
                goto fail;
        }
        
        if ((n = fragment_sizes(cp, media, todo, cu->iov, &cu->iovc, CC_UNITS)) < 0) goto fail;

        /* label hdr and data starts */
        cu->hdr_idx  = 0;
        cu->data_idx = hdr_idx; 
        /* push headers and data against each other */
        cu->iovc    -= MAX_RED_LAYERS - hdr_idx;
        memmove(cu->iov+hdr_idx, 
                cu->iov+MAX_RED_LAYERS, 
                sizeof(struct iovec)*(cu->iovc - hdr_idx));
        memset(cu->iov + cu->iovc, 0, sizeof(struct iovec) * (CC_UNITS - cu->iovc));

        (*trailing)      = max_off/cp->unit_len + n;
        (*inter_pkt_gap) = cp->unit_len * n; 

        return n;

fail:
        for(hdr_idx=0; hdr_idx<cu->iovc; hdr_idx++) {
                debug_msg("%d -> %03d bytes\n", 
                        hdr_idx,
                        cu->iov[hdr_idx].iov_len);
        }

        cu->iovc = 0;
>>>>>>> 1.29
        return 0;
}

int
red_wrapped_pt(char *blk, unsigned int blen)
{
        u_int32 *hdr;
        int todo;

        hdr  = (u_int32*)blk;
        todo = blen;
        while (RED_F(ntohl(*hdr)) && todo>0) {
                hdr++;
                todo -= 4;
        }

        return (todo ? RED_PT(ntohl(*hdr)) : -1);
}

void
red_fix_encodings(session_struct *sp, red_coder_t *r)
{
        int i,j;
        
        sp->encodings[0]  = r->coding[0];
        sp->num_encodings = 1;
        debug_msg("added %d\n", r->coding[0]);
        for(i = 1; i < r->nlayers; i++) {
                for(j = 0; j<sp->num_encodings && sp->encodings[j] != r->coding[i]; j++);
                if (j == sp->num_encodings) {
                        sp->encodings[sp->num_encodings] = r->coding[i];
                        debug_msg("added %d\n", r->coding[i]);
                        sp->num_encodings++;
                }
                assert(r->coding[i]<127);
        }
}
