/*
 * FILE:    auddev_alsa.c
 * PROGRAM: RAT
 * AUTHOR:  Robert Olson
 *
 * $Revision$
 * $Date$
 *
 * Copyright (c) 2000 Argonne National Laboratory
 * All rights reserved.
 *
 */

/*
 * ALSA audio device. 
 */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "auddev_alsa.h"
#include "memory.h"
#include "debug.h"
#include <sys/asoundlib.h>

#define PB SND_PCM_CHANNEL_PLAYBACK
#define CAP SND_PCM_CHANNEL_CAPTURE

#define checkStatus(x) doCheckStatus(x, __LINE__)

/*
 * Structure that keeps track of the cards we know about. Rat wants a linear
 * list of cards, so we'll give it that.
 *
 * This is filled in during the init step.
 */

typedef struct RatCardInfo_t
{
    int cardNumber;
    int pcmDevice;
    int mixerDevice;
    char name[256];
    
} RatCardInfo;

#define MAX_RAT_CARDS 128

static RatCardInfo ratCards[MAX_RAT_CARDS];
static int nRatCards = 0;

/*
 * This is the ALSA port name string map..
 */

static char *alsa_ports[] = {
    SND_MIXER_IN_SYNTHESIZER,
    SND_MIXER_IN_PCM,
    SND_MIXER_IN_DAC,
    SND_MIXER_IN_FM,
    SND_MIXER_IN_DSP,
    SND_MIXER_IN_LINE,
    SND_MIXER_IN_MIC,
    SND_MIXER_IN_CD,
    SND_MIXER_IN_VIDEO,
    SND_MIXER_IN_RADIO,
    SND_MIXER_IN_PHONE,
    SND_MIXER_IN_MONO,
    SND_MIXER_IN_SPEAKER,
    SND_MIXER_IN_AUX,
    SND_MIXER_IN_CENTER,
    SND_MIXER_IN_WOOFER,
    SND_MIXER_IN_SURROUND,
    SND_MIXER_OUT_MASTER,
    SND_MIXER_OUT_MASTER_MONO,
    SND_MIXER_OUT_MASTER_DIGITAL,
    SND_MIXER_OUT_HEADPHONE,
    SND_MIXER_OUT_PHONE,
    SND_MIXER_OUT_CENTER,
    SND_MIXER_OUT_WOOFER,
    SND_MIXER_OUT_SURROUND,
    SND_MIXER_OUT_DSP,
};
#define ALSA_INPORT_INDEX_SYNTHESIZER 0
#define ALSA_INPORT_INDEX_PCM 1
#define ALSA_INPORT_INDEX_DAC 2
#define ALSA_INPORT_INDEX_FM 3
#define ALSA_INPORT_INDEX_DSP 4
#define ALSA_INPORT_INDEX_LINE 5
#define ALSA_INPORT_INDEX_MIC 6
#define ALSA_INPORT_INDEX_CD 7
#define ALSA_INPORT_INDEX_VIDEO 8
#define ALSA_INPORT_INDEX_RADIO 9
#define ALSA_INPORT_INDEX_PHONE 10
#define ALSA_INPORT_INDEX_MONO 11
#define ALSA_INPORT_INDEX_SPEAKER 12
#define ALSA_INPORT_INDEX_AUX 13
#define ALSA_INPORT_INDEX_CENTER 14
#define ALSA_INPORT_INDEX_WOOFER 15
#define ALSA_INPORT_INDEX_SURROUND 16
#define ALSA_OUTPORT_INDEX_MASTER 17
#define ALSA_OUTPORT_INDEX_MASTER_MONO 18
#define ALSA_OUTPORT_INDEX_MASTER_DIGITAL 19
#define ALSA_OUTPORT_INDEX_HEADPHONE 20
#define ALSA_OUTPORT_INDEX_PHONE 21
#define ALSA_OUTPORT_INDEX_CENTER 22
#define ALSA_OUTPORT_INDEX_WOOFER 23
#define ALSA_OUTPORT_INDEX_SURROUND 24
#define ALSA_OUTPORT_INDEX_DSP 25
#define NUM_ALSA_PORTS (sizeof(alsa_ports) / sizeof(alsa_ports[0]))

static snd_mixer_gid_t alsa_gids[NUM_ALSA_PORTS];

/*
 * Mappings for ports. This table maps from ALSA port index to Rat port identifiers,
 * and is what rat wants back from the get_details funcs.
 */
static audio_port_details_t in_ports[] = {
    { ALSA_INPORT_INDEX_MIC, AUDIO_PORT_MICROPHONE },
    { ALSA_INPORT_INDEX_LINE, AUDIO_PORT_LINE_IN },
    { ALSA_INPORT_INDEX_CD, AUDIO_PORT_CD, }
};
#define ALSA_NUM_INPORTS (sizeof(in_ports) / sizeof(in_ports[0]))

static audio_port_details_t out_ports[] = {
    { ALSA_INPORT_INDEX_PCM, AUDIO_PORT_SPEAKER },
};
#define ALSA_NUM_OUTPORTS (sizeof(out_ports) / sizeof(out_ports[0]))

/*
 * Current open audio device
 */

static RatCardInfo *CurCard = 0;
static int CurCardIndex = -1;
static snd_pcm_t *CurHandle = 0;
static snd_mixer_t *CurMixer = 0;
static audio_port_t CurInPort;
static audio_port_t CurOutPort;
static int CurLoopbackGain = 0;
/*static int CurBufSize = 0;*/

/*
 * Utility funcs
 */

static char *encodingToString[] = {
    "pcmu",
    "s8",
    "u8",
    "s16"
};

