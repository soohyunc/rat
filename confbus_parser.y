%{
/*****************************************************************************/
/*
 * FILE:    confbus_parser.y
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

#include <stdio.h>
#include <stdlib.h>
#include "confbus_addr.h"
#include "confbus_cmnd.h"
#include "confbus.h"
#include "util.h"

#define malloc(x) xmalloc(x)
#define free(x)   xfree(x)

/* This is the lexical analyser produced from confbus_lexer.l */
int cblex(void);

/* These are defined in confbus.c */
extern cb_mesg *cb_msg;
extern int	cb_error;

void cberror(char *s)
{
#ifdef DEBUG_CONFBUS
	printf("Error in confbus message \"%s\"\n", s);
#endif
	/* Signal the error condition to the rest of the program... */
	cb_error = 1;
}

int cbwrap(void)
{
	return 1;
}

static void dprint(char *s)
{
#ifdef DEBUG_CONFBUS_PARSER
	printf("ConfbusParser: %s\n", s);
#endif
}

/*****************************************************************************/
%}
%union {
	int	 i;
	float	 f;
	char	*s;
}

%type  <i> seq_num
%type  <s> type
%type  <s> arg
%type  <s> args
%type  <s> addr

%token <i> INTEGER
%token <f> FLOAT
%token <s> SYMBOL
%token <s> STRING
%token     NEWLINE
%token     WILDCARD
%token     LIST_START
%token     LIST_END
%%

packet:	header body 
{
	dprint("Got packet");
};

header:	protocol_id seq_num type srce_addr dest_addr NEWLINE
{
	dprint("Got header");
};

protocol_id: SYMBOL
{
	if (strcmp($1, "mbus/1.0") != 0) {
		free($1);
		cberror("Unknown mbus protocol");
	}
	free($1);
	dprint("Got protocol_id");
};

seq_num: INTEGER
{
	cb_msg->seq_num = $1;
	dprint("Got seq_num");
};

type: SYMBOL
{
	if (strcmp($1, "R") == 0) {
		cb_msg->msg_type = CB_MSG_RELIABLE;
	} else if (strcmp($1, "U") == 0) {
		cb_msg->msg_type = CB_MSG_UNRELIABLE;
	} else if (strcmp($1, "A") == 0) {
		cb_msg->msg_type = CB_MSG_ACK;
	}
	dprint("Got type");
};

srce_addr: LIST_START addr addr addr addr addr LIST_END
{
	cb_msg->srce_addr = cb_addr_init($2, $3, $4, $5, $6);
	dprint("Got srce_addr");
};

dest_addr: LIST_START addr addr addr addr addr LIST_END
{
	cb_msg->dest_addr = cb_addr_init($2, $3, $4, $5, $6);
	dprint("Got dest_addr");
};

addr: SYMBOL   {$$ = $1; }
    | INTEGER  {/* This assumes a 32bit machine, where INT_MAX is */
    		/* 10 digits long, printed as a decimal number.   */
		/* We also assume that someone frees the string!  */
    		char *s = xmalloc(11);
		sprintf(s, "%d", $1);
		$$ = s;
               }
    | WILDCARD {$$ = NULL; }
    ;

body: ack 
    | msg
    ;

msg: command NEWLINE
   | command args NEWLINE
   | command args NEWLINE msg
   ;

ack: INTEGER NEWLINE
{
	cb_msg->ack = $1;
	dprint("Got ack");
};

command: SYMBOL
{
	cb_msg->commands = cb_cmnd_init($1, cb_msg->commands);
	dprint("Got command");
};

args: arg
    | arg args
    ;

arg: INTEGER {
	cb_cmnd_add_param_int(cb_msg->commands, $1);
	dprint("Got arg (integer)");
     }
   | FLOAT {
	cb_cmnd_add_param_flt(cb_msg->commands, $1);
	dprint("Got arg (float)");
     }
   | SYMBOL {
	cb_cmnd_add_param_sym(cb_msg->commands, $1);
	dprint("Got arg (symbol)");
     }
   | STRING {
	cb_cmnd_add_param_str(cb_msg->commands, $1);
	dprint("Got arg (string)");
     };

%%
