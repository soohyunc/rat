/*
 * FILE:	auddev_win32.c
 *
 * Win32 audio interface for RAT.
 *
 * Written by Orion Hodson and Isidor Kouvelas
 * Portions based on the VAT Win95 port by John Brezak.
 *
 * $Id$
 *
 * Copyright (c) 1995,1996 University College London
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
#include "rat_types.h"
#include "audio.h"
#include "util.h"

#define rat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_rat(x)	((x) * MAX_AMP / 255)

static int		error = 0;
static char		errorText[MAXERRORLENGTH];
extern int thread_pri;
static WAVEFORMATEX	format;
static int		duplex;
static int              nLoopGain = 100;
#define MAX_DEV_NAME 64
static char szDevOut[MAX_DEV_NAME], szDevIn[MAX_DEV_NAME];

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
                case MMSYSERR_NOERROR:     sprintf(szMsg, "no error");        break; 
                case MIXERR_INVALLINE:     sprintf(szMsg, "invalid line");    break;
                case MIXERR_INVALCONTROL:  sprintf(szMsg, "invalid control"); break;
                case MIXERR_INVALVALUE:    sprintf(szMsg, "invalid value");   break;
                case MMSYSERR_BADDEVICEID: sprintf(szMsg, "bad device id");   break;
                case MMSYSERR_INVALFLAG:   sprintf(szMsg, "invalid flag");    break;
                case MMSYSERR_INVALHANDLE: sprintf(szMsg, "invalid handle");  break;
                case MMSYSERR_INVALPARAM:  sprintf(szMsg, "invalid param");   break;
                case MMSYSERR_NODRIVER:    sprintf(szMsg, "no driver!");      break;               
        }
}

static int
mixSetLoopback(char *szName)
{
        unsigned int i, loopCtl;
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
        
        assert(mcMix);
        
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
	
        int i,j, nDevs, nDstIn, nDstOut;
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
                while(j < 32 && (mixName[j] = tolower(m.szPname[j]))) j++;
                
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

        mixGetControls(hMixIn, "Rec", nDstIn,  mcMixIn,  &nMixIn);
        mixGetControls(hMixIn, "Master", nDstOut,  mcMixOut,  &nMixOut);
        if (nMixOut == 0) {
                mixGetControls(hMixIn, "Vol", nDstOut,  mcMixOut,  &nMixOut);
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
audio_open_out()
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
		whp->lpData         = bp;
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
audio_close_out()
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
audio_write(int audio_fd, sample *cp, int remain)
{
	int		error, len, ret;

	if (shWaveOut == 0)
		return (remain);

	ret = remain;
	if (write_hdrs_used > 4*nblks/5) {
		debug_msg("Running out of write buffers %d left\n", write_hdrs_used);
	}

	for (; remain > 0; remain -= len) {
		if (write_curr->dwFlags & WHDR_DONE) {
			/* Have overdone it! */
			debug_msg("audio_write, reached end of buffer (%06d bytes remain)\n", remain);
			return (ret - remain);
		}

		len = remain > blksz/smplsz ? blksz/smplsz : remain;

		memcpy(write_curr->lpData, cp, len * smplsz);
		cp += len;

		error = waveOutWrite(shWaveOut, write_curr, sizeof(WAVEHDR));
		
		if (error == WRITE_ERROR_STILL_PLAYING) { /* We've filled device buffer ? */
				debug_msg("Win32Audio - device filled. Discarding %d bytes.\n",
						ret - remain);
					/* we return as if we wrote everything out
					 * to give buffer a little breathing room
					 */
				return ret;
		} else if (error) {
			waveOutGetErrorText(error, errorText, sizeof(errorText));
			debug_msg("Win32Audio: waveOutWrite (%d): %s\n", error,	errorText);
			return (ret - remain);
		}

		write_curr++;
		write_hdrs_used++;
		if (write_curr >= write_hdrs + nblks)
			write_curr = write_hdrs;
	}
	return (ret);
}

/* AUDIO INPUT RELATED FN's *********************************/

static unsigned char audio_ready = 0;

