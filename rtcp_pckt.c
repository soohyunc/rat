/*
 * Filename: rtcp_pckt.c
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

#include "version.h"
#include "rat_types.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "session.h"
#include "ui_control.h"
#include "util.h"
#include "net.h"
#include "transmit.h"
#include "rat_time.h"

/*
 * Sets the ntp 64 bit values, one 32 bit quantity at a time.
 */
#define SECS_BETWEEN_1900_1970 2208988800u

static void 
rtcp_ntp_format(u_int32 * sec, u_int32 * frac)
{
	struct timeval  tv;
	u_int32 usec;

	gettimeofday(&tv, 0);
	*sec = tv.tv_sec + SECS_BETWEEN_1900_1970;
	usec = tv.tv_usec;
	*frac = (usec << 12) + (usec << 8) - ((usec * 3650) >> 6);
}

static u_int32 
ntp_time32(void)
{
	struct timeval  tv;
	u_int32 sec, usec, frac;

	gettimeofday(&tv, 0);
	sec  = tv.tv_sec + SECS_BETWEEN_1900_1970;
	usec = tv.tv_usec;
	frac = (usec << 12) + (usec << 8) - ((usec * 3650) >> 6);
	return (sec & 0xffff) << 16 | frac >> 16;
}

int
rtcp_check_rtcp_pkt(u_int8 *packet, int len)
{
	/* Validity check for a compound RTCP packet. This function returns */
	/* TRUE if the packet is okay, FALSE if the validity check fails.   */
        /*                                                                  */
	/* The following checks can be applied to RTCP packets [RFC1889]:   */
        /* o RTP version field must equal 2.                                */
        /* o The payload type field of the first RTCP packet in a compound  */
        /*   packet must be equal to SR or RR.                              */
        /* o The padding bit (P) should be zero for the first packet of a   */
        /*   compound RTCP packet because only the last should possibly     */
        /*   need padding.                                                  */
        /* o The length fields of the individual RTCP packets must total to */
        /*   the overall length of the compound RTCP packet as received.    */
        /*   This is a fairly strong check.                                 */

	rtcp_t	*pkt  = (rtcp_t *) packet;
	rtcp_t	*end  = (rtcp_t *) (((char *) pkt) + len);
	rtcp_t	*r    = pkt;
	int	 l    = 0;
	int	 last = 0;

	/* All RTCP packets must be compound packets (RFC1889, section 6.1) */
	if (((ntohs(pkt->common.length) + 1) * 4) == len) {
		dprintf("Bogus RTCP packet: not a compound packet\n");
		return FALSE;
	}

	/* Check the RTCP version, payload type and padding of the first in  */
	/* the compund RTCP packet...                                        */
	if (pkt->common.type != 2) {
		dprintf("Bogus RTCP packet: version number != 2 in the first sub-packet\n");
		return FALSE;
	}
	if (pkt->common.p != 0) {
		dprintf("Bogus RTCP packet: padding bit is set, and this is the first packet in the compound\n");
		return FALSE;
	}
	if ((pkt->common.pt != RTCP_SR) && (pkt->common.pt != RTCP_RR)) {
		dprintf("Bogus RTCP packet: compund packet does not start with SR or RR\n");
		return FALSE;
	}

	/* Check all following parts of the compund RTCP packet. The RTP version */
	/* number must be 2, and the padding bit must be zero on all apart from  */
	/* the last packet.                                                      */
	do {
		if (r->common.type != 2) {
			dprintf("Bogus RTCP packet: version number != 2\n");
			return FALSE;
		}
		if (last == 1) {
			dprintf("Bogus RTCP packet: padding bit was set before the last packet in the compound\n");
			return FALSE;
		}
		if (r->common.p == 1) last = 1;
		l += (ntohs(r->common.length) + 1) * 4;
		r  = (rtcp_t *) (((u_int32 *) r) + ntohs(r->common.length) + 1);
	} while (r < end);

	/* Check that the length of the packets matches the length of the UDP */
	/* packet in which they were received...                              */
	if ((r != end) || (l != len))  {
		dprintf("Bogus RTCP packet: length of RTCP packet does not match length of UDP packet\n");
		return FALSE;
	}

	return TRUE;
}


