/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header$ */

#include <stdio.h>
#include "assert.h"
#include "gsm.h"

/*
 *  4.3 FIXED POINT IMPLEMENTATION OF THE RPE-LTP DECODER
 */

static void Postprocessing(struct gsm_state *S, register word *s)
{
	register int		k;
	register word		msr = S->msr;
	register longword	ltmp;	/* for GSM_ADD */
	register word		tmp;

	for (k = 160; k--; s++) {
		tmp = GSM_MULT_R( msr, 28180 );
		msr = (word) GSM_ADD(*s, tmp);  	   /* Deemphasis 	     */
		*s  = GSM_ADD(msr, msr) & 0xFFF8;  /* Truncation & Upscaling */
	}
	S->msr = msr;
}

void Gsm_Decoder(
	struct gsm_state	* S,

	word		 * LARcr,	/* [0..7]		IN	*/

	word		 * Ncr,		/* [0..3] 		IN 	*/
	word		 * bcr,		/* [0..3]		IN	*/
	word		 * Mcr,		/* [0..3] 		IN 	*/
	word		 * xmaxcr,	/* [0..3]		IN 	*/
	word		 * xMcr,		/* [0..13*4]		IN	*/
 
	word		 * s		/* [0..159]		OUT 	*/
)
{
	int		 j, k;
	word		 erp[40], wt[160];
	word	 	 *drp = S->dp0 + 120;

	for (j=0; j <= 3; j++, xmaxcr++, bcr++, Ncr++, Mcr++, xMcr += 13) {
		Gsm_RPE_Decoding(*xmaxcr, *Mcr, xMcr, erp);
		Gsm_Long_Term_Synthesis_Filtering(S, *Ncr, *bcr, erp, drp);
		for (k = 0; k <= 39; k++) {
			wt[ j * 40 + k ] =  drp[ k ];
		}
	}

	Gsm_Short_Term_Synthesis_Filter(S, LARcr, wt, s);
	Postprocessing(S, s);
}

/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header$ */

