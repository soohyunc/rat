/*
 * FILE:	audwin32.c
 *
 * Win32 audio interface for RAT.
 * Portions based on the VAT Win95 port by John Brezak.
 * Modifications by Isidor Kouvelas <I.Kouvelas@cs.ucl.ac.uk>
 * and Orion Hodson <O.Hodson@cs.ucl.ac.uk>.
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

#include <winsock.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "assert.h"

#include "rat_types.h"
#include "audio.h"
#include "util.h"
#include "tk.h"
#include "win32_rat.h"

#define rat_to_device(x)	((x) * 255 / MAX_AMP)
#define device_to_rat(x)	((x) * MAX_AMP / 255)

static int		error = 0;
static char		errorText[MAXERRORLENGTH];
extern int thread_pri;
static WAVEFORMATEX	format;
static int		duplex;

/* Orion's Mixer Code *************************************************/


#define IsMixSrc(x) ((x)>=MIXERLINE_COMPONENTTYPE_SRC_FIRST && \
                     (x)<=MIXERLINE_COMPONENTTYPE_SRC_LAST)

#define IsMixDst(x) ((x)>=MIXERLINE_COMPONENTTYPE_DST_FIRST && \
                     (x)<=MIXERLINE_COMPONENTTYPE_DST_LAST)

#define MAX_MIX_CTLS 10

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
        char szName[5];
        int  nID;
} MapEntry;

static MixCtls  mcMixIn[MAX_MIX_CTLS], mcMixOut[MAX_MIX_CTLS];
static u_int32  nMixIn, nMixOut, nMixInID, curMixIn, curMixOut;
static int32	play_vol, rec_vol;

static HMIXER hMixIn, hMixOut;

static MapEntry meInputs[] = {
        {"mic",  AUDIO_MICROPHONE},
        {"line", AUDIO_LINE_IN},
        {"cd",   AUDIO_CD},
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
        int j;
        const char *szName;
       
        if ((const char*)NULL == (szName = audioIDToName(id, meMap))) return (MixCtls*)NULL;
        for(j = 0; j < nMix; j++) {
                if (strncasecmp(szName,mcMix[j].szName, 3)==0) return (mcMix + j); 
        }
        return (MixCtls *)NULL;
}

static int
mixSetIPort(MixCtls *mcMix)
{
        assert(mcMix);
        
        if (mcMix) {
                fprintf(stderr, "port %s selected\n", mcMix->szName);
        }

        curMixIn = (mcMix - mcMixIn);
        audio_set_gain(0, play_vol);
        return 0;
}

static int 
mixNameMatch(char *s1, char *s2)
{
        char szS1[255], szS2[255];
        char *szS1begin, *szS1end, *szS2begin, *szS2end;
        
        strcpy(szS1, s1); strcpy(szS2, s2);
        szS1end = strrchr(szS1, ' ');
        if (szS1end != NULL) szS1end++;
        szS2end = strrchr(szS2, ' ');
        if (szS2end != NULL) szS2end++;
        szS1begin = strtok(szS1, " ");
        szS2begin = strtok(szS2, " ");

        return (strcmp(szS1begin, szS2begin)|strcmp(szS1end, szS2end));
}