/*
 *  Decode an RTCP packet.
 *
 *  We must be careful not to decode loopback packets! This is relatively easy: all RTCP packets MUST start with
 *  either an SR or RR sub-packet. We check the SSRC of that sub-packet, and if it's our SSRC then throw out the
 *  packet. [csp]
 */
void 
rtcp_decode_rtcp_pkt(session_struct *sp, session_struct *sp2, u_int8 *packet, int len, u_int32 addr, u_int32 cur_time)
{
	rtcp_t			*pkt = (rtcp_t *) packet;
	rtcp_dbentry		*dbe, *other_source;
	rtcp_sdes_item_t	*sdes;
	u_int32			ssrc;
	u_int32			*alignptr;
	int			i, lenstr;
	rtcp_user_rr            *rr;

	len /= 4;
	while (len > 0) {
		len -= ntohs(pkt->common.length) + 1;
		if (len < 0 || pkt->common.length == 0) {
#ifdef DEBUG
			printf("rtcp_decode_rtcp_pkt: packet format is weird... this should never happen\n");
			printf("since the packet has already gone through the header validation step... \n");
			abort();
#else
			return;
#endif
		}
		switch (pkt->common.pt) {
		case RTCP_SR:
			ssrc = ntohl(pkt->r.sr.ssrc);
			if (ssrc == sp->db->myssrc && 
                            sp->filter_loopback) {
				/* Loopback packet, discard it... */
				return;
			}
			dbe = rtcp_getornew_dbentry(sp, ssrc, addr, cur_time);
			dbe->last_active   = cur_time;
			dbe->last_sr       = ntp_time32();

			/* Take note of mapping to use in synchronisation */
			dbe->mapping_valid = TRUE;
			dbe->last_ntp_sec  = ntohl(pkt->r.sr.ntp_sec);
			dbe->last_ntp_frac = ntohl(pkt->r.sr.ntp_frac);
			dbe->last_rtp_ts   = ntohl(pkt->r.sr.rtp_ts);

			/* Store the reception statistics for that user... */
			/* Clear the old RR list... */
			for (rr = dbe->rr; rr != NULL; rr = rr->next) {
				xfree(rr);
			}
			dbe->rr = NULL;
			/* Fill in the new RR list... */
			for (i = 0; i < pkt->common.count; i++) {
				rr = (rtcp_user_rr *) xmalloc(sizeof(rtcp_user_rr));
				rr->next          = dbe->rr;
				rr->ssrc          = ntohl(pkt->r.sr.rr[i].ssrc);
				rr->fraction_lost = ntohl(pkt->r.sr.rr[i].loss) >> 24;
				rr->pckts_lost    = ntohl(pkt->r.sr.rr[i].loss) & 0x00ffffff;
				rr->ext_seq_num   = ntohl(pkt->r.sr.rr[i].last_seq);
				rr->jitter        = ntohl(pkt->r.sr.rr[i].jitter);
				rr->lsr           = ntohl(pkt->r.sr.rr[i].lsr);
				rr->dlsr          = ntohl(pkt->r.sr.rr[i].dlsr);
				dbe->rr      = rr;
				other_source = rtcp_getorew_dbentry(sp, rr->ssrc, addr, cur_time);
				if (dbe->sentry->cname != NULL) {
					ui_update_loss(dbe->sentry->cname, other_source->sentry->cname, (int) ((rr->fraction_lost / 2.56)+0.5));
				}
			}
			break;
		case RTCP_RR:	/* We won't deal with this just yet */
			ssrc = ntohl(pkt->r.sr.ssrc);
			if (ssrc == sp->db->myssrc) {
				/* Loopback packet, discard it... */
				return;
			}
			dbe = rtcp_getornew_dbentry(sp, ntohl(pkt->r.rr.ssrc), addr, cur_time);
			dbe->last_active = cur_time;

			/* Store the reception statistics for that user... */
			/* Clear the old RR list... */
			for (rr = dbe->rr; rr != NULL; rr = rr->next) {
				xfree(rr);
			}
			dbe->rr = NULL;
			/* Fill in the new RR list... */
			for (i = 0; i < pkt->common.count; i++) {
				rr = (rtcp_user_rr *) xmalloc(sizeof(rtcp_user_rr));
				rr->next          = dbe->rr;
				rr->ssrc          = ntohl(pkt->r.rr.rr[i].ssrc);
				rr->fraction_lost = ntohl(pkt->r.rr.rr[i].loss) >> 24;
				rr->pckts_lost    = ntohl(pkt->r.rr.rr[i].loss) & 0x00ffffff;
				rr->ext_seq_num   = ntohl(pkt->r.rr.rr[i].last_seq);
				rr->jitter        = ntohl(pkt->r.rr.rr[i].jitter);
				rr->lsr           = ntohl(pkt->r.rr.rr[i].lsr);
				rr->dlsr          = ntohl(pkt->r.rr.rr[i].dlsr);
				dbe->rr = rr;
				other_source =  rtcp_getornew_dbentry(sp, rr->ssrc, addr, cur_time);
				if (dbe->sentry->cname != NULL) {
					ui_update_loss(dbe->sentry->cname, other_source->sentry->cname, (int) ((rr->fraction_lost / 2.56)+0.5));
				}
			}

			/* is it reporting on my traffic? */
			if ((ntohl(pkt->r.rr.rr[0].ssrc) == sp->db->myssrc) && (dbe->sentry->cname != NULL)) {
				/* Need to store stats in ssrc's db not r.rr.ssrc's */
				dbe->loss_from_me = (ntohl(pkt->r.rr.rr[0].loss) >> 24) & 0xff;
				dbe->last_rr_for_me = cur_time;
				ui_update_loss(sp->db->my_dbe->sentry->cname, dbe->sentry->cname, (dbe->loss_from_me*100)>>8);
			}
			break;
		case RTCP_BYE:
			rtcp_forward(pkt, sp2, sp);
			for (i = 0; i < pkt->common.count; i++) {
				rtcp_delete_dbentry(sp, ntohl(pkt->r.bye.src[i]));
			}
			break;
		case RTCP_SDES:
			rtcp_forward(pkt, sp2, sp);
			alignptr = (u_int32 *) pkt->r.sdes.s;
			for (i = 0; i < pkt->common.count; i++) {
				ssrc = ntohl(*alignptr);
				dbe = rtcp_getornew_dbentry(sp, ssrc, addr, cur_time);
				dbe->last_active = cur_time;
				sdes = (rtcp_sdes_item_t *) (alignptr + 1);
				while (sdes->type) {
					lenstr = sdes->length;
					sdes->length += 2;
					switch (sdes->type) {
					case RTCP_SDES_CNAME:
						if (dbe->sentry->cname) {
							if (strncmp(dbe->sentry->cname, sdes->data, lenstr) != 0) {
								dprintf("CNAME change %d : [%s] --> [%s]\n", lenstr, dbe->sentry->cname, sdes->data);
							}
							break;
						}
						dbe->sentry->cname = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->cname, sdes->data, lenstr);
						dbe->sentry->cname[lenstr] = '\0';
						if (ssrc == sp->db->myssrc && sp->db->my_dbe->sentry->cname &&
								strcmp(dbe->sentry->cname, sp->db->my_dbe->sentry->cname)) { 
							sp->db->old_ssrc   = sp->db->myssrc;
							sp->db->myssrc     = lrand48();
							sp->db->pkt_count  = 0;
							sp->db->byte_count = 0;
						}
                                                if (sp->ui_on) {
                                                        ui_info_update_cname(dbe);
                                                }
						break;
					case RTCP_SDES_NAME:
						if (dbe->sentry->name) {
							if (!strncmp(dbe->sentry->name, sdes->data, lenstr)) {
								break;
							}
							xfree(dbe->sentry->name);
						}
						dbe->sentry->name = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->name, sdes->data, lenstr);
						dbe->sentry->name[lenstr] = '\0';
                                                if (sp->ui_on) {
                                                        ui_info_update_name(dbe);
                                                }
						break;
					case RTCP_SDES_EMAIL:
						if (dbe->sentry->email) {
							if (!strncmp(dbe->sentry->email, sdes->data, lenstr)) {
								break;
							}
							xfree(dbe->sentry->email);
						}
						dbe->sentry->email = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->email, sdes->data, lenstr);
						dbe->sentry->email[lenstr] = '\0';
                                                if (sp->ui_on) {
                                                        ui_info_update_email(dbe);
                                                }
						break;
					case RTCP_SDES_PHONE:
						if (dbe->sentry->phone) {
							if (!strncmp(dbe->sentry->phone, sdes->data, lenstr)) {
								break;
							}
							xfree(dbe->sentry->phone);
						}
						dbe->sentry->phone = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->phone, sdes->data, lenstr);
						dbe->sentry->phone[lenstr] = '\0';
                                                if (sp->ui_on) {
                                                        ui_info_update_phone(dbe);
                                                }
						break;
					case RTCP_SDES_LOC:
						if (dbe->sentry->loc) {
							if (!strncmp(dbe->sentry->loc, sdes->data, lenstr)) {
								break;
							}
							xfree(dbe->sentry->loc);
						}
						dbe->sentry->loc = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->loc, sdes->data, lenstr);
						dbe->sentry->loc[lenstr] = '\0';
                                                if (sp->ui_on) {
                                                        ui_info_update_loc(dbe);
                                                 }
						break;
					case RTCP_SDES_TOOL:
						if (dbe->sentry->tool) {
							if (!strncmp(dbe->sentry->tool, sdes->data, lenstr)) {
								break;
							}
							xfree(dbe->sentry->tool);
						}
						dbe->sentry->tool = (char *) xmalloc(lenstr + 1);
						memcpy(dbe->sentry->tool, sdes->data, lenstr);
						dbe->sentry->tool[lenstr] = '\0';
                                                if (sp->ui_on) {
                                                        ui_info_update_tool(dbe);
                                                }
						break;
					default:
						dprintf("Unknown SDES packet type %d\n", sdes->type);
						break;
					}
					sdes = (rtcp_sdes_item_t *) ((u_int8 *) sdes + sdes->length);
				}
				while ((u_int8 *) sdes >= (u_int8 *) alignptr) {
					alignptr++;	/* Next 32bit boundary */
				}
			}
			break;
		case RTCP_APP:	
			break;
		default:
			break;
		}
		pkt = (rtcp_t *) ((u_int32 *) pkt + ntohs(pkt->common.length) + 1);
	}
}

