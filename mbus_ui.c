/*
 * FILE:    mbus_ui.c
 * AUTHORS: Colin Perkins
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

#include <stdio.h>
#include <string.h>
#include "mbus.h"
#include "mbus_ui.h"
#include "tcltk.h"
#include "util.h"

static struct mbus *mbus_base = NULL;
static struct mbus *mbus_chan = NULL;

void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data)
{
	char        command[1500];
	unsigned int i;

	UNUSED(srce);
	UNUSED(data);

	sprintf(command, "mbus_recv_%s %s", cmnd, args);

	for (i = 0; i < (unsigned)strlen(command); i++) {
		if (command[i] == '[') command[i] = '(';
		if (command[i] == ']') command[i] = ')';
	}

	tcl_send(command);
}

void mbus_ui_tx(int channel, char *dest, char *cmnd, char *args, int reliable)
{
	if (channel == 0) {
		mbus_send(mbus_base, dest, cmnd, args, reliable);
	} else {
		mbus_send(mbus_chan, dest, cmnd, args, reliable);
	}
}

void mbus_ui_tx_queue(int channel, char *cmnd, char *args)
{
	if (channel == 0) {
		mbus_qmsg(mbus_base, cmnd, args);
	} else {
		mbus_qmsg(mbus_chan, cmnd, args);
	}
}

void mbus_ui_init(char *name_ui, int channel)
{
	mbus_base = mbus_init(0, mbus_ui_rx, NULL); mbus_addr(mbus_base, name_ui);
	if (channel == 0) {
		mbus_chan = mbus_base;
	} else {
		mbus_chan = mbus_init((unsigned short) channel, mbus_ui_rx, NULL); mbus_addr(mbus_chan, name_ui);
	}
}

int  mbus_ui_fd(int channel)
{
	if (channel == 0) {
		return mbus_fd(mbus_base);
	} else {
		return mbus_fd(mbus_chan);
	}
}

struct mbus *mbus_ui(int channel)
{
	if (channel == 0) {
		return mbus_base;
	} else {
		return mbus_chan;
	}
}

void mbus_ui_retransmit(void)
{
	mbus_retransmit(mbus_base);
	mbus_retransmit(mbus_chan);
}

int mbus_ui_waiting(void)
{
        return mbus_waiting_acks(mbus_base) | mbus_waiting_acks(mbus_chan);
}