static int doCheckStatus(int channel, int line)
{
  /*    int rc;*/
  
    snd_pcm_channel_status_t status;

    UNUSED(line);

    memset(&status, 0, sizeof(status));
    status.channel = channel;
/*
    if ((rc = snd_pcm_channel_status(CurHandle, &status)) != 0)
	debug_msg("channel status failed: %s\n", snd_strerror(rc));
    else
	debug_msg("status: line=%d channel=%s mode=%d status=%d scount=%d frag=%d count=%d free=%d under=%d over=%d\n",
		  line,
		  channel == PB ? "PB" : "CAP",
		  status.mode, status.status, status.scount,
		  status.free, status.underrun, status.overrun);
*/
    return status.status;
}

static int mapFormat(int encoding)
{
    int format = -1;
    switch (encoding)
    {
    case DEV_PCMU:
	format = SND_PCM_SFMT_MU_LAW;
	break;
    case DEV_S8:
	format = SND_PCM_SFMT_S8;
	break;
    case DEV_U8:
	format = SND_PCM_SFMT_U8;
	break;
    case DEV_S16:
	format = SND_PCM_SFMT_S16;
	break;
    }
    return format;
}

static void dump_audio_format(audio_format *f)
{
    if (f == 0)
	debug_msg("    <null>\n");
    else
	debug_msg("    encoding=%s sample_rate=%d bits_per_sample=%d channels=%d bytes_per_block=%d\n",
		  encodingToString[f->encoding], f->sample_rate, f->bits_per_sample,
		  f->channels, f->bytes_per_block);
}

static void dump_hw_info(struct snd_ctl_hw_info *info)
{
    debug_msg("Hw info: type=%d hwdepdevs=%d pcmdevs=%d mixerdevs=%d\n",
	      info->type, info->hwdepdevs, info->pcmdevs, info->mixerdevs);
    debug_msg("         mididevs=%d timerdevs=%d id=%s abbr=%s name=%s longname=%s\n",
	      info->mididevs, info->timerdevs, info->id, info->abbreviation,
	      info->name, info->longname);
}


static void dump_pcm_info(snd_pcm_info_t *info)
{
    char typestr[128];

    typestr[0] = 0;
    if (info->type & SND_PCM_INFO_PLAYBACK) {
	strcat(typestr, "SND_PCM_INFO_PLAYBACK ");
    }
    if (info->type & SND_PCM_INFO_CAPTURE) {
	strcat(typestr, "SND_PCM_INFO_CAPTURE ");
    }
    if (info->type & SND_PCM_INFO_DUPLEX) {
	strcat(typestr, "SND_PCM_INFO_DUPLEX ");
    }
    if (info->type & SND_PCM_INFO_DUPLEX_RATE) {
	strcat(typestr, "SND_PCM_INFO_DUPLEX_RATE ");
    }
    if (info->type & SND_PCM_INFO_DUPLEX_MONO) {
	strcat(typestr, "SND_PCM_INFO_DUPLEX_MONO ");
    }
    debug_msg("Card type=%d flags=%s id=%s name=%s\n",
	      info->type, typestr, info->id, info->name);
}

