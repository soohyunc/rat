/*
 * FILE:    voxlet.c
 * PROGRAM: RAT
 * AUTHORS: Orion Hodson / Colin Perkins
 *
 *
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 */
 
#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = 
	"$Id$";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "util.h"

#include "ts.h"
#include "audio_types.h"
#include "pdb.h"
#include "mix.h"
#include "sndfile.h"
#include "converter_types.h"
#include "converter.h"

#include "voxlet.h"

struct _voxlet {
        struct s_sndfile  *sound;       /* open sound file                   */
        pdb_entry_t        *pdbe;       /* spoof participant                 */
        struct s_converter *converter;  /* sample rate and channel converter */
};

static const uint32_t VOXLET_SSRC_ID = 0xffff0000; /* 1 in 4 billion */

int  
voxlet_create  (voxlet_t          **ppv, 
                struct s_mix_info  *mixer, 
                struct s_fast_time *clock, 
                struct s_pdb       *pdb, 
                const char         *sndfile)
{
        const  mixer_info_t *mi;
        struct s_sndfile    *sound;
        sndfile_fmt_t        sfmt;
        pdb_entry_t         *pdbe;
        voxlet_t            *pv;

        if (snd_read_open(&sound, (char*)sndfile, NULL) == 0 ||
            snd_get_format(sound, &sfmt) == 0) {
                debug_msg("voxlet could not open: %s\n", sndfile);
                return FALSE;
        }

        if (pdb_item_create(pdb, clock, 8000, VOXLET_SSRC_ID) == FALSE) {
                debug_msg("voxlet could not create spoof participant\n");
                snd_read_close(&sound);
                return FALSE;
        }
        
        pv  = (voxlet_t*)xmalloc(sizeof(voxlet_t));
        if (pv == NULL) {
                debug_msg("Could not allocate voxlet\n");
                pdb_item_destroy(pdb, VOXLET_SSRC_ID);
                snd_read_close(&sound);
                return FALSE;
        }

        *ppv          = pv;
        pv->sound     = sound;
        pv->pdbe      = pdb;
        pv->converter = NULL;
        
        mi = mixer_get_info(ms);

        if (sfmt->sample_rate != mi->sample_rate ||
            sfmt->channels    != mi->channels) {
                const converter_details_t *cd;
                converter_fmt_t cf;
                uint32_t i, n;
                /* Try to get best quality converter */
                n = converter_get_count();
                for (i = 0; i < n; i++) {
                        cd = converter_get_details(i);
                        if (strncmp(cd->name, "High", 4) == 0) {
                                break;
                        }
                }
                /* Safety in case someone changes converter names */
                if (i == n) {
                        debug_msg("Could not find hq converter\n");
                        cd = converter_get_details(0);
                }
                
                if (converter_create(cd->id, &v->converter,
                                     
        }
        voxlet_configure_converter(v, 
                                   sfmt->sample_rate, sfmt->channels,
                                   mi->sample_rate, mi->channels);
        return TRUE;
}




