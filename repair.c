/*
 * FILE:    repair.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
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

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "util.h"
#include "repair.h"
#include "receive.h"
#include "gsm.h"
#include "codec_lpc.h"

/* fade and pitch measures in ms */
#define FADE_DURATION   320.0f
#define MIN_PITCH         5.0f

#define ALPHA             0.9f
#define MATCH_LEN         20    
#define MAX_BUF_LEN       1024

char *
get_repair_name(int id)
{
        switch(id) {
        case REPAIR_NONE:
                return "NONE";
        case REPAIR_REPEAT:
                return "REPEAT";
        case REPAIR_PATTERN_MATCH:
                return "PATTERN-MATCH";
	case REPAIR_PITCH_REPEAT:
		return "PITCH_REPETITION";
        default:
                return "UNKNOWN";
        }
}

/* Consecutive dummies behind */
static int
count_dummies_back(rx_queue_element_struct *up)
{
        int n = 0;
	while (up->prev_ptr && up->dummy==1) {
		up = up->prev_ptr;
		n++;
	}
        return n;
}

/* Consecutive dummies ahead */
#ifdef NDEF
static int
count_dummies_ahead(rx_queue_element_struct *up)
{
        int n = 0; 

        while (up->next_ptr && up->dummy) {
		up = up->next_ptr;
		n++;
	}
	return n;
}
#endif

/* assumes buffer consists of interleaved samples for each channel
 * and that pointers passed are to first of interleaved samples in block i.e.
 *  C1S1 C2S1 C1S2 C2S2 C1S3 C2S3 where C = channel, S = sample 
 * (we always pass pointers to C1S1).
 */ 

static void
repeat_block(short *src_buf, int rep_len, rx_queue_element_struct *ip, int channel)
{
	register float sf,fps; 
	register int step = ip->comp_data[0].cp->channels;
	register short *src,*dst;
	int cd;
	int i=0, j=0;

	dst = ip->native_data[0]+channel;

	if (ip && ip->prev_ptr->dummy) {
		cd = count_dummies_back(ip->prev_ptr);
		fps = 1000.0f/(FADE_DURATION * (float)ip->comp_data[0].cp->freq);
		sf = 1.0f - fps * cd * ip->comp_data[0].cp->unit_len;
		if (sf<=0.0f) 
			sf = fps = 0.0f;
		while(i<ip->comp_data[0].cp->unit_len) {
			j = 0;
			src = src_buf+channel;
			while(j++<rep_len && i++<ip->comp_data[0].cp->unit_len) {
				*dst = (short)(sf*(float)(*src));
				dst += step;
				src += step;
				sf -= fps;
			}
		}
	} else {
		while(i<ip->comp_data[0].cp->unit_len) {
			j = 0;
			src = src_buf+channel;
			while(j++<rep_len && i++<ip->comp_data[0].cp->unit_len) {
				*dst = *src;
				dst += step;
				src += step;
			}
		}
	}
}

/* Based on 'Waveform Subsititution Techniques for Recovering Missing Speech
 *           Segments in Packet Voice Communications', Goodman, D.J., et al,
 *           IEEE Trans Acoustics, Speech and Signal Processing, Vol ASSP-34,
 *           Number 6, pages 1440-47, December 1986. (see also follow up by 
 *           Waseem 1988)
 * Equation (7) as this is cheapest.  Should stretch across multiple 
 * previous units  so that it has a chance to work at higher frequencies. 
 * Should also have option for 2 sided repair with interpolation.[oth]
 */

