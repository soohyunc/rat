/*
 * FILE:    auddev_netbsd.c - RAT driver for NetBSD audio devices
 * PROGRAM: RAT
 * AUTHOR:  Brook Milligan
 *
 * $Id$
 *
 * Copyright (c) 2002-2004 Brook Milligan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
"$Id$";
#endif				/* HIDE_SOURCE_STRINGS */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/audioio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "config_unix.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "auddev_netbsd.h"
#include "codec_g711.h"
#include "memory.h"
#include "debug.h"

#define DEBUG_MIXER 0

/*
 * Path to audio/mixer devices.  In addition to the base names given
 * here, a series of devices with numerical suffixes beginning with 0
 * are also probed.  This series includes at least min_devices-2
 * (e.g., /dev/sound0-/dev/sound7 for min_devices=9), but continues
 * beyond that until the first failure when opening a device.
 */
static char audio_path[MAXPATHLEN] = "/dev/sound";
static char mixer_path[MAXPATHLEN] = "/dev/mixer";
static int min_devices = 1 + 8;

/*
 * Information on each audio device
 */
typedef struct audio_dinfo {
	char *dev_name;		/* Audio device name */
	int fd;			/* File descriptor */
	audio_device_t audio_dev;	/* Kernel device info. */
	int audio_props;	/* Audio device properties */
	audio_encoding_t *audio_enc;	/* Supported kernel encodings */
}           audio_dinfo_t;
/*
 * Information on each mixer device
 */
typedef struct mixer_dinfo {
	char *dev_name;		/* Mixer device name */
	int fd;			/* File descriptor */
	mixer_ctrl_t play_gain;	/* Kernel gain controls */
	mixer_ctrl_t record_gain;
	mixer_ctrl_t loopback_gain;
	int max_play_gain;	/* Maximum gains */
	int max_record_gain;
	int max_loopback_gain;
	int preamp_check;	/* Check if preamp is turned off? */
}           mixer_dinfo_t;
/*
 * Table of all detected audio devices and their corresponding mixer devices
 */
typedef struct audio_devices {
	char *full_name;
	audio_dinfo_t audio_info;
	mixer_dinfo_t mixer_info;
}             audio_devices_t;
audio_devices_t *audio_devices = NULL;
static int n_devices = 0;

/*
 * I/O port mappings between the kernel and rat.
 */
static audio_port_details_t in_ports[] = {
	{AUDIO_MICROPHONE, AUDIO_PORT_MICROPHONE},
	{AUDIO_LINE_IN, AUDIO_PORT_LINE_IN},
	{AUDIO_CD, AUDIO_PORT_CD}
};
#define NETBSD_NUM_INPORTS (sizeof(in_ports) / sizeof(in_ports[0]))

static audio_port_details_t out_ports[] = {
	{AUDIO_SPEAKER, AUDIO_PORT_SPEAKER},
	{AUDIO_HEADPHONE, AUDIO_PORT_HEADPHONE},
	{AUDIO_LINE_OUT, AUDIO_PORT_LINE_OUT}
};
#define NETBSD_NUM_OUTPORTS (sizeof(out_ports) / sizeof(out_ports[0]))

/*
 * Encoding mappings between the kernel and rat
 */
struct audio_encoding_match {
	int netbsd_encoding;
	deve_e rat_encoding;
};
static struct audio_encoding_match audio_encoding_match[] = {
	{AUDIO_ENCODING_ULAW, DEV_PCMU},
	{AUDIO_ENCODING_ALAW, DEV_PCMA},
	{AUDIO_ENCODING_ULINEAR_LE, DEV_U8},
	{AUDIO_ENCODING_ULINEAR_BE, DEV_U8},
	{AUDIO_ENCODING_ULINEAR, DEV_U8},
	{AUDIO_ENCODING_SLINEAR_LE, DEV_S8},
	{AUDIO_ENCODING_SLINEAR_BE, DEV_S8},
	{AUDIO_ENCODING_SLINEAR, DEV_S8},
	{AUDIO_ENCODING_SLINEAR_LE, DEV_S16},
	{AUDIO_ENCODING_SLINEAR_BE, DEV_S16},
	{AUDIO_ENCODING_SLINEAR, DEV_S16},
	{AUDIO_ENCODING_NONE, 0}
};
/*
 * Gain control mixer devices
 */
struct mixer_devices {
	const char *class;
	const char *device;
};
/*
 * Possible mixer play gain devices
 */
static struct mixer_devices mixer_play_gain[] = {
	{AudioCinputs, AudioNdac},
	{AudioCoutputs, AudioNmaster},
	{NULL, NULL}
};
/*
 * Possible mixer record gain devices
 */
static struct mixer_devices mixer_record_gain[] = {
	{AudioCrecord, AudioNmicrophone},
	{AudioCinputs, AudioNmicrophone},
	{AudioCrecord, AudioNvolume},
	{NULL, NULL}
};
/*
 * Possible loopback gain devices
 */
static struct mixer_devices mixer_loopback_gain[] = {
	{AudioCinputs, AudioNmixerout},
	{AudioCinputs, AudioNspeaker},
	{NULL, NULL}
};


static int probe_device(const char *, const char *);
static int probe_audio_device(const char *, audio_dinfo_t *);
static int probe_mixer_device(const char *, mixer_dinfo_t *);
static mixer_ctrl_t
match_mixer_device(int fd,
    mixer_devinfo_t * mixers, int n_mixers,
    struct mixer_devices * devices);
static void set_mode(audio_desc_t ad);
static void set_audio_properties(audio_desc_t ad);
static int get_mixer_gain(int fd, mixer_ctrl_t * mixer_info);
static void set_mixer_gain(int fd, mixer_ctrl_t * mixer_info, int gain);
static u_char average_mixer_level(mixer_ctrl_t mixer_info);
static void check_record_preamp(int fd);
static audio_encoding_t *get_encoding(audio_desc_t ad, audio_format * fmt);
static audio_encoding_t *
set_encoding(audio_desc_t ad, audio_format * ifmt,
    audio_format * ofmt);