#if 0
static void dump_channel_info(snd_pcm_channel_info_t *c)
{
    char formatstr[1024];
    char chanstr[1024];
    formatstr[0] = 0;
    chanstr[0] = 0;

    if (c->flags & SND_PCM_CHNINFO_MMAP) {
	strcat(chanstr, "SND_PCM_CHNINFO_MMAP ");
    }
    if (c->flags & SND_PCM_CHNINFO_STREAM) {
	strcat(chanstr, "SND_PCM_CHNINFO_STREAM ");
    }
    if (c->flags & SND_PCM_CHNINFO_BLOCK) {
	strcat(chanstr, "SND_PCM_CHNINFO_BLOCK ");
    }
    if (c->flags & SND_PCM_CHNINFO_BATCH) {
	strcat(chanstr, "SND_PCM_CHNINFO_BATCH ");
    }
    if (c->flags & SND_PCM_CHNINFO_INTERLEAVE) {
	strcat(chanstr, "SND_PCM_CHNINFO_INTERLEAVE ");
    }
    if (c->flags & SND_PCM_CHNINFO_NONINTERLEAVE) {
	strcat(chanstr, "SND_PCM_CHNINFO_NONINTERLEAVE ");
    }
    if (c->flags & SND_PCM_CHNINFO_BLOCK_TRANSFER) {
	strcat(chanstr, "SND_PCM_CHNINFO_BLOCK_TRANSFER ");
    }
    if (c->flags & SND_PCM_CHNINFO_OVERRANGE) {
	strcat(chanstr, "SND_PCM_CHNINFO_OVERRANGE ");
    }
    if (c->flags & SND_PCM_CHNINFO_MMAP_VALID) {
	strcat(chanstr, "SND_PCM_CHNINFO_MMAP_VALID ");
    }
    if (c->flags & SND_PCM_CHNINFO_PAUSE) {
	strcat(chanstr, "SND_PCM_CHNINFO_PAUSE ");
    }
    if (c->flags & SND_PCM_CHNINFO_GLOBAL_PARAMS) {
	strcat(chanstr, "SND_PCM_CHNINFO_GLOBAL_PARAMS ");
    }


    if (c->formats & SND_PCM_FMT_S8) {
	strcat(formatstr, "SND_PCM_FMT_S8 ");
    }
    if (c->formats & SND_PCM_FMT_U8) {
	strcat(formatstr, "SND_PCM_FMT_U8 ");
    }
    if (c->formats & SND_PCM_FMT_S16_LE) {
	strcat(formatstr, "SND_PCM_FMT_S16_LE ");
    }
    if (c->formats & SND_PCM_FMT_S16_BE) {
	strcat(formatstr, "SND_PCM_FMT_S16_BE ");
    }
    if (c->formats & SND_PCM_FMT_U16_LE) {
	strcat(formatstr, "SND_PCM_FMT_U16_LE ");
    }
    if (c->formats & SND_PCM_FMT_U16_BE) {
	strcat(formatstr, "SND_PCM_FMT_U16_BE ");
    }
    if (c->formats & SND_PCM_FMT_S24_LE) {
	strcat(formatstr, "SND_PCM_FMT_S24_LE ");
    }
    if (c->formats & SND_PCM_FMT_S24_BE) {
	strcat(formatstr, "SND_PCM_FMT_S24_BE ");
    }
    if (c->formats & SND_PCM_FMT_U24_LE) {
	strcat(formatstr, "SND_PCM_FMT_U24_LE ");
    }
    if (c->formats & SND_PCM_FMT_U24_BE) {
	strcat(formatstr, "SND_PCM_FMT_U24_BE ");
    }
    if (c->formats & SND_PCM_FMT_S32_LE) {
	strcat(formatstr, "SND_PCM_FMT_S32_LE ");
    }
    if (c->formats & SND_PCM_FMT_S32_BE) {
	strcat(formatstr, "SND_PCM_FMT_S32_BE ");
    }
    if (c->formats & SND_PCM_FMT_U32_LE) {
	strcat(formatstr, "SND_PCM_FMT_U32_LE ");
    }
    if (c->formats & SND_PCM_FMT_U32_BE) {
	strcat(formatstr, "SND_PCM_FMT_U32_BE ");
    }
    if (c->formats & SND_PCM_FMT_FLOAT_LE) {
	strcat(formatstr, "SND_PCM_FMT_FLOAT_LE ");
    }
    if (c->formats & SND_PCM_FMT_FLOAT_BE) {
	strcat(formatstr, "SND_PCM_FMT_FLOAT_BE ");
    }
    if (c->formats & SND_PCM_FMT_FLOAT64_LE) {
	strcat(formatstr, "SND_PCM_FMT_FLOAT64_LE ");
    }
    if (c->formats & SND_PCM_FMT_FLOAT64_BE) {
	strcat(formatstr, "SND_PCM_FMT_FLOAT64_BE ");
    }
    if (c->formats & SND_PCM_FMT_IEC958_SUBFRAME_LE) {
	strcat(formatstr, "SND_PCM_FMT_IEC958_SUBFRAME_LE ");
    }
    if (c->formats & SND_PCM_FMT_IEC958_SUBFRAME_BE) {
	strcat(formatstr, "SND_PCM_FMT_IEC958_SUBFRAME_BE ");
    }
    if (c->formats & SND_PCM_FMT_MU_LAW) {
	strcat(formatstr, "SND_PCM_FMT_MU_LAW ");
    }
    if (c->formats & SND_PCM_FMT_A_LAW) {
	strcat(formatstr, "SND_PCM_FMT_A_LAW ");
    }
    if (c->formats & SND_PCM_FMT_IMA_ADPCM) {
	strcat(formatstr, "SND_PCM_FMT_IMA_ADPCM ");
    }
    if (c->formats & SND_PCM_FMT_MPEG) {
	strcat(formatstr, "SND_PCM_FMT_MPEG ");
    }
    if (c->formats & SND_PCM_FMT_GSM) {
	strcat(formatstr, "SND_PCM_FMT_GSM ");
    }
    if (c->formats & SND_PCM_FMT_SPECIAL) {
	strcat(formatstr, "SND_PCM_FMT_SPECIAL ");
    }

    debug_msg("subdev=%d subname=%s channel=%d mode=%d flags=%x %s formats=%x %s rates=%x\n",
	      c->subdevice, c->subname, c->channel, c->mode, c->flags, chanstr, c->formats, formatstr, c->rates);
    debug_msg("min_rate=%d max_rate=%d min_voices=%d max_voices=%d buffer_size=%d mixer_device=%d\n",
	      c->min_rate, c->max_rate, c->min_voices, c->max_voices, c->buffer_size, c->mixer_device);
}
#endif

static void dump_mixer_group(snd_mixer_group_t *g)
{
    debug_msg("gid=%d elements_size=%d elements=%d elements_over=%d\n",
	      g->gid, g->elements_size, g->elements, g->elements_over);
    debug_msg("caps=%x channels=%d mute=%d capture=%d capture_group=%d min=%d max=%d\n",
	      g->caps, g->channels, g->mute, g->capture, g->capture_group, g->min, g->max);
    debug_msg("frontleft=%d frontright=%d\n",
	      g->volume.names.front_left, 
	      g->volume.names.front_right);

/*
    debug_msg("elements:\n");
    for (i = 0; i < g->elements; i++)
    {
	p = &(g->pelements[i]);
	debug_msg("element %d: name=%s index=%d type=%d\n",
		  i,
		  p->name, p->index, p->type);
    }
*/
}
    
