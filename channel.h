/*
 * FILE:    channel.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 * 
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1995-98 University College London
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

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <sys/types.h>
#include <sys/uio.h>
#include "rat_types.h"

#define CC_UNITS 20

struct session_tag;
struct s_coded_unit;
struct s_cc_state;
struct s_cc;
struct rx_element_tag;

typedef struct s_cc_unit {
    struct s_cc_coder *cc;
    struct iovec iov[CC_UNITS];
    int          iovc;
    int          src_pt;   /* used by vanilla channel coder to know source coding*/
} cc_unit;

typedef void (*cc_init_f)(struct session_tag *sp, 
                          struct s_cc_state *ccs);
typedef int  (*cc_config_f)(struct session_tag *sp, 
                            struct s_cc_state *ccs, 
                            char *cmd);
typedef void (*cc_query_f)(struct session_tag *sp,
                           struct s_cc_state  *ccs,
                           char *buf,
                           unsigned int       blen);
typedef int  (*cc_bitrate_f)(struct session_tag *sp,
                             struct s_cc_state *ccs);
typedef int  (*cc_encode_f)(struct session_tag *sp, 
                            sample             *raw, 
                            struct s_cc_unit   *cu, 
                            struct s_cc_state  *ccs);
typedef void (*cc_enc_reset_f)(struct s_cc_state *ccs);
typedef int  (*cc_valsplit_f)(char *blk, 
                              unsigned int  blen, 
                              struct s_cc_unit *cu,
                              int *trailing);
typedef int  (*cc_get_pt_f)(char *blk,
                            unsigned int blen);
typedef void (*cc_dec_init_f)(struct session_tag *sp, struct s_cc_state *ccs);
typedef void (*cc_decode_f)(struct rx_element_tag *sp,
                            struct s_cc_state *ccs);

typedef struct s_cc_coder {
    char            *name;
    int              pt;
/* encoder related */
    cc_init_f        enc_init;
    cc_config_f      config;
    cc_query_f       query;
    cc_bitrate_f     bitrate;
    cc_encode_f      encode;
    cc_enc_reset_f   enc_reset;
/* decoder related */
    int              max_cc_per_interval;
    cc_valsplit_f    valsplit; 
    cc_get_pt_f      get_wrapped_pt;
    cc_dec_init_f    dec_init;
    cc_decode_f      decode;
} cc_coder_t;

#define MAX_CC_PER_INTERVAL 2

void  set_units_per_packet(struct session_tag *sp, 
                           int n);
int   get_units_per_packet(struct session_tag *sp);
int   get_bytes(cc_unit *u);
int   get_cc_pt(struct session_tag *sp, 
                char *name);
int   set_cc_pt(char *name, 
                int pt);
struct s_cc_coder *
      get_channel_coder(int pt);
int   channel_code(struct session_tag *sp, 
                   cc_unit *u, 
                   int pt, 
                   sample *raw);
void  channel_decode(struct rx_element_tag *rx);
void  config_channel_coder(struct session_tag *sp, 
                           int pt, 
                           char *cmd);
void  query_channel_coder(struct session_tag *sp, 
                          int pt, 
                          char *buf, 
                          unsigned int blen);
void  get_bitrate(struct session_tag *sp, 
                  int pt);
int   validate_and_split(int pt, 
                         char *data, 
                         unsigned int data_len, 
                         cc_unit *u, 
                         int *trailing);
int   get_wrapped_payload(int pt, 
                          char *data, 
                          int data_len); 
void  reset_channel_encoder(struct session_tag *sp, 
                            int cc_pt);
void  clear_cc_unit(cc_unit *u, 
                    int begin);


/* fn's only for use by channel decoders */

int   add_comp_data(struct rx_element_tag *u, 
                    int pt, 
                    struct iovec *iov,
                    int iovc);

struct rx_element_tag *get_rx_unit(int n, 
                                   int cc_pt, 
                                   struct rx_element_tag *u);

/* defines for coded_unit_to_iov */

#define INCLUDE_STATE    1
#define NO_INCLUDE_STATE 0

int    coded_unit_to_iov(coded_unit   *cu, 
                         struct iovec *iov,
                         int inc_state); 

int    iov_to_coded_unit(struct iovec *iov,  
                         coded_unit   *cu, 
                         int pt);
#endif