static audio_encoding_t *
set_alt_encoding(audio_desc_t ad, audio_format * ifmt,
    audio_format * ofmt);
static int audio_select(audio_desc_t ad, int delay_us);


/*
 * Conversion to/from kernel device gains
 */
#define netbsd_rat_to_device(gain,max_gain)                                 \
	((gain) * (max_gain - AUDIO_MIN_GAIN) / MAX_AMP + AUDIO_MIN_GAIN)
#define netbsd_device_to_rat(gain,max_gain)                                 \
	(((gain) - AUDIO_MIN_GAIN) * MAX_AMP / (max_gain - AUDIO_MIN_GAIN))


/*
 * netbsd_audio_init: initialize data structures
 *
 * Determine how many audio/mixer devices are available.  Return: TRUE
 * if number of devices is greater than 0, FALSE otherwise
 */

int
netbsd_audio_init()
{
	int i, found;
	char audio_dev_name[MAXPATHLEN];
	char mixer_dev_name[MAXPATHLEN];
	char dev_index[MAXPATHLEN];

#if DEBUG_MIXER
	warnx("netbsd_audio_init()");
#endif
	probe_device(audio_path, mixer_path);
	i = 0;
	found = TRUE;
	while (i < min_devices - 1 || found) {
		if (snprintf(dev_index, MAXPATHLEN, "%d", i) >= MAXPATHLEN)
			break;
		strcpy(audio_dev_name, audio_path);
		strcat(audio_dev_name, dev_index);
		strcpy(mixer_dev_name, mixer_path);
		strcat(mixer_dev_name, dev_index);
		found = probe_device(audio_dev_name, mixer_dev_name);
		++i;
	}
#if DEBUG_MIXER
	warnx("netbsd_audio_init():  n_devices=%d", n_devices);
#endif
	return n_devices > 0;
}


/*
 * netbsd_audio_device_count: return number of available audio devices
 */

int
netbsd_audio_device_count()
{
	return n_devices;
}


/*
 * netbsd_audio_device_name: return the full audio device name
 */

char *
netbsd_audio_device_name(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	return audio_devices[ad].full_name;
}


/*
 * netbsd_audio_open: try to open the audio and mixer devices
 *
 * Return: valid file descriptor if ok, -1 otherwise.
 */

int
netbsd_audio_open(audio_desc_t ad, audio_format * ifmt, audio_format * ofmt)
{
	int fd;
	int full_duplex = 1;
	audio_info_t dev_info;
	audio_encoding_t *encp;

	assert(audio_devices && n_devices > ad
	    && audio_devices[ad].audio_info.fd == -1);

	warnx("opening %s (audio device %s, mixer device %s)",
	    audio_devices[ad].full_name,
	    audio_devices[ad].audio_info.dev_name,
	    audio_devices[ad].mixer_info.dev_name);
	debug_msg("Opening %s (audio device %s, mixer device %s)\n",
	    audio_devices[ad].full_name,
	    audio_devices[ad].audio_info.dev_name,
	    audio_devices[ad].mixer_info.dev_name);

	fd = open(audio_devices[ad].audio_info.dev_name, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		/*
		 * Because we opened the device with O_NONBLOCK, the
		 * wait flag was not updated so update it manually.
		 */
		debug_msg("netbsd_audio_open(): setting wait flag.\n");
		fd = open(audio_devices[ad].audio_info.dev_name, O_WRONLY);
		if (fd < 0) {
			AUDIO_INITINFO(&dev_info);
			dev_info.play.waiting = 1;
			(void) ioctl(fd, AUDIO_SETINFO, &dev_info);
			close(fd);
		}
		return -1;
	}
	audio_devices[ad].audio_info.fd = fd;

	if (!(audio_devices[ad].audio_info.audio_props
		& AUDIO_PROP_INDEPENDENT)
	    && !audio_format_match(ifmt, ofmt)) {
		warnx("NetBSD audio: independent i/o formats not supported.");
		close(fd);
		audio_devices[ad].audio_info.fd = -1;
		return -1;
	}
	if (ifmt->bytes_per_block != ofmt->bytes_per_block) {
		warnx("NetBSD audio: "
		    "independent i/o block sizes not supported.");
		close(fd);
		audio_devices[ad].audio_info.fd = -1;
		return -1;
	}
	if (ioctl(fd, AUDIO_FLUSH, NULL) < 0) {
		perror("netbsd_audio_open: flushing device");
		close(fd);
		audio_devices[ad].audio_info.fd = -1;
		return -1;
	}
	if (ioctl(fd, AUDIO_SETFD, &full_duplex) < 0) {
		perror("setting full duplex");
		return FALSE;
	}
	if ((encp = set_encoding(ad, ifmt, ofmt)) == NULL) {
		if ((encp = set_alt_encoding(ad, ifmt, ofmt)) == NULL) {
			perror("netbsd_audio_open: "
			    "no audio encodings supported");
			close(fd);
			audio_devices[ad].audio_info.fd = -1;
			return -1;
		}
	}
	warnx("NetBSD audio format: "
	    "%d Hz, %d bits/sample, %d %s, "
	    "requested rat encoding %d, "
	    "mapped kernel encoding %s)",
	    ifmt->sample_rate, ifmt->bits_per_sample,
	    ifmt->channels, ifmt->channels == 1 ? "channel" : "channels",
	    ifmt->encoding, encp->name);
	if (encp->flags & AUDIO_ENCODINGFLAG_EMULATED)
		warnx("NetBSD %s support is emulated", encp->name);

	set_mode(ad);
	set_audio_properties(ad);
	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_open: getting parameters");
		close(fd);
		audio_devices[ad].audio_info.fd = -1;
		return -1;
	}
	audio_devices[ad].mixer_info.fd = -1;
	if (audio_devices[ad].mixer_info.dev_name) {
		fd = open(audio_devices[ad].mixer_info.dev_name,
		    O_RDWR | O_NONBLOCK);
		if (fd >= 0 && audio_devices[ad].mixer_info.preamp_check) {
			audio_devices[ad].mixer_info.fd = fd;
			if (audio_devices[ad].mixer_info.record_gain.type
			    == AUDIO_MIXER_VALUE)
				check_record_preamp(fd);
			audio_devices[ad].mixer_info.preamp_check = FALSE;
		}
	}
	return audio_devices[ad].audio_info.fd;
}