/*
 * Fill out an SDS item.  I assume here that the item is a NULL terminated
 * string.
 */
int 
rtcp_add_sdes_item(u_int8 * buf, int type, char * val)
{
	rtcp_sdes_item_t *shdr = (rtcp_sdes_item_t *) buf;
	int             namelen;

	if (!val) {
		return 0;
	}
	shdr->type = type;
	namelen = strlen(val);
	shdr->length = namelen;
	strcpy(shdr->data, val);
	return (namelen + 2);
}

/*
 * Fill out a complete SDES packet.  This finds all set values in the
 * database and compiles them into a complete SDES packet to be sent out.
 */
u_int8 *
rtcp_packet_fmt_sdes(session_struct *sp, u_int8 * ptr)
{
	rtcp_common_t  *hdr = (rtcp_common_t *) ptr;
	int             i, len;

	/* Format the SDES header... */
	hdr->t ype  = 2;
	hdr->p     = 0;
	hdr->count = 1;
	hdr->pt    = RTCP_SDES;
	*((u_int32 *) ptr + 1) = htonl(sp->db->myssrc);
	len = 8;

	/* From draft-ietf-avt-profile-new-00:                             */
	/* "Applications may use any of the SDES items described in the    */
        /* RTP specification. While CNAME information is sent every        */
        /* reporting interval, other items should be sent only every third */
        /* reporting interval, with NAME sent seven out of eight times     */
        /* within that slot and the remaining SDES items cyclically taking */
        /* up the eighth slot, as defined in Section 6.2.2 of the RTP      */
        /* specification. In other words, NAME is sent in RTCP packets 1,  */
        /* 4, 7, 10, 13, 16, 19, while, say, EMAIL is used in RTCP packet  */
        /* 22."                                                            */
	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_CNAME, sp->db->my_dbe->sentry->cname);
	sp->db->sdes_pri_count++;
	if ((sp->db->sdes_pri_count % 3) == 0) {
		sp->db->sdes_sec_count++;
		if ((sp->db->sdes_sec_count % 8) == 0) {
			sp->db->sdes_ter_count++;
			switch (sp->db->sdes_ter_count % 4) {
			case 0 : if (sp->db->my_dbe->sentry->email != NULL) {
			         	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_EMAIL, sp->db->my_dbe->sentry->email);
			  	 	break;
			 	 } else {
				   	dprintf("Can't send RTCP SDES EMAIL: NULL pointer\n");
				 }
			case 1 : if (sp->db->my_dbe->sentry->phone != NULL) {
			           	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_PHONE, sp->db->my_dbe->sentry->phone);
			  	   	break;
			 	 } else {
				   	dprintf("Can't send RTCP SDES PHONE: NULL pointer\n");
			 	 }
			case 2 : if (sp->db->my_dbe->sentry->loc != NULL) {
			           	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_LOC, sp->db->my_dbe->sentry->loc);
			  	   	break;
			 	 } else {
				   	dprintf("Can't send RTCP SDES LOC: NULL pointer\n");
			 	 }
			case 3 : if (sp->mode == TRANSCODER) {
				 	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_TOOL, RAT_VERSION " " OSNAME " [Transcoder]");
				 } else {
				 	len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_TOOL, RAT_VERSION " " OSNAME);
				 }
			  	 break;
			default: printf("ERROR: sdes_ter_count has strange value! %ld\n", sp->db->sdes_ter_count);
			         abort();
			}
		} else {
			if (sp->db->my_dbe->sentry->name != NULL) {
				len += rtcp_add_sdes_item(&ptr[len], RTCP_SDES_NAME, sp->db->my_dbe->sentry->name);
			} else {
				dprintf("Can't send RTCP SDES NAME: NULL pointer\n");
			}
		}
	}
	hdr->length = htons(len / 4);
	for (i = len; i < ((int)(len / 4) + 1) * 4; i++) {
		ptr[i] = 0;
	}
	return ptr + 4 * ((int)(len / 4) + 1);
}

