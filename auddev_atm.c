/*
 * FILE:     auddev_atm.c
 * PROGRAM:  RAT
 * AUTHORS:  Julian Cable, Dimitris Terzis
 * MODS:     Orion Hodson
 *
 * auddev_atm acts like an audio device, only it reads and writes raw
 * AAL1 ATM frames containing alaw coded samples.  It assumes that RAT
 * is launched from a process that handles the atm set up and writes
 * the atm file descriptor to a file called "atm_socket".  There are
 * obviously other ways to communicate this info, but this suffices.
 *
 * The ATM stack in question is the Linux one, it may work with other
 * stacks but we don't have access to any suitable ATM equipment at
 * UCL to investigate (or even test this file).
 *
 * The real work here was done by Julian and Dimitris, who not only
 * made initial version of this file, but also chased up ATM stack
 * bugs.  OH fixed atm_audio_supports, simplified read/writes, and
 * changed atm_audio_read to read as many frames as available.
 *
 * Copyright (c) 2000 Nortel Networks
 *           (c) 1995-2000 University College London 
 * All rights reserved.  
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <atm.h>

#include <string.h>
#include "config_unix.h"
#include "debug.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_atm.h"
#include "codec_g711.h"

struct AUDIO
{
	int port;
};

static struct {
	unsigned long atmhdr;
	unsigned char rxresidue_buf[47];
	unsigned char *rxresidue;
	unsigned char txresidue[47];
	int rxresidue_bytes;
	int txresidue_bytes;
	int seqno;
	int monitor_gain;
	int output_muted;
	struct AUDIO play;
	struct AUDIO record;
}	dev_info;

/* AAL1-specific ATM header bytes */
static unsigned char aal1hdr[] = {0x00, 0x17, 0x2d, 0x3a, 0x4e, 0x59, 0x63, 0x74};

static int audio_fd = -1;

enum { AUDIO_LINE_OUT,  AUDIO_LINE_IN };

#define bat_to_device(x)	((x )
#define device_to_bat(x)	((x))

static int atm_socket;

int
atm_audio_device_count()
{
        return 1;
}

const char*
atm_audio_device_name(audio_desc_t ad)
{
        UNUSED(ad);
        return "ATM Network Audio Device (PCM A-law)";
}

int
atm_audio_supports(audio_desc_t ad, audio_format *fmt)
{

        UNUSED(ad);

        /* Only check sample rate and channels */
	if( fmt->sample_rate != 8000 ) return FALSE;
        if( fmt->channels != 1 ) return FALSE;

        return TRUE;
}

/* Try to open the audio device.                        */
/* Essentially, we read an ATM socket file descriptor, */
/* passed to RAT from the command line, from a file */
/* Returns TRUE if ok, 0 otherwise. */
int
atm_audio_open(audio_desc_t ad, audio_format* ifmt, audio_format* ofmt)
{

        int len;
	struct sockaddr_atmpvc vcc_address;
	FILE *f = fopen("atm_socket", "r");

	if (f == NULL) {
                debug_msg("ATM socket file not found");
		return FALSE;
	}
        
	debug_msg("ATM audio device\n");
        
	fread(&atm_socket, sizeof(atm_socket), 1, f);
	fclose(f);

        len = sizeof(vcc_address);

        if (audio_fd != -1) {
                debug_msg("Device already open!");
                atm_audio_close(ad);
                return FALSE;
        }

        if (getsockopt(atm_socket, SOL_ATM, SO_ATMPVC, (char *)&vcc_address, &len) >= 0) {
		unsigned gfc=0, vpi, vci, type=0, clp=0;
		vpi = vcc_address.sap_addr.vpi;
		vci = vcc_address.sap_addr.vci;
		audio_fd = atm_socket;
		dev_info.atmhdr = (gfc << ATM_HDR_GFC_SHIFT) | (vpi << ATM_HDR_VPI_SHIFT) |
                        (vci << ATM_HDR_VCI_SHIFT) |       (type << ATM_HDR_PTI_SHIFT) | clp;
                
		dev_info.txresidue_bytes    = 0;
		dev_info.rxresidue_bytes    = 0;
		dev_info.rxresidue          = dev_info.rxresidue_buf;
		dev_info.seqno              = 0;
		memset(dev_info.txresidue, 0, sizeof(dev_info.txresidue));
		memset(dev_info.rxresidue, 0, sizeof(dev_info.txresidue));
                
		dev_info.monitor_gain       = 0;
		dev_info.output_muted       = 0; /* 0==not muted */
		dev_info.play.port          = AUDIO_LINE_OUT;
		dev_info.record.port        = AUDIO_LINE_IN;

		audio_format_change_encoding(ifmt, DEV_PCMA);
		audio_format_change_encoding(ofmt, DEV_PCMA);

		debug_msg("ATM audio device open (fd=%d, vpi=%d, vci=%d)\n", audio_fd, vpi, vci);
	        atm_audio_drain(ad);

		return audio_fd;
	} else {
                debug_msg("ATM socket descriptor is invalid");
		return FALSE;
	}
}

