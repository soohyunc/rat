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
 * Copyright (c) 1995-98 University College London
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
 *      This product includes software developed by the Computer Science
 *      Department at University College London
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * Use of this software for commercial purposes is explicitly forbidden
 * unless prior written permission is obtained from the authors.
 * 
 *
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Systems
 *      Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
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
#include "assert.h"
#include "audio.h"
#include "debug.h"
#include "memory.h"
#include "auddev_win32.h"
#include "audio_types.h"
#include "audio_fmt.h"

#define rat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_rat(x)	((x) * MAX_AMP / 255)

static int		error = 0;
static char		errorText[MAXERRORLENGTH];
extern int thread_pri;
static WAVEFORMATEX	format;
static int		duplex;
static int              nLoopGain = 100;
#define MAX_DEV_NAME 64

/* Mixer Code (C) 1998 Orion Hodson.
 *
 * no thanks to the person who wrote the microsoft documentation 
 * (circular and information starved) for the mixer, or the folks 
 * who conceived the api in the first place.
 *
 */

#define IsMixSrc(x) ((x)>=MIXERLINE_COMPONENTTYPE_SRC_FIRST && \
                     (x)<=MIXERLINE_COMPONENTTYPE_SRC_LAST)

#define IsMixDst(x) ((x)>=MIXERLINE_COMPONENTTYPE_DST_FIRST && \
                     (x)<=MIXERLINE_COMPONENTTYPE_DST_LAST)

#define MAX_MIX_CTLS 8

typedef struct {
        char      szName[20];
        DWORD     dwCtlID[MAX_MIX_CTLS];
        DWORD     dwCtlType[MAX_MIX_CTLS];
        DWORD     dwCtl[MAX_MIX_CTLS];
        DWORD     dwMultipleItems[MAX_MIX_CTLS];
        DWORD     dwLowerBound[MAX_MIX_CTLS];
        DWORD     dwUpperBound[MAX_MIX_CTLS];
        int       nCtls;
} MixCtls;

typedef struct {
        char szName[12];
        int  nID;
} MapEntry;

static MixCtls  mcMixIn[MAX_MIX_CTLS], mcMixOut[MAX_MIX_CTLS];
static u_int32  nMixIn, nMixOut, nMixInID, curMixIn;
static int32	play_vol, rec_vol;

static HMIXER hMixIn, hMixOut;
static UINT   uWavIn, uWavOut;

static MapEntry meInputs[] = {
        {"mic",  AUDIO_MICROPHONE},
        {"line", AUDIO_LINE_IN},
        {"cd",   AUDIO_CD},
        {"analog cd", AUDIO_CD},
        {"",0}
};

static const char *
audioIDToName(int id, MapEntry *meMap)
{
        int i = 0;
        while(meMap[i].szName) {
                if (meMap[i].nID == id) return meMap[i].szName;
                i++;
        }
        return (const char *)NULL;
}

static int 
nameToAudioID(char *name, MapEntry *meMap)
{
        int i = 0;
        while(meMap[i].szName[0]) {
                if (strncasecmp(name, meMap[i].szName,3) == 0) return meMap[i].nID;
                i++;
        }
        return -1;
}

static MixCtls *
nameToMixCtls(char *szName, MixCtls *mcMix, int nMix)
{
        int i;
        for(i = 0;i < nMix; i++) {
                if (strncasecmp(szName, mcMix[i].szName,3) == 0) return (mcMix+i);
        }
        return (MixCtls*)NULL;
}


static MixCtls *
audioIDToMixCtls(int id, MapEntry *meMap, MixCtls *mcMix, int nMix)
{
        int i,j;
        const char *szName;
        
        if ((const char*)NULL == (szName = audioIDToName(id, meMap))) return (MixCtls*)NULL;
        for(j = 0; j < nMix; j++) {
                i = 0;
                while(meMap[i].szName[0] != 0) {
                        if (meMap[i].nID == id && 
                                strncasecmp(meMap[i].szName,mcMix[j].szName, strlen(meMap[i].szName))==0) 
                                return (mcMix + j); 
                        i++;
                }
        }
        return (MixCtls *)NULL;
}

