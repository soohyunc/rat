
CC = gcc
DEBUG=-fbounds-checking -g
INCLUDE = -I.. -I../../common
CFLAGS  = -W -Wall -Wbad-function-cast -Wmissing-prototypes -Werror -g -fsigned-char  -DHAVE_PCA -DDEBUG -DFreeBSD -DSASR -DFAST -DUSE_FLOAT_MUL -DDEBUG_MEM
LIBS = -L../../common -lm -lcommon

CODEC_OBJS = ../codec.o ../codec_state.o ../codec_dvi.o ../codec_gsm.o \
             ../codec_l16.o ../codec_g711.o ../codec_lpc.o ../codec_types.o \
             ../codec_vdvi.o ../codec_wbs.o ../cx_dvi.o ../cx_gsm.o \
             ../cx_lpc.o     ../cx_vdvi.o   ../cx_wbs.o ../codec_g726.o \
             ../cx_g726.o ../cx_g726_24.o ../cx_g726_32.o ../cx_g726_40.o

CHANNEL_OBJS = ../cc_vanilla.o ../new_channel.o ../channel_types.o

UTIL_OBJS = ../timers.o ../playout.o ../ts.o

3D_OBJS = test_3d.o ../render_3D.o

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
	rm -rf $(CODEC_OBJS) $(3D_OBJS) test_3d test_codecs native \
	test_channel *.o