/*
 * Create a "BYE" packet.
 */
u_int8 *
rtcp_packet_fmt_bye(u_int8 *ptr, u_int32 ssrc, rtcp_dbentry *ssrc_db)
{
	rtcp_t	     *pkt = (rtcp_t *) ptr;
	rtcp_dbentry *entry;
	int           count;

	pkt->common.type   = 2;
	pkt->common.p      = 0;
	pkt->common.pt     = RTCP_BYE;
	pkt->r.bye.src[0]  = htonl(ssrc);

	count = 1;
	for (entry = ssrc_db; entry != NULL; entry = entry->next) {
		pkt->r.bye.src[count++] = htonl(entry->ssrc);
	}
	pkt->common.count  = count;
	pkt->common.length = htons(count);

	return ptr + 4 + (count * 4);
}

/*
 * Format a sender report packet, from the information available in the
 * database.
 */
u_int8 *
rtcp_packet_fmt_sr(session_struct *sp, u_int8 * ptr)
{
	rtcp_common_t  *hdr = (rtcp_common_t *) ptr;
	u_int32				sec;
	u_int32				frac;

	hdr->type  = 2;
	hdr->p     = 0;
	hdr->count = 0;
	hdr->pt    = RTCP_SR;
	*((u_int32 *) ptr + 1) = htonl(sp->db->myssrc);
	rtcp_ntp_format(&sec, &frac);
	sp->db->map_ntp_time = (sec & 0xffff) << 16 | frac >> 16;
	*((u_int32 *) ptr + 2) = htonl(sec);
	*((u_int32 *) ptr + 3) = htonl(frac);

	sp->db->map_rtp_time = get_time(sp->device_clock);
	*((u_int32 *) ptr + 4) = htonl(sp->db->map_rtp_time);
	*((u_int32 *) ptr + 5) = htonl(sp->db->pkt_count);
	*((u_int32 *) ptr + 6) = htonl(sp->db->byte_count);
	return (ptr + 28);
}