/*
 * netbsd_audio_close: close audio and mixer devices (if open)
 */

void
netbsd_audio_close(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);

	if (audio_devices[ad].audio_info.fd >= 0) {
		/* Flush device first */
		if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_FLUSH,
			NULL) < 0)
			perror("netbsd_audio_close: flushing device");
		(void) close(audio_devices[ad].audio_info.fd);
	}
	if (audio_devices[ad].mixer_info.fd >= 0) {
		(void) close(audio_devices[ad].mixer_info.fd);
	}
	warnx("closing %s (audio device %s, mixer device %s)",
	    audio_devices[ad].full_name,
	    audio_devices[ad].audio_info.dev_name,
	    audio_devices[ad].mixer_info.dev_name);
	debug_msg("Closing %s (audio device %s, mixer device %s)\n",
	    audio_devices[ad].full_name,
	    audio_devices[ad].audio_info.dev_name,
	    audio_devices[ad].mixer_info.dev_name);

	audio_devices[ad].audio_info.fd = -1;
	audio_devices[ad].mixer_info.fd = -1;
}


/*
 * netbsd_audio_drain: drain audio buffers
 */

void
netbsd_audio_drain(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_DRAIN, NULL) < 0)
		perror("netbsd_audio_drain");
}


/*
 * netbsd_audio_duplex: check duplex flag for audio device
 *
 * Return: duplex flag, or 0 on failure (shouldn't happen)
 */

int
netbsd_audio_duplex(audio_desc_t ad)
{
	int duplex;

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return 0;

	if (ioctl(audio_devices[ad].audio_info.fd, AUDIO_GETFD, &duplex) < 0) {
		perror("netbsd_audio_duplex: cannot get duplex mode");
		return 0;
	}
	return duplex;
}


/*
 * netbsd_audio_read: read data from audio device
 *
 * Return: number of bytes read
 */

int
netbsd_audio_read(audio_desc_t ad, u_char * buf, int buf_bytes)
{
	int fd;
	int bytes_read = 0;
	int this_read;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	while (buf_bytes > 0) {
		this_read = read(audio_devices[ad].audio_info.fd,
		    (char *) buf, buf_bytes);
		if (this_read < 0) {
			if (errno != EAGAIN && errno != EINTR)
				perror("netbsd_audio_read");
			return bytes_read;
		}
		bytes_read += this_read;
		buf += this_read;
		buf_bytes -= this_read;
	}
	return bytes_read;
}


/*
 * netbsd_audio_write: write data to audio device
 *
 * Return: number of bytes written
 */

int
netbsd_audio_write(audio_desc_t ad, u_char * buf, int buf_bytes)
{
	int fd;
	int bytes_written = 0;
	int this_write;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	while (buf_bytes > 0) {
		this_write = write(fd, buf, buf_bytes);
		if (this_write < 0) {
			if (errno != EAGAIN && errno != EINTR)
				perror("netbsd_audio_write");
			return bytes_written;
		}
		bytes_written += this_write;
		buf += this_write;
		buf_bytes -= this_write;
	}

	return bytes_written;
}


/*
 * netbsd_audio_non_block: set audio device to non-blocking I/O
 */

void
netbsd_audio_non_block(audio_desc_t ad)
{
	int on = 1;		/* Enable non-blocking I/O */

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, FIONBIO, &on) < 0)
		perror("netbsd_audio_non_block");
}


/*
 * netbsd_audio_block: set audio device to blocking I/O
 */

void
netbsd_audio_block(audio_desc_t ad)
{
	int on = 0;		/* Disable non-blocking I/O */

	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	if (ioctl(audio_devices[ad].audio_info.fd, FIONBIO, &on) < 0)
		perror("netbsd_audio_block");
}


/*
 * netbsd_audio_iport_set: set input port
 */

void
netbsd_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
	int fd;
	audio_info_t dev_info;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return;

	AUDIO_INITINFO(&dev_info);
	dev_info.record.port = port;
	if (ioctl(fd, AUDIO_SETINFO, &dev_info) < 0)
		warnx("netbsd_audio_iport_set: "
		    "cannot set input port %d: %s",
		    port, strerror(errno));
}


/*
 * netbsd_audio_iport_get: get information on input port
 */

audio_port_t
netbsd_audio_iport_get(audio_desc_t ad)
{
	int fd;
	audio_info_t dev_info;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_iport_get: getting device parameters");
		return 0;
	}
	return dev_info.record.port;
}


/*
 * netbsd_audio_oport_set: set output port
 */

void
netbsd_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
	int fd;
	audio_info_t dev_info;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return;

	/*
	 * Some drivers report no available ports, because the mixer cannot
	 * change output ports.
	 */
	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_oport_set: getting device parameters");
		return;
	}
	if (dev_info.play.avail_ports) {
		AUDIO_INITINFO(&dev_info);
		dev_info.play.port = port;
		if (ioctl(fd, AUDIO_SETINFO, &dev_info) < 0) {
			warnx("NetBSD audio driver cannot set output port");
		}
	}
}


/*
 * netbsd_audio_oport_get: get information on output port
 */

audio_port_t
netbsd_audio_oport_get(audio_desc_t ad)
{
	int fd;
	audio_info_t dev_info;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_oport_get: getting device parameters");
		return 0;
	}
	return dev_info.play.port;
}