static void
pm_repair(rx_queue_element_struct *pp, rx_queue_element_struct *ip, int channel)
{
	static float mb[MAX_BUF_LEN], *mbp;
	short *src = pp->native_data[0] + channel;
	register short *bp;
	int len = pp->comp_data[0].cp->unit_len, step = pp->comp_data[0].cp->channels;
	register int   i,j;
	register int norm;
	int pos = 0,min_pitch;
	float score, target = 1e6;

	norm = 0;
	bp = src + (len-MATCH_LEN)*step;
	i=0;
	/* determine match buffer*/
	while(i++<MATCH_LEN){
		norm += abs(*bp);
		bp   += step;
	}
	bp = src + (len-MATCH_LEN)*step;
	mbp = mb;
	i=0;
	while(i++<MATCH_LEN){
		*mbp++ = (float)*bp/(float)norm;
		bp    += step;
	}
	/* hit off normalization */
	min_pitch = (int)(ip->comp_data[0].cp->freq*MIN_PITCH/1000);
	i = (len-MATCH_LEN-min_pitch)*step;
	bp = src + i;
	j=0;
	while(j++<MATCH_LEN) {
		norm += abs(*bp);
		bp   += step;
	}
	bp = src + i;
	while(i>0) {
		score = 0.0;
		j = 0;
		mbp = mb;
		while(j++<MATCH_LEN) {
			score += (float)fabs((float)*bp/(float)norm - *mbp++);
			bp    += step;
		}
		if (score<target) {
			pos = i/step;
			target = score;
		}
		norm -= abs(*(bp+step));
		bp -= (MATCH_LEN+1)*step;
		norm += abs(*bp);
		i -= step;
	}

#ifdef DEBUG_REPAIR
	debug_msg("match score %.4f period %d \n", target, len-MATCH_LEN-pos);
#endif

	repeat_block(pp->native_data[0]+(MATCH_LEN+pos)*step, 
		     len-MATCH_LEN-pos, /*period*/ 
		     ip, 
		     channel);
}

static void
repeat_lpc(rx_queue_element_struct *pp, rx_queue_element_struct *ip)
{
	lpc_txstate_t	*lp=NULL;

	assert(pp->comp_data != NULL);
        if (ip->comp_count == 0) {
		/* XXX */
		ip->comp_count = 1;
                ip->comp_data[0].data = (unsigned char*)block_alloc(LPCTXSIZE);
	}
	ip->comp_data[0].data_len = LPCTXSIZE;
	memcpy(ip->comp_data[0].data, pp->comp_data[0].data, LPCTXSIZE);
	ip->comp_data[0].cp = pp->comp_data[0].cp;
	lp = (lpc_txstate_t*)ip->comp_data[0].data;
	if (ip->dummy) 
		lp->gain = (u_char)((float)lp->gain * 0.8f);

	decode_unit(ip);
}

static void
repeat_gsm(rx_queue_element_struct *pp, rx_queue_element_struct *ip)
{
	/* GSM 06.11 repair mechanism */
	int		i;
	gsm_byte	*rep = NULL;
 	char		xmaxc;
    
	assert(pp->comp_data != NULL);
	
	if (ip->comp_count == 0) {
		ip->comp_data[0].data = (unsigned char*)block_alloc(GSM_FRAMESIZE);
		ip->comp_data[0].data_len = GSM_FRAMESIZE;
		ip->comp_count = 1;
	}

	rep = (gsm_byte*)ip->comp_data[0].data;
	memcpy(rep, pp->comp_data[0].data, GSM_FRAMESIZE);

	if (pp->dummy) {
		for(i=6;i<28;i+=7) {
			xmaxc  = (rep[i] & 0x1f) << 1;
			xmaxc |= (rep[i+1] >> 7) & 0x01;
			if (xmaxc > 4) { 
				xmaxc -= 4;
			} else { 
				xmaxc = 0;
			}
			rep[i]   = (rep[i] & 0xe0) | (xmaxc >> 1);
			rep[i+1] = (rep[i+1] & 0x7f) | ((xmaxc & 0x01) << 7);
		}
#ifdef DEBUG_REPAIR
		printf("fade\n");
#endif
	}
#ifdef DEBUG_REPAIR
	else printf("replica\n");
#endif
	decode_unit(ip);

	return;
}