/*
 * Create a recipient report header.
 */
u_int8 *
rtcp_packet_fmt_rrhdr(session_struct *sp, u_int8 * ptr)
{
	rtcp_common_t  *hdr = (rtcp_common_t *) ptr;
	u_int32         sec;
	u_int32         frac;

	/* Update local clock map */
	rtcp_ntp_format(&sec, &frac);
	sp->db->map_ntp_time = (sec & 0xffff) << 16 | frac >> 16;
	sp->db->map_rtp_time = get_time(sp->device_clock);

	hdr->type  = 2;
	hdr->p     = 0;
	hdr->count = 0;
	hdr->pt    = RTCP_RR;
	*((u_int32 *) ptr + 1) = htonl(sp->db->myssrc);
	return ptr + 8;
}

/*
 * Format a recipient report item, given the database item that this should
 * refer to.
 */
u_int8 *
rtcp_packet_fmt_addrr(session_struct *sp, u_int8 * ptr, rtcp_dbentry * dbe)
{
	rtcp_rr_t      *rptr = (rtcp_rr_t *) ptr;
	u_int32		ext_max, expected, expi, reci;
	int32		losti;

	ext_max = dbe->cycles + dbe->lastseqno;
	expected = ext_max - dbe->firstseqno + 1;
	dbe->lost_tot = expected - dbe->pckts_recv;

	if (dbe->lost_tot < 0) dbe->lost_tot = 0;

	expi = expected - dbe->expected_prior;
	dbe->expected_prior = expected;
	reci = dbe->pckts_recv - dbe->received_prior;
	dbe->received_prior = dbe->pckts_recv;
	losti = expi - reci;

	if (expi == 0 || losti <= 0) {
		dbe->lost_frac = 0;
	} else {
		dbe->lost_frac = (losti << 8) / expi;
	}

	ui_update_duration(dbe->sentry->cname, dbe->units_per_packet * 20);
	ui_update_loss(sp->db->my_dbe->sentry->cname, dbe->sentry->cname, (dbe->lost_frac * 100) >> 8);
	ui_update_reception(dbe->sentry->cname, dbe->pckts_recv, dbe->lost_tot, dbe->misordered, dbe->jitter);

	rptr->ssrc     = htonl(dbe->ssrc);
	rptr->loss     = htonl(dbe->lost_frac << 24 | (dbe->lost_tot & 0xffffff));
	rptr->last_seq = htons(dbe->cycles + dbe->lastseqno);
	rptr->jitter   = htonl((u_long) dbe->jitter);

	rptr->lsr      = htonl(dbe->last_sr);
	rptr->dlsr     = htonl(ntp_time32() - dbe->last_sr);
	return ptr + 24;
}

