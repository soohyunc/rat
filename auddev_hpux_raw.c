/*
 * auddev_hpux_raw.c -- Audio device interface for HP/UX
 *
 * By Terje Vernly <terjeve@usit.uio.no>
 * Modifications by Geir Harald Hansen <g.h.hansen@usit.uio.no>
 * Modifications by Colin Perkins <c.perkins@cs.ucl.ac.uk>
 * Modifications by Dirk Meersman <meersman@visiehp4.ruca.ua.ac.be>
 *
 * $Revision$
 * $Date$
 *
 */

#ifdef HPUX
#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"

#define NSAMPLES	4096
#define OUT_BUFFER_SIZE (NSAMPLES/sizeof(short))
#define IN_BUFFER_SIZE  (NSAMPLES/sizeof(short))
static short *ibuf, *obuf;
static short *iptr, *optr, *iend, *oend;

#define MIN_RX_GAIN_2    0
#define MAX_RX_GAIN_2   22
#define MIN_TX_GAIN_2  -84
#define MAX_TX_GAIN_2    9

/*
 * Try to open the audio device.
 * Return TRUE if successful FALSE otherwise.
 */
int
audio_open(audio_format format)
{
  int audio_fd = -1;

  assert(format.encoding        == DEV_L16);
  assert(format.sample_rate     == 8000);
  assert(format.bits_per_sample == 16);
  assert(format.channels    == 1);

  /* set up input(record) and output(play) buffers */

  if (!ibuf) ibuf = (short *)xmalloc(sizeof(short) * IN_BUFFER_SIZE);
  if (!obuf) obuf = (short *)xmalloc(sizeof(short) * OUT_BUFFER_SIZE);
  iptr = ibuf; iend = &ibuf[IN_BUFFER_SIZE];
  optr = obuf; oend = &obuf[OUT_BUFFER_SIZE];

  if (!ibuf || !obuf)
    perror("audio_open: out of memory");
  else if ((audio_fd = open("/dev/audio", O_RDWR, 0)) != -1) {
    struct audio_select_thresholds ast;

    ast.read_threshold = ast.write_threshold = 4096; /* 8192; */
    if (ioctl(audio_fd,AUDIO_SET_SEL_THRESHOLD, &ast) == -1)
      perror("audio_open: ioctl(AUDIO_SET_SEL_THRESHOLD)");
    if (ioctl(audio_fd, AUDIO_SET_DATA_FORMAT,
	      AUDIO_FORMAT_LINEAR16BIT) == -1)
      perror("audio_open: ioctl(AUDIO_SET_DATA_FORMAT)");
    if (ioctl(audio_fd, AUDIO_SET_CHANNELS, 1) == -1)
      perror("audio_open: ioctl(AUDIO_SET_CHANNELS)");
    if (ioctl(audio_fd, AUDIO_SET_SAMPLE_RATE, 8000) == -1)
      perror("audio_open: ioctl(AUDIO_SET_SAMPLE_RATE)");
    if (ioctl(audio_fd, AUDIO_SET_RXBUFSIZE, 32768) == -1)
      perror("audio_open: ioctl(AUDIO_SET_RXBUFSIZE)");
    if (ioctl(audio_fd, AUDIO_SET_TXBUFSIZE, 32768) == -1)
      perror("audio_open: ioctl(AUDIO_SET_TXBUFSIZE)");

    if (ioctl(audio_fd, AUDIO_RESET,
	      (RESET_RX_BUF|RESET_TX_BUF|RESET_RX_OVF|RESET_TX_UNF)) == -1)
      perror("audio_open: ioctl(AUDIO_RESET)");
    if (ioctl(audio_fd, AUDIO_RESUME, (AUDIO_TRANSMIT|AUDIO_RECEIVE)) == -1)
      perror("audio_open: ioctl(AUDIO_RESUME)");
    if (fcntl(audio_fd, F_SETFL, O_NONBLOCK) == -1)
      perror("audio_open: fcntl(O_NONBLOCK)");
  } else
    perror("audio_open: open()");

  return audio_fd;
}

/* Close the audio device */
void
audio_close(int audio_fd)
{
  if (ibuf) {
    xfree(ibuf);
    ibuf = NULL;
  }
  if (obuf) {
    xfree(obuf);
    obuf = NULL;
  }
  if (audio_fd >= 0) {
    close(audio_fd);
  }
}

/* Flush input buffer */
void
audio_drain(int audio_fd)
{
  if (ioctl(audio_fd, AUDIO_DRAIN) == -1) {
    perror("audio_drain: ioctl()");
  }
}

/* Gain and volume values are in the range 0 - MAX_AMP */

void
audio_set_gain(int audio_fd, int gain)
{
  if (audio_fd >= 0) {
    struct audio_gain ag;

    if (ioctl(audio_fd, AUDIO_GET_GAINS, &ag) == 0) {
      int rxgain = ((gain * (MAX_RX_GAIN_2 - MIN_RX_GAIN_2)) / MAX_AMP) +
	MIN_RX_GAIN_2;
      ag.cgain[0].receive_gain = ag.cgain[1].receive_gain = rxgain;
      ag.channel_mask = AUDIO_CHANNEL_RIGHT | AUDIO_CHANNEL_LEFT;
      if (ioctl(audio_fd, AUDIO_SET_GAINS, &ag) != 0) {
	perror("audio_set_gain: ioctl()");
      }
    } else {
      perror("audio_set_gain: ioctl()");
    }
  }
}

int
audio_get_gain(int audio_fd)
{
  int gain = 0;

  if (audio_fd >= 0) {
    struct audio_gain ag;

    if (ioctl(audio_fd, AUDIO_GET_GAINS, &ag) == 0) {
      gain=((ag.cgain[0].receive_gain - MIN_RX_GAIN_2) * MAX_AMP) /
	(MAX_RX_GAIN_2 - MIN_RX_GAIN_2);
    } else {
      perror("audio_get_gain: ioctl()");
    }
  }
  return gain;
}

