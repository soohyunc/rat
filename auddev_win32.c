/*
* FILE:	auddev_win32.c
*
* Win32 audio interface for RAT.
*
* Written by Orion Hodson and Isidor Kouvelas
* Some portions based on the VAT Win95 port by John Brezak.
*
* $Id$
*
* Copyright (c) 1995-99 University College London
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, is permitted, for non-commercial use only, provided
* that the following conditions are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
*    must display the following acknowledgement:
*    This product includes software developed by the Computer Science
*    Department at University College London
* 4. Neither the name of the University nor of the Department may be used
*    to endorse or promote products derived from this software without
*    specific prior written permission.
* Use of this software for commercial purposes is explicitly forbidden
* unless prior written permission is obtained from the authors.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*
*/

#ifdef WIN32

#include "config_win32.h"
#include "audio.h"
#include "debug.h"
#include "memory.h"
#include "auddev_win32.h"
#include "audio_types.h"
#include "audio_fmt.h"
#include "util.h"
#include "mmsystem.h"

#define rat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_rat(x)	((x) * MAX_AMP / 255)

#define W32SDK_MAX_DEVICES 5
static  int have_probed[W32SDK_MAX_DEVICES];
static  int w32sdk_probe_formats(audio_desc_t ad);

static int  error = 0;
static char errorText[MAXERRORLENGTH];
extern int  thread_pri;
static int  nLoopGain = 100;
#define     MAX_DEV_NAME 64

/* 
* Mixer Code (C) 1998-99 Orion Hodson.
*
* no thanks to the person who wrote the microsoft documentation 
* (circular and information starved) for the mixer, or the folks 
* who conceived the api in the first place.  Some of the api works,
* some doesn't.  Too bad documentation does not highlight this. Grrr!
*/

/* mcd_elem_t is a node used to store control state so 
 * we can restore mixer controls when device closes.
 */

typedef struct s_mcd_elem {
        MIXERCONTROLDETAILS *pmcd;
        struct s_mcd_elem   *next;
} mcd_elem_t;

static mcd_elem_t *control_list;

#define MIX_ERR_LEN 32
#define MIX_MAX_CTLS 8
#define MIX_MAX_GAIN 100

static int32	play_vol, rec_vol;
static HMIXER   hMixer;

static DWORD    dwMixDstRec, dwMixDstVol; 

static audio_port_details_t *input_ports, *loop_ports;
static int                   n_input_ports, n_loop_ports;
static int iport; /* Current input port */

static const char *
mixGetErrorText(MMRESULT mmr)
{
#ifndef NDEBUG
        switch (mmr) {
        case MMSYSERR_NOERROR:     return "no error"; 
        case MIXERR_INVALLINE:     return "invalid line";
        case MIXERR_INVALCONTROL:  return "invalid control";
        case MIXERR_INVALVALUE:    return "invalid value";
        case WAVERR_BADFORMAT:     return "bad format";
        case MMSYSERR_BADDEVICEID: return "bad device id";
        case MMSYSERR_INVALFLAG:   return "invalid flag";
        case MMSYSERR_INVALHANDLE: return "invalid handle";
        case MMSYSERR_INVALPARAM:  return "invalid param";
        case MMSYSERR_NODRIVER:    return "no driver!";
        }
        return "Undefined Error";
#endif /* NDEBUG */
        return "Mixer Error.";
}

static const char *
mixGetControlType(DWORD dwCtlType)
{
        switch(dwCtlType) {
        case MIXERCONTROL_CONTROLTYPE_CUSTOM:        return "Custom";         
        case MIXERCONTROL_CONTROLTYPE_BOOLEANMETER:  return "Boolean Meter";
        case MIXERCONTROL_CONTROLTYPE_SIGNEDMETER:   return "Signed Meter";
        case MIXERCONTROL_CONTROLTYPE_PEAKMETER:     return "PeakMeter";
        case MIXERCONTROL_CONTROLTYPE_UNSIGNEDMETER: return "Unsigned Meter";
        case MIXERCONTROL_CONTROLTYPE_BOOLEAN:       return "Boolean";
        case MIXERCONTROL_CONTROLTYPE_ONOFF:         return "OnOff";
        case MIXERCONTROL_CONTROLTYPE_MUTE:          return "Mute";
        case MIXERCONTROL_CONTROLTYPE_MONO:          return "Mono";
        case MIXERCONTROL_CONTROLTYPE_LOUDNESS:      return "Loudness";
        case MIXERCONTROL_CONTROLTYPE_STEREOENH:     return "Stereo Enhanced";
        case MIXERCONTROL_CONTROLTYPE_BUTTON:        return "Button";
        case MIXERCONTROL_CONTROLTYPE_DECIBELS:      return "Decibels";
        case MIXERCONTROL_CONTROLTYPE_SIGNED:        return "Signed";
        case MIXERCONTROL_CONTROLTYPE_UNSIGNED:      return "Unsigned";
        case MIXERCONTROL_CONTROLTYPE_PERCENT:       return "Percent";
        case MIXERCONTROL_CONTROLTYPE_SLIDER:        return "Slider";
        case MIXERCONTROL_CONTROLTYPE_PAN:           return "Pan";
        case MIXERCONTROL_CONTROLTYPE_QSOUNDPAN:     return "Q Sound Pan";
        case MIXERCONTROL_CONTROLTYPE_FADER:         return "Fader";
        case MIXERCONTROL_CONTROLTYPE_VOLUME:        return "Volume";
        case MIXERCONTROL_CONTROLTYPE_BASS:          return "Bass";
        case MIXERCONTROL_CONTROLTYPE_TREBLE:        return "Treble";
        case MIXERCONTROL_CONTROLTYPE_EQUALIZER:     return "Equalizer";
        case MIXERCONTROL_CONTROLTYPE_SINGLESELECT:  return "Single Select";
        case MIXERCONTROL_CONTROLTYPE_MUX:           return "Mux";
        case MIXERCONTROL_CONTROLTYPE_MULTIPLESELECT:return "Multiple Select";
        case MIXERCONTROL_CONTROLTYPE_MIXER:         return "Mixer";
        case MIXERCONTROL_CONTROLTYPE_MICROTIME:     return "Micro Time";
        case MIXERCONTROL_CONTROLTYPE_MILLITIME:     return "Milli Time";
        }
        return "Unknown";
}