/*
 * Calculate the RTCP report interval. This function is copied from rfc1889 [csp]
 */
u_int32 rtcp_interval(int 	 members,
                      int 	 senders,
                      double 	 rtcp_bw,
                      int 	 we_sent,
                      int 	 packet_size,
                      int 	*avg_rtcp_size,
                      int 	 initial)
{
    double RTCP_MIN_TIME           = 5.0;				/* Min time between report, in seconds    */
    double RTCP_SENDER_BW_FRACTION = 0.25;				/* Fraction of RTCP bandwidth used for SR */
    double RTCP_RCVR_BW_FRACTION   = (1-RTCP_SENDER_BW_FRACTION);	/* Fraction of RTCP bandwidth used for RR */
    double RTCP_SIZE_GAIN          = (1.0/16.0);			/*					  */
    double t;                   					/* interval                               */
    double rtcp_min_time 	   = RTCP_MIN_TIME;			/*                                        */
    int n;                      					/* no. of members for computation         */

#ifdef DEBUG_RTCP
    printf("members=%d, senders=%d, rtcp_bw=%f, we_sent=%d, packet_size=%d, avg_rtcp_size=%d, initial=%d\n", 
            members, senders, rtcp_bw, we_sent, packet_size, *avg_rtcp_size, initial);
#endif
    /* Very first call at application start-up uses half the min     */
    /* delay for quicker notification while still allowing some time */
    /* before reporting for randomization and to learn about other   */
    /* sources so the report interval will converge to the correct   */
    /* interval more quickly.  The average RTCP size is initialized  */
    /* to 128 octets which is conservative (it assumes everyone else */
    /* is generating SRs instead of RRs: 20 IP + 8 UDP + 52 SR + 48  */
    /* SDES CNAME).                                                  */
    if (initial) {
        rtcp_min_time /= 2;
        *avg_rtcp_size = 128;
    }

    /* If there were active senders, give them at least a minimum     */
    /* share of the RTCP bandwidth.  Otherwise all participants share */
    /* the RTCP bandwidth equally.                                    */
    n = members;
    if (senders > 0 && senders < members * RTCP_SENDER_BW_FRACTION) {
        if (we_sent) {
            rtcp_bw *= RTCP_SENDER_BW_FRACTION;
            n = senders;
        } else {
            rtcp_bw *= RTCP_RCVR_BW_FRACTION;
            n -= senders;
        }
    }

    /* Update the average size estimate by the size of the report */
    /* packet we just sent.                                       */
    *avg_rtcp_size += (packet_size - *avg_rtcp_size)*RTCP_SIZE_GAIN;

    /* The effective number of sites times the average packet size is */
    /* the total number of octets sent when each site sends a report. */
    /* Dividing this by the effective bandwidth gives the time        */
    /* interval over which those packets must be sent in order to     */
    /* meet the bandwidth target, with a minimum enforced.  In that   */
    /* time interval we send one report so this time is also our      */
    /* average time between reports.                                  */
    t = (*avg_rtcp_size) * n / rtcp_bw;
    if (t < rtcp_min_time) t = rtcp_min_time;

    /* To avoid traffic bursts from unintended synchronization with   */
    /* other sites, we then pick our actual next report interval as a */
    /* random number uniformly distributed between 0.5*t and 1.5*t.   */
    /*                                                                */
    /* Time is in 8kHz audio samples! [csp]                           */
#ifdef DEBUG_RTCP
    printf("RTCP reporting interval is %f seconds\n", t);
#endif
    return (u_int32) (t * (drand48() + 0.5)) * 8000;
}

