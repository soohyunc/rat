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

#include "config.h"
#include "util.h"

#ifdef DEBUG_MEM
#define MAX_ADDRS 65536
static int  *addrs[MAX_ADDRS];
static char *files[MAX_ADDRS];
static int   lines[MAX_ADDRS];
static int   length[MAX_ADDRS];
static int   naddr = 0;
#endif

void xmemchk(void)
{
#ifdef DEBUG_MEM
	int i,j;
	if (naddr > MAX_ADDRS) {
		printf("ERROR: Too many addresses for xmemchk()!\n");
		abort();
	}
	for (i=0; i<naddr; i++) {
		assert(addrs[i] != NULL);
		assert(strlen(files[i]) == length[i]);
		if (*addrs[i] != *(addrs[i] + (*addrs[i] / 4) + 2)) {
			printf("Memory check failed!\n");
			j = i;
			for (i=0; i<naddr; i++) {
				if (i==j) {                        /* Because occasionally you can't spy the */
					printf("* ");              /* deviant block, we'll highlight it.     */
				} else {
					printf("  ");
				}
				printf("addr: %p", addrs[i]);   				fflush(stdout);
				printf("  size: %6d", *addrs[i]);				fflush(stdout);
				printf("  size: %6d", *(addrs[i] + (*addrs[i] / 4) + 2));	fflush(stdout);
				printf("  file: %s", files[i]);					fflush(stdout);
				printf("  line: %d", lines[i]);					fflush(stdout);
				printf("\n");
			}
			abort();
		}
	}
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
		if (addrs[i] == (int *)(x-8)) {
			if (j != 0) {
				printf("ERROR: Attempt to free memory twice! (addr=%p)\n", y);
				abort();
			}
			j = 1;
			/* Trash the contents of the memory we're about to free... */
			for (a = (int *) (x-8); a < (int *) (a + *(x-8) + 8); a++) {
				*a = 0xdeadbeef;
			}
			/* ...and free it! */
			free(x - 8);
			free(files[i]);
		}
		addrs[i] = addrs[i + j];
		files[i] = files[i + j];
		lines[i] = lines[i + j];
		length[i] = length[i + j];
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

char *
_xmalloc(unsigned size, char *filen, int line)
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
	addrs[naddr]  = p;
	files[naddr]  = strdup(filen);
	lines[naddr]  = line;
	length[naddr] = strlen(filen);
	if (++naddr >= MAX_ADDRS) {
		printf("ERROR: Allocated too much! Table overflow in xmalloc()\n");
		printf("       Do you really need to allocate more than %d items?\n", MAX_ADDRS);
		abort();
	}
	xmemchk();
	assert(((char *) (p+2)) != NULL);
	return (char *) (p+2);
#else
	return (char *) malloc(size);
#endif
}

char *_xstrdup(char *s1, char *filen, int line)
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
 
#define BLOCK_SIZE            7
#define SIZE_TO_INDEX(s)      (((s) - 1) >> BLOCK_SIZE)
#define INDEX_TO_SIZE(i)      (((i) + 1) << BLOCK_SIZE)
#define MAX_SIZE              (1 << 17)
#define MAX_INDEX             SIZE_TO_INDEX(MAX_SIZE)
 
static block  *blocks[MAX_INDEX];
 
char *
_block_alloc(unsigned size, char *filen, int line)
{
	int     i, *c;
	char    *p;

	assert(size > 0);
	assert(size < MAX_SIZE);
 
	i = SIZE_TO_INDEX(size);
 
	if (blocks[i] != NULL) {
		p = (char *)blocks[i];
		blocks[i] = blocks[i]->next;
	} else {
		p = _xmalloc(INDEX_TO_SIZE(i) + 8,filen,line);
		*((int *)p) = INDEX_TO_SIZE(i);
		p += 8;
	}
 
	c = (int *)((char *)p - 8);
	if (size > *c) {
		fprintf(stderr, "block_alloc: block is too small %d %d!\n", size, *c);
	}
	c++;
	*c = size;
 
	assert(p != NULL);
	return (p);
}
 
void
block_free(void *p, int size)
{
	int     i, *c;
 
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
	((block *)p)->next = blocks[i];
	blocks[i] = (block *)p;
}