static void
mixerDumpLineInfo(HMIXEROBJ hMix, DWORD dwLineID)
{
        MIXERLINECONTROLS mlc;
        LPMIXERCONTROL    pmc;
        MIXERLINE ml;
        MMRESULT  mmr;
        UINT      i;
        
        /* Determine number of controls */
        ml.cbStruct = sizeof(ml);
        ml.dwLineID = dwLineID;
        
        mmr = mixerGetLineInfo((HMIXEROBJ)hMix, &ml, MIXER_GETLINEINFOF_LINEID | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg(mixGetErrorText(mmr));
                return;
        }
        
        pmc = (LPMIXERCONTROL)xmalloc(sizeof(MIXERCONTROL)*ml.cControls);
        mlc.cbStruct  = sizeof(MIXERLINECONTROLS);
        mlc.cbmxctrl  = sizeof(MIXERCONTROL);
        mlc.pamxctrl  = pmc;
        mlc.cControls = ml.cControls;
        mlc.dwLineID  = dwLineID;
        
        mmr = mixerGetLineControls((HMIXEROBJ)hMix, &mlc, MIXER_GETLINECONTROLSF_ALL | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg(mixGetErrorText(mmr));
                xfree(pmc);
                return;
        }
        
        for(i = 0; i < ml.cControls; i++) {
                debug_msg("- %u %s\t\t %s\n", i, pmc[i].szName, mixGetControlType(pmc[i].dwControlType));
        }
        xfree(pmc);
}

int
mcd_elem_add_control(mcd_elem_t **pplist, MIXERCONTROLDETAILS *pmcd)
{
        mcd_elem_t *elem;
        
        elem = (mcd_elem_t*)xmalloc(sizeof(mcd_elem_t));
        if (elem) {
                elem->pmcd = pmcd;
                elem->next = *pplist;
                *pplist    = elem;
                return TRUE;
        }
        return FALSE;
}

MIXERCONTROLDETAILS*
mcd_elem_get_control(mcd_elem_t **pplist)
{
        MIXERCONTROLDETAILS *pmcd;
        mcd_elem_t *elem;
        
        elem = *pplist;
        if (elem) {
                pmcd    = elem->pmcd;
                *pplist = elem->next;
                xfree(elem);
                return pmcd;
        }
        return NULL;
}

void
mixRestoreControls(UINT uMix, mcd_elem_t **pplist)
{
        MIXERCONTROLDETAILS *pmcd;
        MMRESULT mmr;
        
        while((pmcd = mcd_elem_get_control(pplist)) != NULL) {
                mmr = mixerSetControlDetails((HMIXEROBJ)uMix, pmcd, MIXER_OBJECTF_MIXER);
                xfree(pmcd->paDetails);
                xfree(pmcd);
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("mixerSetControlDetails: %s\n", mixGetErrorText(mmr));
                        continue;
                }
        }
        assert(*pplist == NULL);
}

void
mixSaveLine(UINT uMix, MIXERLINE *pml, mcd_elem_t **pplist)
{
        MIXERCONTROLDETAILS *pmcd;
        MIXERLINECONTROLS mlc;
        MIXERCONTROL     *pmc;
        MMRESULT          mmr;
        UINT              i;
        
        /* Retrieve control types */
        pmc = (MIXERCONTROL*)xmalloc(sizeof(MIXERCONTROL)*pml->cControls);
        
        mlc.cbStruct  = sizeof(mlc);
        mlc.dwLineID  = pml->dwLineID;
        mlc.cControls = pml->cControls;
        mlc.pamxctrl  = pmc;
        mlc.cbmxctrl  = sizeof(MIXERCONTROL);
        
        mmr = mixerGetLineControls((HMIXEROBJ)uMix, &mlc, MIXER_GETLINECONTROLSF_ALL | MIXER_OBJECTF_MIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetLineControls: %s\n", mixGetErrorText(mmr));
                xfree(pmc);
                return;
        }
        
        for(i = 0; i < pml->cControls; i++) {
                DWORD itemCnt, itemSz;
                if (pmc[i].cMultipleItems == 0) {
                        itemCnt = 1;	
                } else {
                        itemCnt = pmc[i].cMultipleItems;
                } 
                
                switch(pmc[i].dwControlType & MIXERCONTROL_CT_UNITS_MASK) {
                        /* Our application on affects boolean types (mute, on/off) and unsigned (vol) */
                case MIXERCONTROL_CT_UNITS_BOOLEAN:
                        itemSz = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
                        break;
                case MIXERCONTROL_CT_UNITS_UNSIGNED:
                        itemSz = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                        break;
                default:
                        debug_msg("not done %s\n", pmc[i].szName);
                        continue;
                }
                pmcd = (MIXERCONTROLDETAILS*)xmalloc(sizeof(MIXERCONTROLDETAILS));
                pmcd->cbStruct       = sizeof(MIXERCONTROLDETAILS);
                pmcd->cMultipleItems = pmc[i].cMultipleItems;
                pmcd->dwControlID    = pmc[i].dwControlID;
                pmcd->cChannels      = 1;
                pmcd->paDetails      = (void*)xmalloc(itemSz * itemCnt);
                pmcd->cbDetails      = itemSz;
                
                mmr = mixerGetControlDetails((HMIXEROBJ)uMix, pmcd, MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_MIXER);
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("mixerGetControlDetails: %s\n", mixGetErrorText(mmr));
                        continue;
                }
                mcd_elem_add_control(pplist, pmcd);
                
                debug_msg("%d %s %s\n", i, pmc[i].szShortName, mixGetControlType(pmc[i].dwControlType));
        }
        
        xfree(pmc);
        UNUSED(pplist);
}


void
mixSaveControls(UINT uMix, mcd_elem_t **pplist)
{
        MIXERLINE ml, sml;
        MIXERCAPS mc;
        MMRESULT  mmr;
        UINT i,j;
        
        mmr = mixerGetDevCaps(uMix, &mc, sizeof(mc));
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetDevCaps: %s\n", mixGetErrorText(mmr));
                return;
        }
        
        for(i = 0; i < mc.cDestinations; i++) {
                memset(&ml, 0, sizeof(ml));
                ml.cbStruct      = sizeof(ml);
                ml.dwDestination = i;
                mmr = mixerGetLineInfo((HMIXEROBJ)uMix, &ml, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_DESTINATION); 
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("mixerGetLineInfo: %s\n", mixGetErrorText(mmr));
                        continue;
                }
                mixSaveLine(uMix, &ml, pplist);
                for (j = 0; j < ml.cConnections; j++) {
                        memset(&sml, 0, sizeof(sml));
                        sml.cbStruct = sizeof(sml);
                        sml.dwSource = j;
                        mmr = mixerGetLineInfo((HMIXEROBJ)uMix, &sml, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_SOURCE); 
                        if (mmr != MMSYSERR_NOERROR) {
                                debug_msg("mixerGetLineInfo: %s\n", mixGetErrorText(mmr));
                                continue;
                        }
                        mixSaveLine(uMix, &sml, pplist);
                }
        }
}