/* Close the audio device */
void
atm_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
        assert(audio_fd > 0);

	if (audio_fd <= 0) {
                debug_msg("Invalid desc");
		return;
        }

	close(audio_fd);
	audio_fd = -1;
}

/* Flush input buffer */
void
atm_audio_drain(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	ioctl(audio_fd, I_FLUSH, (caddr_t)FLUSHR);
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
atm_audio_set_igain(audio_desc_t ad, int gain)
{
        UNUSED(gain);
        UNUSED(ad); assert(audio_fd > 0);
}

int
atm_audio_get_igain(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return 0;
}

void
atm_audio_set_ogain(audio_desc_t ad, int vol)
{
        UNUSED(vol);
        UNUSED(ad); assert(audio_fd > 0);
}

int
atm_audio_get_ogain(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

	return 0;
}

void
atm_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(gain);
        UNUSED(ad); assert(audio_fd > 0);

}

#define ADA_CELL_SZ  52
#define ADA_CELL_HEADER_SZ 5

/* atm_audio_read: modified to read as many atm frames as are available (oh) */

int
atm_audio_read(audio_desc_t ad, u_char *buf, int buf_bytes)
{
	int len, avail, done = 0;
	char cellbuf[ADA_CELL_SZ];

        UNUSED(ad); assert(audio_fd > 0);

        assert(dev_info.rxresidue_bytes >= 0);
        if (dev_info.rxresidue_bytes > 0) {
                if (buf_bytes >= dev_info.rxresidue_bytes) {
                        /* big read that completely drains the residue */
                        memcpy(buf, dev_info.rxresidue, dev_info.rxresidue_bytes);
                        done                    += dev_info.rxresidue.bytes;
			dev_info.rxresidue       = dev_info.rxresidue_buf;
                        dev_info.rxresidue.bytes = 0;
                } else {
                        /* little read */
                        memcpy(buf, dev_info.rxresidue, buf_bytes);
                        dev_info.rxresidue_bytes -= buf_bytes;
                        dev_info.rxresidue       += buf_bytes;
                        done                     += buf_bytes;
                }
        }

        /* Read as much audio as is available */
        len = 0;
        while ((ioctl(audio_fd, FIONREAD, &avail) >= 0) && 
               (buf_bytes - done) >= (ADA_CELL_SZ - ADA_CELL_HEADER_SZ)) {
                len = read(audio_fd, cellbuf, sizeof(cellbuf));
                if (len <= 0) {
                        break;
                }
                assert(len == CELL_SZ);
                memcpy(buf + done, cellbuf + ADA_CELL_HEADER_SZ, ADA_CELL_SZ - ADA_CELL_HEADER_SZ);
                done += ADA_CELL_SZ - ADA_CELL_HEADER_SZ;
        }
        
        if (errno != 0) {
                debug_msg("atm audio read error (%d): len %d avail %d done %d of %d bytes\n", 
                          errno, len, avail, done, buf_bytes);
                /* avail =  0 ioctl failed          */
                /* len   = -1 read failed           */
                /* len   =  0 socket closed by peer */
                errno = 0;
                return done;
        }

        if (done < buf_bytes) {
                /* Copy bytes left over into residue buffer */
                int over;
                assert(dev_info.rxresidue_bytes == 0);
                assert(dev_info.rxresidue == dev_info.rxresidue_buf);

                over = buf_bytes - done;
                assert(over < ADA_CELL_SZ);

                memcpy(dev_info.rxresidue, 
                       cellbuf + sizeof(cellbuf) - over, 
                       sizeof(cellbuf) - over);
                dev_info.residue_bytes = over;
        }
        return done;
}


