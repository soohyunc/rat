/*
 * FILE:    codec_acm.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 * 
 * $Revision$
 * $Date$
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
 */

#ifdef WIN32
#include "config.h"
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <stdio.h>

#include "rat_types.h"
#include "codec_acm.h"
#include "codec.h"
#include "util.h"

static HACMDRIVERID *phCodecID;
static HACMDRIVER    hadActive;
static int nCodecs, n ;

#define CODEC_ACM_INPUT  1
#define CODEC_ACM_OUTPUT 2
#define CODEC_ACM_FRAME  4
#define CODEC_ACM_SAMPLE 8

#define CODEC_ACM_NOT_TESTED 0
#define CODEC_ACM_PRESENT    1

typedef struct s_acm_codec {
        char szShortName[32];
        char szRATCodecName[32]; /* This has to match entry in codec.c */
        WORD nSamplesPerSec;
        WORD nChannels;
        WORD nAvgBytesPerSec;
	HACMDRIVERID had;
	WORD status;
        WAVEFORMATEX wfx;
} acm_codec_t;

/* These are codecs in the codec table in codec.c.
*
* A random curiosity is that the ITU call the upper
* bitrate G723.1 a 6.3kbs coder in all documentation
* whereas microsoft call it 6.4kbs coder and it is :-)
*/

#define ACM_MAX_DYNAMIC 2

acm_codec_t known_codecs[] = {
        {"Microsoft G.723.1", "G723.1(6.3kb/s)", 8000, 1, 800, 0, CODEC_ACM_NOT_TESTED},
        {"Microsoft G.723.1", "G723.1(5.3kb/s)", 8000, 1, 666, 0, CODEC_ACM_NOT_TESTED}
};

static HACMDRIVERID hDrvID[ACM_MAX_DYNAMIC];
static HACMDRIVER   hDrv[ACM_MAX_DYNAMIC];
static int nDrvOpen;

static const char *
acmGetError(int e)
{
        switch (e) {
#ifdef DEBUG
        case ACMERR_NOTPOSSIBLE:      return "The requested operation cannot be performed.";
        case ACMERR_BUSY:             return "Busy.";         
        case ACMERR_UNPREPARED:       return "Structure unprepared.";
        case ACMERR_CANCELED:         return "Cancelled";
      
        case MMSYSERR_BADDEVICEID:    return "device ID out of range";
        case MMSYSERR_NOTENABLED :    return "driver failed enable";
        case MMSYSERR_ALLOCATED:      return "device already allocated";
        case MMSYSERR_INVALHANDLE:    return "device handle is invalid";
        case MMSYSERR_NODRIVER:       return "no device driver present";
        case MMSYSERR_NOMEM:          return "memory allocation error";
        case MMSYSERR_NOTSUPPORTED:   return "function isn't supported";
        case MMSYSERR_BADERRNUM:      return "error value out of range";
        case MMSYSERR_INVALFLAG:      return "invalid flag passed";
        case MMSYSERR_INVALPARAM:     return "invalid parameter passed";
        case MMSYSERR_HANDLEBUSY:     return "handle being used simultaneously on another thread (eg callback)";
        case MMSYSERR_INVALIDALIAS:   return "specified alias not found";
        case MMSYSERR_BADDB:          return "bad registry database";
        case MMSYSERR_KEYNOTFOUND:    return "registry key not found";
        case MMSYSERR_READERROR:      return "registry read error";
        case MMSYSERR_WRITEERROR:     return "registry write error";
        case MMSYSERR_DELETEERROR:    return "registry delete error";
        case MMSYSERR_VALNOTFOUND:    return "registry value not found";
        case MMSYSERR_NODRIVERCB:     return "driver does not call DriverCallback";
#endif
        default:                   
                return "Error not found.\n";
        }
}

static int 
acmAcceptableRate(int rate) 
{
        /* If you want to add multiples of 11025 this code should not break here */
        static const int smplRates[] = {8000, 16000, 32000, 48000};
        static const int nRates = 4;
        int i;
        for(i = 0; i<nRates; i++)
                if (smplRates[i] == rate) return TRUE;
                return FALSE;
}

