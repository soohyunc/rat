/*
 * FILE:     auddev_osprey.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 1998 University College London
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

#ifdef HAVE_OSPREY

#include <sys/audioio.h>

#include <multimedia/audio_hdr.h>
#include <multimedia/audio_device.h>
#include <multimedia/audio_errno.h>

#include <oti_audio_device.h>
#include <dlfcn.h>

#include "config_unix.h"
#include "debug.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_osprey.h"

static audio_info_t     dev_info;
static int audioctl_fd     = -1;
static int audio_fd        = -1;
static int present         = 0;

static void osprey_audio_service(void);

static void 
osprey_poll_handler(int signo)
{
        signal(signo, osprey_poll_handler);

        debug_msg("signal %d (0x%x)\n", signo, signo);
        
        osprey_audio_service();
}

#define OTI_MAX_GAIN 1.0f
#define OTI_RAT_TO_DEVICE(x)	(((double)(x)) * (double)OTI_MAX_GAIN / (double)MAX_AMP)
#define OTI_DEVICE_TO_RAT(x)	((int) ((x) * MAX_AMP / AUDIO_MAX_GAIN))

/* These are function pointers for dynamically loaded functions.  We try to
 * open libotiaudio, then fill in these functions in oi_init().  If this
 * fails we are going to use the native sun interface functions.  The benefit
 * of this being that the same binary will work whether or not people have
 * the sunvideo plus card library installed.  If we just dynamically link it
 * it becomes a fatal error if it is not present on the system.
 *
 * This is really unpleasant but not on the pickled herrings scale.
 *
 * NB These prototypes MUST match those in oti_libaudio.h.  We do not include it
 * as these definitions will clash because we are declaring them as function pointers
 * and in oti_libaudio.h they are functions.
 */

static char *dlh;                /* Handle for dynamic library. */

int (*oti_audio_getinfo)     (int fildes, Audio_info*);
int (*oti_audio_setinfo)     (int fildes, Audio_info*);
int (*oti_audio__setplayhdr) (int fildes, Audio_hdr*, unsigned);
int (*oti_audio__setval)     (int fildes, unsigned*, unsigned);
int (*oti_audio__setgain)    (int fildes, double*, unsigned);
int (*oti_audio__setpause)   (int fildes, unsigned);
int (*oti_audio__flush)      (int fildes, unsigned int);
int (*oti_audio_drain)       (int fildes, int);
int (*oti_audio_play_eof)    (int fildes);

int     (*oti_open)  (const char *path, int oflag);
int     (*oti_close) (int fildes);
ssize_t (*oti_read)  (int fildes, void *buf, size_t nbyte);
ssize_t (*oti_write) (int fildes, const void *buf, size_t nbyte);
int     (*oti_ioctl) (int fildes, int request, void *val);
int     (*oti_fcntl) (int fildes, int cmd, void *val);

int (*oti_audio_init) (char*, char*);

