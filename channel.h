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

/* this is large because interleaver can make things large v. quickly */

#define CC_UNITS 80

struct session_tag;
struct s_coded_unit;
struct s_cc_state;
struct s_cc;
struct rx_element_tag;

typedef struct s_cc_unit {
        int                pt;
        struct s_cc_coder *cc;
        struct iovec       iov[CC_UNITS];
        int                iovc;
        int                hdr_idx;  /* index of header start */
        int                data_idx; /* index of data start   */
} cc_unit;

typedef void (*cc_init_f)      (struct session_tag *sp, struct s_cc_state *ccs);
typedef int  (*cc_config_f)    (struct session_tag *sp, struct s_cc_state *ccs, char *cmd);
typedef void (*cc_query_f)     (struct session_tag *sp, struct s_cc_state *ccs, char *buf, unsigned int blen);
typedef int  (*cc_bitrate_f)   (struct session_tag *sp, struct s_cc_state *ccs);
typedef int  (*cc_encode_f)    (struct session_tag *sp, cc_unit **in, int num_coded, struct s_cc_unit **out, struct s_cc_state *ccs);
typedef void (*cc_enc_reset_f) (struct s_cc_state *ccs);
typedef void (*cc_free_f)      (struct s_cc_state *ccs);
typedef int  (*cc_valsplit_f)  (char *blk, unsigned int blen, struct s_cc_unit *cu, int *trailing);
typedef int  (*cc_get_pt_f)    (char *blk, unsigned int blen);
typedef void (*cc_dec_init_f)  (struct session_tag *sp, struct s_cc_state *ccs);
typedef void (*cc_decode_f)    (struct rx_element_tag *sp, struct s_cc_state *ccs);

typedef struct s_cc_coder {
    char            *name;
    int              pt;
    int              cc_id;
/* encoder related */
    cc_init_f        enc_init;
    cc_config_f      config;
    cc_query_f       query;
    cc_bitrate_f     bitrate;
    cc_encode_f      encode;
    cc_enc_reset_f   enc_reset;
    cc_free_f        enc_free;
/* decoder related */
    int              max_cc_per_interval;
    cc_valsplit_f    valsplit; 
    cc_get_pt_f      get_wrapped_pt;
    cc_dec_init_f    dec_init;
    cc_decode_f      decode;
    cc_free_f        dec_free;
} cc_coder_t;

#define MAX_CC_PER_INTERVAL 2

int   get_bytes (cc_unit *u);
int   get_bps   (struct session_tag *sp, int pt);
int   get_cc_pt (struct session_tag *sp, char *name);
int   set_cc_pt (char *name, int pt);

struct s_cc_coder *get_channel_coder (int pt);

void  channel_set_coder     (struct session_tag *sp, int cc_pt);
int   channel_encode        (struct session_tag *sp, int pt, cc_unit **coded, int num_coded, cc_unit **out);
void  channel_decode        (struct rx_element_tag *rx);
void  channel_encoder_reset (struct session_tag *sp, int cc_pt);

void  config_channel_coder (struct session_tag *sp, int pt, char *cmd);
void  query_channel_coder  (struct session_tag *sp, int pt, char *buf, unsigned int blen);

void  get_bitrate (struct session_tag *sp, int pt);

int   validate_and_split    (int pt, char *data, unsigned int data_len, cc_unit *u, int *trailing);
int   get_wrapped_payload   (int pt, char *data, int data_len); 

void  clear_cc_unit         (cc_unit *u, int begin);

void  clear_cc_encoder_states (struct s_cc_state **list);
void  clear_cc_decoder_states (struct s_cc_state **list);

struct s_collator;
struct s_collator* collator_create     (void);
void               collator_destroy    (struct s_collator *c);
cc_unit*           collate_coded_units (struct s_collator *c, coded_unit *cu, int enc_no);
void               collator_set_units  (struct s_collator *c, int n);
int                collator_get_units  (struct s_collator *c);

/* fn's only for use by channel decoders */

int                    add_comp_data   (struct rx_element_tag *u, int pt, struct iovec *iov, int iovc);
struct rx_element_tag* get_rx_unit     (int n, int cc_pt, struct rx_element_tag *u);
int                    fragment_sizes  (codec_t *cp, int len, struct iovec *store, int *iovc, int iovc_max);
int                    fragment_spread (codec_t *cp, int len, struct iovec *iov,   int iovc,  struct rx_element_tag *u);

#endif