static void
mixGetErrorText(MMRESULT mmr, char *szMsg, int cbMsg)
{
        assert(cbMsg > 15);
        switch (mmr) {
                case MMSYSERR_NOERROR:     sprintf(szMsg, "no error\n");        break; 
                case MIXERR_INVALLINE:     sprintf(szMsg, "invalid line\n");    break;
                case MIXERR_INVALCONTROL:  sprintf(szMsg, "invalid control\n"); break;
                case MIXERR_INVALVALUE:    sprintf(szMsg, "invalid value\n");   break;
                case MMSYSERR_BADDEVICEID: sprintf(szMsg, "bad device id\n");   break;
                case MMSYSERR_INVALFLAG:   sprintf(szMsg, "invalid flag\n");    break;
                case MMSYSERR_INVALHANDLE: sprintf(szMsg, "invalid handle\n");  break;
                case MMSYSERR_INVALPARAM:  sprintf(szMsg, "invalid param\n");   break;
                case MMSYSERR_NODRIVER:    sprintf(szMsg, "no driver!\n");      break;               
        }
}

static int
mixSetLoopback(char *szName)
{
        unsigned int i, loopCtl = 0;
        MIXERCONTROLDETAILS          mcd;
        MIXERCONTROLDETAILS_BOOLEAN  mcdbOnOff;
        MIXERCONTROLDETAILS_UNSIGNED mcduLoopGain;
        MMRESULT r;

        debug_msg("Name %s\n", szName);

        for(i = 0; i < nMixOut; i++) {
                if (!strcasecmp(szName, mcMixOut[i].szName)) {
                        loopCtl = i;
                        break;
                }
        }
        
        if (i == nMixOut) return 0;

        /* We only set loopback for the named input.
         */
        for ( i = 0; i < (unsigned)mcMixOut[loopCtl].nCtls; i++) {
                r = 0;
                mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
                mcd.dwControlID    = mcMixOut[loopCtl].dwCtlID[i];
                mcd.cChannels      = 1;
                mcd.cMultipleItems = 0;
                switch(mcMixOut[loopCtl].dwCtlType[i]) {
                case MIXERCONTROL_CONTROLTYPE_ONOFF:
                case MIXERCONTROL_CONTROLTYPE_MUTE:
                        mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
                        mcd.paDetails = &mcdbOnOff;
                        mcdbOnOff.fValue = nLoopGain ? FALSE : TRUE;
                        r = mixerSetControlDetails((HMIXEROBJ)hMixOut, &mcd, MIXER_OBJECTF_HMIXER);
                        break;
                case MIXERCONTROL_CONTROLTYPE_VOLUME:
                        /* Don't touch volume if just muting */
                        if (nLoopGain) {
                                mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                                mcd.paDetails = &mcduLoopGain;
                                mcduLoopGain.dwValue = ((mcMixOut[loopCtl].dwUpperBound[i] - mcMixOut[loopCtl].dwLowerBound[i])/100) * nLoopGain + mcMixOut[loopCtl].dwLowerBound[i];
                                r = mixerSetControlDetails((HMIXEROBJ)hMixOut, &mcd, MIXER_OBJECTF_HMIXER);                
                        }
                        break;
                default:
#ifdef DEBUG
                        debug_msg("Control type not recognised %x\n", mcMixOut[loopCtl].dwCtlType[i]);
#endif /* DEBUG */
                        continue;
                }
                
                if (r != MMSYSERR_NOERROR) {
                       char szError[30];
                       mixGetErrorText(r, szError, 30);
                       debug_msg(szError);
                }
        }

        return 1;
}