int
atm_audio_write(audio_desc_t ad, u_char *buf, int buf_bytes)
{
	int done;
	unsigned char cellbuf[52];

        UNUSED(ad); assert(audio_fd > 0);

        done = 0;

	/* if we have anything left from before put it in the output cell first 
	 * and then fill the cell from the buffer, fixing pointers and counts
	 */
	if (dev_info.txresidue_bytes > 0 && buf_bytes + dev_info.txresidue_bytes > ADA_CELL_SZ - ADA_CELL_HEADER_SZ) {
		int rem = ADA_CELL_SZ - ADA_CELL_HEADER_SZ - dev_info.txresidue_bytes;
		memcpy(cellbuf, &dev_info.atmhdr, 4);
		cellbuf[4] = aal1hdr[dev_info.seqno];
                dev_info.seq_no = (dev_info.seqno + 1) & 0x07; /* values 0-7 valid */
		memcpy(cellbuf + ATM_CELL_HEADER_SZ, dev_info.txresidue, dev_info.txresidue_bytes);
		memcpy(cellbuf + ATM_CELL_HEADER_SZ + dev_info.txresidue_bytes, buf, rem);
                write(audio_fd, (char *)cellbuf, ATM_CELL_SZ);
		dev_info.txresidue_bytes = 0;
		done += rem;
	}

        while(done - buf_bytes >= ADA_CELL_SZ - ADA_CELL_HEADER_SZ) {
		memcpy(cellbuf, &dev_info.atmhdr, 4);
		cellbuf[4] = aal1hdr[dev_info.seqno];
		memcpy(cellbuf + ADA_CELL_HEADER_SZ, buf + done, ADA_CELL_SZ - ADA_CELL_HEADER_SZ);
                write(audio_fd, (char *)cellbuf, ADA_CELL_SZ);
		done += ADA_CELL_SZ - ADA_CELL_HEADER_SZ;
                dev_info.seq_no = (dev_info.seqno + 1) & 0x07; /* values 0-7 valid */
	}

	if (buf_bytes - done > 0) {
		dev_info.txresidue_bytes = done - buf_bytes;
		memcpy(dev_info.txresidue, buf + done, dev_info.txresidue_bytes);
	}

	return buf_bytes;
}

/* Set ops on audio device to be non-blocking */
void
atm_audio_non_block(audio_desc_t ad)
{
	int	on = 1;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set non blocking mode on audio device!\n");
        } else {
 		debug_msg("Non-blocking mode set on ATM audio device (fd=%d)\n", audio_fd);
        }
}

/* Set ops on audio device to block */
void
atm_audio_block(audio_desc_t ad)
{
	int	on = 0;

        UNUSED(ad); assert(audio_fd > 0);

	if (ioctl(audio_fd, FIONBIO, (char *)&on) < 0) {
		debug_msg("Failed to set blocking mode on audio device!\n");
        } else {
 		debug_msg("Blocking mode set on ATM audio device (fd=%d)\n", audio_fd);
        }
}


static const audio_port_details_t out_ports[] = {
        { AUDIO_LINE_OUT,  AUDIO_PORT_LINE_OUT }
};

#define NUM_OUT_PORTS (sizeof(out_ports)/sizeof(out_ports[0]))

void
atm_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
        UNUSED(ad); assert(audio_fd > 0);

        if (port != AUDIO_LINE_OUT) {
                debug_msg("Port not recognized\n");
                port = AUDIO_LINE_OUT;
        }
        dev_info.play.port = port;
}

audio_port_t
atm_audio_oport_get(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return (dev_info.play.port);
}

int
atm_audio_oport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return (int)NUM_OUT_PORTS;
}

const audio_port_details_t*
atm_audio_oport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        if (idx >= 0 && idx < (int)NUM_OUT_PORTS) {
                return &out_ports[idx];
        }
        return NULL;
}

static const audio_port_details_t in_ports[] = {
        { AUDIO_LINE_IN,    AUDIO_PORT_LINE_IN}
};

#define NUM_IN_PORTS (sizeof(out_ports)/sizeof(out_ports[0]))

void
atm_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
        UNUSED(ad); assert(audio_fd > 0);

        if (port != AUDIO_LINE_IN ) {
                port = AUDIO_LINE_IN;
        }
        
        dev_info.record.port = port;
}

audio_port_t
atm_audio_iport_get(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
	return (dev_info.record.port);
}

int
atm_audio_iport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return (int)NUM_IN_PORTS;
}

const audio_port_details_t*
atm_audio_iport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        if (idx >= 0 && idx < (int)NUM_IN_PORTS) {
                return &in_ports[idx];
        }
        return NULL;
}

int
atm_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);

        return 1;
}

static int
atm_audio_select(audio_desc_t ad, int delay_us)
{
        fd_set rfds;
        struct timeval tv, s1, s2;

        UNUSED(ad); assert(audio_fd > 0);

        tv.tv_sec = 0;
        tv.tv_usec = delay_us;

        FD_ZERO(&rfds);
        FD_SET(audio_fd, &rfds);

        gettimeofday (&s1, 0);
        select(audio_fd+1, &rfds, NULL, NULL, &tv);
        gettimeofday (&s2, 0);

        s2.tv_usec -= s1.tv_usec;
        s2.tv_sec  -= s1.tv_sec;

        if (s2.tv_usec < 0) {
                s2.tv_usec += 1000000;
                s2.tv_sec  -= 1;
        }

        return FD_ISSET(audio_fd, &rfds);
}

void
atm_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        UNUSED(ad); assert(audio_fd > 0);
        atm_audio_select(ad, delay_ms * 1000);
}

int
atm_audio_is_ready(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        return atm_audio_select(ad, 0);
}