static int channelSetParams(snd_pcm_t *handle,
			   int channel,
			   audio_format *format)
{
    snd_pcm_channel_params_t p;
    int rc;

    /*
     * At some point this code should check to make
     * sure the format is valid for the hardware.
     * But for now we'll assume it is, and just
     * live with the error later.
     */
#if 0
    {
	snd_pcm_channel_info_t cinfo;
	memset(&cinfo, 0, sizeof(cinfo));
	cinfo.channel = channel;
	
	if (snd_pcm_channel_info(handle, &cinfo) != 0)
	    debug_msg("snd_pcm_info failed\n");
	else
	    dump_channel_info(&cinfo);
    }
#endif

    memset(&p, 0, sizeof(p));
    p.channel = channel;
    p.mode = SND_PCM_MODE_BLOCK;
    p.format.interleave = 1;
    p.format.format = mapFormat(format->encoding);
    p.format.rate = format->sample_rate;
    p.format.voices = format->channels;

    if (channel == SND_PCM_CHANNEL_PLAYBACK)
    {
	p.start_mode = SND_PCM_START_FULL;
	p.stop_mode = SND_PCM_STOP_ROLLOVER;
    }
    else
    {
	p.start_mode = SND_PCM_START_DATA;
	p.stop_mode = SND_PCM_STOP_STOP;
    }

    p.buf.block.frag_size = format->bytes_per_block;
    p.buf.block.frags_min = 1;
    p.buf.block.frags_max = -1;

    if ((rc = snd_pcm_channel_params(handle, &p)) != 0)
    {
	debug_msg("params playback failed: %s\n", snd_strerror(rc));
	return rc;
    }
    return 0;
}

static int channelPrepare(snd_pcm_t *handle, int channel)
{
    int rc;
    
    if ((rc = snd_pcm_channel_prepare(handle, channel)) != 0)
    {
	debug_msg("snd_pcm_channel_prepare(%s) failed: %s\n",
		  channel == SND_PCM_CHANNEL_PLAYBACK ? "playback" : "capture",
		  snd_strerror(rc));
	return rc;
    }
    return 0;
}

static int channelSetup(snd_pcm_t *handle, int channel,
			int *bufferSize)
{
    int rc;
    struct snd_pcm_channel_setup setup;

    memset(&setup, 0, sizeof(setup));
    setup.mode = SND_PCM_MODE_BLOCK;
    setup.channel = channel;

    if ((rc = snd_pcm_channel_setup(handle, &setup)) != 0)
    {
	debug_msg("setup failed: %s\n", snd_strerror(rc));
	*bufferSize = -1;
	return rc;
    }

    *bufferSize = setup.buf.block.frag_size;
    return 0;
}


static void setupMixer()
{
    snd_mixer_info_t minfo;
    snd_mixer_group_t group;
    snd_mixer_groups_t groups;
    int rc;

    debug_msg("Mixer open: card=%d device=%d\n", CurCard->cardNumber, CurCard->mixerDevice);
	      
    if (snd_mixer_open(&CurMixer, CurCard->cardNumber, CurCard->mixerDevice) != 0)
    {
	debug_msg("Mixer open failed\n");
	return;
    }
    debug_msg("curmixer=%x\n", CurMixer);

    if (snd_mixer_info(CurMixer, &minfo) != 0)
	debug_msg("mixer info failed");
    else
    {
	debug_msg("Mixer info: type=%d attrib=%x id=%s name=%s elements=%d groups=%d\n",
		  minfo.type, minfo.attrib, minfo.id, minfo.name, minfo.elements, minfo.groups);
    }

    memset(&groups, 0, sizeof(groups));
    if ((rc = snd_mixer_groups(CurMixer, &groups)) != 0)
    {
	debug_msg("groups failed: %s\n", snd_strerror(rc));
    }
    else
    {
	int i;

	debug_msg("Have %d groups (size=%d over=%d)\n", groups.groups, groups.groups_size, groups.groups_over);

	groups.groups_size = groups.groups_over;
	groups.pgroups = (snd_mixer_gid_t *) malloc(sizeof(snd_mixer_gid_t) * groups.groups_over);

	groups.groups_over = 0;
	groups.groups =0;
	snd_mixer_groups(CurMixer, &groups);
	debug_msg("Have %d groups (size=%d over=%d)\n", groups.groups, groups.groups_size, groups.groups_over);

	/*
	 * Scan the groups list. Sort out capture and output groups.
	 */
	
	
	for (i = 0; i < groups.groups_size; i++)
	{
	    char capstr[200];
	    unsigned int pidx;

	    memset(&group, 0, sizeof(group));
	    group.gid = groups.pgroups[i];

	    debug_msg("Group %d: name=%s\n", i, group.gid.name);

	    /*
	     * See which port this group corresponds to
	     */
	    for (pidx = 0; pidx < NUM_ALSA_PORTS; pidx++)
	    {
		if (strcmp(alsa_ports[pidx], group.gid.name) == 0)
		{
		    debug_msg("Found port pidx=%d\n", pidx);
		    alsa_gids[pidx] = group.gid;
		}
	    }
	    
	    if ((rc = snd_mixer_group_read(CurMixer, &group)) != 0)
	    {
		debug_msg("group_read failed: %s\n", snd_strerror(rc));
	    }
	    else
	    {
		capstr[0] = 0;
		if (group.caps & SND_MIXER_GRPCAP_VOLUME) {
		    strcat(capstr, "VOLUME ");
		}
		if (group.caps & SND_MIXER_GRPCAP_JOINTLY_VOLUME) {
		    strcat(capstr, "JOINTLY_VOLUME ");
		}
		if (group.caps & SND_MIXER_GRPCAP_MUTE) {
		    strcat(capstr, "MUTE ");
		}
		if (group.caps & SND_MIXER_GRPCAP_JOINTLY_MUTE) {
		    strcat(capstr, "JOINTLY_MUTE ");
		}
		if (group.caps & SND_MIXER_GRPCAP_CAPTURE) {
		    strcat(capstr, "CAPTURE ");
		}
		if (group.caps & SND_MIXER_GRPCAP_JOINTLY_CAPTURE) {
		    strcat(capstr, "JOINTLY_CAPTURE ");
		}
		if (group.caps & SND_MIXER_GRPCAP_EXCL_CAPTURE) {
		    strcat(capstr, "EXCL_CAPTURE ");
		}
		debug_msg("caps: %s\n", capstr);
		dump_mixer_group(&group);
	    }
	}
    }
}