static int
mixSetIPort(MixCtls *mcMix)
{
        MIXERCONTROLDETAILS_LISTTEXT *ltInputs;
        MIXERCONTROLDETAILS_BOOLEAN  *bInputs;
        MIXERCONTROLDETAILS mcd;
        int i,j,r;
        
        assert(mcMix != NULL);
        
        curMixIn = (mcMix - mcMixIn);

        /* now make sure line is selected */
        mixSetLoopback(mcMix->szName);        
        
        for(i = 0; i < mcMixIn->nCtls; i++) {
                switch(mcMixIn->dwCtlType[i]) {
                case MIXERCONTROL_CONTROLTYPE_MUX:   /* a list with single select   */
                case MIXERCONTROL_CONTROLTYPE_MIXER: /* a list with multiple select */
                        mcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
                        mcd.dwControlID    = mcMixIn->dwCtlID[i];
                        mcd.cChannels      = 1;
                        mcd.cMultipleItems = mcMixIn->dwMultipleItems[i];
                        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_LISTTEXT);
                        ltInputs           = (MIXERCONTROLDETAILS_LISTTEXT*)xmalloc(mcMixIn->dwMultipleItems[i] * sizeof(MIXERCONTROLDETAILS_LISTTEXT));
                        mcd.paDetails      = (LPVOID)ltInputs;
                        r = mixerGetControlDetails((HMIXEROBJ)hMixIn, &mcd,MIXER_GETCONTROLDETAILSF_LISTTEXT); 
                        if (r != MMSYSERR_NOERROR) goto done_multi1;
                        bInputs = (MIXERCONTROLDETAILS_BOOLEAN*)xmalloc(mcMixIn->dwMultipleItems[i] * sizeof(MIXERCONTROLDETAILS_BOOLEAN));
                        mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
                        mcd.paDetails = (LPVOID)bInputs;
                        mcd.dwControlID    = mcMixIn->dwCtlID[i];
                        mcd.cChannels      = 1;
                        mcd.cMultipleItems = mcMixIn->dwMultipleItems[i];
                        /* now ltInputs contains names of inputs, 
                         * bInputs contains selection status. 
                         */ 
                        for (j = 0; j < (signed)mcMixIn->dwMultipleItems[i]; j++) {
                                if (strncasecmp(ltInputs[j].szName, mcMix->szName, strlen(mcMix->szName)) == 0) {
                                        bInputs[j].fValue = 1;
                                } else {
                                        /* force single select even when multiple available */
                                        bInputs[j].fValue = 0;
                                }
                        }
                        r = mixerSetControlDetails((HMIXEROBJ)hMixIn, &mcd,MIXER_SETCONTROLDETAILSF_VALUE);
                        xfree(bInputs);
done_multi1:            xfree(ltInputs);

                        return 0;            
                        break;
                default:
#ifdef DEBUG
                        debug_msg("Control type %8x\n", mcMixIn->dwCtlType[i]);
#endif /* DEBUG */
                        return 0;
                }
        }

        return 0;
}

static void
mixGetControls(HMIXER hMix, char *szDstName, int nDst, MixCtls *mcMix, int *nMix)
{
        MIXERLINE         ml;
        MIXERCONTROL     *mc;
        MIXERLINECONTROLS mlc;
        MMRESULT res;

        int i, src, ctl, offset;
        
        offset = 0;
        for(i = 0; i < nDst; i++) { 
                ml.dwDestination = i;
                ml.cbStruct = sizeof(MIXERLINE);
                res = mixerGetLineInfo((HMIXEROBJ)hMix, &ml, MIXER_GETLINEINFOF_DESTINATION);
                if (res != MMSYSERR_NOERROR || strncmp(ml.szShortName, szDstName, strlen(szDstName)) != 0) continue;
                
                if (ml.cControls) {
                        /* Get controls of mixer itself - for input this is 
                         * usually multiplexer + master vol, and for output 
                         * this is usually master mute and vol.
                         */
                        offset = 1;
                        strncpy(mcMix[0].szName, ml.szShortName,20);
                        mc = (MIXERCONTROL*)xmalloc(sizeof(MIXERCONTROL) * ml.cControls);
                        mlc.cbStruct   = sizeof(MIXERLINECONTROLS);
                        mlc.dwLineID   = ml.dwLineID;
                        mlc.cControls  = ml.cControls;
                        mlc.cbmxctrl   = sizeof(MIXERCONTROL);
                        mlc.pamxctrl   = mc;
                        res = mixerGetLineControls((HMIXEROBJ)hMix, &mlc, MIXER_GETLINECONTROLSF_ALL);
                        if (res == MMSYSERR_NOERROR) {
                                src = 0;
                                for(ctl = 0; ctl < (signed)ml.cControls && ctl < MAX_MIX_CTLS; ctl++) {
                                        mcMix[src].dwCtlID[ctl]         = mc[ctl].dwControlID;
                                        mcMix[src].dwCtlType[ctl]       = mc[ctl].dwControlType;
                                        mcMix[src].dwCtl[ctl]           = mc[ctl].fdwControl;
                                        mcMix[src].dwMultipleItems[ctl] = mc[ctl].cMultipleItems;
                                        mcMix[src].dwLowerBound[ctl]    = mc[ctl].Bounds.dwMinimum;
                                        mcMix[src].dwUpperBound[ctl]    = mc[ctl].Bounds.dwMaximum;
                                }
                                mcMix[src].nCtls = ctl;
                        }
                        xfree(mc);
                }
                (*nMix) = min(ml.cConnections+offset,MAX_MIX_CTLS);
                
                for (src = (*nMix)-1; src>=0 ; src--) {
                        ml.dwSource  = src - offset;
                        res = mixerGetLineInfo((HMIXEROBJ)hMix, &ml, MIXER_GETLINEINFOF_SOURCE);
                        if (res != MMSYSERR_NOERROR) continue;
                        strncpy(mcMix[src].szName, ml.szShortName,20);
                        mc = (MIXERCONTROL*)xmalloc(sizeof(MIXERCONTROL) * ml.cControls);
                        mlc.cbStruct   = sizeof(MIXERLINECONTROLS);
                        mlc.dwLineID   = ml.dwLineID;
                        mlc.cControls  = ml.cControls;
                        mlc.cbmxctrl   = sizeof(MIXERCONTROL);
                        mlc.pamxctrl   = mc;
                        res = mixerGetLineControls((HMIXEROBJ)hMix, &mlc, MIXER_GETLINECONTROLSF_ALL);
                        if (res != MMSYSERR_NOERROR) continue;
                        for(ctl = 0; ctl < (signed)ml.cControls && ctl < MAX_MIX_CTLS; ctl++) {
                                mcMix[src].dwCtlID[ctl]         = mc[ctl].dwControlID;
                                mcMix[src].dwCtlType[ctl]       = mc[ctl].dwControlType;
                                mcMix[src].dwCtl[ctl]           = mc[ctl].fdwControl;
                                mcMix[src].dwMultipleItems[ctl] = mc[ctl].cMultipleItems;
                                mcMix[src].dwLowerBound[ctl]    = mc[ctl].Bounds.dwMinimum;
                                mcMix[src].dwUpperBound[ctl]    = mc[ctl].Bounds.dwMaximum;
                        }
                        mcMix[src].nCtls = ctl;
                        xfree(mc);
                }
        }
}

