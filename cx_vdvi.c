/*
 * FILE:    vdvi.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
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

#include "cx_vdvi.h"
#include "assert.h"
#include "stdio.h"

#ifdef TEST_VDVI
#define NUM_TESTS 100000

#include "assert.h"
#include <stdio.h>
#include <stdlib.h>

static  u_char src[80], pad1[4], 
        dst[80], pad2[4], 
        coded[160], pad3[4], safe[80];

void
check_padding()
{
        assert(pad1[0] == '\x77' && pad2[0] == '\x77' && pad3[0] == '\x77');
        assert(pad1[1] == '\x77' && pad2[1] == '\x77' && pad3[1] == '\x77');
        assert(pad1[2] == '\x77' && pad2[2] == '\x77' && pad3[2] == '\x77');
        assert(pad1[3] == '\x77' && pad2[3] == '\x77' && pad3[3] == '\x77');
}

int main()
{
        int i, n, coded_len, out_len;

        memset(pad1, 0x77, 4); /* Memory overwrite test */
        memset(pad2, 0x77, 4);
        memset(pad3, 0x77, 4);

        for(n = 0; n < NUM_TESTS; n++) {
                srand(150121*n);

                for(i = 0; i< 80; i++) {
                        src[i] = random() & 0xff;
                }

                memcpy(safe, src, 80);

                coded_len = vdvi_encode(src,  160, coded, 160);

                assert(!memcmp(src,safe,80));

                check_padding();
                out_len   = vdvi_decode(coded, 160, dst, 160);
                
                assert(!memcmp(src,safe,80));
                assert(coded_len == out_len);

                check_padding();

                for(i = 0; i< 80; i++) {
                        assert(src[i] == dst[i]);
                }
        }
        printf("Tested %d frames\n", n);
        return 1;
}
#endif TEST_DVI

/*
typedef unsigned int  u_int;
typedef unsigned char u_char;
*/
/* Bitstream structure to make life a little easier */

typedef struct {
        u_char *buf;
        u_char *pos;
        u_int   bits_remain;
        u_int   len;
} bs;

__inline static void
bs_init(bs *b, char *buf, int bytes)
{
        b->buf    = b->pos = buf;
        b->len = bytes;
        b->bits_remain = 8;
}

__inline static void
bs_put(bs* b, u_char in, u_int n_in)
{
        register u_int   br, t;
        u_char *p;

        p  = b->pos;
        br = b->bits_remain;
        
        assert(n_in <= 8);
        
        if (n_in >= br) {
                t = n_in - br;
                *p |= in >> t;
                if ((unsigned)(p - b->buf) < (b->len - 1)) {
                        p++;
                        br = 8 - t;
                        *p = in << br;
                } else {
                        /* buffer_full - don't clear way */
                }
        } else {
                *p |= in << ( br - n_in);
                br -= n_in;
        }

        b->pos = p;
        b->bits_remain = br;
        assert(((u_char)(b->pos - b->buf) < b->len) ||
                ((u_char)(b->pos - b->buf) == b->len && b->bits_remain == 8));
}

__inline static u_char
bs_get(bs *b, u_int bits)
{
        register char *p;
        register u_int br;

        u_char mask,out;
        
        p  = b->pos;
        br = b->bits_remain;

        if (bits >= br) {
                mask = 0xff >> (8 - br);
                bits -= br;
                out = (*p & mask) << bits;
                p++;
                br = 8 - bits;
                mask = 0xff << br;
                out |= (*p & mask) >> br;
        } else {
                br -= bits;
                mask = (0xff >> (8 - bits));
                mask <<=  br;
                out  = (*p & mask) >> br;
        }
        b->pos = p;
        b->bits_remain = br;
        assert(((u_char)(b->pos - b->buf) < b->len) ||
                ((u_char)(b->pos - b->buf) == b->len && b->bits_remain == 8));
        return out;
}


/* VDVI translations as defined in draft-ietf-avt-profile-new-00.txt 

DVI4  VDVI        VDVI  VDVI
c/w   c/w         hex   rel. bits
_________________________________
  0          00    00    2
  1         010    02    3
  2        1100    0c    4
  3       11100    1c    5
  4      111100    3c    6
  5     1111100    7c    7
  6    11111100    fc    8
  7    11111110    fe    8
  8          10    02    2
  9         011    03    3
  10       1101    0d    4
  11      11101    1d    5
  12     111101    3d    6
  13    1111101    7d    7
  14   11111101    fd    8
  15   11111111    ff    8
*/

static u_int dmap[16]   = { 0x00, 0x02, 0x0c, 0x1c,
                            0x3c, 0x7c, 0xfc, 0xfe,
                            0x02, 0x03, 0x0d, 0x1d,
                            0x3d, 0x7d, 0xfd, 0xff
};

