%{
#include <string.h>
#include "confbus_parser.h"
%}

escape	(\\\\)|(\\\()|(\\\))|(\\\")|(\\n)|(\\r)|(\\t)
%%

0|1						{ cblval.i = atoi(yytext);   return BOOLEAN; }
-?[0-9]+					{ cblval.i = atoi(yytext);   return INTEGER; }
-?(([0-9]+)|([0-9]*\.[0-9]+)([eE][-+]?[0-9]+)?)	{ cblval.f = atof(yytext);   return FLOAT;   }
[a-zA-Z][-_a-zA-Z0-9]*				{ cblval.s = strdup(yytext); return SYMBOL;  }
\"({escape}|[^\"\n])*\"				{ cblval.s = strdup(yytext); return STRING;  }
[ \t]						;
\n						{ return NEWLINE;  }
\*						{ return WILDCARD; }
.						{ return yytext[0];}

%%