static void
mixGetControls(HMIXER hMix, char *szDstName, int nDst, MixCtls *mcMix, int *nMix)
{
        MIXERLINE         ml;
        MIXERCONTROL     *mc;
        MIXERLINECONTROLS mlc;
        MMRESULT res;

        int i, src, ctl;
        
        for(i = 0; i < nDst; i++) { 
                ml.dwDestination = i;
                ml.cbStruct = sizeof(MIXERLINE);
                res = mixerGetLineInfo(hMix, &ml, MIXER_GETLINEINFOF_DESTINATION);
                if (res != MMSYSERR_NOERROR || strcmp(ml.szShortName, szDstName) != 0) continue;
                (*nMix) = ml.cConnections;
                for (src = ml.cConnections - 1; src>=0;src--) {
                        ml.dwSource  = src;
                        res = mixerGetLineInfo(hMix, &ml, MIXER_GETLINEINFOF_SOURCE);
                        if (res != MMSYSERR_NOERROR) continue;
                        strncpy(mcMix[src].szName, ml.szShortName,20);
#ifdef DEBUG_WIN32_AUDIO
                        fprintf(stderr, "\tID %d Source %s Controls %d Src(%d) Dst (%d),\n", 
                                ml.dwLineID, 
                                ml.szShortName, 
                                ml.cControls, 
                                (ml.fdwLine & MIXERLINE_LINEF_SOURCE) ? 1:0, 
                                (ml.fdwLine & MIXERLINE_LINEF_SOURCE) ? 0:1);
#endif /* DEBUG_WIN32_AUDIO */
                        mc = (MIXERCONTROL*)xmalloc(sizeof(MIXERCONTROL) * ml.cControls);
                        mlc.cbStruct   = sizeof(MIXERLINECONTROLS);
                        mlc.dwLineID   = ml.dwLineID;
                        mlc.cControls  = ml.cControls;
                        mlc.cbmxctrl   = sizeof(MIXERCONTROL);
                        mlc.pamxctrl   = mc;
                        res = mixerGetLineControls(hMix, &mlc, MIXER_GETLINECONTROLSF_ALL);
                        if (res != MMSYSERR_NOERROR) continue;
                        for(ctl = 0; ctl < (signed)ml.cControls && ctl < MAX_MIX_CTLS; ctl++) {
                                mcMix[src].dwCtlID[ctl]         = mc[ctl].dwControlID;
                                mcMix[src].dwCtlType[ctl]       = mc[ctl].dwControlType;
                                mcMix[src].dwCtl[ctl]           = mc[ctl].fdwControl;
                                mcMix[src].dwMultipleItems[ctl] = mc[ctl].cMultipleItems;
                                mcMix[src].dwLowerBound[ctl]    = mc[ctl].Bounds.dwMinimum;
                                mcMix[src].dwUpperBound[ctl]    = mc[ctl].Bounds.dwMaximum;
#ifdef DEBUG_WIN32_AUDIO
                                
                                fprintf(stderr, "\t\t%s\tid %d\tmin %d max %d type %d\n", 
                                        mc[ctl].szShortName, 
                                        mc[ctl].dwControlID,
                                        mc[ctl].Bounds.lMinimum,
                                        mc[ctl].Bounds.lMaximum,
                                        mc[ctl].dwControlType);          
#endif /* DEBUG_WIN32_AUDIO */
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
        
        char szMixOut[255], szMixIn[255];	
        int i, nDevs, nDstIn, nDstOut;

        if (hMixIn)  {mixerClose(hMixIn);  hMixIn  = 0;}
        if (hMixOut) {mixerClose(hMixOut); hMixOut = 0;}

        RegGetValue(HKEY_CURRENT_USER, 
		"Software\\Microsoft\\Multimedia\\Sound Mapper", 
		"Playback", 
		szMixOut, 
		255);
	RegGetValue(HKEY_CURRENT_USER, 
		"Software\\Microsoft\\Multimedia\\Sound Mapper", 
		"Record", 
		szMixIn, 
		255);

        nDevs = mixerGetNumDevs();
        for(i = 0; i < nDevs; i++) {
                char doClose = TRUE;
                /* Strictly we don't need to open mixer here */
                mixerOpen(&hMix, i, (unsigned long)NULL, (unsigned long)NULL, MIXER_OBJECTF_MIXER);
                res = mixerGetDevCaps(i,  &m, sizeof(m));
                if (res == MMSYSERR_NOERROR && 
                    (strstr(m.szPname, "Mixer")||strstr(m.szPname, "mixer")||strstr(m.szPname,"MIXER"))){
                        if (mixNameMatch(m.szPname, szMixOut)==0) {
                                hMixIn  = hMix;
                                nDstIn  = m.cDestinations;
                                doClose = FALSE;
#ifdef DEBUG_WIN32_AUDIO
                                fprintf(stderr, "Input mixer %s\n", m.szPname); 
#endif /* DEBUG_WIN32_AUDIO */
                        }
                        if (mixNameMatch(m.szPname, szMixOut)==0) {
                                hMixOut = hMix;
                                nDstOut = m.cDestinations;
                                doClose = FALSE;
#ifdef DEBUG_WIN32_AUDIO
                                fprintf(stderr, "Output mixer %s\n", m.szPname); 
#endif /* DEBUG_WIN32_AUDIO */
                        }
                }
                if (doClose) mixerClose(hMix);
        }
        /* There are fields within MIXERLINE struct that should say
         * if line is input or output.  Does not work with SB driver
         * so we give a string to match to "Rec" or "Vol", great :-( */

        mixGetControls(hMixIn, "Rec", nDstIn,  mcMixIn,  &nMixIn);
        mixSetIPort(nameToMixCtls("mic", mcMixIn, nMixIn));
}

/* John Brezak's Mixer Code (?) ****************************************/
typedef struct audMux_s {
	MIXERCONTROLDETAILS select_[8];
	MIXERCONTROLDETAILS vol_[8];
	u_char mcnt_;
	u_char vcnt_;
	char mmap_[8];
	char vmap_[8];
	u_char isOut_;
} audMux;

audMux		imux_, omux_;
static int	iports = 0;
static int	oports = 0;
static int	iport = 0;



static int
mapName(audMux *mux, const char* name)
{

	return (0);
#ifdef NDEF
	return (mux->isOut_? StrToOPort(name) : StrToIPort(name));
#endif
}

static int
mapMixerPort(audMux *mux, const char* name)
{
	int i = mapName(mux, name);
	if (i < 0) {
		char nm[64];
		char* cp;
		strcpy(nm, name);
		while ((cp = strrchr(nm, ' ')) != 0) {
			*cp = 0;
			if ((i = mapName(mux, nm)) >= 0)
				break;
		}
	}
	return (i);
}

static void
getMixerDetails(MIXERLINE *ml, MIXERCONTROL *mc, audMux *mux)
{
	MIXERCONTROLDETAILS mcd;

	mcd.cbStruct = sizeof(mcd);
	mcd.dwControlID = mc->dwControlID;
	mcd.cChannels = ml->cChannels;
	mcd.cMultipleItems = mc->cMultipleItems;
	if (mcd.cMultipleItems) {
		u_int sts;
		MIXERCONTROLDETAILS_LISTTEXT mcdt[16];	
		mcd.cbDetails = sizeof(mcdt[0]);
		mcd.paDetails = mcdt;
		sts = mixerGetControlDetails(0, &mcd, MIXER_GETCONTROLDETAILSF_LISTTEXT);
		if (sts == 0) {
			u_int i;
			for (i = 0; i < mc->cMultipleItems; ++i) {
				u_int n, j;
				MIXERCONTROLDETAILS_BOOLEAN* mcdb;
				int port = mapMixerPort(mux, mcdt[i].szName);
				if (port >= 0)
					mux->mmap_[port] = i;

				n = mcd.cMultipleItems * mcd.cChannels;
				mcdb = (MIXERCONTROLDETAILS_BOOLEAN*)xmalloc(n * sizeof(MIXERCONTROLDETAILS_BOOLEAN));
				memset(mcdb, 0, n * sizeof(*mcdb));
				for (j = 0; j < mcd.cChannels; ++j)
					mcdb[j * mc->cMultipleItems+i].fValue = 1;
					
				mux->select_[i] = mcd;
				mux->select_[i].cbDetails = sizeof(*mcdb);
				mux->select_[i].paDetails = mcdb;
			}
			mux->mcnt_ = (u_char)mcd.cMultipleItems;
		}
	} else {
		MIXERCONTROLDETAILS_UNSIGNED* mcdu;
		int i = mux->vcnt_++;
		int port = mapMixerPort(mux, ml->szName);
		if (port >= 0)
			mux->vmap_[port] = i;

		mcdu = (MIXERCONTROLDETAILS_UNSIGNED*)xmalloc(mcd.cChannels * sizeof(MIXERCONTROLDETAILS_UNSIGNED));
		memset(mcdu, 0, mcd.cChannels * sizeof(*mcdu));
		mux->vol_[i] = mcd;
		mux->vol_[i].cbDetails = sizeof(*mcdu);
		mux->vol_[i].paDetails = mcdu;
	}
}

static void
getMixerCtrls(MIXERLINE *ml, audMux *mux)
{
	MIXERLINECONTROLS mlc;
	MIXERCONTROL mc[16];
	u_int i;

	memset(&mlc, 0, sizeof(mlc));
	memset(mc, 0, sizeof(mc));
	mlc.cbStruct = sizeof(mlc);
	mlc.cbmxctrl = sizeof(mc[0]);
	mlc.pamxctrl = &mc[0];
	mlc.dwLineID = ml->dwLineID;
	mlc.cControls = ml->cControls;
	mixerGetLineControls(0, &mlc, MIXER_GETLINECONTROLSF_ALL);
	for (i = 0; i < mlc.cControls; ++i) {
		switch (mc[i].dwControlType) {

		case MIXERCONTROL_CONTROLTYPE_MUX:
		case MIXERCONTROL_CONTROLTYPE_MIXER:
		case MIXERCONTROL_CONTROLTYPE_VOLUME:
			getMixerDetails(ml, &(mc[i]), mux);
			break;
		}
	}
	/*
	 * if there are multiple source lines for this line,
	 * get their controls
	 */
	for (i = 0; i < ml->cConnections; ++i) {
		MIXERLINE src;
		memset(&src, 0, sizeof(src));
		src.cbStruct = sizeof(src);
		src.dwSource = i;
		src.dwDestination = ml->dwDestination;
		if (mixerGetLineInfo(0, &src, MIXER_GETLINEINFOF_SOURCE) == 0)
			getMixerCtrls(&src, mux);
	}
}

static void
setupMux(audMux *mux, DWORD ctype)
{
	MIXERLINE l;
	int s;

        mixSetup();
	memset(&l, 0, sizeof(l));
	l.cbStruct = sizeof(l);
	l.dwComponentType = ctype;
	s = mixerGetLineInfo(0, &l, MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (s == 0)
		getMixerCtrls(&l, mux);
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
		fprintf(stderr, "OpenOut: unable to waveOutOpen: %s\n", errorText);
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
			fprintf(stderr, "Win32Audio: waveOutPrepareHeader: %s\n", errorText);
			exit(1);
		}
	}
	write_tail = write_curr = write_hdrs;

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
		char errmsg[80];
		sprintf(errmsg, 
				"Running out of write buffers %d left\n",
				write_hdrs_used);
		OutputDebugString(errmsg);
	}

	for (; remain > 0; remain -= len) {
		if (write_curr->dwFlags & WHDR_DONE) {
			/* Have overdone it! */
			char msg[80];
			sprintf(msg,
				"audio_write, reached end of buffer (%06d bytes remain)\n",
				remain);
			OutputDebugString(msg);
			return (ret - remain);
		}

		len = remain > blksz/smplsz ? blksz/smplsz : remain;

		memcpy(write_curr->lpData, cp, len * smplsz);
		cp += len;

		error = waveOutWrite(shWaveOut, write_curr, sizeof(WAVEHDR));
		
		if (error == WRITE_ERROR_STILL_PLAYING) { /* We've filled device buffer ? */
				char msg[80];
				sprintf(msg,
						"Win32Audio - device filled. Discarding %d bytes.\n",
						ret - remain);
				OutputDebugString(msg);
					/* we return as if we wrote everything out
					 * to give buffer a little breathing room
					 */

				return ret;
		} else if (error) {
			waveOutGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, 
					"Win32Audio: waveOutWrite (%d): %s\n", 
					error,
					errorText);
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

unsigned char
is_audio_ready()
{
#ifdef DEBUG
        if (audio_ready>nblks/5) {
                printf("Lots of audio available (%d blocks)\n", audio_ready);
        }
#endif

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
		//fprintf(stderr, "Win32Audio: waveInOpen Error: (%d) %s\n", error, errorText);
		return (FALSE);
	}

	/* Provide buffers for reading */
	for (l = 0, whp = read_hdrs, bp = read_mem; l < nblks; l++, whp++, bp += blksz) {
		whp->lpData = bp;
		whp->dwBufferLength = blksz;
		whp->dwFlags = 0;
		error = waveInPrepareHeader(shWaveIn, whp, sizeof(WAVEHDR));
		if (error) {
			waveInGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "waveInPrepareHeader: (%d) %s\n", error, errorText);
			exit(1);
		}
		error = waveInAddBuffer(shWaveIn, whp, sizeof(WAVEHDR));
		if (error) {
			waveInGetErrorText(error, errorText, sizeof(errorText));
			fprintf(stderr, "waveInAddBuffer: (%d) %s\n", error, errorText);
			exit(1);
		}
	}
	read_curr = read_hdrs;

	error = waveInStart(shWaveIn);
	if (error) {
		waveInGetErrorText(error, errorText, sizeof(errorText));
		fprintf(stderr, "Win32Audio: waveInStart: (%d) %s\n", error, errorText);
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
}

int
audio_read(int audio_fd, sample *buf, int samples)
{
        int len = 0;

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
			        fprintf(stderr, "waveInAddBuffer: (%d) %s\n", error, errorText);
			        exit(1);
		        }
		        read_curr++;
		        if (read_curr == read_hdrs + nblks) read_curr = read_hdrs;
		        if (audio_ready > 0) audio_ready--;
                }
        
                if (audio_ready && len < samples && ~read_curr->dwFlags & WHDR_DONE) {
                        char msg[255];
                        int i,used;
                        for(i=0,used=0;i<nblks;i++) 
                                if (read_hdrs[i].dwFlags & WHDR_DONE) used++;
                        sprintf(msg, "Read buffer too small used %d of %d, samples %d len %d, ready %d\n", used, nblks, samples, len, audio_ready);
		        OutputDebugString(msg);	
                }
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
	format.wFormatTag      = WAVE_FORMAT_PCM;
	format.nChannels       = fmt.num_channels;
	format.nSamplesPerSec  = fmt.sample_rate;
	format.wBitsPerSample  = fmt.bits_per_sample;
        smplsz                 = format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nChannels * format.nSamplesPerSec * smplsz;
	format.nBlockAlign     = format.nChannels * smplsz;
	format.cbSize          = 0;

        /* Use 1 sec device buffer */
	blksz  = fmt.blocksize * smplsz;
	nblks  = format.nAvgBytesPerSec / blksz;

	if (audio_open_in() == FALSE)   return -1;
	duplex = audio_open_out();

	if (mixerGetNumDevs()) {
		int i;
		/* set up the mixer controls for input & out select & gain */
		memset(&imux_, 0, sizeof(imux_));
		memset(&imux_.mmap_, -1, sizeof(imux_.mmap_));
		setupMux(&imux_, MIXERLINE_COMPONENTTYPE_DST_WAVEIN);
		for (i = 0; i < sizeof(imux_.mmap_); ++i)
			if (iports < imux_.mmap_[i])
				iports = imux_.mmap_[i];

		memset(&omux_, 0, sizeof(omux_));
		memset(&omux_.mmap_, -1, sizeof(omux_.mmap_));
		omux_.isOut_ = 1;
		setupMux(&omux_, MIXERLINE_COMPONENTTYPE_DST_SPEAKERS);
		for (i = 0; i < sizeof(omux_.mmap_); ++i)
			if (oports < omux_.mmap_[i])
				oports = omux_.mmap_[i];
	}

	switch(thread_pri) {
	case 1:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		break;
	case 2:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		break;
	case 3:
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		break;
	default:
		break;
	}

	return 1;
}


