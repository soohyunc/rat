/*
 * FILE:    settings.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins 
 * 
 * $Revision$
 * $Date$
 * 
 * Copyright (c) 1999 University College London
 * All rights reserved.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "ts.h"
#include "channel.h"
#include "net_udp.h"
#include "rtcp_pckt.h"
#include "rtcp_db.h"
#include "session.h"
#include "repair.h"
#include "timers.h"
#include "codec_types.h"
#include "codec.h"
#include "auddev.h"
#include "version.h"
#include "settings.h"

void save_settings(session_struct *sp)
{
	FILE				*outf = stdout;
	codec_id_t	 		 pri_id;
        const codec_format_t 		*pri_cf;
        cc_details 			 cd;
	int				 cc_len;
	char				*cc_param;
        converter_details_t  		 converter;
	int		 		 i;
	audio_device_details_t		 ad;
        const audio_format 		*af;
        const audio_port_details_t 	*iapd, *oapd;

	pri_id   = codec_get_by_payload(sp->encodings[0]);
        pri_cf   = codec_get_format(pri_id);
        cc_len   = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cc_param = (char*) xmalloc(cc_len);
        channel_encoder_get_parameters(sp->channel_coder, cc_param, cc_len);
        channel_get_coder_identity(sp->channel_coder, &cd);

        for(i = 0; i < (int) converter_get_count(); i++) {
                converter_get_details(i, &converter);
                if (sp->converter == converter.id) {
			break;
                }
        }

        for(i = 0; i < audio_get_device_count(); i++) {
                if (audio_get_device_details(i, &ad) && sp->audio_device == ad.descriptor) {
                        break;
                }
        }

        af = audio_get_ifmt(sp->audio_device);

        for(i = 0; i < audio_get_iport_count(sp->audio_device); i++) {
                iapd = audio_get_iport_details(sp->audio_device, i);
                if (iapd->port == audio_get_iport(sp->audio_device)) {
                        break;
                }
        }

        for(i = 0; i < audio_get_oport_count(sp->audio_device); i++) {
                oapd = audio_get_oport_details(sp->audio_device, i);
                if (oapd->port == audio_get_oport(sp->audio_device)) {
                        break;
                }
        }

	fprintf(outf, "*rtpName:                %s\n",  sp->db->my_dbe->sentry->name);
	fprintf(outf, "*rtpEmail:               %s\n",  sp->db->my_dbe->sentry->email);
	fprintf(outf, "*rtpPhone:               %s\n",  sp->db->my_dbe->sentry->phone);
	fprintf(outf, "*rtpLoc:                 %s\n",  sp->db->my_dbe->sentry->loc);
	fprintf(outf, "*audioTool:              %s\n",  sp->db->my_dbe->sentry->tool);

	fprintf(outf, "*audioPrimary:           %s\n",  pri_cf->short_name);
	fprintf(outf, "*audioUnits:             %d\n",  channel_encoder_get_units_per_packet(sp->channel_coder)); 
	fprintf(outf, "*audioChannelCoding:     %s\n",  cd.name);
	fprintf(outf, "*audioChannelParameters: %s\n",  cc_param);

	fprintf(outf, "*audioRepair:            %s\n",  repair_get_name(sp->repair));
	fprintf(outf, "*audioAutoConvert:       %s\n",  converter.name);
	fprintf(outf, "*audioLimitPlayout:      %d\n",  sp->limit_playout);
	fprintf(outf, "*audioMinPlayout:        %ld\n", sp->min_playout);
	fprintf(outf, "*audioMaxPlayout:        %ld\n", sp->max_playout);
	fprintf(outf, "*audioLecture:           %d\n",  sp->lecture);
	fprintf(outf, "*audio3dRendering:       %d\n",  sp->render_3d);

	fprintf(outf, "*audioDevice:            %s\n",  ad.name);
	fprintf(outf, "*audioFrequency:         %d\n",  af->sample_rate);
	fprintf(outf, "*audioChannelsIn:        %d\n",  af->channels); 
	fprintf(outf, "*audioSilence:           %d\n",  sp->detect_silence);
	fprintf(outf, "*audioAGC:               %d\n",  sp->agc_on);
	fprintf(outf, "*audioLoopback:          %d\n",  sp->loopback_gain); 
	fprintf(outf, "*audioEchoSuppress:      %d\n",  sp->echo_suppress);
	fprintf(outf, "*audioOutputGain:        %d\n",  audio_get_ogain(sp->audio_device));
	fprintf(outf, "*audioInputGain:         %d\n",  audio_get_igain(sp->audio_device));
	fprintf(outf, "*audioOutputPort:        %s\n",  oapd->name);
	fprintf(outf, "*audioInputPort:         %s\n",  iapd->name); 

	fprintf(outf, "*audioPowermeters:       %d\n",  sp->meter);
	fprintf(outf, "*audioLipSync:           %d\n",  sp->sync_on);
/*	fprintf(outf, "*audioHelpOn:            %s\n",  ); */
/*	fprintf(outf, "*audioMatrixOn:          %s\n",  ); */
/*	fprintf(outf, "*audioPlistOn:           %s\n",  ); */
/*	fprintf(outf, "*audioFilesOn:           %s\n",  ); */
}