static void mixer_rebuild(void *private)
{
    UNUSED(private);
    debug_msg("Mixer rebuild\n");
}

static void mixer_element(void *private, int cmd, snd_mixer_eid_t *eid)
{
    UNUSED(private);
    UNUSED(cmd);
    UNUSED(eid);
    debug_msg("mixer_element\n");
}

static void mixer_group(void *private, int cmd, snd_mixer_gid_t *gid)
{
    UNUSED(private);
    debug_msg("mixer_group %d %s\n", cmd, gid->name);
}

static snd_mixer_callbacks_t MixerCallbacks = {
    0, mixer_rebuild, mixer_element, mixer_group, { 0 }
};

static void updateMixer()
{
/*    debug_msg("start mixer read\n");*/
    snd_mixer_read(CurMixer, &MixerCallbacks);
/*    debug_msg("finish mixer read\n");*/
}

int
alsa_audio_open(audio_desc_t ad, audio_format *infmt, audio_format *outfmt)
{
    int playbackBufferSize, captureBufferSize;
    
    debug_msg("Audio open ad=%d\n", ad);
    debug_msg("Input format:\n");
    dump_audio_format(infmt);
    debug_msg("Output format:\n");
    dump_audio_format(outfmt);

    if (CurHandle != 0)
    {
	debug_msg("Device already open!\n");
	return FALSE;
    }

    CurCard = &(ratCards[ad]);
    CurCardIndex = ad;
    debug_msg("Opening card %s\n", CurCard->name);
    
    if (snd_pcm_open(&CurHandle, CurCard->cardNumber, CurCard->pcmDevice,
		     SND_PCM_OPEN_DUPLEX) != 0)
    {
	debug_msg("Card open failed\n");
	return FALSE;
    }

    /* Open up the mixer too */

    if (CurCard->mixerDevice != -1)
    {
	setupMixer();
    }
    else
	debug_msg("No mixer for device\n");

#define TRY_SETUP(func, args)		\
    {	\
	int rc;	\
	rc = func args;	\
	if (rc != 0)	\
	{	\
	    _dprintf("%d:%s:%d Card setup failed with %s\n",	\
		     getpid(), __FILE__, __LINE__, snd_strerror(rc));	\
	    if (CurHandle != 0) \
	        snd_pcm_close(CurHandle);	\
	    if (CurMixer != 0) \
		snd_mixer_close(CurMixer);	\
	    CurHandle = 0;	\
	    CurMixer = 0;	\
	    return FALSE;	\
	}	\
    }

    /* Set up the playback side */

    TRY_SETUP(channelSetParams, (CurHandle, SND_PCM_CHANNEL_PLAYBACK, outfmt));
    TRY_SETUP(channelPrepare, (CurHandle, SND_PCM_CHANNEL_PLAYBACK));
    TRY_SETUP(channelSetup, (CurHandle, SND_PCM_CHANNEL_PLAYBACK,
			     &playbackBufferSize));
       

    /* And the capture side */
    
    TRY_SETUP(channelSetParams, (CurHandle, SND_PCM_CHANNEL_CAPTURE, infmt));
    TRY_SETUP(channelPrepare, (CurHandle, SND_PCM_CHANNEL_CAPTURE));
    TRY_SETUP(channelSetup, (CurHandle, SND_PCM_CHANNEL_CAPTURE,
			     &captureBufferSize));
       

    debug_msg("Card open succeeded playback buffer=%d record buffer=%d\n",
	      playbackBufferSize, captureBufferSize);

    CurInPort = ALSA_INPORT_INDEX_MIC;
    CurOutPort = ALSA_OUTPORT_INDEX_MASTER;

    return TRUE;
}

/*
 * Shutdown.
 */
void
alsa_audio_close(audio_desc_t ad)
{
    int rc;

    debug_msg("Closing device %d\n", ad);
    
    if (CurCardIndex != ad)
    {
	debug_msg("Hm, CurCardIndex(%d) doesn't match request(%d)\n",
		  CurCardIndex, ad);
	return;
    }

    if ((rc = snd_pcm_close(CurHandle)) != 0)
    {
	debug_msg("Error on close: %s\n", snd_strerror(rc));
    }

    if ((rc = snd_mixer_close(CurMixer)) != 0)
    {
	debug_msg("Error on mixer close: %s\n", snd_strerror(rc));
    }

    CurMixer = 0;
    CurHandle = 0;
    CurCard = 0;
    CurCardIndex = -1;
    return;
}

/*
 * Flush input buffer.
 */
void
alsa_audio_drain(audio_desc_t ad)
{
    if (CurCardIndex != ad)
    {
	debug_msg("alsa_audio_drain: CurCardIndex(%d) doesn't match request(%d)\n",
		  CurCardIndex, ad);
	return;
    }

    debug_msg("audio_drain\n");
/*    if (snd_pcm_capture_flush(CurHandle) != 0)
	debug_msg("snd_pcm_flush_record failed\n");
*/
}

/*
 * Get the gain (0-100) on a mixer group
 */
static int get_group_gain(snd_mixer_gid_t *gid)
{
    snd_mixer_group_t group;
    int level;
    int rc;
    
    updateMixer();
    memset(&group, 0, sizeof(group));
    group.gid = *gid;

#ifdef DEBUG_MIXER
    debug_msg("gid is %s %d handle=%x\n", group.gid.name, group.gid.index, CurMixer);
#endif

    level = 0;
    if ((rc = snd_mixer_group_read(CurMixer, &group)) != 0)
    {
	debug_msg("mixer read failed: %s\n", snd_strerror(rc));
	return 0;
    }
    dump_mixer_group(&group);
    level = (int) 100.0 * ((double) (group.volume.names.front_left - group.min) / (double) (group.max - group.min));

#ifdef DEBUG_MIXER
    debug_msg("returning level=%d\n", level);
#endif


    return level;
}