static void 
mixSetup()
{
        MIXERCAPS m;
        MMRESULT  res;
        HMIXER    hMix;
	
        int i,j, nDevs, nDstIn = 0, nDstOut = 0;
        char mixName[32];

        if (hMixIn)  {mixerClose(hMixIn);  hMixIn  = 0;}
        if (hMixOut) {mixerClose(hMixOut); hMixOut = 0;}

        nDevs = mixerGetNumDevs();
        for(i = 0; i < nDevs; i++) {
                char doClose = TRUE;
                /* Strictly we don't need to open mixer here */
                mixerOpen(&hMix, i, (unsigned long)NULL, (unsigned long)NULL, MIXER_OBJECTF_MIXER);
                res = mixerGetDevCaps(i,  &m, sizeof(m));
                
                j = 0;
                while(j < 32 && mixName[j]) {
                        mixName[j] = (char)tolower(m.szPname[j]);
                        j++;
                }
                
                if (res == MMSYSERR_NOERROR){
                        if ((unsigned)i == uWavIn) {
                                hMixIn  = hMix;
                                nDstIn  = m.cDestinations;
                                doClose = FALSE;
                                debug_msg("Input mixer %s destinations %d\n", m.szPname, nDstIn); 
                        }
                        if ((unsigned)i == uWavOut) {
                                hMixOut = hMix;
                                nDstOut = m.cDestinations;
                                doClose = FALSE;
                                debug_msg("Output mixer %s destinations %d\n", m.szPname, nDstOut); 
                        }
                }
                if (doClose) mixerClose(hMix);
        }
        /* There are fields within MIXERLINE struct that should say
         * if line is input or output.  Does not work with SB driver
         * so we give a string to match to "Rec" or "Vol", great :-( */
        
        mixGetControls(hMixIn, "Rec", nDstIn,  mcMixIn,  (int*)&nMixIn);
        mixGetControls(hMixIn, "Master", nDstOut,  mcMixOut, (int*)&nMixOut);
        if (nMixOut == 0) {
                mixGetControls(hMixIn, "Vol", nDstOut,  mcMixOut, (int*)&nMixOut);
        }
        mixSetIPort(nameToMixCtls("mic", mcMixIn, nMixIn));
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
w32sdk_audio_open_out()
{
	int		i;
	WAVEHDR		*whp;
	u_char		*bp;

	if (shWaveOut)
		return (TRUE);

	error = waveOutOpen(&shWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
	if (error) {
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		debug_msg("waveOutOpen: (%d) %s\n", error, errorText);
#endif
		return (FALSE);
	}

	if (write_mem != NULL) xfree(write_mem);
	write_mem = (u_char*)xmalloc(nblks * blksz);
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
                
                done       += blksz;
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
w32sdk_audio_open_in()
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
                           WAVE_MAPPER, 
                           &format,
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

        while (write_tail->dwFlags & WHDR_DONE) {
                write_tail->dwFlags &= ~WHDR_DONE;
                write_tail++;
                write_hdrs_used--;
                if (write_tail >= write_hdrs + nblks)
                        write_tail = write_hdrs;
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
        WAVEFORMATEX tfmt;
	
        if (audio_dev_open) {
                debug_msg("Device not closed! Fix immediately");
                w32sdk_audio_close(ad);
        }

        assert(audio_format_match(fmt, ofmt));
        if (fmt->encoding != DEV_S16) return FALSE; /* Only support L16 for time being */

        uWavIn = uWavOut = (UINT)ad;
        mixSetup();

        format.wFormatTag      = WAVE_FORMAT_PCM;
	format.nChannels       = (WORD)fmt->channels;
	format.nSamplesPerSec  = fmt->sample_rate;
	format.wBitsPerSample  = (WORD)fmt->bits_per_sample;
        smplsz                 = format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nChannels * format.nSamplesPerSec * smplsz;
	format.nBlockAlign     = (WORD)(format.nChannels * smplsz);
	format.cbSize          = 0;
        
        memcpy(&tfmt, &format, sizeof(format));
        /* Use 1 sec device buffer */
	
        blksz  = fmt->bytes_per_block;
	nblks  = format.nAvgBytesPerSec / blksz;
	
        if (w32sdk_audio_open_in() == FALSE)   return FALSE;
        
        if ((duplex = w32sdk_audio_open_out()) == FALSE) {
                w32sdk_audio_close_in();
                return FALSE;
        }
        /* because i've seen these get corrupted... */
        assert(tfmt.wFormatTag      == format.wFormatTag);
        assert(tfmt.nChannels       == format.nChannels);
        assert(tfmt.nSamplesPerSec  == format.nSamplesPerSec);
        assert(tfmt.wBitsPerSample  == format.wBitsPerSample);
        assert(tfmt.nAvgBytesPerSec == format.nAvgBytesPerSec);
        assert(tfmt.nBlockAlign     == format.nBlockAlign);
	
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
        audio_dev_open = FALSE;
}

int
w32sdk_audio_duplex(audio_desc_t ad)
{
        UNUSED(ad);
	return (duplex);
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
w32sdk_audio_set_gain(audio_desc_t ad, int level)
{
        int i;
        MIXERCONTROLDETAILS          mcd;
        MIXERCONTROLDETAILS_UNSIGNED mcduDevLevel;
        MMRESULT r;
        UNUSED(ad);
        
        for(i = 0; i < mcMixIn[curMixIn].nCtls; i++) {
                switch (mcMixIn[curMixIn].dwCtlType[i]) {
                case MIXERCONTROL_CONTROLTYPE_VOLUME:
                        mcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
                        mcd.dwControlID    = mcMixIn[curMixIn].dwCtlID[i];
                        mcd.cChannels      = 1;
                        mcd.cMultipleItems = 0;
                        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                        mcd.paDetails      = &mcduDevLevel;
                        mcduDevLevel.dwValue = ((mcMixIn[curMixIn].dwUpperBound[i] - mcMixIn[curMixIn].dwLowerBound[i])/100) * level + mcMixIn[curMixIn].dwLowerBound[i];
                        play_vol   = level;
                        r = mixerSetControlDetails((HMIXEROBJ)hMixIn, &mcd, MIXER_OBJECTF_HMIXER);
                        if (r != MMSYSERR_NOERROR) {
                                char szError[30];
                                mixGetErrorText(r, szError, 30);
                                debug_msg(szError);
                        }
                        
                        break; 
                }
        }
}

int
w32sdk_audio_get_gain(audio_desc_t ad)
{
        UNUSED(ad);
	return (rec_vol);
}

void
w32sdk_audio_set_volume(audio_desc_t ad, int level)
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
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		debug_msg("Win32Audio: waveOutSetVolume: %s\n", errorText);
#endif
	}
}

int
w32sdk_audio_get_volume(audio_desc_t ad)
{
	DWORD	vol;
        
        UNUSED(ad);

	if (shWaveOut == 0)
		return (play_vol);

	error = waveOutGetVolume(shWaveOut, &vol);
	if (error) {
#ifdef DEBUG
		waveOutGetErrorText(error, errorText, sizeof(errorText));
		debug_msg("Win32Audio: waveOutGetVolume Error: %s\n", errorText);
#endif
		return (0);
	} else
		return (device_to_rat(vol & 0xff));
}

void
w32sdk_audio_loopback(audio_desc_t ad, int gain)
{
        UNUSED(ad);
        
        nLoopGain = gain;
        mixSetLoopback(mcMixIn[curMixIn].szName);
}

void
w32sdk_audio_set_oport(audio_desc_t ad, int port)
{
	UNUSED(ad);
	UNUSED(port);
}

/* Return selected output port */
int w32sdk_audio_get_oport(audio_desc_t ad)
{
        UNUSED(ad);
	return (AUDIO_SPEAKER);
}

/* Select next available output port */
int
w32sdk_audio_next_oport(audio_desc_t ad)
{
        UNUSED(ad);
        return (AUDIO_SPEAKER);
}

void 
w32sdk_audio_set_iport(audio_desc_t ad, int port)
{
        MixCtls *mcMix;
        
        UNUSED(ad);

        mcMix = audioIDToMixCtls(port, meInputs, mcMixIn, nMixIn);
        
        if (mcMix) mixSetIPort(mcMix);
}

/* Return selected input port */
int
w32sdk_audio_get_iport(audio_desc_t ad)
{
        int id = nameToAudioID(mcMixIn[curMixIn].szName, meInputs);
        UNUSED(ad);
	return (id);
}

/* Select next available input port */
int
w32sdk_audio_next_iport(audio_desc_t ad)
{
        u_int32 trialMixIn;
        int id = -1;

        UNUSED(ad);

        trialMixIn = curMixIn;
        do {
                trialMixIn = (trialMixIn + 1) % nMixIn;
                id = nameToAudioID(mcMixIn[trialMixIn].szName, meInputs);
        } while(id == -1);
        mixSetIPort(audioIDToMixCtls(id, meInputs, mcMixIn, nMixIn));
        return (id);
}

void
w32sdk_audio_wait_for(audio_desc_t ad, int delay_ms)
{
        DWORD   dwPeriod;
        
        dwPeriod = (DWORD)delay_ms/2;
        /* The blocks we are passing to the audio interface are of duration dwPeriod.
         * dwPeriod is usually around 20ms (8kHz), but mmtask often doesn't give
         * us audio that often, more like every 40ms.  In order to make UI more responsive we
         * block for half specified delay as the process of blocking seems to incur noticeable
         * delay.  If anyone has more time this is worth looking into.
         */

        if (!w32sdk_audio_is_ready(ad)) {
                Sleep(dwPeriod);
        }
}

#define W32SDK_MAX_NAME_LEN 32
#define W32SDK_MAX_DEVS      3

static char szDevNames[W32SDK_MAX_DEVS][W32SDK_MAX_NAME_LEN];
static int  nDevs;

void 
w32sdk_audio_query_devices(void)
{
        WAVEINCAPS wic;
        int nWaveInDevs, nWaveOutDevs;
        int i;

        nWaveInDevs = waveInGetNumDevs();
        nWaveOutDevs = waveOutGetNumDevs();

        if (nWaveInDevs != nWaveOutDevs) {
                debug_msg("Number of input devices (%d) does not correspond to number of output devices (%d)\n",
                        nWaveInDevs, nWaveOutDevs);
                /* This is fatal for all code as we assume a 1-to-1 correspondence 
                 * between wave input devices, wave output devices, and mixers.
                 * We don't abort just in case things work.  Look out for some really
                 * strange bug reports.
                 */
        }

        nDevs = min(nWaveInDevs, nWaveInDevs);
        nDevs = min(nDevs, W32SDK_MAX_DEVS);

        for(i = 0; i < nDevs; i++) {
                waveInGetDevCaps((UINT)i, &wic, sizeof(WAVEINCAPS));
                strncpy(szDevNames[i], wic.szPname, W32SDK_MAX_NAME_LEN);
        }
}

int
w32sdk_get_device_count()
{
        return nDevs;
}

char *
w32sdk_get_device_name(int idx)
{
        if (idx >= 0 && idx < nDevs) {
                return szDevNames[idx];
        }
        return NULL;
}
#endif /* WIN32 */
