/*
 * FILE:    lbl_confbus.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 *
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1997 University College London
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

#include "lbl_confbus.h"
#include "session.h"
#include "audio.h"
#include "net.h"

#define LBL_CB_ADDR 	0xe0ffdeef
#define LBL_CB_PORT 	0xdeaf
#define LBL_CB_MAGIC	0x0ef5ee0a
#define CB_BUF_SIZE    	1024

typedef struct {
        int     magic;
        short   type;
        short   pid;
        char    buf[1];
} lbl_cb_msg;

void lbl_cb_init(session_struct *sp)
{
  char loop = 1;
  char addr = 1;

  sp->lbl_cb_base_socket = sock_init(LBL_CB_ADDR, LBL_CB_PORT, 0);
  setsockopt(sp->lbl_cb_base_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
  setsockopt(sp->lbl_cb_base_socket, IPPROTO_IP, SO_REUSEADDR, &addr, sizeof(addr));
}

void lbl_cb_init_channel(session_struct *sp, int channel)
{
  char loop = 1;
  char addr = 1;

  sp->lbl_cb_channel_socket = sock_init(LBL_CB_ADDR, LBL_CB_PORT+channel, 0);
  setsockopt(sp->lbl_cb_channel_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
  setsockopt(sp->lbl_cb_channel_socket, IPPROTO_IP, SO_REUSEADDR, &addr, sizeof(addr));
  sp->lbl_cb_channel = channel;
}

void lbl_cb_process(session_struct *sp, lbl_cb_msg *msg, int channel)
{
  char *cmd;

  assert(ntohl(msg->magic) == LBL_CB_MAGIC);
  
  cmd = strtok(msg->buf, " ");
  if (strcmp(cmd, "audio-release") == 0) {
    int	request_pid = atoi(strtok(NULL, " "));
    if ((request_pid == getpid()) || (request_pid == 0)) {
      audio_device_take(sp);
    }
    return;
  }
  if (strcmp(cmd, "audio-request") == 0) {
    int	request_pid = atoi(strtok(NULL, " "));
    int request_pri = atoi(strtok(NULL, " "));
    if (request_pri > sp->lbl_cb_priority) {
      if (sp->have_device && sp->keep_device == FALSE) {
        audio_device_give(sp);
        lbl_cb_send_release(sp, request_pid);
      }
    }
    return;
  }
  if (strcmp(cmd, "audio-demand") == 0) {
    int	request_pid = atoi(strtok(NULL, " "));
    if (request_pid == getpid())  {
      /* This is a loop-back message, ignore it! */
      return;
    }
    if (sp->have_device && sp->keep_device == FALSE) {
      audio_device_give(sp);
      lbl_cb_send_release(sp, request_pid);
    }
    return;
  }
  if (strcmp(cmd, "focus") == 0) {
    /* Focus is used by vic, we don't need to process it... */
    return;
  }
#ifdef DEBUG
  printf("LBL ConfBus: unknown message %d %d %d %s\n", channel, ntohs(msg->type), ntohs(msg->pid), msg->buf);
#endif
}

