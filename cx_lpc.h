/*
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

#ifndef _LPC_H_
#define _LPC_H_

#define FRAMESIZE 160
#define BUFLEN   ((FRAMESIZE * 3) / 2)

#define LPC_FILTORDER		10

/* lpc transmitted state */

typedef struct {
	unsigned short period;
	unsigned char gain;
	char k[LPC_FILTORDER];
	char pad;
} lpc_txstate_t;

#define LPCTXSIZE 14

/*
 * we can't use 'sizeof(lpcparams_t)' because some compilers
 * add random padding so define size of record that goes over net.
 */

/* lpc decoder internal state */
typedef struct {
	double Oldper, OldG, Oldk[LPC_FILTORDER + 1], bp[LPC_FILTORDER + 1];
	int pitchctr;
} lpc_intstate_t;

/* Added encoder state by removing static buffers
 * that are used for storing float represetantions
 * of audio from previous frames.  Multiple coders
 * will no longer interfere. It's all filter state.
 */
typedef struct {
        float u, u1, yp1, yp2;
        float raw[BUFLEN];
        float filtered[BUFLEN];
} lpc_encstate_t;

void lpc_init(void);
void lpc_enc_init(lpc_encstate_t* state);
void lpc_dec_init(lpc_intstate_t* state);
void lpc_analyze(const short *buf, lpc_encstate_t *enc, lpc_txstate_t *params);
void lpc_synthesize(short *buf, lpc_txstate_t *params, lpc_intstate_t* state);
void lpc_extend_synthesize(short *buf, int len, lpc_intstate_t* state);

#endif /* _LPC_H_ */