/* mixGetInputInfo - attempt to find corresponding wavein index
* for mixer uMix and corresponding destination line of mixer.  
* Returns TRUE if successful.
*/

int mixGetInputInfo(UINT uMix, UINT *puWavIn, UINT *puMixLineWaveOut)
{
        UINT i, nWavIn;
        MIXERLINE  ml;
        MMRESULT   mmr;
        WAVEINCAPS wic;
        MIXERCAPS  mc;
        
        mmr = mixerGetDevCaps(uMix, &mc, sizeof(mc));
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetDevCaps: %s\n", mixGetErrorText(mmr));
                return FALSE;
        }
        
        nWavIn = waveInGetNumDevs();
        for(i = 0; i < nWavIn; i++) {
                mmr = waveInGetDevCaps(i, &wic, sizeof(wic));
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("waveInGetDevCaps: %s\n", mixGetErrorText(mmr));
                        continue;
                }
                
                ml.cbStruct       = sizeof(ml);
                ml.Target.dwType  = MIXERLINE_TARGETTYPE_WAVEIN;
                strncpy(ml.Target.szPname, wic.szPname, MAXPNAMELEN);
                ml.Target.vDriverVersion = wic.vDriverVersion;
                ml.Target.wMid    = wic.wMid;
                ml.Target.wPid    = wic.wPid;
                
                mmr = mixerGetLineInfo((HMIXEROBJ)uMix, &ml, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_TARGETTYPE);
                if (mmr == MMSYSERR_NOERROR) {
                        *puWavIn          = i;
                        *puMixLineWaveOut = ml.dwDestination;
                        return TRUE;
                } else {
                        debug_msg("mixerGetLineInfo (ignore this error): %s\n", mixGetErrorText(mmr));
                }
        }
        return FALSE;
}

/* mixGetOutputInfo - attempt to find corresponding waveout index
* and corresponding destination line of mixer.  Returns TRUE if
* successful.
*/
int 
mixGetOutputInfo(UINT uMix, UINT *puWavOut, UINT *puMixLineWaveOut)
{
        UINT i, nWavOut;
        MIXERLINE  ml;
        MMRESULT   mmr;
        WAVEOUTCAPS woc;
        MIXERCAPS  mc;
        
        mmr = mixerGetDevCaps(uMix, &mc, sizeof(mc));
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetDevCaps: %s\n", mixGetErrorText(mmr));
                return FALSE;
        }
        
        nWavOut = waveOutGetNumDevs();
        for(i = 0; i < nWavOut; i++) {
                mmr = waveOutGetDevCaps(i, &woc, sizeof(woc));
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("waveOutGetDevCaps: %s\n", mixGetErrorText(mmr));
                        continue;
                }
                ml.cbStruct       = sizeof(ml);
                ml.Target.dwType  = MIXERLINE_TARGETTYPE_WAVEOUT;
                strncpy(ml.Target.szPname, woc.szPname, MAXPNAMELEN);
                ml.Target.vDriverVersion = woc.vDriverVersion;
                ml.Target.wMid    = woc.wMid;
                ml.Target.wPid    = woc.wPid;
                
                mmr = mixerGetLineInfo((HMIXEROBJ)uMix, &ml, MIXER_OBJECTF_MIXER | MIXER_GETLINEINFOF_TARGETTYPE);
                if (mmr == MMSYSERR_NOERROR) {
                        *puWavOut          = i;
                        *puMixLineWaveOut = ml.dwDestination;
                        return TRUE;
                } 
        }
        return FALSE;
}

/* mixerEnableInputLine - enables the input line whose name starts with beginning of portname.  
 * We cannot just use the port index like we do for volume because the mute controls are
 * not necessarily in the same order as the volume controls (grrr!).  The only card
 * that we have seen where this is necessary is the Winnov Videum AV, but there are
 * bound to be others.
 * Muting for input lines on the toplevel control (Rec, or whatever driver happens to call it).
 * It usually has a single control a MUX/Mixer that has "multiple items", one mute for
 * each input line.  Depending on the control type it may be legal to have multiple input
 * lines enabled, or just one.  So mixerEnableInputLine disables all lines other than
 * one selected.
 */

static int
mixerEnableInputLine(HMIXEROBJ hMix, char *portname)
{
        MIXERCONTROLDETAILS_BOOLEAN *mcdbState;
        MIXERCONTROLDETAILS_LISTTEXT *mcdlText;
        MIXERCONTROLDETAILS mcd;
        MIXERLINECONTROLS mlc;
        MIXERCONTROL mc;
        MIXERLINE ml;
        MMRESULT  mmr;
        UINT      i, matchingLine;
        
        ml.cbStruct = sizeof(ml);
        ml.dwDestination = dwMixDstRec;
        
        mmr = mixerGetLineInfo(hMix, &ml, MIXER_GETLINEINFOF_DESTINATION|MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetLineInfo: %s\n", mixGetErrorText(mmr));
        }
        
        /* Get Mixer/MUX control information (need control id to set and get control details) */
        mlc.cbStruct      = sizeof(mlc);
        mlc.dwLineID      = ml.dwLineID;
        mlc.pamxctrl      = &mc;
        mlc.cbmxctrl      = sizeof(mc);
        
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MUX; /* Single Select */
        mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE|MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MIXER; /* Multiple Select */
                mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE|MIXER_OBJECTF_HMIXER);
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("mixerGetLineControls: %s\n", mixGetErrorText(mmr));
                        return FALSE;
                }
        }
        
        mcd.cbStruct    = sizeof(mcd);
        mcd.dwControlID = mc.dwControlID;
        mcd.cChannels   = 1;
        mcd.cMultipleItems = mc.cMultipleItems;
        mcdlText = (MIXERCONTROLDETAILS_LISTTEXT*)xmalloc(sizeof(MIXERCONTROLDETAILS_LISTTEXT)*mc.cMultipleItems);        
        mcd.paDetails = mcdlText;
        mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
        mmr = mixerGetControlDetails(hMix, &mcd, MIXER_GETCONTROLDETAILSF_LISTTEXT | MIXER_OBJECTF_MIXER);
        for(i = 0; i < mcd.cMultipleItems; i++) {
                if (!strncmp(mcdlText[i].szName, portname, strlen(mcdlText[i].szName))) {
                        matchingLine = i;
                }
                debug_msg("%d - %s\n", i, mcdlText[i].szName);
        }

        /* Now get control itself */
        mcd.cbStruct    = sizeof(mcd);
        mcd.dwControlID = mc.dwControlID;
        mcd.cChannels   = 1;
        mcd.cMultipleItems = mc.cMultipleItems;
        mcdbState = (MIXERCONTROLDETAILS_BOOLEAN*)xmalloc(sizeof(MIXERCONTROLDETAILS_BOOLEAN)*mc.cMultipleItems);        
        mcd.paDetails = mcdbState;
        mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
        
        mmr = mixerGetControlDetails(hMix, &mcd, MIXER_GETCONTROLDETAILSF_VALUE|MIXER_OBJECTF_MIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerGetControlDetails: %s\n", mixGetErrorText(mmr));
                xfree(mcdbState);
                return FALSE;
        }
        
        for(i = 0; i < mcd.cMultipleItems; i++) {
                if (i == matchingLine) {
                        mcdbState[i].fValue = FALSE;
                } else {
                        mcdbState[i].fValue = TRUE;
                }
        }
        
        mmr = mixerSetControlDetails(hMix, &mcd, MIXER_OBJECTF_MIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("mixerSetControlDetails: %s\n", mixGetErrorText(mmr));
                xfree(mcdbState);
                return FALSE;
        }
        
        xfree(mcdbState);
        return TRUE;
}

