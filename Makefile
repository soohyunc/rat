#
# Makefile for the RAT project. This probably requires GNU make.
#

DEFS = -DDEBUG
# -DDEBUG_MIX -DDEBUG_PLAYOUT -DDEBUG_CUSHION
# -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DNDEBUG -DTEST -DGSM -DDEBUG_REPAIR
# -DDEBUG_RTP -DREPEAT -DLOG_PARTICIPANTS

DEBUG=-g -fbounds-checking
# -g -ggdb

OPTS= 
#-O -O2 -O3 -O4 

PROFILE=
# -pg / call graph profile
# -a  / line coverage profile

DEFS += -D$(OSTYPE) -D$(OSTYPE)_$(OSMVER)
# If your code doesn't compile with all these -W... flags, fix the code don't remove the warnings!
CFLAGS = -W -Wall -Wbad-function-cast -Wmissing-prototypes -Werror $(INCS) $(DEFS) $(DEBUG) -fsigned-char -pipe $(PROFILE) $(OPTS)
CC     = gcc
LDFLAGS= $(PROFILE) $(OPTS)
LDLIBS=  $(LDLIBS)

# Not sure these are correct for anything other than a sparc??? [csp]
GSMFLAGS   = -DSASR -DFAST -DUSE_FLOAT_MUL

include Makefile_$(OSTYPE)_$(OSMVER)

OBJS  += convert.o time.o codec.o repair.o receive.o transmit.o codec_lpc.o codec_adpcm.o codec_wbs.o   \
         channel.o cc_red.o cc_intl.o rtcp_db.o rtcp_pckt.o qfDES.o gsm_add.o gsm_create.o gsm_encode.o \
         gsm_preprocess.o gsm_table.o gsm_code.o gsm_decode.o gsm_long_term.o gsm_rpe.o gsm_destroy.o   \
         gsm_lpc.o gsm_short_term.o audio.o cushion.o session.o tabulaw.o tabalaw.o util.o interfaces.o \
         statistics.o mix.o parameters.o ui_original.o tcl_libs.o tcltk.o rtcp.o speaker_table.o net.o  \
         ui_control.o transcoder.o crypt.o crypt_random.o md5.o mbus.o mbus_ui.o mbus_engine.o main.o

rat: $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) $(LDFLAGS) -o rat

%.o: %.c session.h
	$(CC) $(CFLAGS) $(GSMFLAGS) -c $*.c -o $*.o

init_session.o: 	version.h
rtcp.o:      		version.h
tcltk.o:    		version.h
tcltk.o:      		xbm/ucl.xbm
tcltk.o:	      	xbm/mic.xbm
tcltk.o:	      	xbm/speaker.xbm
tcltk.o:	      	xbm/head.xbm
tcltk.o:	      	xbm/line_out.xbm
tcltk.o:	      	xbm/line_in.xbm
tcltk.o:	      	xbm/rat_med.xbm
tcltk.o:	      	xbm/rat_small.xbm

tcl2c: tcl2c.c
	$(CC) -o tcl2c tcl2c.c

ui_original.o: ui_original.tcl tcl2c
	cat ui_original.tcl | tcl2c ui_original > ui_original.c
	$(CC) $(CFLAGS) -c ui_original.c -o ui_original.o

tcl_libs.o: tcl2c
	cat tcl/*.tcl tk/*.tcl | tcl2c TCL_LIBS > tcl_libs.c
	$(CC) $(CFLAGS) -c tcl_libs.c -o tcl_libs.o

clean:
	-rm -f $(OBJS) tcl_libs.c ui_original.c tcl2c rat

tags:
	etags *.[ch]