static int dmap_bits[16] = {   2,    3,    4,    5,
                               6,    7,    8,    8,
                               2,    3,    4,    5,
                               6,    7,    8,    8
};

int
vdvi_encode(u_char *dvi_buf, int dvi_samples, u_char *out, int out_bytes)
{
        register u_char s1, s2;
        u_char *dvi_end, *dp, t;
        bs dst;
        int bytes_used;

        assert(dvi_samples == VDVI_SAMPLES_PER_FRAME);
        
        memset(out, 0, out_bytes);

        bs_init(&dst, out, out_bytes);

        dvi_end = dvi_buf + dvi_samples / 2;
        dp      = dvi_buf;
        while (dp != dvi_end) {
                t = *dp;
                s1 = (*dp  & 0xf0) >> 4;
                s2 = (*dp  & 0x0f);
                bs_put(&dst, (u_char)dmap[s1], dmap_bits[s1]);
                bs_put(&dst, (u_char)dmap[s2], dmap_bits[s2]);
                assert(*dp == t);
                dp ++;
        }
        /* Return number of bytes used */
        bytes_used = (dst.pos - dst.buf) + (dst.bits_remain != 8) ? 1 : 0;
        assert(bytes_used <= out_bytes);
        return bytes_used;
}

int /* Returns number of bytes in in_bytes used to generate dvi_samples */
vdvi_decode(unsigned char *in, int in_bytes, unsigned char *dvi_buf, int dvi_samples)
{
        bs bout;
        bs bin;
        u_char cw, cb;
        u_int i;
        int bytes_used;
        
        /* This code is ripe for optimization ... */

        assert(dvi_samples == VDVI_SAMPLES_PER_FRAME);

        memset(dvi_buf, 0, dvi_samples / 2);

        bs_init(&bin, in, in_bytes);
        bs_init(&bout, dvi_buf, dvi_samples / 2);
        
        while(dvi_samples) {
#ifdef TEST_DVI
                check_padding();
#endif
                cb = 2;
                cw = bs_get(&bin, 2);
                do {
                        for(i = 0; i < 16; i++) {
                                if (dmap_bits[i] != cb) continue;
                                if (dmap[i] == cw) goto dvi_out_pack;
                        }
                        cb++;
                        cw <<=1;
                        cw |= bs_get(&bin, 1);
                        assert(cb <= 8);
#ifdef TEST_DVI
                check_padding();
#endif
                } while(1);
        dvi_out_pack:
#ifdef TEST_DVI
                check_padding();
#endif
                bs_put(&bout, (u_char)i, 4);
                dvi_samples--;
#ifdef TEST_DVI
                check_padding();
#endif

        }

        bytes_used = (bin.pos - bin.buf) + (bin.bits_remain != 8) ? 1 : 0;
        assert(bytes_used <= in_bytes);
        return bytes_used;
}

#ifdef TEST_BS
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include "assert.h"
#define NUM_TESTS 1000
#define TEST_SIZE 10000

int main()
{
        int i,n;
        u_char tmp[TEST_SIZE];
        u_int  src[TEST_SIZE];
        u_int  bits_used ;
        u_int  d;
        struct rusage r1, r2;

        bs     b;
        getrusage(0, &r1);
        for(n = 0; n < NUM_TESTS; n++) {
                srand(n * 2510101); 

                memset(tmp, 0, TEST_SIZE);
                bs_init(&b, tmp, TEST_SIZE);
                bits_used = 0;
                for(i = 0; i < TEST_SIZE; i++) {
                        src[i] = random() & 0x0f;
                        bs_put(&b, dmap[src[i]], dmap_bits[src[i]]);
/*                printf("%2d %03d.%02d 0x%02x %d\n", i, bits_used / 8, bits_used % 8, 
                  dmap[src[i]] ,dmap_bits[src[i]]);
                  */
                        bits_used += dmap_bits[src[i]];
                        assert(b.bits_remain == (8 - (bits_used % 8)));
                }
                /* rewind bs */
                bs_init(&b, tmp, TEST_SIZE);
                bits_used = 0;
                for(i = 0; i < TEST_SIZE; i++) {
                        d = bs_get(&b, dmap_bits[src[i]]);
                        bits_used += dmap_bits[src[i]];
                        assert(d == dmap[src[i]]);
                        assert(b.bits_remain == (8 - (bits_used % 8)));
                }
        }
        getrusage(0, &r2);
        printf("%u samples took %u us(u) %u us(s)\n",
               NUM_TESTS * TEST_SIZE,
               (r2.ru_utime.tv_sec - r1.ru_utime.tv_sec) * 1000000 + r2.ru_utime.tv_usec - r1.ru_utime.tv_usec,
               (r2.ru_stime.tv_sec - r1.ru_stime.tv_sec) * 1000000 + r2.ru_stime.tv_usec - r1.ru_stime.tv_usec);

        return 0;
}

#endif TEST_BS




