/*
 * Filename: rtcp_pckt.h
 * Author:   Paul Stewart
 * Modified: Vicky Hardman + Colin Perkins
 * Purpose:  RTCP protocol routines
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
 *
 * Copyright (c) 1994 Paul Stewart 
 * All rights reserved.
 * 
 * Permission is hereby granted, without written agreement and without license
 * or royalty fees, to use, copy, modify, and distribute this software and
 * its documentation for any purpose, provided that the above copyright
 * notice appears in all copies of this software.
 */

#ifndef _RTCP_PCKT
#define _RTCP_PCKT

#include "rat_types.h"

struct session_tag;
struct s_rtcp_dbentry;

#define MAX_PACKLEN     1200

typedef struct {
	u_int32         ssrc;
	u_int32         addr;
	char           *cname;
	char           *name;
	char           *email;
	char           *phone;
	char           *loc;
	char           *txt;
	char           *tool;
	void           *app_specific;
} ssrc_entry;

typedef struct {
	u_int32         ssrc;		/* SSRC this report is about                 */
	u_int32		loss;		/* frac(8) + cumulative(24)                  */
	u_int32		last_seq;	/* Extended highest sequence number received */
	u_int32         jitter;		/* interarrival jitter                       */
	u_int32         lsr;		/* last SR packet from this source           */
	u_int32         dlsr;		/* delay since last SR packet                */
} recv_rpt;

typedef struct {
	u_int32         ssrc;		/* source this RTCP packet refers to */
	u_int32         ntp_sec;	/* NTP timestamp                     */
	u_int32         ntp_frac;
	u_int32         rtp_ts;		/* RTP timestamp */
	u_int32         sender_pcount;
	u_int32         sender_bcount;
} sender_rpt;

/******************************************************************/


#define RTP_SEQ_MOD (1<<16)
#define RTP_TS_MOD  (0xffffffff)

#define RTP_MAX_SDES 256	/* maximum text length for SDES */

typedef enum {
	RTCP_SR = 200,
	RTCP_RR,
	RTCP_SDES,
	RTCP_BYE,
	RTCP_APP
} rtcp_type_t;

typedef enum {
	RTCP_SDES_INVALID,
	RTCP_SDES_CNAME,
	RTCP_SDES_NAME,
	RTCP_SDES_EMAIL,
	RTCP_SDES_PHONE,
	RTCP_SDES_LOC,
	RTCP_SDES_TOOL,
	RTCP_SDES_NOTE,
	RTCP_SDES_PRIV
} rtcp_sdes_type_t;

typedef struct {
#ifndef DIFF_BYTE_ORDER
	unsigned short  type:2;	/* packet type            */
	unsigned short  p:1;	/* padding flag           */
	unsigned short  x:1;	/* header extension flag  */
	unsigned short  cc:4;	/* CSRC count             */
	unsigned short  m:1;	/* marker bit             */
	unsigned short  pt:7;	/* payload type           */
#else
	unsigned short  cc:4;	/* CSRC count             */
	unsigned short  x:1;	/* header extension flag  */
	unsigned short  p:1;	/* padding flag           */
	unsigned short  type:2;	/* packet type            */
	unsigned short  pt:7;	/* payload type           */
	unsigned short  m:1;	/* marker bit             */
#endif
	u_int16         seq;	/* sequence number        */
	u_int32         ts;	/* timestamp              */
	u_int32         ssrc;	/* synchronization source */
	u_int32         csrc[16];/* optional CSRC list     */
} rtp_hdr_t;

typedef struct {
#ifndef DIFF_BYTE_ORDER
	unsigned short  type:2;	/* packet type            */
	unsigned short  p:1;	/* padding flag           */
	unsigned short  count:5;/* varies by payload type */
#else
	unsigned short  count:5;/* varies by payload type */
	unsigned short  p:1;	/* padding flag           */
	unsigned short  type:2;	/* packet type            */
#endif
	unsigned short  pt:8;	/* payload type           */
	u_int16         length;	/* packet length          */
} rtcp_common_t;

/* reception report */
typedef recv_rpt rtcp_rr_t;	/* Defined in rtcp.h */

typedef struct {
	u_int8          type;	/* type of SDES item (rtcp_sdes_type_t) */
	u_int8          length;	/* length of SDES item (in bytes)       */
	char            data[1];/* text, not zero-terminated            */
} rtcp_sdes_item_t;

/* one RTCP packet */
typedef struct {
	rtcp_common_t   common;	
	union {
		struct {
			u_int32         ssrc;		/* source this RTCP packet refers to */
			u_int32         ntp_sec;	/* NTP timestamp */
			u_int32         ntp_frac;
			u_int32         rtp_ts;		/* RTP timestamp */
			u_int32         sender_pcount;
			u_int32         sender_bcount;
			rtcp_rr_t       rr[1];		/* variable-length list */
		} sr;
		struct {
			u_int32         ssrc;		/* source this RTCP packet is coming from */
			rtcp_rr_t       rr[1];		/* variable-length list */
		} rr;
		struct {
			u_int32         src[1];		/* list of sources */
		} bye;
		struct rtcp_sdes_t {
			rtcp_sdes_item_t s[1];		/* list of SDES */
		} sdes;
	} r;
} rtcp_t;


#define RTP_SSRC_EXPIRE 	70*8000

int 		rtcp_check_rtcp_pkt(u_int8 *packet, int len);
void 		rtcp_decode_rtcp_pkt(struct session_tag *sp, struct session_tag *sp2, u_int8 *packet, int len, u_int32 addr, u_int32 cur_time);
int 		rtcp_add_sdes_item(u_int8 * buf, int type, char * val);
u_int8 	       *rtcp_packet_fmt_sdes(struct session_tag *sp, u_int8 * ptr);
u_int8 	       *rtcp_packet_fmt_bye(u_int8 * ptr, u_int32 ssrc, struct s_rtcp_dbentry *ssrc_db);
u_int8 	       *rtcp_packet_fmt_sr(struct session_tag *sp, u_int8 * ptr);
u_int8 	       *rtcp_packet_fmt_rrhdr(struct session_tag *sp, u_int8 * ptr);
u_int8         *rtcp_packet_fmt_addrr(struct session_tag *sp, u_int8 * ptr, struct s_rtcp_dbentry * dbe);
void 		rtcp_exit(struct session_tag *sp1, struct session_tag *sp2, int fd, u_int32 addr, u_int16 port);
u_int32  	rtcp_interval(int members, int senders, double rtcp_bw, int we_sent, int packet_size, int *avg_rtcp_size, int initial);
void 		rtcp_update(struct session_tag *sp, int fd, u_int32 addr, u_int16 port);
void	 	rtcp_forward(rtcp_t *pckt, struct session_tag *sp1, struct session_tag *sp2);

#endif