static void
acmFrameMetrics(HACMSTREAM has, WORD wBitsPerSample, DWORD dwSamplesPerSec, DWORD dwBytesPerSec, int *piSamplesPerFrame, int *piFrameSize)
{
        DWORD dwSrcSize = wBitsPerSample/8, dwDstSize = 0;
        MMRESULT mmr = 0;
        
        while(dwDstSize == 0 && (mmr == 0 || mmr == ACMERR_NOTPOSSIBLE)) {
                dwSrcSize += wBitsPerSample/8;
                mmr = acmStreamSize(has, dwSrcSize, &dwDstSize, ACM_STREAMSIZEF_SOURCE);
        }
        
        if (mmr) debug_msg(acmGetError(mmr));
                (*piFrameSize) = dwDstSize;
        
        /* some codecs return frame size irrespective of source block size (groan!) */
        (*piSamplesPerFrame) = dwSamplesPerSec * dwDstSize / dwBytesPerSec;
}

static ACMDRIVERDETAILS add;

BOOL CALLBACK 
acmFormatEnumProc(HACMDRIVERID hadid, LPACMFORMATDETAILS pafd, DWORD dwInstance, DWORD fdwSupport)
{
        MMRESULT mmr;
        WAVEFORMATEX wfxDst;
        int i;
        /* So we have a format now we need frame sizes (if pertinent)   */
        
        /* Copy this because ACM calls can trash it */
        memcpy(&wfxDst, pafd->pwfx, sizeof(WAVEFORMATEX));
        
        /* We use a crude guess at whether format of pafd->pwfx is PCM,
        * only interested in 16-bit (rat's native format) PCM to other 
        * format (and vice-versa) here.
        */
        if (acmAcceptableRate(wfxDst.nSamplesPerSec)) {
                HACMSTREAM has = 0;
                WAVEFORMATEX wfxPCM;
                int          iIOAvail = 0;
                int          iType, iSamplesPerFrame = 0, iFrameSize = 0, iFixedHdrSize = 0;
                
                /* This is a dumb test but frame based codecs are inconsistent in their bits per sample reported */
                if ((wfxDst.wBitsPerSample & 0x07)||(wfxDst.nBlockAlign == wfxDst.nChannels * wfxDst.wBitsPerSample/8)) {
                        iType = CODEC_ACM_SAMPLE;
                } else   {
                        iType = CODEC_ACM_FRAME;
                } 
                
                wfxPCM.wFormatTag      = WAVE_FORMAT_PCM;
                wfxPCM.nChannels       = wfxDst.nChannels;
                wfxPCM.nSamplesPerSec  = wfxDst.nSamplesPerSec;
                wfxPCM.wBitsPerSample  = 16;
                wfxPCM.nBlockAlign     = wfxPCM.nChannels * wfxPCM.wBitsPerSample / 8;
                wfxPCM.nAvgBytesPerSec = wfxPCM.nBlockAlign * wfxPCM.nSamplesPerSec;
                wfxPCM.cbSize          = 0;
                
                mmr = acmStreamOpen(&has, hadActive, &wfxPCM, &wfxDst, NULL, 0L, 0L, ACM_STREAMOPENF_QUERY);
                /* We usually fail because we cannot convert format in real-time, e.g.
                 * MPEG Layer III on this machine above 16kHz.  These don't appear
                 * to be related to machine type (?).
                 */
                if (0 == mmr) {
                        iIOAvail |= CODEC_ACM_INPUT;
                        switch(iType) {
                        case CODEC_ACM_FRAME:
                                /* In nearly all cases Frame size is the same as alignment, but do not assume this */
                                acmFrameMetrics(has, 
                                        wfxPCM.wBitsPerSample,
                                        wfxPCM.nSamplesPerSec,
                                        pafd->pwfx->nAvgBytesPerSec, 
                                        &iSamplesPerFrame, 
                                        &iFrameSize);
                                break;
                        case CODEC_ACM_SAMPLE:
                                
                                break;
                        }
                        acmStreamClose(has, 0);
                
                        mmr = acmStreamOpen(&has, hadActive, &wfxDst, &wfxPCM, NULL, 0L, 0L, ACM_STREAMOPENF_QUERY);
                
                        if (0 == mmr) {
                                iIOAvail |= CODEC_ACM_OUTPUT;
                                acmStreamClose(has,0);
                        } else {
                                debug_msg("%s\n",acmGetError(mmr));
                        }
                } else {
                        debug_msg("%s\n",acmGetError(mmr));
                }
                if (iIOAvail != (CODEC_ACM_OUTPUT|CODEC_ACM_INPUT)) {
                        return TRUE;
                }
                
                for(i = 0; i < ACM_MAX_DYNAMIC; i++) {
                        if (!strcmp(known_codecs[i].szShortName, add.szShortName) &&
                                known_codecs[i].nSamplesPerSec == wfxPCM.nSamplesPerSec &&
                                known_codecs[i].nChannels == wfxPCM.nChannels &&
                                known_codecs[i].nAvgBytesPerSec == pafd->pwfx->nAvgBytesPerSec) {
                                /* This code is a little naive. */
                                known_codecs[i].status = CODEC_ACM_PRESENT;
                                known_codecs[i].had    = hadid;
                                memcpy(&known_codecs[i].wfx, &wfxDst, sizeof(WAVEFORMATEX));
                                debug_msg("Added %s\n", known_codecs[i].szShortName);
                        }
                }
                
                debug_msg("\t\t%4.4lXH %4.4lXH, %s (%d Bps, Rate %d, align %d bytes, %d bits per sample, %d channels, cbsize %d)\n", pafd->dwFormatTag, pafd->dwFormatIndex, pafd->szFormat, pafd->pwfx->nAvgBytesPerSec, pafd->pwfx->nSamplesPerSec, pafd->pwfx->nBlockAlign, pafd->pwfx->wBitsPerSample, pafd->pwfx->nChannels, pafd->pwfx->cbSize); 
                
                switch(iType) {
                case CODEC_ACM_SAMPLE:
                        printf("\t\t\tSample Based: ");
                        break;
                case CODEC_ACM_FRAME:
                        printf("\t\t\tFrame  Based: ");
                        break;
                }
                debug_msg("\t\t\tInput(%d) Output(%d) Samples per Frame(%d), Frame Size(%d)\n", 
                        (iIOAvail&CODEC_ACM_INPUT) ? 1: 0,
                        (iIOAvail&CODEC_ACM_OUTPUT) ? 1: 0,
                        iSamplesPerFrame,
                        iFrameSize);
                
        }
        return TRUE;
}