static int
mixerEnableOutputLine(HMIXEROBJ hMix, DWORD dwLineID, int state)
{
        MIXERCONTROLDETAILS_BOOLEAN mcdbState;
        MIXERCONTROLDETAILS mcd;
        MIXERLINECONTROLS mlc;
        MIXERCONTROL      mc;
        MMRESULT          mmr;
        
        mlc.cbStruct      = sizeof(mlc);
        mlc.pamxctrl      = &mc;
        mlc.cbmxctrl      = sizeof(MIXERCONTROL);
        mlc.dwLineID      = dwLineID;
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
        
        mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                mlc.cbStruct      = sizeof(mlc);
                mlc.pamxctrl      = &mc;
                mlc.cbmxctrl      = sizeof(MIXERCONTROL);
                mlc.dwLineID      = dwLineID;
                mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_ONOFF;
                mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
                if (mmr != MMSYSERR_NOERROR) {
                        debug_msg("Could not get mute control for line 0x%08x: %s\n", 
                                dwLineID,
                                mixGetErrorText(mmr));
                        mixerDumpLineInfo(hMix, dwLineID);
                        return FALSE;
                }
        }
        
        mcd.cbStruct       = sizeof(mcd);
        mcd.dwControlID    = mc.dwControlID;
        mcd.cChannels      = 1;
        mcd.cMultipleItems = mc.cMultipleItems;
        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
        mcd.paDetails      = &mcdbState;
        mcdbState.fValue   = !((UINT)state);
        
        mmr = mixerSetControlDetails((HMIXEROBJ)hMix, &mcd, MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not set mute state for line 0x%08x\n", dwLineID);
                return FALSE;
        }
        return TRUE;
}

/* MixerSetLineGain - sets gain of line (range 0-MIX_MAX_GAIN) */
static int
mixerSetLineGain(HMIXEROBJ hMix, DWORD dwLineID, int gain)
{
        MIXERCONTROLDETAILS_UNSIGNED mcduGain;
        MIXERCONTROLDETAILS mcd;
        MIXERLINECONTROLS mlc;
        MIXERCONTROL      mc;
        MMRESULT          mmr;
        
        mlc.cbStruct      = sizeof(mlc);
        mlc.pamxctrl      = &mc;
        mlc.cbmxctrl      = sizeof(MIXERCONTROL);
        mlc.dwLineID      = dwLineID;
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
        
        mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not volume control for line 0x%08x: %s\n", 
                        dwLineID,
                        mixGetErrorText(mmr));
                return FALSE;        
        }
        
        mcd.cbStruct       = sizeof(mcd);
        mcd.dwControlID    = mc.dwControlID;
        mcd.cChannels      = 1;
        mcd.cMultipleItems = mc.cMultipleItems;
        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
        mcd.paDetails      = &mcduGain;
        mcduGain.dwValue   = ((mc.Bounds.dwMaximum - mc.Bounds.dwMinimum) * gain)/MIX_MAX_GAIN;
        
        mmr = mixerSetControlDetails((HMIXEROBJ)hMix, &mcd, MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not set gain for line 0x%08x: %s\n", dwLineID, mixGetErrorText(mmr));
                return FALSE;
        }
        return TRUE;
}

/* MixerGetLineGain - returns gain of line (range 0-MIX_MAX_GAIN) */
static int
mixerGetLineGain(HMIXEROBJ hMix, DWORD dwLineID)
{
        MIXERCONTROLDETAILS_UNSIGNED mcduGain;
        MIXERCONTROLDETAILS mcd;
        MIXERLINECONTROLS mlc;
        MIXERCONTROL      mc;
        MMRESULT          mmr;
        
        mlc.cbStruct      = sizeof(mlc);
        mlc.pamxctrl      = &mc;
        mlc.cbmxctrl      = sizeof(MIXERCONTROL);
        mlc.dwLineID      = dwLineID;
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
        
        mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not find volume control for line 0x%08x\n", dwLineID);
                return 0;        
        }
        
        mcd.cbStruct       = sizeof(mcd);
        mcd.dwControlID    = mc.dwControlID;
        mcd.cChannels      = 1;
        mcd.cMultipleItems = mc.cMultipleItems;
        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
        mcd.paDetails      = &mcduGain;
        
        mmr = mixerGetControlDetails((HMIXEROBJ)hMix, &mcd, MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not get gain for line 0x%08x\n", dwLineID);
                return 0;
        }
        return (int)(mcduGain.dwValue * MIX_MAX_GAIN / (mc.Bounds.dwMaximum - mc.Bounds.dwMinimum));
}