void
repair(int repair, rx_queue_element_struct *ip)
{
	static codec_t *gsmcp, *lpccp;
	static int virgin = 1;

	rx_queue_element_struct *pp;
        u_int32 size;
	int i;

	if (virgin) {
		gsmcp = get_codec_by_pt(codec_matching("GSM", 8000, 1));
                lpccp = get_codec_by_pt(codec_matching("LPC", 8000, 1));
                assert(gsmcp);
                assert(lpccp);
		virgin = 0;
	}
	
	pp = ip->prev_ptr;
	if (!pp) {
		return;
#ifdef DEBUG_REPAIR
		fprintf(stderr,"repair: no previous unit\n");
#endif
	}
	ip->dummy = TRUE;

	if (pp->comp_data[0].cp) {
		ip->comp_data[0].cp = pp->comp_data[0].cp;
	} else {
#ifdef DEBUG_REPAIR
		fprintf(stderr, "Could not repair as no codec pointer for previous interval\n");
#endif
		return;
	}
	
	if (!pp->native_count) decode_unit(pp); /* XXX should never happen */
	
	assert(!ip->native_count);

        size  = ip->comp_data[0].cp->sample_size *
                ip->comp_data[0].cp->channels*
                ip->comp_data[0].cp->unit_len;
        ip->native_size[0] = (unsigned short) size;
	ip->native_data[0] = (sample*)block_alloc(size);
	ip->native_count   = 1;
        
	switch(repair) {
	case REPAIR_REPEAT:
		if (pp->comp_data[0].cp && pp->comp_data[0].cp == lpccp)
			repeat_lpc(pp, ip);
		else if (pp->comp_data[0].cp && pp->comp_data[0].cp == gsmcp) 
			repeat_gsm(pp, ip);
		else 
			for(i=0;i<pp->comp_data[0].cp->channels;i++)
				repeat_block(pp->native_data[0],pp->comp_data[0].cp->unit_len,ip,i);
		break;
	case REPAIR_PATTERN_MATCH:
		for(i=0;i<pp->comp_data[0].cp->channels;i++)
			pm_repair(pp,ip,i);
		break;
        } 
        return;
}

#ifdef REPAIR_TEST
#include "session.h"

/* Compile with:
% cc -DREPAIR_TEST -DSASR repair.c codec*.c gsm*.c util.c -lm
*/

void usage()
{
        printf("repair [-b <samples>] [-c <channels>] [-f <freq>] [-l <loss>] -p [-r <repair>] [-s <seed>] infile outfile\n");
        exit(-1);
}

static void
print_block(sample *s, int len, int freq)
{
        static int blk;
        int i;
        for(i = 0; i < len; i++) {
                printf("%.6f\t %d\n", ((float)(blk*len+i))/freq, s[i]);
        }
        blk++;
}

