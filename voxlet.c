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
        struct s_sndfile  *sound;       /* open sound file                     */
        pdb_entry_t        *pdbe;        /* voxlet pretends to be a participant */
        struct s_converter *converter;   /* sample rate and channel converter   */
};

static const uint32_t VOXLET_SSRC_ID = 0xffff0000; /* 1 in 4 billion */

int  
voxlet_create  (voxlet_t **ppv, struct s_mix_info *mixer, struct s_fast_time *clock, struct s_pdb *pdb, const char *sndfile)
{
        voxlet_t         *pv;
        struct s_sndfile *sound;
        pdb_entry_t      *pdbe;

        if (snd_read_open(&sound, (char*)sndfile, NULL) == 0) {
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
}