static int
mixerGetLineName(HMIXEROBJ hMix, DWORD dwLineID, char *szName, UINT uLen)
{
        MIXERLINECONTROLS   mlc;
        MIXERCONTROL        mc;
        MMRESULT            mmr;
        
        mlc.cbStruct      = sizeof(mlc);
        mlc.pamxctrl      = &mc;
        mlc.cbmxctrl      = sizeof(MIXERCONTROL);
        mlc.dwLineID      = dwLineID;
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
        
        mmr = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                debug_msg("Could not find volume control for line 0x%08x: %s\n", dwLineID, mixGetErrorText(mmr));
                return FALSE;        
        }

        debug_msg("\"%s\", \"%s\"\n", mc.szName, mc.szShortName);

        strncpy(szName, mc.szName, uLen);
        return TRUE;
}

/* MixQueryControls: Get all line names and id's, fill into ppapd, and return number of lines */
static int
mixQueryControls(HMIXEROBJ hMix, DWORD dwDst, audio_port_details_t** ppapd)
{
        MIXERLINE mlt, mlc;
        audio_port_details_t *papd;
        MMRESULT mmr;
        UINT     i;
        
        memset(&mlt, 0, sizeof(mlt));
        mlt.cbStruct = sizeof(mlt);
        mlt.dwDestination = dwDst;
        
        mmr = mixerGetLineInfo(hMix, &mlt, MIXER_GETLINEINFOF_DESTINATION|MIXER_OBJECTF_HMIXER);
        if (mmr != MMSYSERR_NOERROR) {
                return 0;
        }
        
        papd = (audio_port_details_t*)xmalloc(sizeof(audio_port_details_t)*mlt.cConnections);
        if (papd == NULL) {
                return 0;
        }
        
        mixerDumpLineInfo((HMIXEROBJ)hMix, mlt.dwLineID);
        
        for(i = 0; i < mlt.cConnections; i++) {
                memcpy(&mlc, &mlt, sizeof(mlc));
                mlc.dwSource = i;
                mmr = mixerGetLineInfo((HMIXEROBJ)hMixer, &mlc, MIXER_GETLINEINFOF_SOURCE|MIXER_OBJECTF_HMIXER);
                if (mmr != MMSYSERR_NOERROR) {
                        xfree(papd);
                        return 0;
                }
                strncpy(papd[i].name, mlc.szShortName, AUDIO_PORT_NAME_LENGTH);
                papd[i].port = mlc.dwLineID;
        }
        
        *ppapd = papd;
        return (int)mlt.cConnections;
}

static int 
mixSetup(UINT uMixer)
{
        MIXERCAPS mc;
        MMRESULT  res;
        
        if (hMixer)  {mixerClose(hMixer);  hMixer  = 0;}
        
        res = mixerOpen(&hMixer, uMixer, (unsigned long)NULL, (unsigned long)NULL, MIXER_OBJECTF_MIXER);
        if (res != MMSYSERR_NOERROR) {
                debug_msg("mixerOpen failed: %s\n", mixGetErrorText(res));
                return FALSE;
        }
        
        res = mixerGetDevCaps((UINT)hMixer, &mc, sizeof(mc));
        if (res != MMSYSERR_NOERROR) {
                debug_msg("mixerGetDevCaps failed: %s\n", mixGetErrorText(res));
                return FALSE;
        }
        
        if (mc.cDestinations < 2) {
                debug_msg("mixer does not have 2 destinations?\n");
                return FALSE;
        }
        
        if (input_ports != NULL) {
                xfree(input_ports);
                input_ports   = NULL;
                n_input_ports = 0;
        }
        
        n_input_ports = mixQueryControls((HMIXEROBJ)hMixer, dwMixDstRec, &input_ports);
        debug_msg("Input ports %d\n", n_input_ports);
        if (n_input_ports == 0) {
                return FALSE;
        }
        
        if (loop_ports != NULL) {
                xfree(loop_ports);
                loop_ports   = NULL;
                n_loop_ports = 0;
        }
        
        n_loop_ports = mixQueryControls((HMIXEROBJ)hMixer, dwMixDstVol, &loop_ports);
        debug_msg("Loop ports %d\n", n_loop_ports);
        if (n_loop_ports == 0) {
                return 0;
        }
        return TRUE;
}

static int blksz;
static int nblks;
static int smplsz;
/* AUDIO OUTPUT RELATED FN's ********************************/

static WAVEHDR *write_hdrs, *write_curr, *write_tail;
static u_char  *write_mem;
static int      write_hdrs_used;
static HWAVEOUT	shWaveOut;

static int
w32sdk_audio_open_out(UINT uId, WAVEFORMATEX *pwfx)
{
        int		i;
        WAVEHDR		*whp;
        u_char		*bp;
        
        if (shWaveOut)
                return (TRUE);
        
        error = waveOutOpen(&shWaveOut, uId, pwfx, 0, 0, CALLBACK_NULL);
        if (error) {
#ifdef DEBUG
                waveOutGetErrorText(error, errorText, sizeof(errorText));
                debug_msg("waveOutOpen: (%d) %s\n", error, errorText);
#endif
                return (FALSE);
        }
        
        if (write_mem != NULL) xfree(write_mem);
        write_mem = (u_char*)xmalloc(nblks * blksz);
        memset(write_mem, 0, nblks * blksz);
        if (write_hdrs != NULL) xfree(write_hdrs);
        write_hdrs = (WAVEHDR*)xmalloc(sizeof(WAVEHDR)*nblks);
        memset(write_hdrs, 0, sizeof(WAVEHDR)*nblks);
        for (i = 0, whp = write_hdrs, bp = write_mem; i < nblks; i++, whp++, bp += blksz) {
                whp->dwFlags        = 0;
                whp->dwBufferLength = blksz;
                whp->lpData         = (char*)bp;
                error = waveOutPrepareHeader(shWaveOut, whp, sizeof(WAVEHDR));
                if (error) {
                        waveOutGetErrorText(error, errorText, sizeof(errorText));
                        debug_msg("Win32Audio: waveOutPrepareHeader: %s\n", errorText);
                        exit(1);
                }
        }
        write_tail      = write_curr = write_hdrs;
        write_hdrs_used = 0;
        return (TRUE);
}

static void
w32sdk_audio_close_out()
{
        int	i;
        WAVEHDR		*whp;
        
        if (shWaveOut == 0)
                return;
        
        waveOutReset(shWaveOut);
        
        for (i = 0, whp = write_hdrs; i < nblks; i++, whp++)
                if (whp->dwFlags & WHDR_PREPARED)
                        waveOutUnprepareHeader(shWaveOut, whp, sizeof(WAVEHDR));
                
                (void) waveOutClose(shWaveOut);
                xfree(write_hdrs); write_hdrs = NULL;
                xfree(write_mem);  write_mem  = NULL;
                xmemchk();
                shWaveOut = 0;
}