int
main(int argc, char *argv[])
{
        FILE *fin, *fout;
        session_struct sp;
        int ac, rep = REPAIR_NONE, print = FALSE;
        int blksz = 160, freq = 8000, channels = 1;
        double loss = 0.0;
        rx_queue_element_struct rx_curr, rx_prev;

        memset(&sp, 0, sizeof(session_struct));
	set_dynamic_payload(&sp.dpt_list, "WBS-16K-MONO",   PT_WBS_16K_MONO);
	set_dynamic_payload(&sp.dpt_list, "L16-8K-MONO",    PT_L16_8K_MONO);
	set_dynamic_payload(&sp.dpt_list, "L16-8K-STEREO",  PT_L16_8K_STEREO);
	set_dynamic_payload(&sp.dpt_list, "L16-16K-MONO",   PT_L16_16K_MONO);
	set_dynamic_payload(&sp.dpt_list, "L16-16K-STEREO", PT_L16_16K_STEREO);
	set_dynamic_payload(&sp.dpt_list, "L16-32K-MONO",   PT_L16_32K_MONO);
	set_dynamic_payload(&sp.dpt_list, "L16-32K-STEREO", PT_L16_32K_STEREO);
	set_dynamic_payload(&sp.dpt_list, "L16-48K-MONO",   PT_L16_48K_MONO);
	set_dynamic_payload(&sp.dpt_list, "L16-48K-STEREO", PT_L16_48K_STEREO);
        codec_init (&sp);
        
        ac = 1;
        while(ac < argc && argv[ac][0]=='-') {
                switch(argv[ac][1]) {
                case 'b':
                        /* block size in samples */
                        blksz = atoi(argv[++ac]);
                        break;
                case 'c':
                        channels = atoi(argv[++ac]);
                        break;
                case 'f':
                        freq = atoi(argv[++ac]);
                        break;
                case 'l':
                        loss = strtod(argv[++ac], NULL) / 100.0;
                        break;
                case 'p':
                        print = TRUE;
                        break;
                case 'r':
                        switch(argv[++ac][0]) {
                        case 'r':
                                rep = REPAIR_REPEAT;
                                break;
                        case 'p':
                                rep = REPAIR_PATTERN_MATCH;
                                break;
                        default:
                                rep = REPAIR_NONE;
                                break;
                        }
                        break;
                case 's':
                        srand48(atoi(argv[++ac]));
                        break;
        
                default:
                        usage();
                }
                ac++;
        }
        
        if ((argc - ac) != 2) usage();
        
        fin  = fopen(argv[ac++], "rb");
        fout = fopen(argv[ac++], "wb");
        
        if (!fin) { 
                fprintf(stderr, "Could not open %s\n", argv[ac-2]); 
                exit(-1);
        }

        if (!fout) { 
                fprintf(stderr, "Could not open %s\n", argv[ac-1]); 
                exit(-1);
        }

        memset(&rx_curr,0,sizeof(rx_queue_element_struct));
        memset(&rx_prev,0,sizeof(rx_queue_element_struct));

        rx_curr.prev_ptr = &rx_prev;
        rx_prev.comp_data[0].cp = get_codec_by_pt(codec_matching("Linear-16",freq,channels));
        rx_prev.native_data[0] = (sample*)block_alloc(sizeof(sample) * blksz);
        rx_prev.native_size[0] = sizeof(sample) * blksz;
        rx_prev.native_count   = 1;
        fread  (rx_prev.native_data[0], sizeof(sample), blksz, fin);
        fwrite (rx_prev.native_data[0], sizeof(sample), blksz, fout);

        while (!feof(fin)) {
                rx_curr.native_data[0]  = block_alloc(sizeof(sample) * blksz);
                rx_curr.native_size[0]  = sizeof(sample) * blksz;
                memset(rx_curr.native_data[0], 0, rx_curr.native_size[0]);
                rx_curr.dummy           = 0;
                
                fread(rx_curr.native_data[0], 2, blksz, fin);
                if (drand48()<loss) {
                        if (rep == REPAIR_NONE) {
                                memset(rx_curr.native_data[0],0,sizeof(sample) * blksz);
                        } else {
                                block_free(rx_curr.native_data[0], rx_curr.native_size[0]);
                                rx_curr.native_data[0] = NULL;
                                rx_curr.native_size[0]  = 0;
                                repair(rep, &rx_curr);
                        }
                }
                fwrite(rx_curr.native_data[0], 1, rx_curr.native_size[0], fout);
                if (print) print_block(rx_curr.native_data[0], rx_curr.native_size[0]/sizeof(sample), freq);
                block_free(rx_prev.native_data[0], rx_prev.native_size[0]);
                rx_prev.native_data[0] = rx_curr.native_data[0];
                rx_prev.native_size[0] = rx_curr.native_size[0];
                rx_prev.dummy          = rx_curr.dummy;
                rx_curr.native_data[0] = NULL;
                rx_curr.native_size[0] = 0;
                rx_curr.native_count   = 0;
                rx_curr.dummy          = FALSE;
        }

        return 0;
}

#endif REPAIR_TEST