/*
 * netbsd_audio_set_igain: set record gain (percent (%) of maximum)
 */

void
netbsd_audio_set_igain(audio_desc_t ad, int gain)
{
	int fd;
	audio_info_t audio_info;
	mixer_ctrl_t mixer_info;
	int max_gain;

	assert(audio_devices && n_devices > ad);

	fd = audio_devices[ad].mixer_info.fd;
	if (fd >= 0) {
		mixer_info = audio_devices[ad].mixer_info.record_gain;
		max_gain = audio_devices[ad].mixer_info.max_record_gain;
		set_mixer_gain(fd, &mixer_info,
		    netbsd_rat_to_device(gain, max_gain));
		audio_devices[ad].mixer_info.record_gain = mixer_info;
		return;
	}
	fd = audio_devices[ad].audio_info.fd;
	if (fd >= 0) {
		AUDIO_INITINFO(&audio_info);
		audio_info.record.gain =
		    netbsd_rat_to_device(gain, AUDIO_MAX_GAIN);
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_igain: "
			    "setting audio parameters");
			return;
		}
	}
}


/*
 * netbsd_audio_get_igain: get record gain (percent (%) of maximum)
 */

int
netbsd_audio_get_igain(audio_desc_t ad)
{
	int fd;
	audio_info_t audio_info;
	mixer_ctrl_t mixer_info;
	int gain, max_gain;

	assert(audio_devices && n_devices > ad);

	fd = audio_devices[ad].mixer_info.fd;
	if (fd >= 0) {
		mixer_info = audio_devices[ad].mixer_info.record_gain;
		if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0) {
			perror("netbsd_audio_get_igain: "
			    "getting mixer parameters");
			return 0;
		}
		audio_devices[ad].mixer_info.record_gain = mixer_info;
		gain = average_mixer_level(mixer_info);
		max_gain = audio_devices[ad].mixer_info.max_record_gain;
		return netbsd_device_to_rat(gain, max_gain);
	}
	fd = audio_devices[ad].audio_info.fd;
	if (fd >= 0) {
		if (ioctl(fd, AUDIO_GETINFO, &audio_info) < 0) {
			perror("netbsd_audio_get_igain: "
			    "getting audio parameters");
			return 0;
		}
		return netbsd_device_to_rat(audio_info.record.gain,
		    AUDIO_MAX_GAIN);
	}
	return 0;
}


/*
 * netbsd_audio_set_ogain: set play (output) gain (percent (%) of maximum)
 */

void
netbsd_audio_set_ogain(audio_desc_t ad, int gain)
{
	int fd;
	audio_info_t audio_info;
	mixer_ctrl_t mixer_info;
	int max_gain;

	assert(audio_devices && n_devices > ad);

	fd = audio_devices[ad].mixer_info.fd;
	if (fd >= 0) {
		mixer_info = audio_devices[ad].mixer_info.play_gain;
		if (mixer_info.type == AUDIO_MIXER_VALUE) {
			max_gain = audio_devices[ad].mixer_info.max_play_gain;
			set_mixer_gain(fd, &mixer_info,
			    netbsd_rat_to_device(gain, max_gain));
			audio_devices[ad].mixer_info.play_gain = mixer_info;
			return;
		}
	}
	fd = audio_devices[ad].audio_info.fd;
	if (fd >= 0) {
		AUDIO_INITINFO(&audio_info);
		audio_info.play.gain =
		    netbsd_rat_to_device(gain, AUDIO_MAX_GAIN);
		if (ioctl(fd, AUDIO_SETINFO, &audio_info) < 0) {
			perror("netbsd_audio_set_ogain: "
			    "setting audio parameters");
			return;
		}
	}
}


/*
 * netbsd_audio_get_ogain: get play (output) gain (percent (%) of maximum)
 */

int
netbsd_audio_get_ogain(audio_desc_t ad)
{
	int fd;
	audio_info_t audio_info;
	mixer_ctrl_t mixer_info;
	int gain, max_gain;

	assert(audio_devices && n_devices > ad);

	fd = audio_devices[ad].mixer_info.fd;
	if (fd >= 0) {
		mixer_info = audio_devices[ad].mixer_info.play_gain;
		if (mixer_info.type == AUDIO_MIXER_VALUE) {
			if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0) {
				perror("netbsd_audio_get_ogain: "
				    "getting mixer parameters");
				return 0;
			}
			audio_devices[ad].mixer_info.play_gain = mixer_info;
			gain = average_mixer_level(mixer_info);
			max_gain = audio_devices[ad].mixer_info.max_play_gain;
			return netbsd_device_to_rat(gain, max_gain);
		}
	}
	fd = audio_devices[ad].audio_info.fd;
	if (fd >= 0) {
		if (ioctl(fd, AUDIO_GETINFO, &audio_info) < 0) {
			perror("netbsd_audio_get_igain: "
			    "getting audio parameters");
			return 0;
		}
		return netbsd_device_to_rat(audio_info.play.gain,
		    AUDIO_MAX_GAIN);
	}
	return 0;
}


/*
 * netbsd_audio_iport_details: get kernel-rat device mappings for input port
 */

const audio_port_details_t *
netbsd_audio_iport_details(audio_desc_t ad, int idx)
{
	assert(audio_devices && n_devices > ad);
	assert((unsigned) idx < NETBSD_NUM_INPORTS);
	return &in_ports[idx];
}


/*
 * netbsd_audio_oport_details: get kernel-rat device mappings for output port
 */

const audio_port_details_t *
netbsd_audio_oport_details(audio_desc_t ad, int idx)
{
	assert(audio_devices && n_devices > ad);
	assert((unsigned) idx < NETBSD_NUM_OUTPORTS);
	return &out_ports[idx];
}


/*
 * netbsd_audio_iport_count: get number of available input ports.
 */