#define WRITE_ERROR_STILL_PLAYING 33

int
w32sdk_audio_write(audio_desc_t ad, u_char *buf , int buf_bytes)
{
        int		error, len, done;
        
        UNUSED(ad);
        
        assert(shWaveOut != 0);
        
        while (write_tail->dwFlags & WHDR_DONE) {
                write_tail->dwFlags &= ~WHDR_DONE;
                write_hdrs_used--;
                write_tail++;
                if (write_tail >= write_hdrs + nblks)
                        write_tail = write_hdrs;
        }
        
        if (write_hdrs_used > 4*nblks/5) {
                debug_msg("Running out of write buffers %d left\n", write_hdrs_used);
        }
        
        done = 0;
        while (buf_bytes > 0) {
                if (write_curr->dwFlags & WHDR_DONE) {
                        /* Have overdone it! */
                        debug_msg("w32sdk_audio_write, reached end of buffer (%06d bytes remain)\n", buf_bytes);
                        return (done);
                }
                
                len = (buf_bytes > blksz) ? blksz: buf_bytes;
                
                memcpy(write_curr->lpData, buf, len);
                buf += len;
                
                error = waveOutWrite(shWaveOut, write_curr, sizeof(WAVEHDR));
                
                if (error == WRITE_ERROR_STILL_PLAYING) { /* We've filled device buffer ? */
                        debug_msg("Win32Audio - device filled. Discarding %d bytes.\n",
                                buf_bytes);
                                /* we return as if we wrote everything out
                                * to give buffer a little breathing room
                        */
                        return done;
                } else if (error) {
                        waveOutGetErrorText(error, errorText, sizeof(errorText));
                        debug_msg("Win32Audio: waveOutWrite (%d): %s\n", error,	errorText);
                        return (buf_bytes);
                }
                
                write_curr++;
                write_hdrs_used++;
                if (write_curr >= write_hdrs + nblks) {
                        write_curr = write_hdrs;
                }
                
                done      += blksz;
                buf_bytes -= blksz;
        }
        return (done);
}

/* AUDIO INPUT RELATED FN's *********************************/

static unsigned char audio_ready = 0;

int
w32sdk_audio_is_ready(audio_desc_t ad)
{
        UNUSED(ad);
        if (audio_ready>nblks/5) {
                debug_msg("Lots of audio available (%d blocks)\n", audio_ready);
        }
        
        return (audio_ready>0) ? TRUE : FALSE;
}

static void CALLBACK
waveInProc(HWAVEIN hwi,
           UINT    uMsg,
           DWORD   dwInstance,
           DWORD   dwParam1,
           DWORD   dwParam2)
{
        UNUSED(dwInstance);
        UNUSED(dwParam1);
        UNUSED(dwParam2);
        UNUSED(hwi);
        
        switch(uMsg) {
        case WIM_DATA:
                audio_ready++;
                break;
        default:
                ;  /* nothing to do currently */
        }
        
        return;
}

static WAVEHDR	*read_hdrs, *read_curr;
static u_char	*read_mem;
static HWAVEIN	shWaveIn;

static int
w32sdk_audio_open_in(UINT uId, WAVEFORMATEX *pwfx)
{
        WAVEHDR	*whp;
        int	     l;
        u_char  *bp;
        
        if (shWaveIn) return (TRUE);
        
        if (read_mem != NULL) xfree(read_mem);
        read_mem = (u_char*)xmalloc(nblks * blksz);
        
        if (read_hdrs != NULL) xfree(read_hdrs);
        read_hdrs = (WAVEHDR*)xmalloc(sizeof(WAVEHDR)*nblks); 
        
        error = waveInOpen(&shWaveIn, 
                uId, 
                pwfx,
                (unsigned long)waveInProc,
                0,
                CALLBACK_FUNCTION);
        if (error != MMSYSERR_NOERROR) {
                waveInGetErrorText(error, errorText, sizeof(errorText));
                debug_msg("waveInOpen: (%d) %s\n", error, errorText);
                return (FALSE);
        }
        
        /* Provide buffers for reading */
        audio_ready = 0;
        for (l = 0, whp = read_hdrs, bp = read_mem; l < nblks; l++, whp++, bp += blksz) {
                whp->lpData = (char*)bp;
                whp->dwBufferLength = blksz;
                whp->dwFlags = 0;
                error = waveInPrepareHeader(shWaveIn, whp, sizeof(WAVEHDR));
                if (error) {
                        waveInGetErrorText(error, errorText, sizeof(errorText));
                        debug_msg("waveInPrepareHeader: (%d) %s\n", error, errorText);
                        exit(1);
                }
                error = waveInAddBuffer(shWaveIn, whp, sizeof(WAVEHDR));
                if (error) {
                        waveInGetErrorText(error, errorText, sizeof(errorText));
                        debug_msg("waveInAddBuffer: (%d) %s\n", error, errorText);
                        exit(1);
                }
        }
        read_curr = read_hdrs;
        
        error = waveInStart(shWaveIn);
        if (error) {
                waveInGetErrorText(error, errorText, sizeof(errorText));
                debug_msg("Win32Audio: waveInStart: (%d) %s\n", error, errorText);
                exit(1);
        }
        
        return (TRUE);
}

static void
w32sdk_audio_close_in()
{
        int		i;
        WAVEHDR		*whp;
        
        if (shWaveIn == 0)
                return;
        
        waveInStop(shWaveIn);
        waveInReset(shWaveIn);
        
        for (i = 0, whp = read_hdrs; i < nblks; i++, whp++)
                if (whp->dwFlags & WHDR_PREPARED)
                        waveInUnprepareHeader(shWaveIn, whp, sizeof(WAVEHDR));
                
                waveInClose(shWaveIn);
                shWaveIn = 0;
                xfree(read_hdrs); read_hdrs = NULL;
                xfree(read_mem);  read_mem  = NULL;
                xmemchk();
}

