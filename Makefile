#
#	Makefile for the RAT project. 
#
# Note: On many systems (eg: HP-UX 9.x and FreeBSD) this REQUIRES GNU make
#

DEFS = -DDEBUG -DDEBUG_MEM 
# -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DNDEBUG -DTEST -DGSM -DDEBUG_MIX
# -DDEBUG_RTP -DREPEAT -DLOG_PARTICIPANTS
# -DDEBUG_REPAIR
DEFS += -D$(OSTYPE) -D$(OSTYPE)_$(OSMVER) -D$(USER)
CC     = gcc
CFLAGS = -Wall $(INCS) $(DEFS) -O -g -fsigned-char -pipe
LDFLAGS=
LDLIBS=  $(LDLIBS) -lm
RANLIB = ranlib

# Not sure these are correct for anything other than a sparc??? [csp]
GSMFLAGS   = -DSASR -DFAST -DUSE_FLOAT_MUL

include Makefile_$(OSTYPE)_$(OSMVER)

OBJS  += convert.o \
	 time.o \
	 codec.o \
         repair.o \
         receive.o \
         transmit.o \
         codec_lpc.o \
         codec_adpcm.o \
         codec_wbs.o \
         rtcp_db.o \
         rtcp_pckt.o \
         qfDES.o \
         gsm_add.o \
         gsm_create.o \
         gsm_encode.o \
         gsm_preprocess.o \
         gsm_table.o \
         gsm_code.o \
         gsm_decode.o \
         gsm_long_term.o \
         gsm_rpe.o \
         gsm_destroy.o \
         gsm_lpc.o \
         gsm_short_term.o \
         audio.o \
         session.o \
         tabmulaw.o \
         util.o \
         interfaces.o \
         statistics.o \
         mix.o \
         parameters.o \
         ui_original.o \
	 ui_anna.o \
	 ui_relate.o \
         tcl_libs.o \
         rtcp.o \
         lbl_confbus.o \
         speaker_table.o \
         net.o \
         ui.o \
         transcoder.o \
         agc.o \
	 confbus.o \
	 confbus_ack.o \
	 confbus_addr.o \
	 confbus_cmnd.o \
	 confbus_misc.o \
	 confbus_parser.o \
	 confbus_lexer.o \
         main.o

CRYPTOBJS=crypt.o \
          crypt_random.o \
          md5.o

rat-$(OSTYPE)-$(OSVERS): $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(RATOBJS)
	rm -f rat-$(OSTYPE)-$(OSVERS)
	$(CC) $(RATOBJS) $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(LDLIBS) $(LDFLAGS) -o rat-$(OSTYPE)-$(OSVERS)

%.o: %.c
	$(CC) $(CFLAGS) $(GSMFLAGS) $(CRYPTFLAGS) -c $*.c

init_session.o: 	version.h
rtcp.o:      		version.h
ui.o:       		version.h
ui.o:	      		xbm/ucl.xbm
ui.o:	      		xbm/mic.xbm
ui.o:	      		xbm/mic_mute.xbm
ui.o:	      		xbm/speaker.xbm
ui.o:	      		xbm/speaker_mute.xbm
ui.o:	      		xbm/head.xbm
ui.o:	      		xbm/head_mute.xbm
ui.o:	      		xbm/line_out.xbm
ui.o:	      		xbm/line_out_mute.xbm
ui.o:	      		xbm/line_in.xbm
ui.o:	      		xbm/line_in_mute.xbm
ui.o:	      		xbm/rat_med.xbm
ui.o:	      		xbm/rat_small.xbm

confbus_parser.c: confbus_parser.y
	bison -p cb -d -o confbus_parser.c confbus_parser.y

confbus_lexer.c: confbus_lexer.l
	flex -Pcb -s -oconfbus_lexer.c confbus_lexer.l

tcl2c: tcl2c.c
	$(CC) -o tcl2c tcl2c.c

ui_original.o: ui_original.tcl tcl2c
	cat ui_original.tcl | tcl2c ui_original > ui_original.c
	$(CC) $(CFLAGS) -c ui_original.c -o ui_original.o

ui_anna.o: ui_anna.tcl tcl2c
	cat ui_anna.tcl | tcl2c ui_anna > ui_anna.c
	$(CC) $(CFLAGS) -c ui_anna.c -o ui_anna.o

ui_relate.o: ui_relate.tcl tcl2c
	cat ui_relate.tcl | tcl2c ui_relate > ui_relate.c
	$(CC) $(CFLAGS) -c ui_relate.c -o ui_relate.o

tcl_libs.c: tcl2c
	cat tcl/*.tcl tk/*.tcl | tcl2c TCL_LIBS > tcl_libs.c

tcl_libs.o: tcl_libs.c
	$(CC) $(CFLAGS) -c tcl_libs.c -o tcl_libs.o

tclc: tcl_libs.c rat_tcl.c

clean:
	-rm -f *.o
	-rm -f tcl2c
	-rm -f rat_tcl.c tcl_libs.c
	-rm -f ui_anna.c
	-rm -f ui_original.c
	-rm -f ui_relate.c
	-rm -f confbus_parser.[ch] 
	-rm -f confbus_lexer.c
	-rm -f rat-$(OSTYPE)-$(OSVERS)

tags:
	ctags -e *.[ch]

depend:
	makedepend $(DEFS) $(INCS) -f Makefile_$(OSTYPE)_$(OSMVER) *.[ch]