/*
 * Set the gain (0-100) on a mixer group
 */
static void set_group_gain(snd_mixer_gid_t *gid, int gain)
{
    int rc;
    snd_mixer_group_t group;
    int level;

    updateMixer();
    memset(&group, 0, sizeof(group));
    group.gid = *gid;

#ifdef DEBUG_MIXER
    debug_msg("gid is %s %d handle=%x\n", group.gid.name, group.gid.index, CurMixer);
#endif

    level = 0;
    if ((rc =snd_mixer_group_read(CurMixer, &group)) != 0)
    {
	debug_msg("mixer read failed: %s\n", snd_strerror(rc));
	return;
    }
    
    level = (int) (((double) gain / 100.0) * ((double) (group.max - group.min))) + group.min;
    group.volume.names.front_left = group.volume.names.front_right = level;

    if ((rc = snd_mixer_group_write(CurMixer, &group)) != 0)
    {
	debug_msg("mixer_group_write failed: %s\n", snd_strerror(rc));
	return;
    }

#ifdef DEBUG_MIXER
    debug_msg("set level to %d\n", level);
#endif
}

/*
 * Set record gain.
 */
void
alsa_audio_set_igain(audio_desc_t ad, int gain)
{
#if 0
    debug_msg("Set igain %d %d\n", ad, gain);

    debug_msg("cur in is %d gid=%s %d\n", CurInPort,
	      alsa_in_gids[CurInPort].name,
	      alsa_in_gids[CurInPort].index);
#endif
    UNUSED(ad);

    set_group_gain(&(alsa_gids[CurInPort]), gain);
    
    return;
}

/*
 * Get record gain.
 */
int
alsa_audio_get_igain(audio_desc_t ad)
{
    UNUSED(ad);
    return get_group_gain(&(alsa_gids[CurInPort]));
}

int
alsa_audio_duplex(audio_desc_t ad)
{
    UNUSED(ad);
  /*    debug_msg("set duplex for %d\n", ad);*/
    return TRUE;
}

/*
 * Set play gain.
 */
void
alsa_audio_set_ogain(audio_desc_t ad, int vol)
{
    UNUSED(ad);
    set_group_gain(&(alsa_gids[CurOutPort]), vol);
    return;
}

/*
 * Get play gain.
 */
int
alsa_audio_get_ogain(audio_desc_t ad)
{
    UNUSED(ad);
    return get_group_gain(&(alsa_gids[CurOutPort]));
}

/*
 * Record audio data.
 */
int
alsa_audio_read(audio_desc_t ad, u_char *buf, int buf_bytes)
{
    int read_bytes;
    static int nerrs = 30;

#if 0
//    int nbufs = buf_bytes / CurBufSize;
    int nbufs = 1;

    if (nbufs * CurBufSize > buf_bytes)
    {
	debug_msg("Not enoughs pace in read, nbufs=%d cur=%d bytes=%d\n",
		  nbufs, CurBufSize, buf_bytes);
	abort();
    }
#endif
    
    UNUSED(ad);

    /*    checkStatus(CAP); */
    if ((read_bytes = snd_pcm_read(CurHandle, buf, buf_bytes)) < 0)
    {
	if (nerrs-- > 0)
	{
	    checkStatus(CAP);

	    debug_msg("read(%x %d) failed: %s\n",buf, buf_bytes, snd_strerror(read_bytes));
	}
	read_bytes = 0;
    }
    else
    {
#if 0
	static FILE *fp = 0;
	if (fp == 0)
	{
	    fp = fopen("/tmp/out", "w");
	}
	fprintf(fp, "%d %d\n", read_bytes, buf_bytes);
#endif
/*      	debug_msg("read %d of %d\n", read_bytes, buf_bytes);*/
    }

    return read_bytes;
}

/*
 * Playback audio data.
 */
int
alsa_audio_write(audio_desc_t ad, u_char *buf, int buf_bytes)
{
    int rc;
    int write_bytes;

    UNUSED(ad);

    write_bytes  = snd_pcm_write(CurHandle, buf, buf_bytes);

    if (write_bytes < 0)
    {
	if (write_bytes == -EAGAIN)
	    return 0;
	else if (write_bytes == -EIO)
	{
	    if ((rc = snd_pcm_playback_flush(CurHandle)) != 0)
		debug_msg("playback_flush failed: %s\n", snd_strerror(rc));
	    checkStatus(PB);
	    return 0;
	}
	else if (write_bytes == -EINVAL)
	{
	    int status;
	    
	    /*
	    debug_msg("write %d failed: %d %s\n",
		      buf_bytes, 
		      write_bytes, snd_strerror(write_bytes));
	    */
	    status = checkStatus(PB);
	    /* debug_msg("status was %d\n", status); */
	    if (status == SND_PCM_STATUS_READY)
	    {
		if ((rc = snd_pcm_channel_prepare(CurHandle, SND_PCM_CHANNEL_PLAYBACK)) != 0)
		{
		    debug_msg("playback prepare failed: %s\n", snd_strerror(rc));
		}
		debug_msg("prepared\n");

		checkStatus(PB);

		/*
		if ((rc = snd_pcm_playback_go(CurHandle) ) != 0)
		{
		    debug_msg("playback_go failed with %s\n", snd_strerror(rc));
		}
		debug_msg("go!\n");
		checkStatus(PB);
	    */
	    }
	    
	    return 0;
	}
	

	debug_msg("write failed: %d %s\n", write_bytes, snd_strerror(write_bytes));
	return 0;
    }
    
    return write_bytes;
}


