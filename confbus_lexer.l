%{
/*
 * FILE:    confbus_lexer.l
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

#include <string.h>
#include "assert.h"
#include "util.h"
#include "confbus_parser.h"
#include "confbus_misc.h"

/* The buffer from which to take input... */
extern char *cb_buffer;
extern int   cb_buffer_len;
extern int   cb_buffer_pos;

/* Redefine the input routine to read from the cb_buffer */
/* array, rather than from the standard input.           */
#undef  YY_INPUT
#define YY_INPUT(b, r, ms) (r = cb_input(b, ms))

int cb_input(char *buffer, int buffer_size)
{
	int n;
	
	n = buffer_size<(cb_buffer_len-cb_buffer_pos)?buffer_size:(cb_buffer_len-cb_buffer_pos);
	if (n > 0) {
		memcpy(buffer, cb_buffer + cb_buffer_pos, n);
		cb_buffer_pos += n;
	}
	assert(cb_buffer_pos <= cb_buffer_len);
	return n;
}

%}

escape	(\\\\)|(\\\()|(\\\))|(\\\")|(\\n)|(\\r)|(\\t)
%%

-?[0-9]+					{ cblval.i = atoi(yytext);    return INTEGER; }
-?(([0-9]+)|([0-9]*\.[0-9]+)([eE][-+]?[0-9]+)?)	{ cblval.f = atof(yytext);    return FLOAT;   }
[^ \t\r\"\\\n\*\(\)]+				{ cblval.s = xstrdup(yytext); return SYMBOL;  }
\"({escape}|[^\"\n])*\"				{ cblval.s = xstrdup(yytext); return STRING;  }
[ \t]						;
\n						{ return NEWLINE;    }
\*						{ return WILDCARD;   }
\(						{ return LIST_START; }
\)						{ return LIST_END;   }
.						{ return yytext[0];  }

%%
