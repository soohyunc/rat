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

void lbl_srandom(int seed);
int  lbl_random(void);

