/*
 * These are for the RAT project:
 * $Revision$
 * $Date$
 */

/*
** adpcm.h - include file for adpcm coder.
**
** Version 1.0, 7-Jul-92.
*/

#ifndef _ADPCM_H_
#define _ADPCM_H_

struct adpcm_state {
    short valprev;		/* Previous output value */
    unsigned char index;	/* Index into stepsize table */
    unsigned char pad;
};

void adpcm_coder(const short*, unsigned char*, int, struct adpcm_state *);
void adpcm_decoder(const unsigned char*, short*, int, struct adpcm_state *);

#endif /* _ADPCM_H_ */