int
w32sdk_audio_read(audio_desc_t ad, u_char *buf, int buf_bytes)
{
        static int virgin = 0;
        int len = 0;
        
        UNUSED(ad); 
        
        if (!virgin) {
                debug_msg("ready %d\n", audio_ready);
                virgin++;
        }
        
        assert(buf_bytes >= blksz);
        buf_bytes -= buf_bytes % blksz;
        
        if (audio_ready) {
                while ((read_curr->dwFlags & WHDR_DONE) && len < buf_bytes) {
                        memcpy(buf, read_curr->lpData, blksz);
                        buf += blksz;
                        len += blksz;
                        read_curr->dwFlags &= ~WHDR_DONE;
                        error = waveInAddBuffer(shWaveIn, read_curr, sizeof(WAVEHDR));
                        if (error) {
                                waveInGetErrorText(error, errorText, sizeof(errorText));
                                debug_msg("waveInAddBuffer: (%d) %s\n", error, errorText);
                                exit(1);
                        }
                        read_curr++;
                        if (read_curr == read_hdrs + nblks) read_curr = read_hdrs;
                        if (audio_ready > 0) audio_ready--;
                }
#ifdef DEBUG
                if (audio_ready > 3*nblks/4) {
                        int i,used;
                        for(i=0,used=0;i<nblks;i++) 
                                if (read_hdrs[i].dwFlags & WHDR_DONE) used++;
                                debug_msg("RB small %d of %d len %d, ready %d\n", used, nblks, len, audio_ready);
                }
#endif
        }
        
        return (len);
}

static int audio_dev_open = 0;

int
w32sdk_audio_open(audio_desc_t ad, audio_format *fmt, audio_format *ofmt)
{
        static int virgin;
        WAVEFORMATEX owfx, wfx;
        UINT uWavIn, uWavOut;
        
        if (audio_dev_open) {
                debug_msg("Device not closed! Fix immediately");
                w32sdk_audio_close(ad);
        }
        
        assert(audio_format_match(fmt, ofmt));
        if (fmt->encoding != DEV_S16) {
                return FALSE; /* Only support L16 for time being */
        }
        
        if (mixGetInputInfo(ad, &uWavIn, &dwMixDstRec) != TRUE) {
                debug_msg("Could not get wave in or mixer destination for mix %u\n", ad);
                return FALSE;
        }

        if (mixGetOutputInfo(ad, &uWavOut, &dwMixDstVol) != TRUE) {
                debug_msg("Could not get wave out or mixer destination for mix %u\n", ad);
                return FALSE;
        }
        
        if (mixSetup(ad) == FALSE) {
                return FALSE; /* Could not secure mixer */
        }

        mixSaveControls(ad, &control_list);

        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = (WORD)fmt->channels;
        wfx.nSamplesPerSec  = fmt->sample_rate;
        wfx.wBitsPerSample  = (WORD)fmt->bits_per_sample;
        smplsz              = wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nChannels * wfx.nSamplesPerSec * smplsz;
        wfx.nBlockAlign     = (WORD)(wfx.nChannels * smplsz);
        wfx.cbSize          = 0;
        
        memcpy(&owfx, &wfx, sizeof(wfx));
        
        /* Use 1 sec device buffer */	
        blksz  = fmt->bytes_per_block;
        nblks  = wfx.nAvgBytesPerSec / blksz;
        
        if (w32sdk_audio_open_in(uWavIn, &wfx) == FALSE){
                debug_msg("Open input failed\n");
                return FALSE;
        }
        
        assert(memcmp(&owfx, &wfx, sizeof(WAVEFORMATEX)) == 0);
        
        if (w32sdk_audio_open_out(uWavOut, &wfx) == FALSE) {
                debug_msg("Open output failed\n");
                w32sdk_audio_close_in();
                return FALSE;
        }
        
        if (!have_probed[ad]) {
                have_probed[ad] = w32sdk_probe_formats(ad);
        }
        
        /* because i've seen these get corrupted... */
        assert(memcmp(&owfx, &wfx, sizeof(WAVEFORMATEX)) == 0);
        
        switch(thread_pri) {
        case 1:
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                debug_msg("Above Normal Priority\n");
                break;
        case 2:
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
                debug_msg("Time Critical Priority\n");
                break;
        case 3:
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
                debug_msg("Highest Thread Priority\n"); /* Kiss all processes bye-bye ;-) */
                break;
        default:
                break;
        }
        
        audio_dev_open = TRUE;
        return TRUE;
}

void
w32sdk_audio_close(audio_desc_t ad)
{
        UNUSED(ad);
        
        debug_msg("Closing input device.\n");
        w32sdk_audio_close_in();
        
        debug_msg("Closing output device.\n");
        w32sdk_audio_close_out();
        
        if (input_ports != NULL) {
                xfree(input_ports);
                input_ports = NULL;
        }
        
        if (loop_ports != NULL) {
                xfree(loop_ports);
                loop_ports = NULL;
        }
        
        mixRestoreControls(ad, &control_list);

        audio_dev_open = FALSE;
}

int
w32sdk_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad);
        return (TRUE);
}

void
w32sdk_audio_drain(audio_desc_t ad)
{
        while(read_curr->dwFlags & WHDR_DONE) {
                read_curr->dwFlags &= ~WHDR_DONE;
                error = waveInAddBuffer(shWaveIn, read_curr, sizeof(WAVEHDR));
                if (error) {
                        waveInGetErrorText(error, errorText, sizeof(errorText));
                        debug_msg("waveInAddBuffer: (%d) %s\n", error, errorText);
                        exit(1);
                }
                read_curr++;
                if (read_curr == read_hdrs + nblks) read_curr = read_hdrs;
                if (audio_ready > 0) audio_ready--;     
        }
        assert(audio_ready == 0);
}

void
w32sdk_audio_non_block(audio_desc_t ad)
{
        UNUSED(ad);
        debug_msg("Windows audio interface is asynchronous!\n");
}

void
w32sdk_audio_block(audio_desc_t ad)
{
        UNUSED(ad);
        debug_msg("Windows audio interface is asynchronous!\n");
}

void
w32sdk_audio_set_ogain(audio_desc_t ad, int level)
{
        DWORD	vol;
        
        UNUSED(ad);
        
        play_vol = level;
        
        if (shWaveOut == 0)
                return;
        
        level = rat_to_device(level);
        if (level >= 255)
                level = (short)-1;
        else
                level <<= 8;
        vol = level | (level << 16);
        
        error = waveOutSetVolume(shWaveOut, vol);
        if (error) {
                waveOutGetErrorText(error, errorText, sizeof(errorText));
                debug_msg("Win32Audio: waveOutSetVolume: %s\n", errorText);
        }
}

int
w32sdk_audio_get_ogain(audio_desc_t ad)
{
        DWORD	vol;
        
        UNUSED(ad);
        
        if (shWaveOut == 0)
                return (play_vol);
        
        error = waveOutGetVolume(shWaveOut, &vol);
        if (error) {
                waveOutGetErrorText(error, errorText, sizeof(errorText));
                debug_msg("Win32Audio: waveOutGetVolume Error: %s\n", errorText);
                return (0);
        } else
                return (device_to_rat(vol & 0xff));
}

