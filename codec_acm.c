/* 
 * Copyright Orion Hodson O.Hodson@cs.ucl.ac.uk
 * University College London, 1998.
 */

#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <stdio.h>

#include "config.h"
#include "codec_acm.h"
#include "util.h"

static HACMDRIVERID *phCodecID, hadActive;
static int nCodecs, n ;

#define CODEC_ACM_INPUT  1
#define CODEC_ACM_OUTPUT 2
#define CODEC_ACM_FRAME  4
#define CODEC_ACM_SAMPLE 8

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
        
        switch(mmr) {
        case 0:
                break;
        case MMSYSERR_INVALFLAG:
                printf("Invalid flag\n");
                break;
        case MMSYSERR_INVALHANDLE:
                printf("Invalid handle\n");
                break;
        case MMSYSERR_INVALPARAM:
                printf("Invalid parameter\n");
                break;
        default:
                printf("Something not documented happened.\n");
        }
        (*piFrameSize) = dwDstSize;
        
        /* some codecs return frame size irrespective of source block size (groan!) */
        (*piSamplesPerFrame) = dwSamplesPerSec * dwDstSize / dwBytesPerSec;
}

BOOL CALLBACK 
acmFormatEnumProc(HACMDRIVERID hadid, LPACMFORMATDETAILS pafd, DWORD dwInstance, DWORD fdwSupport)
{
        MMRESULT mmr;
        LPWAVEFORMATEX lpwfx;
        /* So we have a format now we need frame sizes (if pertinent)   */
        
        lpwfx = pafd->pwfx;

        /* We use a crude guess at whether format of pafd->pwfx is PCM,
         * only interested in 16-bit (rat's native format) PCM to other 
         * format (and vice-versa) here.
        */
        if (acmAcceptableRate(lpwfx->nSamplesPerSec)) {
                HACMSTREAM   has = 0;
                WAVEFORMATEX wfxPCM;
                int          iIOAvail = 0;
                int          iType, iSamplesPerFrame = 0, iFrameSize = 0, iFixedHdrSize = 0;
                
                /* This is a dumb test but frame based codecs are inconsistent in their bits per sample reported */
                if ((lpwfx->wBitsPerSample & 0x07)||(lpwfx->nBlockAlign == lpwfx->nChannels * lpwfx->wBitsPerSample/8)) {
                        iType = CODEC_ACM_SAMPLE;
                } else   {
                        iType = CODEC_ACM_FRAME;
                } 

                wfxPCM.wFormatTag      = WAVE_FORMAT_PCM;
                wfxPCM.nChannels       = lpwfx->nChannels;
                wfxPCM.nSamplesPerSec  = lpwfx->nSamplesPerSec;
                wfxPCM.wBitsPerSample  = 16;
                wfxPCM.nBlockAlign     = wfxPCM.nChannels * wfxPCM.wBitsPerSample / 8;
                wfxPCM.nAvgBytesPerSec = wfxPCM.nBlockAlign * wfxPCM.nSamplesPerSec;
                wfxPCM.cbSize          = 0;
            
                mmr = acmStreamOpen(&has, hadActive, &wfxPCM, lpwfx, NULL, 0L, 0L, 0);
                /* We usually fail because we cannot convert format in real-time, e.g.
                 * MPEG Layer III on this machine above 16kHz.
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
                }
                
                mmr = acmStreamOpen(&has, hadActive, lpwfx, &wfxPCM, NULL, 0L, 0L, ACM_STREAMOPENF_QUERY);
                if (0 == mmr) iIOAvail |= CODEC_ACM_OUTPUT;
                
                if (iIOAvail != (CODEC_ACM_OUTPUT|CODEC_ACM_INPUT)) {
                        return TRUE;
                }
                
                printf("\t\t%4.4lXH %4.4lXH, %s (%d Bps, Rate %d, align %d bytes, %d bits per sample, %d channels, cbsize %d)\n", pafd->dwFormatTag, pafd->dwFormatIndex, pafd->szFormat, pafd->pwfx->nAvgBytesPerSec, pafd->pwfx->nSamplesPerSec, pafd->pwfx->nBlockAlign, pafd->pwfx->wBitsPerSample, pafd->pwfx->nChannels, pafd->pwfx->cbSize);
                
                switch(iType) {
                case CODEC_ACM_SAMPLE:
                        printf("\t\t\tSample Based: ");
                        break;
                case CODEC_ACM_FRAME:
                        printf("\t\t\tFrame  Based: ");
                        break;
                }
                printf("\t\t\tInput(%d) Output(%d) Samples per Frame(%d), Frame Size(%d)\n", 
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
        ACMDRIVERDETAILS add;
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
        if (acmMetrics(hadActive, ACM_METRIC_MAX_SIZE_FORMAT, &dwSize)) return;
        
        pwf = (WAVEFORMATEX*)xmalloc(dwSize);
        memset(pwf, 0, dwSize);
        pwf->cbSize     = LOWORD(dwSize) - sizeof(WAVEFORMATEX);
        pwf->wFormatTag = WAVE_FORMAT_UNKNOWN;
        
        memset(&afd,0,sizeof(ACMFORMATDETAILS));
        afd.cbStruct = sizeof(ACMFORMATDETAILS);
        afd.pwfx = pwf;
        afd.cbwfx = dwSize;
        afd.dwFormatTag = WAVE_FORMAT_UNKNOWN;
        printf("\tInput Formats (suggested):\n");
        switch(acmFormatEnum(hadActive, &afd, acmFormatEnumProc, ACM_FORMATENUMF_INPUT, 0)) {
        case ACMERR_NOTPOSSIBLE:   printf("not possible\n");   break;
        case MMSYSERR_INVALFLAG:   printf("invalid flag\n");   break;
        case MMSYSERR_INVALHANDLE: printf("invalid handle\n"); break;
        case MMSYSERR_INVALPARAM:  printf("invalid param\n");  break;
        }

        xfree(pwf);
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

void
acmInit()
{
        MMRESULT mmr;
        DWORD dwCodecs = 0, dwDrivers = 0;

        mmr = acmMetrics(NULL, ACM_METRIC_COUNT_CODECS, &dwCodecs);
        if (mmr) {
                fprintf(stderr, "ACM Codecs not available.\n");
                return;
        } else {
                fprintf(stderr, "There are %d ACM codecs\n", dwCodecs);
        }
        acmMetrics(NULL, ACM_METRIC_COUNT_DRIVERS, &dwDrivers);
        phCodecID = (HACMDRIVERID*)xmalloc(sizeof(HACMDRIVER)*dwCodecs);
        acmDriverEnum(acmDriverEnumProc, 0L, 0L);
}

#endif WIN32
