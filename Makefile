#
#	Makefile for the RAT project. 
#
# Note: On many systems (eg: HP-UX 9.x and FreeBSD) this REQUIRES GNU make
#

DEFS = -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DNDEBUG -DTEST -DGSM -DDEBUG_MIX
# -DDEBUG_RTP -DREPEAT -DLOG_PARTICIPANTS

DEFS += -D$(OSTYPE) -D$(OSTYPE)_$(OSMVER) -D$(USER)
CC     = gcc
CFLAGS = -Wall $(INCS) $(DEFS) -g -O -fsigned-char
LDFLAGS=
LDLIBS=  $(LDLIBS) -lm
RANLIB = ranlib

# Not sure these are correct for anything other than a sparc??? [csp]
GSMFLAGS   = -DSASR -DFAST -DUSE_FLOAT_MUL

BINDIR = .
SRCDIR = .
OBJDIR = .

include Makefile_$(OSTYPE)_$(OSMVER)

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
	 $(OBJDIR)/confbus_parser.o \
	 $(OBJDIR)/confbus_lexer.o \
         $(OBJDIR)/main.o

CRYPTOBJS=$(OBJDIR)/crypt.o \
          $(OBJDIR)/crypt_random.o \
          $(OBJDIR)/md5.o

rat-$(OSTYPE)-$(OSVERS): $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(RATOBJS)
	rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)
	$(CC) $(RATOBJS) $(OBJS) $(GSMOBJS) $(CRYPTOBJS) $(LDLIBS) $(LDFLAGS) -o $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(GSMFLAGS) $(CRYPTFLAGS) -c $(SRCDIR)/$*.c -o $(OBJDIR)/$*.o

$(OBJDIR)/init_session.o: 	$(SRCDIR)/version.h
$(OBJDIR)/rtcp.o:      		$(SRCDIR)/version.h
$(OBJDIR)/ui.o:       		$(SRCDIR)/version.h
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/ucl.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/mic.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/mic_mute.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/speaker.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/speaker_mute.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/head.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/head_mute.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/line_out.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/line_out_mute.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/line_in.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/line_in_mute.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/rat_med.xbm
$(OBJDIR)/ui.o:	      		$(SRCDIR)/xbm/rat_small.xbm

$(SRCDIR)/confbus_parser.c: $(SRCDIR)/confbus_parser.y
	bison -p cb -d -o $(SRCDIR)/confbus_parser.c $(SRCDIR)/confbus_parser.y

$(SRCDIR)/confbus_lexer.c: $(SRCDIR)/confbus_lexer.l
	flex -Pcb -s -o$(SRCDIR)/confbus_lexer.c $(SRCDIR)/confbus_lexer.l

$(OBJDIR)/tcl2c: $(SRCDIR)/tcl2c.c
	$(CC) -o $(OBJDIR)/tcl2c $(SRCDIR)/tcl2c.c

$(OBJDIR)/ui_original.o: $(SRCDIR)/ui_original.tcl $(OBJDIR)/tcl2c
	cat $(SRCDIR)/ui_original.tcl | $(OBJDIR)/tcl2c ui_original > $(OBJDIR)/ui_original.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_original.c -o $(OBJDIR)/ui_original.o

$(OBJDIR)/ui_anna.o: $(SRCDIR)/ui_anna.tcl $(OBJDIR)/tcl2c
	cat $(SRCDIR)/ui_anna.tcl | $(OBJDIR)/tcl2c ui_anna > $(OBJDIR)/ui_anna.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_anna.c -o $(OBJDIR)/ui_anna.o

$(OBJDIR)/ui_relate.o: $(SRCDIR)/ui_relate.tcl $(OBJDIR)/tcl2c
	cat $(SRCDIR)/ui_relate.tcl | $(OBJDIR)/tcl2c ui_relate > $(OBJDIR)/ui_relate.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_relate.c -o $(OBJDIR)/ui_relate.o

$(OBJDIR)/tcl_libs.c: $(OBJDIR)/tcl2c
	cat tcl/*.tcl tk/*.tcl | $(OBJDIR)/tcl2c TCL_LIBS > $(OBJDIR)/tcl_libs.c

$(OBJDIR)/tcl_libs.o: $(OBJDIR)/tcl_libs.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/tcl_libs.c -o $(OBJDIR)/tcl_libs.o

tclc: $(OBJDIR)/tcl_libs.c $(OBJDIR)/rat_tcl.c

clean:
	-rm -f $(OBJDIR)/*.o
	-rm -f $(OBJDIR)/tcl2c
	-rm -f $(OBJDIR)/rat_tcl.c $(OBJDIR)/tcl_libs.c
	-rm -f $(SRCDIR)/confbus_parser.[ch] 
	-rm -f $(SRCDIR)/confbus_lexer.c
	-rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

tags:
	(cd src; ctags -e *.[ch])

tar:
	-rm -rf objs/* 
	-rm -rf bin/* 
	-rm -rf dist/*
	-rm -rf win95/rat.exe
	(cd ..; gtar zcvf rat-`date +%Y%m%d`.tgz rat)

depend:
	makedepend $(DEFS) $(INCS) -f Makefile_$(OSTYPE)_$(OSMVER) $(SRCDIR)/*.[ch]


