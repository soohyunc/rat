/*
 * FILE: util.c
 * PROGRAM: RAT
 * AUTHOR: Isidor Kouvelas + Colin Perkins + Mark Handley + Orion Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995,1996,1997 University College London
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
#include "debug.h"
#include "memory.h"
#include "util.h"

typedef struct s_block {
	struct s_block  *next;
} block;
 
#define BLOCK_SIZE            5
#define SIZE_TO_INDEX(s)      (((s) - 1) >> BLOCK_SIZE)
#define INDEX_TO_SIZE(i)      (((i) + 1) << BLOCK_SIZE)
#define MAX_SIZE              (1 << 17)
#define MAX_INDEX             SIZE_TO_INDEX(MAX_SIZE)
 
static block  *blocks[MAX_INDEX];
static int     blocks_alloced;

#ifdef DEBUG_MEM

/* Block tracking for when things are going wrong */

static struct iovec blk_out[1280];
static int          nblks_out;

static int
get_blk_idx_from_ptr(char *addr)
{
        int i;
        for(i = 0; i < nblks_out; i++) {
                if (blk_out[i].iov_base == addr) {
                        return i;
                }
        }
        return -1;
}
#endif /* DEBUG_MEM */
 
void *
_block_alloc(unsigned int size, const char *filen, int line)
{
	int         	 i;
	unsigned int 	*c;
	char        	*p;

	assert(size > 0);
	assert(size < MAX_SIZE);

	i = SIZE_TO_INDEX(size);
 
	if (blocks[i] != NULL) {
		p = (char *)blocks[i];
		blocks[i] = blocks[i]->next;
	} else {
#ifdef DEBUG_MEM_BREAK
                /* This can only go here if this file is merged with memory.c! [oh] */
                mem_item[naddr].blen = size;
#endif /* DEBUG_MEM_BREAK */
		p = (char *) _xmalloc(INDEX_TO_SIZE(i) + 8,filen,line);
		*((int *)p) = INDEX_TO_SIZE(i);
		p += 8;
                blocks_alloced++;
	}
	c = (unsigned int *)((char *)p - 8);
	if (size > *c) {
		fprintf(stderr, "block_alloc: block is too small %d %d!\n", size, *c);
	}
#ifdef DEBUG_MEM
        blk_out[nblks_out].iov_base = (char*)c;
        blk_out[nblks_out].iov_len  = size;
        nblks_out++;
#endif /* DEBUG_MEM */
	c++;
	*c = size;
	assert(p != NULL);
	return (void*)p;
}

void
block_trash_check()
{
#ifdef DEBUG_MEM
        int i,n,*c;
        block *b;

        for(i = 0; i<MAX_INDEX;i++) {
                b = blocks[i];
                n = 0;
                while(b) {
                        b = b->next;
                        assert(n++ < blocks_alloced);
                }
        }
        for (i = 0; i<nblks_out; i++) {
                c = (int*)blk_out[i].iov_base;
                c++;
                assert((unsigned int)*c == (unsigned int)blk_out[i].iov_len);
        }
#endif  /* DEBUG_MEM */
}

void
block_check(char *p)
{
#ifdef DEBUG_MEM
        char *blk_base;
        assert(p!=NULL);
        blk_base = p-8;
        assert(get_blk_idx_from_ptr(blk_base) != -1);
#endif /* DEBUG_MEM */
        UNUSED(p);
}
 
void
_block_free(void *p, int size, int line)
{
	int     i, *c;
#ifdef DEBUG
        block *bp;
        int    n;
#endif
        UNUSED(line);

	c = (int *)((char *)p - 8);

#ifdef DEBUG_MEM
        n = get_blk_idx_from_ptr((char*)c);
        if (n == -1) {
                debug_msg("Freeing block (addr 0x%x, %d bytes) that was not allocated with block_alloc(?)\n", (int)c, *(c+1));
                xfree(c);
                assert(n != -1);
        }
#endif

	if (size > *c) {
		fprintf(stderr, "block_free: block was too small! %d %d\n", size, *c);
	}
	c++;
	if (size != *c) {
		fprintf(stderr, "block_free: Incorrect block size given! %d %d\n", size, *c);
		assert(size == *c);
	}
 
	i = SIZE_TO_INDEX(size);
#ifdef DEBUG
        bp = blocks[i];
        n = 0;
        while(bp) {
                if (bp == (block*)p) {
                        debug_msg("already freed line %d\n", *((int *)p+1));
                        assert(0);
                }
                bp = bp->next;
                n++;
        }
        if (i >= 4) {
                *((int*)p+1) = line;
        }
#endif /* DEBUG */
	((block *)p)->next = blocks[i];
	blocks[i] = (block *)p;
#ifdef DEBUG_MEM
        c--;
        i = get_blk_idx_from_ptr((char*)c);
        assert(i != -1);
        memmove(blk_out+i, blk_out + i + 1, sizeof(struct iovec) * (nblks_out - i)); 
        nblks_out --;
#endif /* DEBUG_MEM */
}

void
block_release_all(void)
{
    int i;
    char *p,*q;

    printf("Freeing memory: "); fflush(stdout);
    for(i=0;i<MAX_INDEX;i++) {
        p = (char*)blocks[i];
        while(p) {
            q = (char*)((block*)p)->next;
            xfree(p-8);
	    printf("+"); fflush(stdout);
            p = q;
        }
    }
    printf("\n");
}

void
purge_chars(char *src, char *to_go)
{
        char *r, *w;
        r = w = src;
        debug_msg("Input string: \"%s\"\n", src);
        while(*r) {
                *w = *r;
                if (!strchr(to_go, (int)*r)) {
                        w++;
                }
                r++;
        }
        *w = '\0';
        debug_msg("Output string: \"%s\"\n", src);
}