int
netbsd_audio_iport_count(audio_desc_t ad)
{
	int fd;
	audio_info_t dev_info;
	audio_port_t n = 0;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_iport_count: getting parameters");
		return 0;
	}
	while (dev_info.record.avail_ports != 0) {
		if (dev_info.record.avail_ports & 1)
			n++;
		dev_info.record.avail_ports >>= 1;
	}
	return n;
}


/*
 * netbsd_audio_oport_count: get number of available output ports
 */

int
netbsd_audio_oport_count(audio_desc_t ad)
{
	int fd;
	audio_info_t dev_info;
	int n = 0;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return 0;

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_oport_count: getting parameters");
		return 0;
	}
	while (dev_info.play.avail_ports != 0) {
		if (dev_info.play.avail_ports & 1)
			n++;
		dev_info.play.avail_ports >>= 1;
	}
	/*
	 * Some drivers report no available ports, because the mixer
	 * cannot change output ports.  Assume one port is available
	 * in these cases.
	 */
	if (n == 0)
		n = 1;
	return n;
}


/*
 * netbsd_audio_loopback: set loopback gain (percent (%) of maximum)
 */

void
netbsd_audio_loopback(audio_desc_t ad, int gain)
{
	int fd;
	mixer_ctrl_t mixer_info;
	int max_gain;

	assert(audio_devices && n_devices > ad);

	fd = audio_devices[ad].mixer_info.fd;
	if (fd >= 0) {
		mixer_info = audio_devices[ad].mixer_info.loopback_gain;
		max_gain = audio_devices[ad].mixer_info.max_loopback_gain;
		set_mixer_gain(fd, &mixer_info,
		    netbsd_rat_to_device(gain, max_gain));
		audio_devices[ad].mixer_info.loopback_gain = mixer_info;
	}
	/* Nothing to do; loopback gain is not available via audio device */
}


/*
 * netbsd_audio_is_ready: is audio device ready for reading?
 */

int
netbsd_audio_is_ready(audio_desc_t ad)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return FALSE;

	return audio_select(ad, 0);
}


/*
 * netbsd_audio_wait_for: delay the process
 */

void
netbsd_audio_wait_for(audio_desc_t ad, int delay_ms)
{
	assert(audio_devices && n_devices > ad);
	if (audio_devices[ad].audio_info.fd < 0)
		return;

	audio_select(ad, delay_ms * 1000);
}


/*
 * netbsd_audio_supports: does the driver support format fmt?
 */

int
netbsd_audio_supports(audio_desc_t ad, audio_format * fmt)
{
	int fd;
	audio_info_t dev_info;	/* Save state to restore later */
	audio_encoding_t *supported;

	assert(audio_devices && n_devices > ad);
	fd = audio_devices[ad].audio_info.fd;
	if (fd < 0)
		return FALSE;

	/*
	 * Note that the fmt argument is not necessarily correctly
	 * filled by rat.  This function is called by
	 * audio_device_supports() in the file auddev.c, which does
	 * not initialize the bits_per_sample field in fmt.  Calling
	 * audio_format_change_encoding() ensures that all fields are
	 * initialized.
	 */
	audio_format_change_encoding(fmt, fmt->encoding);

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("netbsd_audio_supports: getting device parameters");
		return FALSE;
	}
	supported = set_encoding(ad, fmt, fmt);
	debug_msg("NetBSD audio: "
	    "%d Hz, %d bits/sample, %d %s, "
	    "requested rat encoding %d, "
	    "mapped kernel encoding %s)",
	    fmt->sample_rate, fmt->bits_per_sample,
	    fmt->channels, fmt->channels == 1 ? "channel" : "channels",
	    fmt->encoding, supported ? supported->name : "none");
	return supported != NULL;
}


/*
 * auddev_netbsd_setfd: set full duplex mode
 *
 * This function is needed for OSS support, because the
 * SNDCTL_DSP_SETDUPLEX ioctl is not implemented in NetBSD.
 */

int
auddev_netbsd_setfd(int fd)
{
	int full_duplex = 1;
	int error;

	error = ioctl(fd, AUDIO_SETFD, &full_duplex);
	if (error < 0)
		perror("setting full duplex");

	return error;
}


/*
 * probe_device: probe audio and mixer devices
 */

int
probe_device(const char *audio_dev_name, const char *mixer_dev_name)
{
	audio_dinfo_t audio_info;
	mixer_dinfo_t mixer_info;

	if (!probe_audio_device(audio_dev_name, &audio_info))
		return FALSE;

	probe_mixer_device(mixer_dev_name, &mixer_info);
	audio_devices = (audio_devices_t *) realloc
	    (audio_devices, (n_devices + 1) * sizeof(audio_devices_t));
	audio_devices[n_devices].full_name
	    = (char *) malloc(strlen(audio_info.audio_dev.name)
	    + strlen(audio_info.dev_name)
	    + 4 + 1);
	strcpy(audio_devices[n_devices].full_name,
	    audio_info.audio_dev.name);
	strcat(audio_devices[n_devices].full_name, " -- ");
	strcat(audio_devices[n_devices].full_name, audio_info.dev_name);
	audio_devices[n_devices].audio_info = audio_info;
	audio_devices[n_devices].mixer_info = mixer_info;
	++n_devices;
	return TRUE;
}


/*
 * probe_audio_device: probe audio device
 */

