%{
#include <stdlib.h>
#include "confbus.h"
int cblex(void);
extern mbus *mesg;

%}
%union {
	int	 i;
	float	 f;
	char	*s;
}

%type  <i> seq_num
%type  <s> type
%type  <s> command
%type  <s> arg
%type  <s> args
%type  <s> msg
%type  <s> addr

%token <i> BOOLEAN
%token <i> INTEGER
%token <f> FLOAT
%token <s> SYMBOL
%token <s> STRING
%token     NEWLINE
%token     WILDCARD
%%

packet:	header msg ;

header:	protocol_id seq_num type srce_addr dest_addr NEWLINE

protocol_id: SYMBOL '/' FLOAT 
{
	if (strcmp($1, "mbus") != 0) {
		free($1);
		cberror("Unknown protocol name");
	}
	free($1);
	if ($3 != 1.0) {
		cberror("Unknown protocol version");
	}
};

seq_num: INTEGER
{
	mesg->seq_num = $1;
};

type: SYMBOL
{
	if (strcmp($1, "R") == 0) {
		mesg->msg_type = RELIABLE;
	} else if (strcmp($1, "U") == 0) {
		mesg->msg_type = UNRELIABLE;
	} else if (strcmp($1, "A") == 0) {
		mesg->msg_type = ACK;
	}
};

srce_addr: '(' addr addr addr addr addr ')'
{
	mesg->srce_addr = (mbus_addr *) malloc(sizeof(mbus_addr));
	mesg->srce_addr->media_type     = $2;
	mesg->srce_addr->module_type    = $3;
	mesg->srce_addr->media_handling = $4;
	mesg->srce_addr->app_name       = $5;
	mesg->srce_addr->app_instance   = $6;
};

dest_addr: '(' addr addr addr addr addr ')'
{
	mesg->dest_addr = (mbus_addr *) malloc(sizeof(mbus_addr));
	mesg->dest_addr->media_type     = $2;
	mesg->dest_addr->module_type    = $3;
	mesg->dest_addr->media_handling = $4;
	mesg->dest_addr->app_name       = $5;
	mesg->dest_addr->app_instance   = $6;
};

addr: SYMBOL
    | WILDCARD {$$ = NULL;}
    ;

msg: command
   | command args NEWLINE
   | command args NEWLINE msg
   ;

command: SYMBOL
{
	printf("%s\n", $1);
};

args: arg
    | arg args
    ;

arg: SYMBOL
{
	printf("%s\n", $1);
};

%%
