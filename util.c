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

#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "util.h"

#ifdef DEBUG_MEM
#define MAX_ADDRS 65536

typedef struct s_alloc_blk {
    int  	*addr;    /* address */
    char 	*filen;   /* file where allocated */
    int   	 line;    /* line where allocated */
    size_t  	 length;  /* size of allocation   */
    int   	 blen;    /* size passed to block_alloc (if relevent) */
    int   	 est;     /* order in all allocation's */
} alloc_blk;

static alloc_blk mem_item[MAX_ADDRS];

static int   naddr = 0;
static int   tick  = 0;
static int   init  = 0;
#endif

void xdoneinit(void) 
{
#ifdef DEBUG_MEM
	init = tick++;
#endif
}
	
void xmemchk(void)
{
#ifdef DEBUG_MEM
	int i,j;
	if (naddr > MAX_ADDRS) {
		printf("ERROR: Too many addresses for xmemchk()!\n");
		abort();
	}
	for (i=0; i<naddr; i++) {
		assert(mem_item[i].addr != NULL);
		assert(strlen(mem_item[i].filen) == mem_item[i].length);
		if (*mem_item[i].addr != *(mem_item[i].addr + (*mem_item[i].addr / 4) + 2)) {
			printf("Memory check failed!\n");
			j = i;
			for (i=0; i<naddr; i++) {
				if (i==j) {                        /* Because occasionally you can't spy the */
					printf("* ");              /* deviant block, we'll highlight it.     */
				} else {
					printf("  ");
				}
				printf("addr: %p", mem_item[i].addr);                                           fflush(stdout);
				printf("  size: %6d", *mem_item[i].addr);                                       fflush(stdout);
				printf("  size: %6d", *(mem_item[i].addr + (*mem_item[i].addr / 4) + 2));	fflush(stdout);
                                printf("  age: %6d", tick - mem_item[i].est);                                   fflush(stdout);
				printf("  file: %s", mem_item[i].filen);					fflush(stdout);
				printf("  line: %d", mem_item[i].line);   					fflush(stdout);
				printf("\n");
			}
			abort();
		}
	}
#endif
}

void xmemdmp(void)
{
#ifdef DEBUG_MEM
	int i;
        block_release_all();
	if (naddr > MAX_ADDRS) {
		printf("ERROR: Too many addresses for xmemchk()!\n");
		abort();
	}
        for (i=0; i<naddr; i++) {
            printf("%5d",i);                              fflush(stdout);
            printf("  addr: %p", mem_item[i].addr);       fflush(stdout);
            printf("  size: %5d", *mem_item[i].addr);     fflush(stdout);
            printf("  age: %6d", tick - mem_item[i].est); fflush(stdout);
            printf("  file: %s", mem_item[i].filen);      fflush(stdout);
            printf(":%d", mem_item[i].line);              fflush(stdout);
            if (mem_item[i].blen != 0) { 
                printf("  \tblen %d", mem_item[i].blen);   
                fflush(stdout);
            }
            printf("\n");
        }
	printf("Program initialisation finished at age %6d\n", tick-init);
#endif
}

void xfree(void *y)
{
#ifdef DEBUG_MEM
	char	*x = (char *) y;
	int	 i, j;
	int	*a;

	if (x == NULL) {
		printf("ERROR: Attempt to free NULL pointer!\n");
		abort();
	}

	j = 0;
	for (i = 0; i < naddr - j; i++) {
		if (mem_item[i].addr == (int *)(x-8)) {
			if (j != 0) {
				printf("ERROR: Attempt to free memory twice! (addr=%p)\n", y);
				abort();
			}
			j = 1;
			/* Trash the contents of the memory we're about to free... */
                        a = (int*) (x-8);
			for (a = (int *) (x-8); a < (int *) (a + *(x-8) + 8); a++) {
				*a = 0xdeadbeef;
			}
			/* ...and free it! */
			free(x - 8);
			free(mem_item[i].filen);
		}
                memmove(mem_item+i,mem_item+i+j,sizeof(alloc_blk));
	}
	if (j != 1) {
		printf("ERROR: Attempt to free memory which was never allocated! (addr=%p)\n", y);
		abort();
	}
	naddr -= j;
	xmemchk();
#else
	free(y);
#endif
}

void *
_xmalloc(unsigned size, const char *filen, int line)
{
#ifdef DEBUG_MEM
	int  s = (((size + 7)/8)*8) + 8;
	int *p = (int *) malloc(s + 16);
	int  q;

#ifdef NDEF
printf("malloc %d %s %d\n", size, filen, line);
#endif

	assert(p     != NULL);
	assert(filen != NULL);
/*	assert(strlen(filen) != 0); */
	*p = s;
	*(p+(s/4)+2) = s;
	for (q = 0; q < s/4; q++) {
		*(p+q+2) = rand();
	}
	mem_item[naddr].addr   = p;
	mem_item[naddr].filen  = strdup(filen);
	mem_item[naddr].line   = line;
	mem_item[naddr].length = strlen(filen);
        mem_item[naddr].est    = tick++;
        
	if (++naddr >= MAX_ADDRS) {
		printf("ERROR: Allocated too much! Table overflow in xmalloc()\n");
		printf("       Do you really need to allocate more than %d items?\n", MAX_ADDRS);
		abort();
	}

	xmemchk();
	assert(((char *) (p+2)) != NULL);
	return (void *) (p+2);
#else
        UNUSED(filen);
        UNUSED(line);
        
	return (void *) malloc(size);
#endif
}

char *_xstrdup(const char *s1, const char *filen, int line)
{
	char 	*s2;
  
	s2 = (char *) _xmalloc(strlen(s1)+1, filen, line);
	if (s2 != NULL)
		strcpy(s2, s1);
	return (s2);
}

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
#ifdef DEBUG_MEM
                mem_item[naddr].blen = size;
#endif
		p = (char *) _xmalloc(INDEX_TO_SIZE(i) + 8,filen,line);
		*((int *)p) = INDEX_TO_SIZE(i);
		p += 8;
                blocks_alloced++;
	}
	c = (unsigned int *)((char *)p - 8);
	if (size > *c) {
		fprintf(stderr, "block_alloc: block is too small %d %d!\n", size, *c);
	}
	c++;
	*c = size;
	assert(p != NULL);
	return (void*)p;
}

void
block_trash_chk()
{
#ifdef DEBUG_MEM
        int i,n;
        block *b;
        for(i = 0; i<MAX_INDEX;i++) {
                b = blocks[i];
                n = 0;
                while(b) {
                        b = b->next;
                        assert(n++ < blocks_alloced);
                }
        }
#endif
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
#endif
	((block *)p)->next = blocks[i];
	blocks[i] = (block *)p;
        block_trash_chk();
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

void _dprintf(const char *format, ...)
{
#ifdef DEBUG
        va_list ap;
 
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
#else
        UNUSED (format);
#endif
}