/*
 * Set options on audio device to be non-blocking.
 */
void
alsa_audio_non_block(audio_desc_t ad)
{
    int rc;
    debug_msg("set nonblocking\n");
    checkStatus(CAP);
    if (ad != CurCardIndex)
	debug_msg("alsa_audio_non_block: ad != current\n");
    if ((rc = snd_pcm_nonblock_mode(CurHandle, 1)) != 0)
	debug_msg("nonblock_mode failed: %s\n", snd_strerror(rc));
    checkStatus(CAP);
}

/*
 * Set options on audio device to be blocking.
 */
void
alsa_audio_block(audio_desc_t ad)
{
    int rc;
    debug_msg("set blocking\n");
    checkStatus(CAP);
    if (ad != CurCardIndex)
	debug_msg("alsa_audio_non_block: ad != current\n");
    if ((rc = snd_pcm_nonblock_mode(CurHandle, 0)) != 0)
	debug_msg("nonblock_mode failed: %s\n", snd_strerror(rc));
    checkStatus(CAP);
}

/*
 * Set output port.
 */
void
alsa_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
    debug_msg("oport_set %d %d %s\n", ad, port, alsa_ports[port]);
    CurOutPort = port;
    return;
}

/*
 * Get output port.
 */

audio_port_t
alsa_audio_oport_get(audio_desc_t ad)
{
    debug_msg("oport_get %d\n", ad);
    return CurOutPort;
}

int
alsa_audio_oport_count(audio_desc_t ad)
{
	int n = ALSA_NUM_OUTPORTS;
	debug_msg("get oport count %d returning %d\n", ad, n);
	return n;
}

const audio_port_details_t*
alsa_audio_oport_details(audio_desc_t ad, int idx)
{
	debug_msg("oport details ad=%d idx=%d\n", ad, idx);
        return &out_ports[idx];
}
/*
 * Set mute on this input port.
 */
static void enable_mute(int port, int enabled)
{
    int rc;
    snd_mixer_group_t group;

    updateMixer();
    
    memset(&group, 0, sizeof(group));
    group.gid = alsa_gids[port];

    if ((rc = snd_mixer_group_read(CurMixer, &group)) != 0)
    {
	debug_msg("mixer read failed: %s\n", snd_strerror(rc));
	return;
    }

    /* dump_mixer_group(&group); */
    if (enabled)
    {
	int val;
	val = SND_MIXER_CHN_MASK_STEREO;

	group.mute = val;
	/*	if (group.caps & SND_MIXER_GRPCAP_JOINTLY_MUTE)
		val = group.channels;*/
	/* group.capture ^= (val & group.channels); */

    }
    else
	group.mute = 0;

    /**/
    debug_msg("set group.mute to %d for port %d %s\n",
    group.mute, port, alsa_ports[port]);
    /**/

    if ((rc = snd_mixer_group_write(CurMixer, &group)) != 0)
    {
	debug_msg("enable_capture: mixer write failed: %d %s\n", rc, snd_strerror(rc));
	return;
    }

    checkStatus(CAP);
}


/*
 * Set capture on this input port.
 */
static void enable_capture(int port, int enabled)
{
    int rc;
    snd_mixer_group_t group;

    updateMixer();
    
    memset(&group, 0, sizeof(group));
    group.gid = alsa_gids[port];

    if ((rc = snd_mixer_group_read(CurMixer, &group)) != 0)
    {
	debug_msg("mixer read failed: %s\n", snd_strerror(rc));
	return;
    }

    /* dump_mixer_group(&group); */
    if (enabled)
    {
	int val;
	val = SND_MIXER_CHN_MASK_STEREO;

	group.capture = val;
	/*	if (group.caps & SND_MIXER_GRPCAP_JOINTLY_CAPTURE)
	    val = group.channels;
	    group.capture ^= (val & group.channels);*/

    }
    else
	group.capture = 0;

    /*
    debug_msg("set group.capture to %d for port %d %s\n",
    group.capture, port, alsa_ports[port]);
    */

    if ((rc = snd_mixer_group_write(CurMixer, &group)) != 0)
    {
	debug_msg("enable_capture: mixer write failed: %d %s\n", rc, snd_strerror(rc));
	return;
    }

    checkStatus(CAP);
}


/*
 * set the loopback gain on the currently selected port to match CurLoopbackGain
 * This might properly turn off loopback on the other devices.
 */
static void setLoopbackGain()
{
    int mute;

    mute = (CurLoopbackGain == 0);
    enable_mute(CurInPort, mute);
}

/*
 * Set input port.
 */
void
alsa_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
    unsigned int i;

    UNUSED(ad);
/*    debug_msg("iport_set %d %d %s\n", ad, port, alsa_ports[port]);*/

    for (i = 0; i < ALSA_NUM_INPORTS; i++)
    {
      enable_capture(in_ports[i].port, 0);
    }
    CurInPort = port;
    enable_capture(CurInPort, 1);

    /*
     * Also set the loopback status for this. It's a global state in
     * rat, not per-port.
     */
    setLoopbackGain();
    return;
}


/*
 * Get input port.
 */
audio_port_t
alsa_audio_iport_get(audio_desc_t ad)
{
    UNUSED(ad);
/*    debug_msg("iport_get %d\n", ad);*/
    return CurInPort;
}

int
alsa_audio_iport_count(audio_desc_t ad)
{
    int n =  ALSA_NUM_INPORTS;
    UNUSED(ad);
/*    debug_msg("get iport count %d returning %d\n", ad, n);*/
    return n;
}