int
probe_audio_device(const char *dev_name, audio_dinfo_t * audio_info)
{
	int fd;
	audio_device_t audio_dev;
	int audio_props;
	audio_encoding_t aenc;
	audio_encoding_t *audio_enc;
	int nenc;
	int i;

	debug_msg("probe_audio_device(%s)\n", dev_name);
	if ((fd = open(dev_name, O_RDONLY | O_NONBLOCK)) == -1)
		return FALSE;

	if (ioctl(fd, AUDIO_GETDEV, &audio_dev) < 0) {
		close(fd);
		return FALSE;
	}
	if (ioctl(fd, AUDIO_GETPROPS, &audio_props) < 0) {
		perror("getting audio device properties");
		close(fd);
		return FALSE;
	}
	if (!(audio_props & AUDIO_PROP_FULLDUPLEX)) {
		warnx("skipping %s; only full duplex devices supported.",
		    audio_info->dev_name);
		return FALSE;
	}
	for (nenc = 0;; nenc++) {
		aenc.index = nenc;
		if (ioctl(fd, AUDIO_GETENC, &aenc) < 0)
			break;
	}
	audio_enc = calloc(nenc + 1, sizeof *audio_enc);

	for (i = 0; i < nenc; i++) {
		audio_enc[i].index = i;
		ioctl(fd, AUDIO_GETENC, &audio_enc[i]);
	}
	close(fd);

	audio_info->dev_name = strdup(dev_name);
	audio_info->fd = -1;
	audio_info->audio_dev = audio_dev;
	audio_info->audio_props = audio_props;
	audio_info->audio_enc = audio_enc;
	return TRUE;
}


/*
 * probe_mixer_device: probe device mixer
 */

int
probe_mixer_device(const char *dev_name, mixer_dinfo_t * mixer_info)
{
	int fd;
	mixer_devinfo_t devinfo;
	mixer_devinfo_t *mixers;
	int n_mixers = 0;
	mixer_ctrl_t minfo;
	int i;
	int ok;

	debug_msg("probe_mixer_device(%s)\n", dev_name);
	memset(mixer_info, 1, sizeof(*mixer_info));
	mixer_info->fd = -1;
	mixer_info->preamp_check = TRUE;

	if ((fd = open(dev_name, O_RDONLY | O_NONBLOCK)) == -1)
		return FALSE;

	/* Count number of mixer devices */
	while (1) {
		devinfo.index = n_mixers;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &devinfo) < 0)
			break;
		++n_mixers;
	}
	if (n_mixers <= 0)
		return FALSE;
	mixers = calloc(n_mixers, sizeof(*mixers));

	/* Get kernel information on mixer devices */
	for (i = 0; i < n_mixers; i++) {
		mixers[i].index = i;
		ioctl(fd, AUDIO_MIXER_DEVINFO, &mixers[i]);
	}

	/* Find suitable mixer devices */
	mixer_info->play_gain = match_mixer_device
	    (fd, mixers, n_mixers, mixer_play_gain);
	mixer_info->record_gain = match_mixer_device
	    (fd, mixers, n_mixers, mixer_record_gain);
	mixer_info->loopback_gain = match_mixer_device
	    (fd, mixers, n_mixers, mixer_loopback_gain);

	free(mixers);

	ok = mixer_info->play_gain.type == AUDIO_MIXER_VALUE
	    || mixer_info->record_gain.type == AUDIO_MIXER_VALUE
	    || mixer_info->loopback_gain.type == AUDIO_MIXER_VALUE;

	if (ok)
		mixer_info->dev_name = strdup(dev_name);

	/*
	 * Find maximum mixer gains.  Note that the maximum gain for
	 * any given mixer device is hardware dependent and may be
	 * less than AUDIO_MAX_GAIN.  The maximum gain is stored so
	 * that the rat controls will act consistently across mixer
	 * devices.
	 */
	minfo = mixer_info->play_gain;
	if (minfo.type == AUDIO_MIXER_VALUE) {
		set_mixer_gain(fd, &minfo, AUDIO_MAX_GAIN);
		mixer_info->max_play_gain = get_mixer_gain(fd, &minfo);
		set_mixer_gain(fd, &minfo, AUDIO_MIN_GAIN);
	} else
		mixer_info->max_play_gain = 0;
	minfo = mixer_info->record_gain;
	if (minfo.type == AUDIO_MIXER_VALUE) {
		set_mixer_gain(fd, &minfo, AUDIO_MAX_GAIN);
		mixer_info->max_record_gain = get_mixer_gain(fd, &minfo);
		set_mixer_gain(fd, &minfo, AUDIO_MIN_GAIN);
	} else
		mixer_info->max_record_gain = 0;
	minfo = mixer_info->loopback_gain;
	if (minfo.type == AUDIO_MIXER_VALUE) {
		set_mixer_gain(fd, &minfo, AUDIO_MAX_GAIN);
		mixer_info->max_loopback_gain = get_mixer_gain(fd, &minfo);
		set_mixer_gain(fd, &minfo, AUDIO_MIN_GAIN);
	} else
		mixer_info->max_loopback_gain = 0;

	close(fd);

	/* Report matched mixer devices */
	if (ok) {
		warnx("NetBSD mixer gain controls: %s", dev_name);
		if (mixer_info->play_gain.type == AUDIO_MIXER_VALUE) {
			int i = mixer_info->play_gain.dev;
#if DEBUG_MIXER
			warnx("  output gain:   %s.%s [%d]",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name, i);
#else
			warnx("  output gain:   %s.%s",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name);
#endif
		}
		if (mixer_info->record_gain.type == AUDIO_MIXER_VALUE) {
			int i = mixer_info->record_gain.dev;
#if DEBUG_MIXER
			warnx("  record gain:   %s.%s [%d]",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name, i);
#else
			warnx("  record gain:   %s.%s",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name);
#endif
		}
		if (mixer_info->loopback_gain.type == AUDIO_MIXER_VALUE) {
			int i = mixer_info->loopback_gain.dev;
#if DEBUG_MIXER
			warnx("  loopback gain: %s.%s [%d]",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name, i);
#else
			warnx("  loopback gain: %s.%s",
			    mixers[mixers[i].mixer_class].label.name,
			    mixers[i].label.name);
#endif
		}
	} else
		warnx("no NetBSD mixer gain contols found: %s", dev_name);

	return ok;
}