void lbl_cb_read(session_struct *sp)
{
  fd_set		fds;
  int		 	sel_fd;
  struct timeval 	timeout;
  char			buffer[CB_BUF_SIZE];
  lbl_cb_msg		*msg = (lbl_cb_msg *) buffer;
  int			read_len;

  timeout.tv_sec  = 0;
  timeout.tv_usec = 0;

  FD_ZERO(&fds);
  FD_SET(sp->lbl_cb_base_socket, &fds);
  if (sp->lbl_cb_channel_socket != -1) {
    FD_SET(sp->lbl_cb_channel_socket, &fds);
  }

  sel_fd = max(sp->lbl_cb_base_socket, sp->lbl_cb_channel_socket) + 1;

#ifdef HPUX
  if (select(sel_fd, (int *) &fds, NULL, NULL, &timeout) > 0) {
#else
  if (select(sel_fd, &fds, NULL, NULL, &timeout) > 0) {
#endif
    if (FD_ISSET(sp->lbl_cb_base_socket, &fds)) {
      read_len = read(sp->lbl_cb_base_socket, buffer, CB_BUF_SIZE);
      if (ntohl(msg->magic) == LBL_CB_MAGIC) {
	lbl_cb_process(sp, msg, 0);
      }
    }
    if ((sp->lbl_cb_channel_socket != -1) && FD_ISSET(sp->lbl_cb_channel_socket, &fds)) {
      read_len = read(sp->lbl_cb_channel_socket, buffer, CB_BUF_SIZE);
      if (ntohl(msg->magic) == LBL_CB_MAGIC) {
	lbl_cb_process(sp, msg, sp->lbl_cb_channel);
      }
    }
  }
}

void lbl_cb_send_focus(session_struct *sp, char *cname)
{
  char		 	 buffer[CB_BUF_SIZE];
  lbl_cb_msg		*msg = (lbl_cb_msg *) buffer;
  struct sockaddr_in	 dest;
  u_long		 addr = htonl(LBL_CB_ADDR);
  int			 ret;

  if (sp->lbl_cb_channel_socket == -1) {
    /* Conference bus is not being used! */
    return;
  }

  memcpy((char *) &dest.sin_addr.s_addr, (char *) &addr, sizeof(addr));
  dest.sin_family = AF_INET;
  dest.sin_port   = htons(LBL_CB_PORT + sp->lbl_cb_channel);
  msg->magic = htonl(LBL_CB_MAGIC);
  msg->type  = htons(0);
  msg->pid   = htons(getpid());
  sprintf(msg->buf, "focus %s", cname);
  if ((ret = sendto(sp->lbl_cb_channel_socket, buffer, strlen(msg->buf) + 9, 0, (struct sockaddr *) &dest, sizeof(dest))) < 0) {
    perror("lbl_cb_send_focus: sendto");
  }
}

void lbl_cb_send_release(session_struct *sp, int pid)
{
  char		 	 buffer[CB_BUF_SIZE];
  lbl_cb_msg		*msg = (lbl_cb_msg *) buffer;
  struct sockaddr_in	 dest;
  u_long		 addr = htonl(LBL_CB_ADDR);
  int			 ret;

  if (sp->lbl_cb_base_socket == -1) {
    /* Conference bus is not being used! */
    return;
  }

  memcpy((char *) &dest.sin_addr.s_addr, (char *) &addr, sizeof(addr));
  dest.sin_family = AF_INET;
  dest.sin_port   = htons(LBL_CB_PORT);
  msg->magic = htonl(LBL_CB_MAGIC);
  msg->type  = htons(0);
  msg->pid   = htons(getpid());
  sprintf(msg->buf, "audio-release %d", pid);
  if ((ret = sendto(sp->lbl_cb_base_socket, buffer, strlen(msg->buf) + 9, 0, (struct sockaddr *) &dest, sizeof(dest))) < 0) {
    perror("lbl_cb_send_release: sendto");
  }
}

void lbl_cb_send_request(session_struct *sp)
{
  char		 	 buffer[CB_BUF_SIZE];
  lbl_cb_msg		*msg = (lbl_cb_msg *) buffer;
  struct sockaddr_in	 dest;
  u_long		 addr = htonl(LBL_CB_ADDR);
  int			 ret;

  if (sp->lbl_cb_base_socket == -1) {
    /* Conference bus is not being used! */
    return;
  }

  memcpy((char *) &dest.sin_addr.s_addr, (char *) &addr, sizeof(addr));
  dest.sin_family = AF_INET;
  dest.sin_port   = htons(LBL_CB_PORT);
  msg->magic = htonl(LBL_CB_MAGIC);
  msg->type  = htons(0);
  msg->pid   = htons(getpid());
  sprintf(msg->buf, "audio-request %d %d", (int) getpid(), sp->lbl_cb_priority);
  if ((ret = sendto(sp->lbl_cb_base_socket, buffer, strlen(msg->buf) + 9, 0, (struct sockaddr *) &dest, sizeof(dest))) < 0) {
    perror("lbl_cb_send_request: sendto");
  }
}

void lbl_cb_send_demand(session_struct *sp)
{
  char		 	 buffer[CB_BUF_SIZE];
  lbl_cb_msg		*msg = (lbl_cb_msg *) buffer;
  struct sockaddr_in	 dest;
  u_long		 addr = htonl(LBL_CB_ADDR);
  int			 ret;

  if (sp->lbl_cb_base_socket == -1) {
    /* Conference bus is not being used! */
    return;
  }

  memcpy((char *) &dest.sin_addr.s_addr, (char *) &addr, sizeof(addr));
  dest.sin_family = AF_INET;
  dest.sin_port   = htons(LBL_CB_PORT);
  msg->magic = htonl(LBL_CB_MAGIC);
  msg->type  = htons(0);
  msg->pid   = htons(getpid());
  sprintf(msg->buf, "audio-demand %d", (int) getpid());
  if ((ret = sendto(sp->lbl_cb_base_socket, buffer, strlen(msg->buf) + 9, 0, (struct sockaddr *) &dest, sizeof(dest))) < 0) {
    perror("lbl_cb_send_demand: sendto");
  }
}