static void
acmCodecCaps(HACMDRIVERID hadid)
{
        
        DWORD            dwSize;
        WAVEFORMATEX    *pwf;
        ACMFORMATDETAILS afd;
        
        add.cbStruct = sizeof(ACMDRIVERDETAILS);
        if (acmDriverDetails(hadid, &add, 0)) return; 
        printf("   Short name: %s\n", add.szShortName);
        printf("   Long name:  %s\n", add.szLongName);
        printf("   Copyright:  %s\n", add.szCopyright);
        printf("   Licensing:  %s\n", add.szLicensing);
        printf("   Features:   %s\n", add.szFeatures);
        printf("   Supports %u formats\n", add.cFormatTags);
        printf("   Supports %u filter formats\n", add.cFilterTags);
        
        if (acmDriverOpen(&hadActive, hadid, 0)) return;
        if (!acmMetrics((HACMOBJ)hadActive, ACM_METRIC_MAX_SIZE_FORMAT, &dwSize)){ 
                pwf = (WAVEFORMATEX*)xmalloc(dwSize);
                memset(pwf, 0, dwSize);
                pwf->cbSize     = LOWORD(dwSize) - sizeof(WAVEFORMATEX);
                pwf->wFormatTag = WAVE_FORMAT_UNKNOWN;
                memset(&afd,0,sizeof(ACMFORMATDETAILS));
                afd.cbStruct = sizeof(ACMFORMATDETAILS);
                afd.pwfx  = pwf;
                afd.cbwfx = dwSize;
                afd.dwFormatTag = WAVE_FORMAT_UNKNOWN;
                printf("\tInput Formats (suggested):\n");
                acmFormatEnum(hadActive, &afd, acmFormatEnumProc, ACM_FORMATENUMF_INPUT, 0);
                xfree(pwf);
        }
        acmDriverClose(hadActive,0);
}

BOOL CALLBACK 
acmDriverEnumProc(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
        if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CODEC) {
                phCodecID[nCodecs++] = hadid;
                acmCodecCaps(hadid);
        }
        
        return TRUE;
}

static HACMDRIVER*
acmGetDriverByID(HACMDRIVERID hid)
{
        int i;
        for(i = 0; i < nDrvOpen; i++) {
                if (hDrvID[i] == hid) return (hDrv + i);
        }
        return NULL;
}

static int
acmSecureDriver(HACMDRIVERID hid)
{
        if (acmGetDriverByID(hid) || 
            acmDriverOpen(&hDrv[nDrvOpen], hid, 0)) {
                hDrvID[nDrvOpen] = hid;
                nDrvOpen++;
                return TRUE;
        }
        
        debug_msg("Failed to open driver.\n");
        
        return FALSE;
}