/*
 * match_mixer_device
 */

mixer_ctrl_t
match_mixer_device(int fd,
    mixer_devinfo_t * mixers, int n_mixers,
    struct mixer_devices * devices)
{
	mixer_ctrl_t mixer_info;
	mixer_devinfo_t *m;
	struct mixer_devices *d;
	const char *class_name;
	const char *device_name;

	for (d = devices; d->class != NULL; ++d) {
#if DEBUG_MIXER
		warnx("match_mixer_device():  target=%s.%s",
		    d->class, d->device);
#endif
		for (m = mixers; m < mixers + n_mixers; ++m) {
			if (m->type != AUDIO_MIXER_VALUE)
				continue;
			class_name = mixers[m->mixer_class].label.name;
			device_name = m->label.name;
#if DEBUG_MIXER
			warnx("  %d. %s.%s", m - mixers,
			    class_name, device_name);
#endif
			if (strcmp(class_name, d->class) != 0)
				continue;
			if (strcmp(device_name, d->device) != 0)
				continue;
#if DEBUG_MIXER
			warnx("match_mixer_device():  matched");
#endif
			mixer_info.dev = m->index;
			mixer_info.type = m->type;
			mixer_info.un.value.num_channels =
			    m->un.v.num_channels;
			if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0) {
				perror("match_mixer_device");
				continue;
			}
			return mixer_info;
		}
	}
	mixer_info.dev = 0;
	mixer_info.type = 0;
	return mixer_info;
}


/*
 * set_mode: set kernel audio device mode
 *
 * Note that AUMODE_PLAY_ALL is critical here to ensure continuous
 * audio output.  Rat attempts to manage the timing of the output
 * audio stream; it is important that the kernel not attempt to do the
 * same.
 */

void
set_mode(audio_desc_t ad)
{
	int fd = audio_devices[ad].audio_info.fd;
	audio_info_t dev_info;

	if (ioctl(fd, AUDIO_GETINFO, &dev_info) < 0) {
		perror("set_mode: getting parameters");
		return;
	}
	if (!(dev_info.mode & AUMODE_PLAY_ALL)
	    || !(dev_info.mode & AUMODE_RECORD)) {
		AUDIO_INITINFO(&dev_info);
		dev_info.mode = AUMODE_PLAY_ALL | AUMODE_RECORD;
		if (ioctl(fd, AUDIO_SETINFO, &dev_info) < 0) {
			perror("setting play/record mode");
		}
	}
}


/*
 * set_audio_properties: set audio device properties
 *
 * Initialize kernel audio device balance and record port.  Note that
 * audio gains remain as before.
 */

void
set_audio_properties(audio_desc_t ad)
{
	int fd = audio_devices[ad].audio_info.fd;
	audio_info_t dev_info;

	AUDIO_INITINFO(&dev_info);

	/* Input port setup */
	dev_info.record.port = AUDIO_MICROPHONE;
	dev_info.record.balance = AUDIO_MID_BALANCE;

	/* Output port setup */
	dev_info.play.balance = AUDIO_MID_BALANCE;

	if (ioctl(fd, AUDIO_SETINFO, &dev_info) < 0) {
		perror("setting properties");
		return;
	}
}


/*
 * get mixer gain (kernel units)
 */

int
get_mixer_gain(int fd, mixer_ctrl_t * mixer_info)
{
	assert(fd >= 0);
	assert(mixer_info->type == AUDIO_MIXER_VALUE);

	if (ioctl(fd, AUDIO_MIXER_READ, mixer_info) < 0) {
		perror("get_mixer_gain: getting mixer parameters");
		return 0;
	}
	return average_mixer_level(*mixer_info);
}


/*
 * set_mixer_gain (kernel units)
 */

void
set_mixer_gain(int fd, mixer_ctrl_t * mixer_info, int gain)
{
	mixer_ctrl_t devinfo;
	int i;

	assert(fd >= 0);
	assert(mixer_info->type == AUDIO_MIXER_VALUE);

	devinfo = *mixer_info;
	if (ioctl(fd, AUDIO_MIXER_READ, &devinfo) < 0) {
		perror("set_mixer_gain: reading mixer parameters");
		return;
	}
	for (i = 0; i < devinfo.un.value.num_channels; ++i)
		devinfo.un.value.level[i] = gain;

	if (ioctl(fd, AUDIO_MIXER_WRITE, &devinfo) < 0) {
		perror("set_mixer_gain: setting mixer parameters");
		return;
	}
	*mixer_info = devinfo;
}


/*
 * average_mixer_level: calculate the average across channels
 */

u_char
average_mixer_level(mixer_ctrl_t mixer_info)
{
	int i;
	int num_channels = mixer_info.un.value.num_channels;
	int sum = 0;

	for (i = 0; i < num_channels; ++i)
		sum += mixer_info.un.value.level[i];
	return sum / num_channels;
}


/*
 * check_record_preamp: warn about potential preamps turned off
 *
 * Rat provides only a minimal interface to the mixer device.  Because
 * there is no control for this, users are often unaware that the
 * microphone preamp is off when it should be on.  This provides a
 * reminder.
 */

