
CC = gcc
DEBUG=-g
INCLUDE = -I.. -I../../common
CFLAGS  = -W -Wall -Wbad-function-cast -Wmissing-prototypes -Werror -g -fsigned-char -DSASR -DFAST -DUSE_FLOAT_MUL -DDEBUG_MEM
LIBS = -L../../common -lm -luclmmbase

CHANNEL_OBJS = ../cc_vanilla.o ../new_channel.o ../channel_types.o

CODEC_OBJS = ../codec.o ../codec_state.o ../codec_dvi.o ../codec_gsm.o \
             ../codec_l16.o ../codec_g711.o ../codec_lpc.o ../codec_types.o \
             ../codec_vdvi.o ../codec_wbs.o ../cx_dvi.o ../cx_gsm.o \
             ../cx_lpc.o     ../cx_vdvi.o   ../cx_wbs.o ../codec_g726.o \
             ../cx_g726.o ../cx_g726_16.o ../cx_g726_24.o ../cx_g726_32.o \
             ../cx_g726_40.o \
             ../converter.o ../convert_linear.o ../convert_util.o \
             ../convert_extra.o ../convert_sinc.o ../bitstream.o

REPAIR_OBJS  = ../repair.o ../audio_util.o

SNDFILE_OBJS = ../sndfile.o ../sndfile_raw.o ../sndfile_au.o ../sndfile_wav.o

UTIL_OBJS = ../timers.o ../playout.o ../ts.o

3D_OBJS = test_3d.o ../render_3D.o

RDNCY_OBJS = ../cc_rdncy.o ../channel_types.o test_rdncy.o

test_repair: $(CODEC_OBJS) $(REPAIR_OBJS) $(SNDFILE_OBJS) test_repair.o 
	$(CC) $(DEBUG)  -o $@ $(CODEC_OBJS) $(REPAIR_OBJS) $(SNDFILE_OBJS) $(LIBS) test_repair.o

test_convert: $(CODEC_OBJS) test_convert.o
	$(CC) $(DEBUG)  -o $@ $(CODEC_OBJS) $(LIBS) test_convert.o

test_rdncy: $(RDNCY_OBJS) $(CODEC_OBJS) $(UTIL_OBJS)
	$(CC) $(DEBUG) -o $@ $(RDNCY_OBJS) $(CODEC_OBJS) $(UTIL_OBJS) $(LIBS)

test_3d: $(3D_OBJS) $(CODEC_OBJS)
	$(CC) $(DEBUG)  -o $@  $(3D_OBJS) $(CODEC_OBJS) $(LIBS)

native: native.o $(CODEC_OBJS)
	$(CC) $(DEBUG)  -o $@  $(CODEC_OBJS) native.o $(LIBS)

test_channel: $(CODEC_OBJS) $(CHANNEL_OBJS) $(UTIL_OBJS) test_channel.o
	$(CC) $(DEBUG)  -o $@ $(CODEC_OBJS) $(CHANNEL_OBJS) $(UTIL_OBJS) \
	$(LIBS) test_channel.o

test_codecs: $(CODEC_OBJS) test_codecs.o
	$(CC) $(DEBUG)  -o $@ $(CODEC_OBJS) $(LIBS) test_codecs.o

test_ts: $(UTIL_OBJS) test_ts.o
	$(CC) $(DEBUG)  -o $@ $(UTIL_OBJS) $(LIBS) test_ts.o

.c.o:
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(CODEC_OBJS) $(3D_OBJS) $(RDNCY_OBJS) $(UTIL_OBJS) test_3d \
		test_codecs native test_channel *.o


