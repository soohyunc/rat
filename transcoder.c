/*
 * FILE:     transcoder.c
 * PROGRAM:  Rat
 * AUTHOR:   Colin Perkins
 *
 * Based on auddev_mux.c, revision 1.11
 *
 * $Revision$
 * $Date$
 *
 * Copyright (C) 1996,1997 University College London
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

#include "transcoder.h"
#include "audio.h"
#include "util.h"

#define CHANNEL_SIZE  	8192
#define NUM_CHANNELS 	2

static int             num_open_channels = 0;
static sample         *channel[NUM_CHANNELS+1];
static int             head[NUM_CHANNELS+1];
static int             tail[NUM_CHANNELS+1];
static struct timeval  last_time[NUM_CHANNELS+1];
static struct timeval  curr_time[NUM_CHANNELS+1];
static int             first_time[NUM_CHANNELS+1];

int
transcoder_open(void)
{
  /* Open a fake audio channel. The value we return is used to identify the */
  /* channel for the other routines in this module. The #ifdefs in net.c    */
  /* prevent it being used in a select().                                   */
  /* Note: We must open EXACTLY two channels, before the other routines in  */
  /*       this module function correctly.                                  */
  int id, i;

  assert((num_open_channels >= 0) && (num_open_channels < NUM_CHANNELS));
  id = ++num_open_channels;
  head[id]       = 0;
  tail[id]       = 0;
  first_time[id] = 0;
  channel[id] = (sample *) xmalloc(CHANNEL_SIZE * sizeof(sample));
  for (i=0; i<CHANNEL_SIZE; i++) {
    channel[id][i] = L16_AUDIO_ZERO;
  }
  return id;
}

void
transcoder_close(int id)
{
  assert(num_open_channels > 0);
  assert(id > 0 && id <= num_open_channels);
  xfree(channel[id]);
  num_open_channels--;
}

int
transcoder_read(int id, sample *buf, int buf_size)
{
  int i, read_size, copy_size;

  assert(buf != 0);
  assert(buf_size > 0);
  assert(id > 0 && id <= num_open_channels);
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);
  
  if (first_time[id] == 0) {
    gettimeofday(&last_time[id], NULL);
    first_time[id] = 1;
  }
  gettimeofday(&curr_time[id], NULL);
  read_size = (((curr_time[id].tv_sec - last_time[id].tv_sec) * 1e6) + (curr_time[id].tv_usec - last_time[id].tv_usec)) / 125;
  if (read_size > buf_size) read_size = buf_size;
  for (i=0; i<read_size; i++) {
    buf[i] = L16_AUDIO_ZERO;
  }
  last_time[id] = curr_time[id];

  copy_size = tail[id] - head[id];	/* The amount of data available in this module... */
  if (copy_size >= read_size) {
    copy_size = read_size;
  } else {
#ifdef DEBUG_TRANSCODER
    printf("transcoder_read: underflow, silence substituted -- want %d got %d channel %d\n", read_size, copy_size, id);
#endif
  }
  assert((head[id] + copy_size) <= tail[id]);
  for (i=0; i<copy_size; i++) {
    buf[i] = channel[id][head[id] + i];
  }
  head[id] += copy_size;

  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);
  return read_size;
}

int
transcoder_write(int id, sample *buf, int buf_size)
{
  int i;

  assert(buf != 0);
  assert(buf_size > 0);
  assert(id > 0 && id <= num_open_channels);
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);

  if ((tail[id] + buf_size) > CHANNEL_SIZE) {
    for (i=0; i < (CHANNEL_SIZE - head[id]); i++) {
      channel[id][i] = channel[id][i + head[id]];
    }
    tail[id] -= head[id];
    head[id]  = 0;
  }
  assert((tail[id] + buf_size) <= CHANNEL_SIZE);

  for (i=0; i<buf_size; i++) {
    channel[id][tail[id] + i] = buf[i];
  }
  tail[id] += buf_size;
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);
  return buf_size;
}