void
audio_close(int audio_fd)
{
	audio_close_in();
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


static void
old_audio_set_gain(int audio_fd, int level)
{
	MIXERCONTROLDETAILS* mcd;
	u_int i;

	rec_vol = level;

	if (shWaveIn == 0)
		return;

	level = rat_to_device(level);

	if (level > 255)
		level = 255;
	level <<= 8;
	mcd = &imux_.vol_[imux_.vmap_[iport]];
	for (i = 0; i < mcd->cChannels; ++i)
	   ((MIXERCONTROLDETAILS_UNSIGNED*)mcd->paDetails + i)->dwValue = level;
	mixerSetControlDetails(0, mcd, MIXER_SETCONTROLDETAILSF_VALUE);
}

void
audio_set_gain(int audio_fd, int level)
{
        int i;
        MIXERCONTROLDETAILS          mcd;
        MIXERCONTROLDETAILS_UNSIGNED mcduDevLevel;
        MIXERCONTROLDETAILS_BOOLEAN  mcdbOn;
        MMRESULT r;
        UNUSED(audio_fd);
        
        for(i = 0; i < mcMixIn[curMixIn].nCtls; i++) {
                switch (mcMixIn[curMixIn].dwCtlType[i]) {
                case MIXERCONTROL_CONTROLTYPE_VOLUME:
                        fprintf(stderr, "expect %d got %d \n",MIXERCONTROL_CONTROLTYPE_VOLUME,mcMixIn[curMixIn].dwCtlType[i]); 
                        mcd.cbStruct       = sizeof(MIXERCONTROLDETAILS);
                        mcd.dwControlID    = mcMixIn[curMixIn].dwCtlID[i];
                        mcd.cChannels      = 1;
                        mcd.cMultipleItems = 0;
                        mcd.cbDetails      = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
                        mcd.paDetails      = &mcduDevLevel;
                        mcduDevLevel.dwValue = ((mcMixIn[curMixIn].dwUpperBound[i] - mcMixIn[curMixIn].dwLowerBound[i])/100) * level + mcMixIn[curMixIn].dwLowerBound[i];
                        fprintf(stderr, "%d %d\n", level, mcduDevLevel.dwValue);
                        play_vol   = level;
                        r = mixerSetControlDetails(hMixIn, &mcd, MIXER_OBJECTF_HMIXER);
                        switch (r) {
                                case MMSYSERR_NOERROR:    break; 
                                case MIXERR_INVALLINE:     fprintf(stderr, "invalid line\n"); break;
                                case MIXERR_INVALCONTROL:  fprintf(stderr, "invalid control\n"); break;
                                case MIXERR_INVALVALUE:    fprintf(stderr, "invalid value\n"); break;
                                case MMSYSERR_BADDEVICEID: fprintf(stderr, "bad device id\n");   break;
                                case MMSYSERR_INVALFLAG:   fprintf(stderr, "invalid flag\n");    break;
                                case MMSYSERR_INVALHANDLE: fprintf(stderr, "invalid handle\n");  break;
                                case MMSYSERR_INVALPARAM:  fprintf(stderr, "invalid param\n");   break;
                                case MMSYSERR_NODRIVER:    fprintf(stderr, "no driver!\n");      break;
                                default:                   fprintf(stderr, "mixerSetControlDetails ?");
                        }
                        break;
                 case MIXERCONTROL_CONTROLTYPE_ONOFF:
                 /* if line is muted unmute it */
                        break;
                 default:
                         fprintf(stderr, "Could use 0x%08x\n", mcMixIn[curMixIn].dwCtlType[i]);
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
		fprintf(stderr, "Win32Audio: waveOutSetVolume: %s\n", errorText);
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
		fprintf(stderr, "Win32Audio: waveOutGetVolume Error: %s\n", errorText);
#endif
		return (0);
	} else
		return (device_to_rat(vol & 0xff));
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