int
audio_is_ready()
{
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
audio_open_in()
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
		whp->lpData = bp;
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
audio_close_in()
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
audio_read(int audio_fd, sample *buf, int samples)
{
        static int virgin = 0;
        int len = 0;

        if (!virgin) {
                debug_msg("ready %d\n", audio_ready);
                virgin++;
        }

        if (shWaveIn == 0) {
        /* officially we dont support half-duplex anymore but just in case */
                assert(shWaveOut);
		for (len = 0; write_tail->dwFlags & WHDR_DONE;) {
			if (len + write_tail->dwBufferLength / smplsz > (unsigned)samples)
				break;
			else
				len += write_tail->dwBufferLength / smplsz;

			write_tail->dwFlags &=~ WHDR_DONE;
			write_tail++;
			write_hdrs_used--;
			if (write_tail >= write_hdrs + nblks)
				write_tail = write_hdrs;
		}

		if (write_curr == write_tail && len + blksz/smplsz <= samples)
			len += blksz/smplsz;

		return (len);
	} else if (duplex) {
                while (write_tail->dwFlags & WHDR_DONE) {
			write_tail->dwFlags &= ~WHDR_DONE;
			write_tail++;
			write_hdrs_used--;
			if (write_tail >= write_hdrs + nblks)
				write_tail = write_hdrs;
		}
	}

        if (audio_ready) {
	        while ((read_curr->dwFlags & WHDR_DONE) && len < samples) {
		        memcpy(buf, read_curr->lpData, blksz);
		        buf += blksz/smplsz;
		        len += blksz/smplsz;
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
                        debug_msg("RB small %d of %d, samples %d len %d, ready %d\n", used, nblks, samples, len, audio_ready);
                }
#endif
	}

	return (len);
}

/* BEST OF THE REST (SIC) ************************************/

int audio_get_channels()
{
	return format.nChannels;
}

int
audio_open(audio_format fmt)
{
        static int virgin;
        WAVEFORMATEX tfmt;
	
        if (virgin == 0) {
                HKEY hKey = HKEY_CURRENT_USER;
                WAVEOUTCAPS woc;
                WAVEINCAPS  wic;
                UINT        uDevId,uNumDevs;

                RegGetValue(&hKey, 
		    "Software\\Microsoft\\Multimedia\\Sound Mapper", 
		    "Playback", 
		    szDevOut, 
		    MAX_DEV_NAME);
	        RegGetValue(&hKey, 
		    "Software\\Microsoft\\Multimedia\\Sound Mapper", 
		    "Record", 
		    szDevIn, 
		    MAX_DEV_NAME);
                
                uWavOut  = WAVE_MAPPER;
                uNumDevs = waveOutGetNumDevs();
                for (uDevId = 0; uDevId < uNumDevs; uDevId++) {
                       waveOutGetDevCaps(uDevId, &woc, sizeof(woc));
                       if (strcmp(woc.szPname, szDevOut) == 0) {
                               uWavOut = uDevId;
                               break;
                       }
                }
                
                uWavIn   = WAVE_MAPPER;
                uNumDevs = waveInGetNumDevs();
                for (uDevId = 0; uDevId < uNumDevs; uDevId++) {
                       waveInGetDevCaps(uDevId, &wic, sizeof(wic));
                       if (strcmp(wic.szPname, szDevIn) == 0) {
                               uWavIn = uDevId;
                               break;
                       }
                }
                
                if (mixerGetNumDevs()) {
                        mixSetup();    
	        }
                virgin = 1;
        }

        format.wFormatTag      = WAVE_FORMAT_PCM;
	format.nChannels       = fmt.num_channels;
	format.nSamplesPerSec  = fmt.sample_rate;
	format.wBitsPerSample  = fmt.bits_per_sample;
        smplsz                 = format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nChannels * format.nSamplesPerSec * smplsz;
	format.nBlockAlign     = format.nChannels * smplsz;
	format.cbSize          = 0;
        memcpy(&tfmt, &format, sizeof(format));
        /* Use 1 sec device buffer */
	blksz  = fmt.blocksize * smplsz;
	nblks  = format.nAvgBytesPerSec / blksz;
	if (audio_open_in() == FALSE)   return -1;
        if ((duplex = audio_open_out()) == FALSE) {
                audio_close_in();
                return -1;
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

	return 1;
}


void
audio_close(int audio_fd)
{
        debug_msg("Closing input device.\n");
	audio_close_in();
        debug_msg("Closing output device.\n");
	audio_close_out();
}

int
audio_duplex(int audio_fd)
{
	return (duplex);
}


void
audio_drain(int audio_fd)
{
}

void
audio_non_block(int audio_fd)
{
}

void
audio_set_gain(int audio_fd, int level)
{
        int i;
        MIXERCONTROLDETAILS          mcd;
        MIXERCONTROLDETAILS_UNSIGNED mcduDevLevel;
        MMRESULT r;
        UNUSED(audio_fd);
        
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
audio_get_gain(int audio_fd)
{
	return (rec_vol);
}

void
audio_set_volume(int audio_fd, int level)
{
	DWORD	vol;

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
audio_get_volume(int audio_fd)
{
	DWORD	vol;

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
audio_loopback(int audio_fd, int gain)
{
        UNUSED(audio_fd);
        
        nLoopGain = gain;
        mixSetLoopback(mcMixIn[curMixIn].szName);
}

void
audio_set_oport(int audio_fd, int port)
{
	UNUSED(audio_fd);
	UNUSED(port);
}

/* Return selected output port */
int audio_get_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

/* Select next available output port */
int
audio_next_oport(int audio_fd)
{
	return (AUDIO_SPEAKER);
}

void 
audio_set_iport(int audio_fd, int port)
{
        MixCtls *mcMix;
        
        UNUSED(audio_fd);

        mcMix = audioIDToMixCtls(port, meInputs, mcMixIn, nMixIn);
        
        if (mcMix) mixSetIPort(mcMix);
}

/* Return selected input port */
int
audio_get_iport(int audio_fd)
{
        int id = nameToAudioID(mcMixIn[curMixIn].szName, meInputs); 
	return (id);
}

/* Select next available input port */
int
audio_next_iport(int audio_fd)
{
        u_int32 trialMixIn;
        int id = -1;

        trialMixIn = curMixIn;
        do {
                trialMixIn = (trialMixIn + 1) % nMixIn;
                id = nameToAudioID(mcMixIn[trialMixIn].szName, meInputs);
        } while(id == -1);
        mixSetIPort(audioIDToMixCtls(id, meInputs, mcMixIn, nMixIn));
        return (id);
}

#endif /* WIN32 */