static u_int8 *
rtcp_packet_fmt_srrr(session_struct *sp, u_int8 *ptr)
{
	u_int8	       *packet 	= ptr;
	rtcp_common_t  *hdr    	= (rtcp_common_t *) ptr;
	rtcp_dbentry   *sptr 	= sp->db->ssrc_db;
	rtcp_dbentry   *sptmp	= NULL;
	u_int32		now 	= get_time(sp->device_clock);
	int             packlen = 0;
	int		offset	= 0;

	sp->db->senders = 0;
	if (sp->db->sending) {
		ptr = rtcp_packet_fmt_sr(sp, ptr);
		sp->db->senders++;
	} else {
		ptr = rtcp_packet_fmt_rrhdr(sp, ptr);
	}
	while (sptr) {
		sptmp = sptr->next;	/* We may free things below */
		if (now - sptr->last_active > RTP_SSRC_EXPIRE) {
			rtcp_delete_dbentry(sp, sptr->ssrc);
		} else {
			if (sptr->is_sender) {	/* Is this an active source? */
				sp->db->senders++;
				sptr->is_sender = 0;	/* Reset this every report time */
				ptr = rtcp_packet_fmt_addrr(sp, ptr, sptr);
				hdr->count++;
				packlen = ptr - packet;
				hdr->length = htons((packlen - offset) / 4 - 1);
				if (packlen + 84 > MAX_PACKLEN) {	/* In case packet filled in report */
					/* Too many sources sent data, and the result doesn't fit into a */
					/* single SR/RR packet. We just ignore the excess here. Oh well. */
					break;
				}
			}
		}
		sptr = sptmp;
	}
	packlen = ptr - (u_int8 *) packet;
	hdr->length = htons((packlen - offset) / 4 - 1);
	return ptr;
}

