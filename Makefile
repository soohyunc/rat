#
#	Makefile for the RAT project. 
#
# Note: On many systems (eg: HP-UX 9.x and FreeBSD) this REQUIRES GNU make
#

DEFS =-DDEBUG
# -DDEBUG_MIX
# -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DNDEBUG -DTEST -DGSM -DDEBUG_REPAIR
# -DDEBUG_RTP -DREPEAT -DLOG_PARTICIPANTS

DEFS += -D$(OSTYPE) -D$(OSTYPE)_$(OSMVER) -D$(USER)
CC     = gcc
CFLAGS = -Wall $(INCS) $(DEFS) -g -ggdb -fsigned-char -pipe
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
         tabalaw.o \
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
=======
SRCDIR = .
BINDIR = .
OBJDIR = /tmp
OBJS  += $(OBJDIR)/convert.o \
	 $(OBJDIR)/time.o \
	 $(OBJDIR)/codec.o \
         $(OBJDIR)/repair.o \
         $(OBJDIR)/receive.o \
         $(OBJDIR)/transmit.o \
         $(OBJDIR)/codec_lpc.o \
         $(OBJDIR)/codec_adpcm.o \
         $(OBJDIR)/codec_wbs.o \
         $(OBJDIR)/rtcp_db.o \
         $(OBJDIR)/rtcp_pckt.o \
         $(OBJDIR)/qfDES.o \
         $(OBJDIR)/gsm_add.o \
         $(OBJDIR)/gsm_create.o \
         $(OBJDIR)/gsm_encode.o \
         $(OBJDIR)/gsm_preprocess.o \
         $(OBJDIR)/gsm_table.o \
         $(OBJDIR)/gsm_code.o \
         $(OBJDIR)/gsm_decode.o \
         $(OBJDIR)/gsm_long_term.o \
         $(OBJDIR)/gsm_rpe.o \
         $(OBJDIR)/gsm_destroy.o \
         $(OBJDIR)/gsm_lpc.o \
         $(OBJDIR)/gsm_short_term.o \
         $(OBJDIR)/audio.o \
         $(OBJDIR)/session.o \
         $(OBJDIR)/tabmulaw.o \
         $(OBJDIR)/tabalaw.o \
         $(OBJDIR)/util.o \
         $(OBJDIR)/interfaces.o \
         $(OBJDIR)/statistics.o \
         $(OBJDIR)/mix.o \
         $(OBJDIR)/parameters.o \
         $(OBJDIR)/ui_original.o \
	 $(OBJDIR)/ui_anna.o \
	 $(OBJDIR)/ui_relate.o \
         $(OBJDIR)/tcl_libs.o \
         $(OBJDIR)/rtcp.o \
         $(OBJDIR)/lbl_confbus.o \
         $(OBJDIR)/speaker_table.o \
         $(OBJDIR)/net.o \
         $(OBJDIR)/ui.o \
         $(OBJDIR)/transcoder.o \
         $(OBJDIR)/agc.o \
	 $(OBJDIR)/confbus.o \
	 $(OBJDIR)/confbus_ack.o \
	 $(OBJDIR)/confbus_addr.o \
	 $(OBJDIR)/confbus_cmnd.o \
	 $(OBJDIR)/confbus_misc.o \
	 $(OBJDIR)/confbus_parser.o \
	 $(OBJDIR)/confbus_lexer.o \
         $(OBJDIR)/main.o

CRYPTOBJS=$(OBJDIR)/crypt.o \
          $(OBJDIR)/crypt_random.o \
          $(OBJDIR)/md5.o

$(BINDIR)/rat-$(OSTYPE)-$(OSVERS): $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(RATOBJS)
	rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)
	$(CC) $(RATOBJS) $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(LDLIBS) $(LDFLAGS) -o $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(GSMFLAGS) $(CRYPTFLAGS) -c $*.c -o $(OBJDIR)/$*.o

$(OBJDIR)/init_session.o: 	version.h
$(OBJDIR)/rtcp.o:      		version.h
$(OBJDIR)/ui.o:       		version.h
$(OBJDIR)/ui.o:	      		xbm/ucl.xbm
$(OBJDIR)/ui.o:	      		xbm/mic.xbm
$(OBJDIR)/ui.o:	      		xbm/mic_mute.xbm
$(OBJDIR)/ui.o:	      		xbm/speaker.xbm
$(OBJDIR)/ui.o:	      		xbm/speaker_mute.xbm
$(OBJDIR)/ui.o:	      		xbm/head.xbm
$(OBJDIR)/ui.o:	      		xbm/head_mute.xbm
$(OBJDIR)/ui.o:	      		xbm/line_out.xbm
$(OBJDIR)/ui.o:	      		xbm/line_out_mute.xbm
$(OBJDIR)/ui.o:	      		xbm/line_in.xbm
$(OBJDIR)/ui.o:	      		xbm/line_in_mute.xbm
$(OBJDIR)/ui.o:	      		xbm/rat_med.xbm
$(OBJDIR)/ui.o:	      		xbm/rat_small.xbm

$(SRCDIR)/confbus_parser.c: $(SRCDIR)/confbus_parser.y
	bison -p cb -d -o $(SRCDIR)/confbus_parser.c $(SRCDIR)/confbus_parser.y

$(SRCDIR)/confbus_lexer.c: $(SRCDIR)/confbus_lexer.l
	flex -Pcb -s -o$(SRCDIR)/confbus_lexer.c $(SRCDIR)/confbus_lexer.l

$(BINDIR)/tcl2c: $(SRCDIR)/tcl2c.c
	$(CC) -o $(BINDIR)/tcl2c $(SRCDIR)/tcl2c.c

$(OBJDIR)/ui_original.o: $(SRCDIR)/ui_original.tcl tcl2c
	cat $(SRCDIR)/ui_original.tcl | tcl2c ui_original > $(OBJDIR)/ui_original.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_original.c -o $(OBJDIR)/ui_original.o

$(OBJDIR)/ui_anna.o: $(SRCDIR)/ui_anna.tcl tcl2c
	cat $(SRCDIR)/ui_anna.tcl | tcl2c ui_anna > $(OBJDIR)/ui_anna.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_anna.c -o $(OBJDIR)/ui_anna.o

$(OBJDIR)/ui_relate.o: ui_relate.tcl tcl2c
	cat $(SRCDIR)/ui_relate.tcl | tcl2c ui_relate > $(OBJDIR)/ui_relate.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_relate.c -o $(OBJDIR)/ui_relate.o

$(OBJDIR)/tcl_libs.o: $(OBJDIR)/tcl2c
	cat tcl/*.tcl tk/*.tcl | tcl2c TCL_LIBS > $(OBJDIR)/tcl_libs.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/tcl_libs.c -o $(OBJDIR)/tcl_libs.o


clean:
	-rm -f $(OBJDIR)/*.o
	-rm -f $(BINDIR)/tcl2c
	-rm -f $(OBJDIR)/tcl_libs.c
	-rm -f $(OBJDIR)/ui_anna.c
	-rm -f $(OBJDIR)/ui_original.c
	-rm -f $(OBJDIR)/ui_relate.c
	-rm -f $(SRCDIR)/confbus_parser.[ch] 
	-rm -f $(SRCDIR)/confbus_lexer.c
	-rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

tags:
	ctags -e *.[ch]

depend:
	makedepend $(DEFS) $(INCS) -f Makefile_$(OSTYPE)_$(OSMVER) *.[ch]