int gsm_decode(gsm s, gsm_byte * c, gsm_signal * target)
{
	word  	LARc[8], Nc[4], Mc[4], bc[4], xmaxc[4], xmc[13*4];

	/* GSM_MAGIC  = (*c >> 4) & 0xF; */

	if (((*c >> 4) & 0x0F) != GSM_MAGIC) return -1;

	LARc[0]  = (*c++ & 0xF) << 2;		/* 1 */
	LARc[0] |= (*c >> 6) & 0x3;
	LARc[1]  = *c++ & 0x3F;
	LARc[2]  = (*c >> 3) & 0x1F;
	LARc[3]  = (*c++ & 0x7) << 2;
	LARc[3] |= (*c >> 6) & 0x3;
	LARc[4]  = (*c >> 2) & 0xF;
	LARc[5]  = (*c++ & 0x3) << 2;
	LARc[5] |= (*c >> 6) & 0x3;
	LARc[6]  = (*c >> 3) & 0x7;
	LARc[7]  = *c++ & 0x7;
	Nc[0]  = (*c >> 1) & 0x7F;
	bc[0]  = (*c++ & 0x1) << 1;
	bc[0] |= (*c >> 7) & 0x1;
	Mc[0]  = (*c >> 5) & 0x3;
	xmaxc[0]  = (*c++ & 0x1F) << 1;
	xmaxc[0] |= (*c >> 7) & 0x1;
	xmc[0]  = (*c >> 4) & 0x7;
	xmc[1]  = (*c >> 1) & 0x7;
	xmc[2]  = (*c++ & 0x1) << 2;
	xmc[2] |= (*c >> 6) & 0x3;
	xmc[3]  = (*c >> 3) & 0x7;
	xmc[4]  = *c++ & 0x7;
	xmc[5]  = (*c >> 5) & 0x7;
	xmc[6]  = (*c >> 2) & 0x7;
	xmc[7]  = (*c++ & 0x3) << 1;		/* 10 */
	xmc[7] |= (*c >> 7) & 0x1;
	xmc[8]  = (*c >> 4) & 0x7;
	xmc[9]  = (*c >> 1) & 0x7;
	xmc[10]  = (*c++ & 0x1) << 2;
	xmc[10] |= (*c >> 6) & 0x3;
	xmc[11]  = (*c >> 3) & 0x7;
	xmc[12]  = *c++ & 0x7;
	Nc[1]  = (*c >> 1) & 0x7F;
	bc[1]  = (*c++ & 0x1) << 1;
	bc[1] |= (*c >> 7) & 0x1;
	Mc[1]  = (*c >> 5) & 0x3;
	xmaxc[1]  = (*c++ & 0x1F) << 1;
	xmaxc[1] |= (*c >> 7) & 0x1;
	xmc[13]  = (*c >> 4) & 0x7;
	xmc[14]  = (*c >> 1) & 0x7;
	xmc[15]  = (*c++ & 0x1) << 2;
	xmc[15] |= (*c >> 6) & 0x3;
	xmc[16]  = (*c >> 3) & 0x7;
	xmc[17]  = *c++ & 0x7;
	xmc[18]  = (*c >> 5) & 0x7;
	xmc[19]  = (*c >> 2) & 0x7;
	xmc[20]  = (*c++ & 0x3) << 1;
	xmc[20] |= (*c >> 7) & 0x1;
	xmc[21]  = (*c >> 4) & 0x7;
	xmc[22]  = (*c >> 1) & 0x7;
	xmc[23]  = (*c++ & 0x1) << 2;
	xmc[23] |= (*c >> 6) & 0x3;
	xmc[24]  = (*c >> 3) & 0x7;
	xmc[25]  = *c++ & 0x7;
	Nc[2]  = (*c >> 1) & 0x7F;
	bc[2]  = (*c++ & 0x1) << 1;		/* 20 */
	bc[2] |= (*c >> 7) & 0x1;
	Mc[2]  = (*c >> 5) & 0x3;
	xmaxc[2]  = (*c++ & 0x1F) << 1;
	xmaxc[2] |= (*c >> 7) & 0x1;
	xmc[26]  = (*c >> 4) & 0x7;
	xmc[27]  = (*c >> 1) & 0x7;
	xmc[28]  = (*c++ & 0x1) << 2;
	xmc[28] |= (*c >> 6) & 0x3;
	xmc[29]  = (*c >> 3) & 0x7;
	xmc[30]  = *c++ & 0x7;
	xmc[31]  = (*c >> 5) & 0x7;
	xmc[32]  = (*c >> 2) & 0x7;
	xmc[33]  = (*c++ & 0x3) << 1;
	xmc[33] |= (*c >> 7) & 0x1;
	xmc[34]  = (*c >> 4) & 0x7;
	xmc[35]  = (*c >> 1) & 0x7;
	xmc[36]  = (*c++ & 0x1) << 2;
	xmc[36] |= (*c >> 6) & 0x3;
	xmc[37]  = (*c >> 3) & 0x7;
	xmc[38]  = *c++ & 0x7;
	Nc[3]  = (*c >> 1) & 0x7F;
	bc[3]  = (*c++ & 0x1) << 1;
	bc[3] |= (*c >> 7) & 0x1;
	Mc[3]  = (*c >> 5) & 0x3;
	xmaxc[3]  = (*c++ & 0x1F) << 1;
	xmaxc[3] |= (*c >> 7) & 0x1;
	xmc[39]  = (*c >> 4) & 0x7;
	xmc[40]  = (*c >> 1) & 0x7;
	xmc[41]  = (*c++ & 0x1) << 2;
	xmc[41] |= (*c >> 6) & 0x3;
	xmc[42]  = (*c >> 3) & 0x7;
	xmc[43]  = *c++ & 0x7;			/* 30  */
	xmc[44]  = (*c >> 5) & 0x7;
	xmc[45]  = (*c >> 2) & 0x7;
	xmc[46]  = (*c++ & 0x3) << 1;
	xmc[46] |= (*c >> 7) & 0x1;
	xmc[47]  = (*c >> 4) & 0x7;
	xmc[48]  = (*c >> 1) & 0x7;
	xmc[49]  = (*c++ & 0x1) << 2;
	xmc[49] |= (*c >> 6) & 0x3;
	xmc[50]  = (*c >> 3) & 0x7;
	xmc[51]  = *c & 0x7;			/* 33 */

	Gsm_Decoder(s, LARc, Nc, bc, Mc, xmaxc, xmc, target);

	return 0;
}

