#
#	Makefile for the RAT project. 
#
# Note: On many systems (eg: HP-UX 9.x and FreeBSD) this REQUIRES GNU make
#

DEFS = -DDEBUG -DDEBUG_MEM 
# -DDEBUG_MIX
# -DDEBUG -DDEBUG_MEM -DDEBUG_CONFBUS
# -DNDEBUG -DTEST -DGSM -DDEBUG_REPAIR
# -DDEBUG_RTP -DREPEAT -DLOG_PARTICIPANTS

DEFS += -D$(OSTYPE) -D$(OSTYPE)_$(OSMVER)
CC     = gcc
CFLAGS = -W -Wall -Wbad-function-cast $(INCS) $(DEFS) -O -g -fsigned-char -pipe
LDFLAGS=
LDLIBS=  $(LDLIBS) -lm
RANLIB = ranlib

# Not sure these are correct for anything other than a sparc??? [csp]
GSMFLAGS   = -DSASR -DFAST -DUSE_FLOAT_MUL

include Makefile_$(OSTYPE)_$(OSMVER)

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
         $(OBJDIR)/channel.o \
         $(OBJDIR)/cc_red.o \
         $(OBJDIR)/cc_intl.o \
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
         $(OBJDIR)/tabulaw.o \
         $(OBJDIR)/tabalaw.o \
         $(OBJDIR)/util.o \
         $(OBJDIR)/interfaces.o \
         $(OBJDIR)/statistics.o \
         $(OBJDIR)/mix.o \
         $(OBJDIR)/parameters.o \
         $(OBJDIR)/ui_original.o \
         $(OBJDIR)/tcl_libs.o \
         $(OBJDIR)/rtcp.o \
         $(OBJDIR)/speaker_table.o \
         $(OBJDIR)/net.o \
         $(OBJDIR)/ui.o \
         $(OBJDIR)/transcoder.o \
         $(OBJDIR)/agc.o \
	 $(OBJDIR)/crypt.o \
         $(OBJDIR)/crypt_random.o \
         $(OBJDIR)/md5.o \
	 $(OBJDIR)/mbus.o \
	 $(OBJDIR)/mbus_ui.o \
	 $(OBJDIR)/mbus_engine.o \
         $(OBJDIR)/main.o

$(BINDIR)/rat-$(OSTYPE)-$(OSVERS): $(OBJS) $(GSMOBJS) $(RATOBJS)
	rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)
	$(CC) $(RATOBJS) $(OBJS) $(GSMOBJS) $(LDLIBS) $(LDFLAGS) -o $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(GSMFLAGS) -c $*.c -o $(OBJDIR)/$*.o

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

$(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS): $(SRCDIR)/tcl2c.c
	$(CC) -o $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS) $(SRCDIR)/tcl2c.c

$(OBJDIR)/ui_original.o: $(SRCDIR)/ui_original.tcl $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS)
	cat $(SRCDIR)/ui_original.tcl | $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS) ui_original > $(OBJDIR)/ui_original.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/ui_original.c -o $(OBJDIR)/ui_original.o

$(OBJDIR)/tcl_libs.o: $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS)
	cat tcl/*.tcl tk/*.tcl | $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS) TCL_LIBS > $(OBJDIR)/tcl_libs.c
	$(CC) $(CFLAGS) -c $(OBJDIR)/tcl_libs.c -o $(OBJDIR)/tcl_libs.o

clean:
	-rm -f $(OBJDIR)/*.o
	-rm -f $(OBJDIR)/tcl_libs.c
	-rm -f $(OBJDIR)/ui_original.c
	-rm -f $(BINDIR)/tcl2c-$(OSTYPE)-$(OSVERS)
	-rm -f $(BINDIR)/rat-$(OSTYPE)-$(OSVERS)

tags:
	ctags -e *.[ch]

depend:
	makedepend $(DEFS) $(INCS) -f Makefile_$(OSTYPE)_$(OSMVER) *.[ch]