#define DL_MAP_CAUTIOUS(h, symbol, errflag) \
        symbol = dlsym((h),(#symbol)); \
        if (!symbol) { \
                  debug_msg("Failed to map %s (%s)\n",#symbol,dlerror()); \
                  errflag = 1; \
        }

/* This function attempts to locate the dynamic link library
 * libotiaudio.so, open it, and map function addresses to the
 * above function pointers
 */

int 
osprey_audio_init()
{
        int success;
        char path[255], *locenv, otipath[] = "/libotiaudio.so";
        int err;
        dlh = NULL;

        /* Find and load dynamic library */

        locenv = getenv("O1KHOME");
        if (locenv) {
                path[0] = 0;
                strncat(path, locenv, 255);
                strncat(path, "lib", 255);
                strncat(path, otipath, 255);
        }
        dlh = dlopen(path, RTLD_NOW); /* BIND NOW */

        if (!dlh) {
                char* ldpath = getenv("LD_LIBRARY_PATH");
                locenv = strtok(ldpath, ":");
                do {
                        path[0] = 0;
                        strncat(path, locenv, 255);
                        strncat(path, otipath, 255);
                        dlh = dlopen(path, RTLD_NOW);
                        locenv = strtok(NULL, ":");
                } while (!dlh && locenv);
                if (!dlh) return 0; /* Could not get it */
        }

        /* Map functions */
        err = 0;
        DL_MAP_CAUTIOUS(dlh, oti_audio_getinfo, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio_setinfo, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio__setplayhdr, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio__setval,   err);
        DL_MAP_CAUTIOUS(dlh, oti_audio__setgain,  err);
        DL_MAP_CAUTIOUS(dlh, oti_audio__setpause, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio__flush, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio_drain, err);
        DL_MAP_CAUTIOUS(dlh, oti_audio_play_eof, err);

        DL_MAP_CAUTIOUS(dlh, oti_open,  err);
        DL_MAP_CAUTIOUS(dlh, oti_close, err);
        DL_MAP_CAUTIOUS(dlh, oti_read,  err);
        DL_MAP_CAUTIOUS(dlh, oti_write, err);
        DL_MAP_CAUTIOUS(dlh, oti_ioctl, err);
        DL_MAP_CAUTIOUS(dlh, oti_fcntl, err);

        DL_MAP_CAUTIOUS(dlh, oti_audio_init, err);

        if (err) {
                dlclose(dlh);
                return FALSE;
        }

        success = oti_audio_init(NULL, NULL);
        if (success != AUDIO_SUCCESS) {
                fprintf(stderr, "oti_audio_init failed (reason %d)\n", success);
                return FALSE;
        }

        present = 1;

        return TRUE;
}

static void af2apri(audio_format *fmt, audio_prinfo_t *ap)
{
        assert(fmt);
        assert(ap);
        ap->sample_rate = fmt->sample_rate;
        ap->channels    = fmt->channels;
        ap->precision   = fmt->bits_per_sample;
	ap->buffer_size   = DEVICE_BUF_UNIT * (fmt->sample_rate / 8000) * (fmt->bits_per_sample / 8);

        switch(fmt->encoding) {
        case DEV_PCMU: ap->encoding = AUDIO_ENCODING_ULAW;   assert(ap->precision == 8);  break;
        case DEV_S8:   ap->encoding = AUDIO_ENCODING_LINEAR; assert(ap->precision == 8);  break;
        case DEV_S16:  ap->encoding = AUDIO_ENCODING_LINEAR; assert(ap->precision == 16); break;
        default: debug_msg("Format not recognized\n"); assert(0); 
        }
}


/* Try to open the audio device.                        */
/* Returns a TRUE if ok, FALSE otherwise.               */
int
osprey_audio_open(audio_desc_t ad, audio_format *ifmt, audio_format *ofmt)
{
        char audctl_device[16] = "/dev/o1kctl0";
        char audio_device[16]  = "/dev/o1k0";

        if (audio_fd != -1) {
                osprey_audio_close(ad);
                debug_msg("Osprey device was not closed\n");
        }

        signal(SIGPOLL, osprey_poll_handler);

	audio_fd = oti_open(audio_device, O_RDWR | O_NONBLOCK);

        if (audio_fd <= 0) {
                audio_fd = -1;
                debug_msg("Could not open Osprey Device\n");
                return FALSE;
        }
#ifndef OSPREY_USE_SIGNALS
        if (oti_ioctl(audio_fd, I_SETSIG, (void*)(S_INPUT | S_OUTPUT | S_MSG)) < AUDIO_SUCCESS) {
                perror("I_SETSIG ioctl(audio_fd)");
                exit(-1);
        }
#endif /* OSPREY_USE_SIGNALS */
        if ((audioctl_fd = oti_open(audctl_device, O_RDWR))<0){
                fprintf(stderr,"could not open audio ctl device\n");
                osprey_audio_close(ad);
                return FALSE;
        }   
#ifndef OSPREY_USE_SIGNALS
        if (oti_ioctl(audioctl_fd, I_SETSIG, (void*)(S_MSG)) < 0){
                perror("I_SETSIG ioctl(audioctl_fd)");
                osprey_audio_close(ad);
                return FALSE;
        }
#endif /* OSPREY_USE_SIGNALS */

        AUDIO_INITINFO(&dev_info);
        dev_info.monitor_gain       = 0;
        dev_info.output_muted       = 0; /* 0==not muted */
        af2apri(ifmt, &dev_info.record);
        af2apri(ofmt, &dev_info.play);
        dev_info.play.gain          = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
        dev_info.record.gain        = (AUDIO_MAX_GAIN - AUDIO_MIN_GAIN) * 0.75;
        dev_info.play.port          = AUDIO_HEADPHONE;
        dev_info.record.port        = AUDIO_MICROPHONE;
        dev_info.play.balance       = AUDIO_MID_BALANCE;
        dev_info.record.balance     = AUDIO_MID_BALANCE;

        if (oti_audio_setinfo(audio_fd, &dev_info) != AUDIO_SUCCESS) {
                perror("Setting info");
                if (ifmt->encoding == DEV_S16) {
                        debug_msg("Attempting ulaw...\n");
                        audio_format_change_encoding(ifmt, DEV_PCMU);
                        audio_format_change_encoding(ofmt, DEV_PCMU);
                        af2apri(ifmt, &dev_info.record);
                        af2apri(ofmt, &dev_info.play);
                        if (ioctl(audio_fd, AUDIO_SETINFO, (caddr_t)&dev_info) < 0) {
                                perror("Setting MULAW audio paramterts");
                                osprey_audio_close(0);
                                return FALSE;
                        }
                }
        }       
        
        return TRUE;
}

/* Close the audio device */
void
osprey_audio_close(audio_desc_t ad)
{
        UNUSED(ad);

        if (audio_fd != -1) {
/*                oti_audio_pause(audio_fd);
                oti_audio_flush(audio_fd); */
                oti_close(audio_fd);
        }

        if (audioctl_fd != -1)
                oti_close(audioctl_fd);

        audio_fd = audioctl_fd = -1;
}

/* Flush input buffer */
void
osprey_audio_drain(audio_desc_t ad)
{
/*        audio_info_t tmpinfo;
        oti_audio_drain(audio_fd, FALSE);  
        oti_audio_pause(audio_fd);
        AUDIO_INITINFO(&tmpinfo);
        oti_audio_getinfo(audio_fd, &tmpinfo);
        tmpinfo.record.pause   = 0;
        tmpinfo.record.samples = 0;
        tmpinfo.record.error   = 0;
        tmpinfo.play.pause     = 0;
        tmpinfo.play.samples   = 0;
        tmpinfo.play.error     = 0;

        oti_audio_setinfo(audio_fd, &tmpinfo); 

        oti_audio_resume(audio_fd); */

        UNUSED(ad);
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
osprey_audio_set_igain(int ad, int gain)
{
        double igain;

	if (audio_fd <= 0)
		return;

        igain = OTI_RAT_TO_DEVICE(gain);

        oti_audio_set_play_gain(audio_fd, &igain);

        UNUSED(ad);
}

int
osprey_audio_get_igain(audio_desc_t ad)
{
        double igain;

        UNUSED(ad); assert(audio_fd > 0);
        
        oti_audio_get_play_gain(audio_fd, &igain);

	return (OTI_DEVICE_TO_RAT(igain));
}

void
osprey_audio_set_ogain(audio_desc_t ad, int vol)
{
        double ogain;

        UNUSED(ad); assert(audio_fd > 0);
        
        ogain = OTI_RAT_TO_DEVICE(vol);
        oti_audio_set_record_gain(audio_fd, &ogain);
}

int
osprey_audio_get_ogain(audio_desc_t ad)
{
        double ogain;

        UNUSED(ad); assert(audio_fd > 0);
	
        oti_audio_get_record_gain(audio_fd, &ogain);

	return (OTI_DEVICE_TO_RAT(ogain));
}

void
osprey_audio_loopback(audio_desc_t ad, int gain)
{
        double mgain;

        UNUSED(ad); assert(audio_fd > 0);

        mgain = OTI_RAT_TO_DEVICE(gain);
        oti_audio_set_monitor_gain(audio_fd, &mgain);
}

int
osprey_audio_read(audio_desc_t ad, u_char *buf, int buf_bytes)
{
        int len = 0;

        UNUSED(ad); assert(audio_fd > 0);

        if (oti_ioctl(audio_fd, FIONREAD, &len) < 0) {
                debug_msg("FIONREAD failed\n");
                return 0;
        }

        len = min(buf_bytes, len);

        len = oti_read(audio_fd, (char *)buf, len * BYTES_PER_SAMPLE);

        if (len < 0) {
                debug_msg("read failed\n", len); 
                return 0;
        } 

        return max(0,len);
}

int
osprey_audio_write(audio_desc_t ad, u_char *buf, int buf_bytes)
{
        int done = 0, len = buf_bytes;

        UNUSED(ad); assert(audio_fd > 0);

	while (len > 0) {
		if ((done = oti_write(audio_fd, buf, len)) == len)
			break;
		if (errno != EINTR)
			return (buf_bytes - done);
		len -= done;
		buf += done;
	}

	return (buf_bytes);
}

/* Set ops on audio device to be non-blocking */
void
osprey_audio_non_block(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        debug_msg("audio_non_block: device always non-blocking\n");
}

/* Set ops on audio device to block */
void
osprey_audio_block(audio_desc_t ad)
{
        UNUSED(ad); assert(audio_fd > 0);
        debug_msg("audio_block: device always non-blocking\n");
}

void
osprey_audio_set_oport(audio_desc_t ad, int port)
{
        UNUSED(ad); assert(audio_fd > 0);
        oti_audio_set_record_port(audio_fd, (unsigned int*)&port);
}

int
osprey_audio_get_oport(audio_desc_t ad)
{
        unsigned int port;
 
        UNUSED(ad); assert(audio_fd > 0);

        oti_audio_get_record_port(audio_fd, &port);

        return (int)port;
}

int
osprey_audio_next_oport(audio_desc_t ad)
{
	int	port, max_port;

        UNUSED(ad); assert(audio_fd > 0);

	AUDIO_INITINFO(&dev_info);
        if (oti_audio_getinfo(audio_fd, &dev_info) < 0)
                perror("Getting port");
	
	port = dev_info.play.port;
	port <<= 1;

        /* oti reports upto 6 playback ports - only 3 are enumerated:
         * AUDIO_MICROPHONE, AUDIO_LINE_IN, AUDIO_CD 
         */
        max_port = min(dev_info.play.avail_ports, 3);

        if (port >= (1 << max_port))
                port = 1;

        oti_audio_set_play_port(audio_fd, (unsigned int *)&port);

	return (port);
}

void
osprey_audio_set_iport(audio_desc_t ad, int port)
{
        UNUSED(ad); assert(audio_fd > 0);

        oti_audio_set_record_port(audio_fd, (unsigned int*)&port); 
}

int
osprey_audio_get_iport(audio_desc_t ad)
{
        int port;

        UNUSED(ad); assert(audio_fd > 0);

        oti_audio_get_record_port(audio_fd, (unsigned int*)&port); 
	return port;
}

int
osprey_audio_next_iport(audio_desc_t ad)
{
	int	port, max_port;

        UNUSED(ad); assert(audio_fd > 0);
	
	AUDIO_INITINFO(&dev_info);
	if (ioctl(audio_fd, AUDIO_GETINFO, (caddr_t)&dev_info) < 0)
		perror("Getting port");

	port = dev_info.record.port;
	port <<= 1;
        
        /* Port should be one of AUDIO_MICROPHONE, AUDIO_LINE_IN, AUDIO_CD (==AUDIO_INTERNAL_CD_IN) */

        max_port = min(dev_info.play.avail_ports, 3);

        if (port >= (1 << max_port))
                port = 1;

        oti_audio_set_record_port(audio_fd, (unsigned int *)&port);

	return (port);
}

int
osprey_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad);
        return 1;
}

int 
osprey_audio_is_ready(audio_desc_t ad)
{
        int len;

        audio_info_t tmpinfo;

        UNUSED(ad); assert(audio_fd > 0);

        oti_audio_getinfo(audio_fd, &tmpinfo);

        if (oti_ioctl(audio_fd, FIONREAD, &len) < 0) {
                debug_msg("FIONREAD failed\n");
                return FALSE;
        }
        debug_msg("%d bytes ready\n", len);

        return len;
}

void
osprey_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        int len;
        struct timeval tv;

#ifdef OSPREY_USE_SIGNALS
        signal(SIGPOLL, osprey_poll_handler);
#endif /* OSPREY_USE_SIGNALS */

        len = osprey_audio_is_ready(ad);
        
        if (len != 0) {
                return;
        }

        tv.tv_sec = 0;
        tv.tv_usec = delay_ms * 1000;
        select(0, NULL, NULL, NULL, &tv); 
}

static void 
osprey_audio_service()
{
        static int inservice = FALSE;

        if (inservice) return;
        inservice = TRUE;
        debug_msg("osprey audio service\n");

        inservice = FALSE;
}

int
osprey_audio_device_count()
{
        return present;
}

const char*
osprey_audio_device_name(audio_desc_t ad)
{
        UNUSED(ad);
        return "Osprey Audio Device";
}

#endif /* HAVE_OSPREY */