const audio_port_details_t*
alsa_audio_iport_details(audio_desc_t ad, int idx)
{
    UNUSED(ad);
/*	debug_msg("iport details ad=%d idx=%d\n", ad, idx);*/
        return &in_ports[idx];
}

/*
 * Enable hardware loopback
 */
void 
alsa_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        UNUSED(gain);
	debug_msg("loopback gain=%d\n", gain);
	CurLoopbackGain = gain;
	setLoopbackGain();
        /* Nothing doing... */
}

/*
 * For external purposes this function returns non-zero
 * if audio is ready.
 */
int
alsa_audio_is_ready(audio_desc_t ad)
{
    fd_set wfds, rfds;
    int n;
    int cap_fd, play_fd;
    struct timeval tv;

    UNUSED(ad);
    
    cap_fd = snd_pcm_file_descriptor(CurHandle, SND_PCM_CHANNEL_CAPTURE);
    play_fd = snd_pcm_file_descriptor(CurHandle, SND_PCM_CHANNEL_PLAYBACK);

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

/*    FD_SET(play_fd, &wfds); */
    FD_SET(cap_fd, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = 10;
    
    n = select(20,
	       &rfds, &wfds, 0, &tv);
    if (n > 0)
    {
	/*	debug_msg("Audio ready\n");*/
    }
    else if (n < 0)
    {
	debug_msg("select error: %s\n", strerror(errno));
    }
    else
    {
//	 debug_msg("no audio ready on %d\n", cap_fd); 
    }

    /* checkStatus(CAP); */
/*    return 1;*/
    return n > 0;
}

static void 
alsa_audio_select(audio_desc_t ad, int delay_ms)
{
    fd_set rfds;
    int n;
    int cap_fd;
    struct timeval tv;

    UNUSED(ad);
    
    cap_fd = snd_pcm_file_descriptor(CurHandle, SND_PCM_CHANNEL_CAPTURE);
/*    play_fd = snd_pcm_file_descriptor(CurHandle, SND_PCM_CHANNEL_PLAYBACK);*/

    FD_ZERO(&rfds);
/*    FD_ZERO(&wfds);*/

/*    FD_SET(play_fd, &wfds); */
    FD_SET(cap_fd, &rfds); 

    tv.tv_sec = delay_ms / 1000;
    tv.tv_usec = 1000 * (delay_ms % 1000);
    
/*    n = select((cap_fd > play_fd ? cap_fd : play_fd) + 1,
      &rfds, &wfds, 0, &tv); */
    n = select(cap_fd + 1, 
	       &rfds, 0, 0, &tv);
}

void
alsa_audio_wait_for(audio_desc_t ad, int delay_ms)
{
    if (alsa_audio_is_ready(ad) == FALSE) {
	alsa_audio_select(ad, delay_ms);
    }
}

const char *
alsa_get_device_name(audio_desc_t idx)
{
        debug_msg("get name for card %d\n", idx);
	if (idx < 0 || idx >= nRatCards)
	{
	    debug_msg("Card %d out of range 0..%d\n",
		      idx, nRatCards - 1);
	    return NULL;
	}
	else
	{
	    return(ratCards[idx].name);
	}
}

int
alsa_audio_init()
{
    int n_cards = snd_cards();
    int card;

    /*
     * Find all the PCM devices
     */
    for (card = 0; card < n_cards; card++)
    {
	int err;  
	snd_ctl_t *handle;  
	snd_ctl_hw_info_t info;
	unsigned int pcmdev;

	if ((err = snd_ctl_open(&handle, card)) < 0) {  
	    debug_msg("open failed: %s\n", snd_strerror(err)); 
	    continue;
	}  

	if ((err = snd_ctl_hw_info(handle, &info)) < 0) { 
	    debug_msg("hw info failed: %s\n", 
		    snd_strerror(err));  

	    snd_ctl_close(handle); 
	    continue;
	} 

	dump_hw_info(&info);

	/* Scan the PCM device list */
	for (pcmdev = 0; pcmdev < info.pcmdevs; pcmdev++)
	{
	    snd_pcm_info_t pinfo;
	    RatCardInfo *ratCard;
	    
	    if (snd_ctl_pcm_info(handle, pcmdev, &pinfo) == 0)
	    {
		dump_pcm_info(&pinfo);
	    }
	    else
	    {
		debug_msg("Couldn't get pcm info for card %d dev %d\n",
			  card, pcmdev);
	    }
	    /*
	     * Hm, goofy. SbLive shows 3 pcm devices per card. We'll just
	     * use the first one.
	     */

	    if (pcmdev == 0)
	    {
		ratCard = &(ratCards[nRatCards++]);
		ratCard->cardNumber = card;
		ratCard->pcmDevice = 0;
		snprintf(ratCard->name, sizeof(ratCard->name),
			 "%s (alsa %s)",
			 info.name, info.id);
		if (info.mixerdevs > 0)
		    ratCard->mixerDevice = 0;
		else
		    ratCard->mixerDevice = -1;
	    }
	}

	    
	snd_ctl_close(handle);  
    }
    debug_msg("alsa_audio_device_count: nDevices = %d\n", nRatCards);
    
    return nRatCards;
}

int
alsa_get_device_count()
{
    return nRatCards;
}

int
alsa_audio_supports(audio_desc_t ad, audio_format *fmt)
{
        UNUSED(ad);
        if ((!(fmt->sample_rate % 8000) || !(fmt->sample_rate % 11025)) && 
            (fmt->channels == 1 || fmt->channels == 2)) {
                return TRUE;
        }
        return FALSE;
}