void
check_record_preamp(int fd)
{
	int n_mixers = 0;
	mixer_devinfo_t *mixers;
	mixer_devinfo_t mixer;
	mixer_devinfo_t *m;
	mixer_ctrl_t mixer_info;
	const char *class_name;
	const char *parent_name;
	const char *device_name;
	int i, j;
	int off;

	/* Count number of mixer devices */
	while (1) {
		mixer.index = n_mixers;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mixer) < 0)
			break;
		++n_mixers;
	}
	if (n_mixers <= 0)
		return;

	mixers = calloc(n_mixers, sizeof(*mixers));

	for (i = 0; i < n_mixers; i++) {
		mixers[i].index = i;
		ioctl(fd, AUDIO_MIXER_DEVINFO, &mixers[i]);
	}

	/* Search for preamp device */
	for (m = mixers; m < mixers + n_mixers; ++m) {
		parent_name = NULL;
		for (j = m->prev; j != AUDIO_MIXER_LAST; j = mixers[j].prev)
			parent_name = mixers[j].label.name;
		class_name = mixers[m->mixer_class].label.name;
		device_name = m->label.name;
		if (strcmp(device_name, AudioNpreamp) != 0)
			continue;
		if (m->type != AUDIO_MIXER_ENUM)
			continue;
		mixer_info.dev = m->index;
		mixer_info.type = m->type;
		if (ioctl(fd, AUDIO_MIXER_READ, &mixer_info) < 0) {
			perror("checking NetBSD preamp");
			continue;
		}
		if (mixer_info.type != AUDIO_MIXER_ENUM)
			continue;
		for (off = 0; off < m->un.e.num_mem; ++off)
			if (strcmp(m->un.e.member[off].label.name, AudioNoff)
			    == 0)
				break;
		/* Preamp off? */
		if (mixer_info.un.ord == m->un.e.member[off].ord) {
			warnx("mixerctl %s.%s.%s=%s; consider setting it %s",
			    class_name, parent_name, device_name,
			    AudioNoff, AudioNon);
			break;
		}
	}

	free(mixers);
}


/*
 * get_encoding: get audio device encoding matching fmt
 *
 * Find the best match among supported encodings.  Prefer encodings
 * that are not emulated in the kernel.
 */

audio_encoding_t *
get_encoding(audio_desc_t ad, audio_format * fmt)
{
	audio_encoding_t *best_encp = NULL;
	struct audio_encoding_match *match_encp;

	match_encp = audio_encoding_match;
	while (match_encp->netbsd_encoding != AUDIO_ENCODING_NONE) {
		if (match_encp->rat_encoding == fmt->encoding) {
			audio_encoding_t *encp
			= audio_devices[ad].audio_info.audio_enc;
			while (encp->precision != 0) {
				if (encp->encoding
				    == match_encp->netbsd_encoding
				    && encp->precision == fmt->bits_per_sample
				    && (best_encp == NULL
					|| (best_encp->flags &
					    AUDIO_ENCODINGFLAG_EMULATED)
					|| !(encp->flags &
					    AUDIO_ENCODINGFLAG_EMULATED)))
					best_encp = encp;
				++encp;
			}
		}
		++match_encp;
	}
	return best_encp;
}


/*
 * set_encoding: set audio device I/O encoding
 *
 * Inform kernel of selected input/output encodings, channels, precision, ... .
 */

audio_encoding_t *
set_encoding(audio_desc_t ad, audio_format * ifmt, audio_format * ofmt)
{
	int fd = audio_devices[ad].audio_info.fd;
	audio_info_t dev_info;
	audio_encoding_t *ienc, *oenc;

	AUDIO_INITINFO(&dev_info);
	assert(ifmt->bytes_per_block == ofmt->bytes_per_block);
	/* XXX - driver may not set blocksize? */
	dev_info.blocksize = ifmt->bytes_per_block;

	/* Input port setup */
	dev_info.record.sample_rate = ifmt->sample_rate;
	dev_info.record.channels = ifmt->channels;
	dev_info.record.precision = ifmt->bits_per_sample;
	if ((ienc = get_encoding(ad, ifmt)) == NULL)
		return NULL;
	dev_info.record.encoding = ienc->encoding;

	/* Output port setup */
	dev_info.play.sample_rate = ofmt->sample_rate;
	dev_info.play.channels = ofmt->channels;
	dev_info.play.precision = ofmt->bits_per_sample;
	if ((oenc = get_encoding(ad, ofmt)) == NULL)
		return NULL;
	dev_info.play.encoding = oenc->encoding;

	if (ioctl(fd, AUDIO_SETINFO, &dev_info) < 0)
		return NULL;

	if (ienc != oenc)
		warnx("mismatched NetBSD kernel input/output encoding: "
		    "%s != %s", ienc->name, oenc->name);

	return ienc;
}


/*
 * set_alt_encoding: set alternate audio device I/O encoding
 *
 * Select an alternate supported encoding and inform kernel of same.
 * Prefer encodings that are not emulated in the kernel.
 */

audio_encoding_t *
set_alt_encoding(audio_desc_t ad, audio_format * ifmt, audio_format * ofmt)
{
	audio_encoding_t *best_encp = NULL;
	audio_format best_fmt, fmt;
	struct audio_encoding_match *match_encp;

	fmt = *ifmt;
	match_encp = audio_encoding_match;
	while (match_encp->netbsd_encoding != AUDIO_ENCODING_NONE) {
		audio_encoding_t *encp;
		audio_format_change_encoding(&fmt, match_encp->rat_encoding);
		encp = get_encoding(ad, &fmt);
		if (encp != NULL
		    && netbsd_audio_supports(ad, &fmt)
		    && (best_encp == NULL
			|| (best_encp->flags & AUDIO_ENCODINGFLAG_EMULATED)
			|| !(encp->flags & AUDIO_ENCODINGFLAG_EMULATED))) {
			best_encp = encp;
			best_fmt = fmt;
		}
		++match_encp;
	}
	if (best_encp != NULL) {
		*ifmt = best_fmt;
		*ofmt = best_fmt;
		return set_encoding(ad, ifmt, ofmt);
	}
	return NULL;
}


/*
 * audio_select: wait for delay_us microseconds
 */

int
audio_select(audio_desc_t ad, int delay_us)
{
	int fd;
	fd_set rfds;
	struct timeval tv;

	fd = audio_devices[ad].audio_info.fd;
	tv.tv_sec = 0;
	tv.tv_usec = delay_us;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	select(fd + 1, &rfds, NULL, NULL, &tv);

	return FD_ISSET(fd, &rfds) ? TRUE : FALSE;
}