void 
rtcp_exit(session_struct *sp1, session_struct *sp2, int fd, u_int32 addr, u_int16 port)
{
	u_int32          packet[MAX_PACKLEN / 4];
	u_int8          *ptr = (u_int8 *) packet;
	rtcp_dbentry	*src;

	/* Send an RTCP BYE packet... */
	ptr = rtcp_packet_fmt_srrr(sp1, ptr);
	ptr = rtcp_packet_fmt_bye(ptr, sp1->db->myssrc, sp1->mode == TRANSCODER? sp2->db->ssrc_db: NULL);
	net_write(fd, addr, port, (u_int8 *) packet, ptr - (u_int8 *) packet, PACKET_RTCP);

	rtcp_free_dbentry(sp1->db->my_dbe);
	while ((src = sp1->db->ssrc_db) != NULL) {
		rtcp_delete_dbentry(sp1, src->ssrc);
	}
	if (sp1->mode == TRANSCODER) {
		rtcp_free_dbentry(sp2->db->my_dbe);
		while ((src = sp2->db->ssrc_db) != NULL) {
			rtcp_delete_dbentry(sp2, src->ssrc);
		}
	}
}

/*
 * rtcp_update() Performs the following periodic functions: 
 *   Expires elements from the SSRC database
 *   Sends out sender reports/receiver reports 
 *   Sends the obligatory BYE packet if SSRC was changed
 *
 */
void 
rtcp_update(session_struct *sp, int fd, u_int32 addr, u_int16 port)
{
	u_int32  	packet[MAX_PACKLEN / 4];
	u_int8         *ptr 	= (u_int8 *) packet;
	int             packlen = 0;
	u_int32		now 	= get_time(sp->device_clock);

	if (sp->db->old_ssrc) {
		/* Our SSRC has changed, so send a BYE packet for that SSRC */
		ptr = rtcp_packet_fmt_srrr(sp, ptr);
		ptr = rtcp_packet_fmt_bye(ptr, sp->db->old_ssrc, sp->mode==TRANSCODER?sp->db->ssrc_db:NULL);
		packlen = ptr - (u_int8 *) packet;
		net_write(fd, addr, port, (u_int8 *) packet, packlen, PACKET_RTCP);
		sp->db->last_rpt = now;
		sp->db->old_ssrc = 0;
	}

	/* Check if it's time to send an RTCP packet... */
	if ((now - sp->db->last_rpt) > sp->db->report_interval) {
		ptr = rtcp_packet_fmt_srrr(sp, ptr);
		ptr = rtcp_packet_fmt_sdes(sp, ptr);
		packlen = ptr - (u_int8 *) packet;
		net_write(fd, addr, port, (u_int8 *) packet, packlen, PACKET_RTCP);

		/* Calculate the interval until we're due to send another RTCP packet... */
		sp->db->report_interval = rtcp_interval(sp->db->members, sp->db->senders, sp->db->rtcp_bw, sp->db->sending, 
                                                      	packlen, &(sp->db->avg_size), sp->db->initial_rtcp);
		/* Reset per-report statistics... */
		sp->db->last_rpt     = now;
		sp->db->initial_rtcp = FALSE;
		sp->db->sending      = FALSE;
		sp->db->senders      = 0;
	}
}

void
rtcp_forward(rtcp_t *pckt, session_struct *sp1, session_struct *sp2)
{
	u_int32  	packet[MAX_PACKLEN / 4];
	u_int8         *ptr 	= (u_int8 *) packet;
	int             packlen = 0;
	/* XXX why are these ints */
	u_int32		now 	= get_time(sp2->device_clock);

	if (sp1 == NULL || sp1->mode != TRANSCODER || sp2->mode != TRANSCODER)
		return;

	ptr = rtcp_packet_fmt_srrr(sp2, ptr);
	memcpy(ptr, pckt, (pckt->common.length+1)*4);
	ptr += (pckt->common.length+1)*4;
	packlen = ptr - (u_int8 *) packet;
	net_write(sp1->rtcp_fd, sp1->net_maddress, sp1->rtcp_port, (u_int8 *)packet, packlen, PACKET_RTCP);
	sp2->db->last_rpt = now;
}

