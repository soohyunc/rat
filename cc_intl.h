/*
 * FILE: cc_intl.h
 * PROGRAM: RAT / interleaver
 * AUTHOR: Orion Hodson
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-97 University College London
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

#ifndef __INTERLEAVE_H_
#define __INTERLEAVE_H_
struct s_intl_coder;
struct session_tag;
struct rx_element_tag;

struct s_intl_coder *new_intl_coder();
int  intl_config(struct session_tag    *sp,
                 struct s_intl_coder   *s,
                 char                  *cmd);
void intl_qconfig(struct session_tag   *sp,
                  struct s_intl_coder  *s, 
                  char                 *buf, 
                  unsigned int          blen);
int  intl_encode(struct session_tag    *sp,
                sample                 *raw,
                cc_unit                *cu,
                struct s_intl_coder    *s);
int  intl_bps(struct session_tag       *sp, 
             struct s_intl_coder       *s);
void intl_decode(struct rx_element_tag *rx,
                 struct s_intl_coder   *s);
int  intl_valsplit(char                *blk,
                   unsigned int         blen,
                   cc_unit             *cu,
                   int                 *trailing);
int  intl_wrapped_pt(char         *blk,
                     unsigned int  blen);
void intl_reset(struct s_intl_coder    *s);

#endif /* __INTERLEAVE_H_ */


