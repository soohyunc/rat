/*
 * FILE:    vdvi.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998-99 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "cx_vdvi.h"

#ifdef TEST_VDVI
#define NUM_TESTS 100000

static  u_char src[80], pad1[4], 
        dst[80], pad2[4], 
        coded[160], pad3[4], safe[80];

void
check_padding()
{
        assert(pad1[0] == 0xff && pad2[0] == 0xff && pad3[0] == 0xff);
        assert(pad1[1] == 0xff && pad2[1] == 0xff && pad3[1] == 0xff);
        assert(pad1[2] == 0xff && pad2[2] == 0xff && pad3[2] == 0xff);
        assert(pad1[3] == 0xff && pad2[3] == 0xff && pad3[3] == 0xff);
}

int main()
{
        int i, n, coded_len, out_len, a, amp;

        memset(pad1, 0xff, 4); /* Memory overwrite test */
        memset(pad2, 0xff, 4);
        memset(pad3, 0xff, 4);

        srandom(123213);

        for(n = 0; n < NUM_TESTS; n++) {
                amp = (random() &0x0f);
                for(i = 0; i< 80; i++) {
                        a = (int)(amp * sin(M_PI * 2.0 * (float)i/16.0));
                        assert(abs(a) < 16);
                        src[i] = (a << 4) & 0xf0;
                        a = amp;
                        assert(abs(a) < 16);
                        src[i] |= (a & 0x0f);
                }

                memcpy(safe, src, 80);

                coded_len = vdvi_encode(src, 160, coded, 160);

                assert(!memcmp(src,safe,80));

                check_padding();
                out_len   = vdvi_decode(coded, 160, dst, 160);
                
                assert(!memcmp(src,safe,80));
                assert(!memcmp(dst,safe,80)); /* dst matches sources */

                assert(coded_len == out_len);

                check_padding();

                for(i = 0; i< 80; i++) {
                        assert(src[i] == dst[i]);
                }
                if (0 == (n % 1000)) {
                        printf(".");
                        fflush(stdout);
                }
        }
        printf("\nTested %d frames\n", n);
        return 1;
}
#endif /* TEST_DVI */

/* Bitstream structure to make life a little easier */

typedef struct {
        u_char *buf;    /* head of bitstream            */
        u_char *pos;    /* current byte in bitstream    */
        u_int   remain; /* bits remaining               */
        u_int   len;    /* length of bitstream in bytes */
} bs;

__inline static void
bs_init(bs *b, u_char *buf, int blen)
{
        b->buf    = b->pos = buf;
        b->remain = 8;
        b->len    = blen;
}

__inline static void
bs_put(bs* b, u_char bits, u_int nbits)
{
        assert(nbits != 0 && nbits <= 8);
        
        if (b->remain == 0) {
                b->pos++;
                b->remain = 8;
        }

        if (nbits > b->remain) {
                u_int over = nbits - b->remain;
                (*b->pos) |= (bits >> over);
                b->pos++;
                b->remain = 8 - over;
                (*b->pos)  = (bits << b->remain);
        } else {
                (*b->pos) |= bits << (b->remain - nbits);
                b->remain -= nbits;
        }
        
        assert((u_int)(b->pos - b->buf) <= b->len);
}

__inline static u_char
bs_get(bs *b, u_int nbits)
{
        u_char out;

        if (b->remain == 0) {
                b->pos++;
                b->remain = 8;
        }

        if (nbits > b->remain) {
                /* Get high bits */
                out = *b->pos;
                out <<= (8 - b->remain);
                out >>= (8 - nbits);
                b->pos++;
                b->remain += 8 - nbits;
                out |= (*b->pos) >> b->remain;
        } else {
                out = *b->pos;
                out <<= (8 - b->remain);
                out >>= (8 - nbits);
                b->remain -= nbits;
        }

        assert((u_int)(b->pos - b->buf) <= b->len);
        return out;
}

static u_int 
bs_used(bs *b)
{
        u_int used = (u_int)(b->pos - b->buf);
        if (b->remain != 8) {
                used++;
        }
        return used;
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

        /* Worst case is 8 bits per sample -> VDVI_SAMPLES_PER_FRAME */
        assert(out_bytes   == VDVI_SAMPLES_PER_FRAME); 

        memset(out, 0, out_bytes);
        bs_init(&dst, out, out_bytes);
        dvi_end = dvi_buf + dvi_samples / 2;
        dp      = dvi_buf;
        while (dp != dvi_end) {
                t = *dp;
                s1 = (t & 0xf0) >> 4;
                s2 = (t & 0x0f);
                bs_put(&dst, (u_char)dmap[s1], dmap_bits[s1]);
                bs_put(&dst, (u_char)dmap[s2], dmap_bits[s2]);
                assert(*dp == t);
                dp ++;
        }
        /* Return number of bytes used */
        bytes_used  = bs_used(&dst);
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

        assert(in_bytes >= 40);
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

        bytes_used = bs_used(&bin);

        assert(bytes_used <= in_bytes);
        return bytes_used;
}

#ifdef TEST_BS
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

#endif /* TEST_BS */




