/*
 * Copyright (c) 1993 Regents of the University of California.
 * All rights reserved.
 *
 */

/*
 * LBL random number generator.
 *
 * Written by Steve McCanne & Chris Torek (mccanne@ee.lbl.gov,
 * torek@ee.lbl.gov), November, 1992.
 *
 * This implementation is based on ``Two Fast Implementations of
 * the "Minimal Standard" Random Number Generator", David G. Carta,
 * Communications of the ACM, Jan 1990, Vol 33 No 1.
 */

#include "crypt_random.h"

static int randseed = 1;

void
lbl_srandom(int seed)
{
	randseed = seed;
}

int
lbl_random(void)
{
	register int x = randseed;
	register int hi, lo, t;

	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t <= 0)
		t += 0x7fffffff;
	randseed = t;
	return (t);
}