void
audio_set_volume(int audio_fd, int vol)
{
  if (audio_fd >= 0) {
    struct audio_gain ag;

    if (ioctl(audio_fd, AUDIO_GET_GAINS, &ag) == 0) {
      int txgain=((vol*(MAX_TX_GAIN_2 - MIN_TX_GAIN_2)) / MAX_AMP) +
	MIN_TX_GAIN_2;
      ag.cgain[0].transmit_gain = ag.cgain[1].transmit_gain = txgain;
      ag.channel_mask = AUDIO_CHANNEL_RIGHT | AUDIO_CHANNEL_LEFT;
      if (ioctl(audio_fd,AUDIO_SET_GAINS, &ag) != 0) {
	perror("audio_set_volume: ioctl()");
      }
    } else {
      perror("audio_set_volume: ioctl()");
    }
  }
}

int
audio_get_volume(int audio_fd)
{
  int vol = 0;

  if (audio_fd >= 0) {
    struct audio_gain ag;

    if (ioctl(audio_fd,AUDIO_GET_GAINS, &ag) == 0) {
      vol=((ag.cgain[0].transmit_gain - MIN_TX_GAIN_2) * MAX_AMP) /
	(MAX_TX_GAIN_2 - MIN_TX_GAIN_2);
    } else {
      perror("audio_get_volume: ioctl()");
    }
  }
  return vol;
}

int
audio_read(int audio_fd, sample *buf, int samples)
{
  int len = 0;
  fd_set rfd;
  struct timeval tout = { 0, 0 };

  len = min(IN_BUFFER_SIZE * sizeof(short), samples * BYTES_PER_SAMPLE);

  assert(buf != NULL);   /* We can't write into a non-existant buffer */
  assert(samples != 0);  /* Reading no data is silly....              */

  FD_ZERO(&rfd);
  FD_SET(audio_fd, &rfd);
#ifdef HPUX_10
  if (select(FD_SETSIZE, &rfd, NULL, NULL, &tout) <= 0)
#else  
  if (select(FD_SETSIZE, (int*)&rfd, NULL, NULL, &tout) <= 0)
#endif  	
    return 0;

  if ((len = read(audio_fd, buf, samples * BYTES_PER_SAMPLE)) < 0) {
    if (errno == EIO) {
      struct audio_status as;
      if (ioctl(audio_fd, AUDIO_GET_STATUS, &as) == 0 && as.receive_status == AUDIO_PAUSE) {
	ioctl(audio_fd, AUDIO_RESUME, AUDIO_RECEIVE);
      }
    }
    return 0;
  }
  return len / BYTES_PER_SAMPLE;
}

int
audio_write(int audio_fd, sample *buf, int samples)
{
        int	done, len;
 
        len = samples * BYTES_PER_SAMPLE;
        while (1) {
                if ((done = write(audio_fd, buf, len)) == len) {
                        break;
                }
                if (errno != EINTR) {
                        return samples - ((len - done) / BYTES_PER_SAMPLE);
                }
                len -= done;
                buf += done;
        }
        return samples;
}

/* Set ops on audio device to be non-blocking */
void
audio_non_block(int audio_fd)
{
  /* Do nothing... */
}

/* Set ops on audio device to block */
void
audio_block(int audio_fd)
{
  /* Do nothing... */
}

/*
 * Check to see whether another application has requested the
 * audio device.
 */
int
audio_requested(int audio_fd)
{
  return FALSE;
}

void
audio_set_oport(int audio_fd, int port)
{
  if (audio_fd >= 0) {
    if (ioctl(audio_fd, AUDIO_SET_OUTPUT, port) != 0) {
      perror("audio_set_oport: ioctl()");
    }
  }
}

int
audio_get_oport(int audio_fd)
{
  int port = AUDIO_OUT_SPEAKER;

  if (audio_fd >= 0) {
    if (ioctl(audio_fd, AUDIO_GET_OUTPUT, &port) != 0) {
      perror("audio_set_oport: ioctl()");
    }
  }
  return port;
}

int
audio_next_oport(int audio_fd)
{
  int port;

  port = audio_get_oport(audio_fd);

  if (audio_fd >= 0) {
    if ((port <<= 1) > AUDIO_OUT_LINE)
      port = AUDIO_OUT_SPEAKER;
    audio_set_oport(audio_fd, port);
  }
  return port;
}

void
audio_set_iport(int audio_fd, int port)
{
  if (audio_fd >= 0) {
    if (ioctl(audio_fd, AUDIO_SET_INPUT, port) != 0) {
      perror("audio_set_oport: ioctl()");
    }
  }
}

int
audio_get_iport(int audio_fd)
{
  int port = AUDIO_IN_MIKE;

  if (audio_fd >= 0) {
    if (ioctl(audio_fd, AUDIO_GET_INPUT, &port) != 0) {
      perror("audio_set_oport: ioctl()");
    }
  }
  return port;
}

int
audio_next_iport(int audio_fd)
{
  int port;

  port = audio_get_iport(audio_fd);

  if (audio_fd >= 0) {
    if ((port <<= 1) > AUDIO_IN_LINE)
      port = AUDIO_IN_MIKE;
    audio_set_iport(audio_fd, port);
  }
  return port;
}

void
audio_switch_out(int audio_fd, cushion_struct *ap)
{
  /* Full duplex device: do nothing! */
}

void
audio_switch_in(int audio_fd)
{
  /* Full duplex device: do nothing! */
}

int
audio_duplex(int audio_fd)
{
  return 1;
}

#endif /* HPUX */