static void
acmCodecsInit(void) 
{
        codec_t *cp;
        int i;

        for(i = 0; i < ACM_MAX_DYNAMIC; i++) {
                if (!known_codecs[i].status & CODEC_ACM_PRESENT ||
                        !acmSecureDriver(known_codecs[i].had))     {
                        cp = get_codec_by_pt(codec_matching(known_codecs[i].szRATCodecName,
                                                              known_codecs[i].nSamplesPerSec,
                                                              known_codecs[i].nChannels));
                        if (cp) {
                                disable_codec(cp);
                        }
                }
        }
}

void
acmStartup()
{
        DWORD dwCodecs = 0, dwDrivers = 0;
        
        acmMetrics(NULL, ACM_METRIC_COUNT_CODECS, &dwCodecs);
        acmMetrics(NULL, ACM_METRIC_COUNT_DRIVERS, &dwDrivers);
        debug_msg("There are %d ACM codecs in %d drivers\n", dwCodecs, dwDrivers);
        phCodecID = (HACMDRIVERID*)xmalloc(sizeof(HACMDRIVER)*dwCodecs);
        acmDriverEnum(acmDriverEnumProc, 0L, 0L);
        acmCodecsInit();        
}

void
acmShutdown()
{
        int i, j, done;

        for (i = 0; i < nDrvOpen; i++) {
                done = FALSE;
                for(j = 0; j < i; j++) if (hDrvID[i] == hDrvID[j]) done = TRUE;
                if (!done) acmDriverClose(hDrv[i], 0);
        }
        nDrvOpen = 0;
}

static acm_codec_t *
acmMatchingCodec(codec_t *cp)
{
        int i;
        for(i = 0; i < ACM_MAX_DYNAMIC; i++) {
                if (cp->short_name == known_codecs[i].szRATCodecName &&
                        known_codecs[i].status == CODEC_ACM_PRESENT) {
                        return known_codecs + i;
                }
        }
        return NULL;
}

typedef struct s_acm_state {
       HACMSTREAM   acms;
       codec_t     *cp;
       acm_codec_t *acp;
} acm_state_t;

acm_state_t * 
acmEncoderCreate(codec_t *cp)
{
        acm_codec_t *acp;
        acm_state_t *s;
        WAVEFORMATEX wfxRaw;

        assert(cp);
        acp = acmMatchingCodec(cp);
        if (!acp) return NULL;
        
        s = (acm_state_t*)xmalloc(sizeof(acm_state_t));

        wfxRaw.cbSize = 0;
        wfxRaw.nAvgBytesPerSec = cp->channels * cp->freq * cp->sample_size;
        wfxRaw.nBlockAlign     = cp->unit_len * cp->sample_size;
        wfxRaw.nChannels       = cp->channels;
        wfxRaw.nSamplesPerSec  = cp->freq;
        wfxRaw.wBitsPerSample  = cp->sample_size * 8;
        wfxRaw.wFormatTag      = 0;

        if (!acmStreamOpen(&s->acms, *(acmGetDriverByID(acp->had)), &wfxRaw, &acp->wfx, NULL, 0L, 0L, 0L)) {
                debug_msg("acmStreamOpen failed!\n");
                xfree(s);
                return NULL;
        }
        
        s->cp  = cp;
        s->acp = acp;

        return s;
}

void
acmEncode(struct s_acm_state *s, sample *src, struct s_coded_unit *dst)
{
        ACMSTREAMHEADER ash;
        
        ash.cbStruct    = sizeof(ACMSTREAMHEADER);
        memset(&ash, 0, sizeof(ACMSTREAMHEADER));

        ash.pbSrc           = (u_char*) src;
        ash.cbSrcLength     = s->cp->unit_len * s->cp->sample_size;
        ash.cbSrcLengthUsed = 0;

        ash.pbDst           = dst->data;
        ash.cbDstLength     = dst->data_len;
        ash.cbDstLengthUsed = 0;

        if (acmStreamPrepareHeader(s->acms, &ash, 0)) {
                debug_msg("acmStreamHeaderPrepare failed\n");
                return;
        }

        if (acmStreamConvert(s->acms, &ash, ACM_STREAMCONVERTF_BLOCKALIGN)) {
                debug_msg("acmStreamConvert failed\n");
                return;
        }

        acmStreamUnprepareHeader(s->acms, &ash, 0);
}

void 
acmEncoderDestroy(struct s_acm_state *s)
{
        acmStreamClose(s->acms,0UL);
        xfree(s->acms);
}


#endif WIN32