void
w32sdk_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        
        nLoopGain = gain;
}

#define WIN32_SPEAKER 0x101010
static audio_port_details_t outports[] = {
        { WIN32_SPEAKER, AUDIO_PORT_SPEAKER}
};
#define NUM_OPORTS (sizeof(outports)/sizeof(outports[0]))

void
w32sdk_audio_oport_set(audio_desc_t ad, audio_port_t port)
{
        UNUSED(ad);
        UNUSED(port);
}

/* Return selected output port */
audio_port_t 
w32sdk_audio_oport_get(audio_desc_t ad)
{
        UNUSED(ad);
        return (WIN32_SPEAKER);
}

int
w32sdk_audio_oport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return (int)NUM_OPORTS;
}

const audio_port_details_t*
w32sdk_audio_oport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        assert(idx >= 0 && idx < NUM_OPORTS);
        return &outports[idx];
}

void 
w32sdk_audio_iport_set(audio_desc_t ad, audio_port_t port)
{
        char portname[MIXER_LONG_NAME_CHARS+1];
        int i, j, gain;
        UNUSED(ad);
        
        for(i = 0; i < n_input_ports; i++) {
                if (input_ports[i].port == port) {
                        /* save gain */
                        gain = mixerGetLineGain((HMIXEROBJ)hMixer, input_ports[iport].port);
                        if (mixerGetLineName((HMIXEROBJ)hMixer, input_ports[iport].port, portname, MIXER_LONG_NAME_CHARS)) {
                                mixerEnableInputLine((HMIXEROBJ)hMixer, portname);
                        }
                        mixerSetLineGain((HMIXEROBJ)hMixer, input_ports[i].port, gain);
                        
                        /* Do loopback */
                        for(j = 0; j < n_loop_ports; j++) {
                                if (strcmp(loop_ports[j].name, input_ports[i].name) == 0) {
                                        mixerEnableOutputLine((HMIXEROBJ)hMixer, loop_ports[j].port, 1);
                                        mixerSetLineGain((HMIXEROBJ)hMixer, loop_ports[j].port, nLoopGain); 
                                }
                        }
                        iport = i;
                        return;
                }
        }
        debug_msg("Port %d not found\n", port);
}

/* Return selected input port */
audio_port_t
w32sdk_audio_iport_get(audio_desc_t ad)
{
        UNUSED(ad);
        return input_ports[iport].port;
}

int
w32sdk_audio_iport_count(audio_desc_t ad)
{
        UNUSED(ad);
        return n_input_ports;
}

const audio_port_details_t*
w32sdk_audio_iport_details(audio_desc_t ad, int idx)
{
        UNUSED(ad);
        assert(idx >= 0 && idx < n_input_ports);
        return &input_ports[idx];
}

void
w32sdk_audio_set_igain(audio_desc_t ad, int level)
{
        UNUSED(ad);
        assert(iport >= 0 && iport < n_input_ports);
        mixerSetLineGain((HMIXEROBJ)hMixer, input_ports[iport].port, level);
}

int
w32sdk_audio_get_igain(audio_desc_t ad)
{
        UNUSED(ad);
        assert(iport >= 0 && iport < n_input_ports);
        return mixerGetLineGain((HMIXEROBJ)hMixer, input_ports[iport].port);
}


void
w32sdk_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        DWORD   dwPeriod;
        int cnt = 4;
        
        dwPeriod = (DWORD)delay_ms/2;
        /* The blocks we are passing to the audio interface are of duration dwPeriod.
        * dwPeriod is usually around 20ms (8kHz), but mmtask often doesn't give
        * us audio that often, more like every 40ms.  In order to make UI more responsive we
        * block for half specified delay as the process of blocking seems to incur noticeable
        * delay.  If anyone has more time this is worth looking into.
        */
        
        while (!w32sdk_audio_is_ready(ad) && cnt--) {
                Sleep(dwPeriod);
        } 
}

/* Probing support */

static audio_format af_sup[W32SDK_MAX_DEVICES][10];
static int          n_af_sup[W32SDK_MAX_DEVICES];

int
w32sdk_probe_format(int rate, int channels)
{
        WAVEFORMATEX wfx;
        
        wfx.cbSize = 0; /* PCM format */
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample  = 16; /* 16 bit linear */
        wfx.nChannels       = channels;
        wfx.nSamplesPerSec  = rate;
        wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        
        if (waveInOpen(NULL, (UINT)shWaveIn, &wfx, (UINT)NULL, (UINT)NULL, WAVE_FORMAT_QUERY)) {
                debug_msg("%d %d supported\n", rate, channels);
                return TRUE;
        }
        
        debug_msg("%d %d not supported\n", rate, channels);
        return FALSE;      
}

int 
w32sdk_probe_formats(audio_desc_t ad) 
{
        int rate, channels;

        for (rate = 8000; rate <= 48000; rate+=8000) {
                if (rate == 24000 || rate == 40000) continue;
                for(channels = 1; channels <= 2; channels++) {
                        if (w32sdk_probe_format(rate, channels)) {
                                af_sup[ad][n_af_sup[ad]].sample_rate = rate;
                                af_sup[ad][n_af_sup[ad]].channels    = channels;
                                n_af_sup[ad]++;
                        }
                }
        }
        return (n_af_sup[ad] ? TRUE : FALSE); /* Managed to find at least 1 we support */
                                              /* We have this test since if we cannot get device now (because in use elsewhere)
        * we will want to test it later */
}

int
w32sdk_audio_supports(audio_desc_t ad, audio_format *paf)
{
        int i;
        for(i = 0; i < n_af_sup[ad]; i++) {
                if (af_sup[ad][i].sample_rate  == paf->sample_rate &&
                        af_sup[ad][i].channels == paf->channels) {
                        return TRUE;
                }
        }
        return FALSE;
}

int
w32sdk_get_device_count()
{
        /* We are only interested in devices with mixers */
        return (int)mixerGetNumDevs();
}

static char tmpname[MAXPNAMELEN];

char *
w32sdk_get_device_name(int idx)
{
        MIXERCAPS mc;
        
        if ((UINT)idx < mixerGetNumDevs()) {
                mixerGetDevCaps((UINT)idx, &mc, sizeof(mc));
                strcpy(tmpname, mc.szPname);
                return tmpname;
        }
        return NULL;
}
#endif /* WIN32 */
