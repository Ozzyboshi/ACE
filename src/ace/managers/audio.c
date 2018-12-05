#include <ace/managers/audio.h>
#include <ace/managers/memory.h>
#include <ace/managers/log.h>
#include <ace/utils/custom.h>
#include <ace/managers/system.h>

typedef struct _tChannelControls {
	BYTE bPlayCount;
	UBYTE ubChannel;
} tChannelControls;

tChannelControls s_pControls[4];

void INTERRUPT audioIntHandler(
	UNUSED_ARG REGARG(volatile tCustom *pCustom, "a0"),
	UNUSED_ARG REGARG(volatile void *pData, "a1")
) {
	volatile tChannelControls *pCtrl = (volatile tChannelControls *)pData;
	if(pCtrl->bPlayCount != -1) {
		if(pCtrl->bPlayCount) {
			--pCtrl->bPlayCount;
		}
		else {
			audioStop(pCtrl->ubChannel);
		}
	}
}

void audioCreate(void) {
	logBlockBegin("audioCreate()");
	for(UBYTE i = 0; i < 4; ++i) {
		systemSetDma(DMAB_AUD0+i, 0);
		s_pControls[i].bPlayCount = 0;
		s_pControls[i].ubChannel = i;
		systemSetInt(INTB_AUD0+i, audioIntHandler, &s_pControls[i]);
	}
	// Disable audio filter
	g_pCiaA->pra ^= BV(1);
	logBlockEnd("audioCreate()");
}

void audioDestroy(void) {
	logBlockBegin("audioDestroy()");
	for(UBYTE i = 0; i < 4; ++i) {
		systemSetDma(DMAB_AUD0+i, 0);
		systemSetInt(INTB_AUD0+i, 0, 0);
	}
	logBlockEnd("audioDestroy()");
}

void audioPlay(
	UBYTE ubChannel, tSample *pSample, UBYTE ubVolume, BYTE bPlayCount
) {
	// Stop playback on given channel
	systemSetDma(ubChannel, 0);

	s_pControls[ubChannel].bPlayCount = bPlayCount;
	volatile struct AudChannel *pChannel = &g_pCustom->aud[ubChannel];
	pChannel->ac_ptr = (UWORD *) pSample->pData;
	pChannel->ac_len = pSample->uwLength >> 1; // word count
	pChannel->ac_vol = ubVolume;
	pChannel->ac_per = pSample->uwPeriod;

	// Now that channel regs are set, start playing
	systemSetDma(ubChannel, 1);
}

void audioStop(UBYTE ubChannel) {
	systemSetDma(ubChannel, 0);
	// Clear already fetched data so that Paula won't produce non-zero signal
	g_pCustom->aud[ubChannel].ac_dat = 0;
}

tSample *sampleCreate(UWORD uwLength, UWORD uwPeriod) {
	logBlockBegin("sampleCreate(uwLength: %hu, uwPeriod: %hu)", uwLength, uwPeriod);
	tSample *pSample = memAllocFast(sizeof(tSample));
	pSample->uwLength = uwLength;
	pSample->pData = memAllocChipClear(uwLength);
	pSample->uwPeriod = uwPeriod;
	logBlockEnd("sampleCreate()");
	return pSample;
}

tSample *sampleCreateFromFile(const char *szPath, UWORD uwSampleRateKhz) {
	systemUse();
	logBlockBegin(
		"sampleCreateFromFile(szPath: '%s', uwSampleRateKhz: %hu)",
		szPath, uwSampleRateKhz
	);
	LONG lLength = fileGetSize(szPath);
	if(lLength <= 0) {
		logWrite("ERR: File doesn't exist!\n");
		logBlockEnd("sampleCreateFromFile()");
		return 0;
	}
	// NOTE: 3546895 is for PAL, for NTSC use 3579545
	UWORD uwPeriod = (3546895 + uwSampleRateKhz/2) / uwSampleRateKhz;
	tSample *pSample = sampleCreate(lLength, uwPeriod);
	FILE *pSampleFile = fileOpen(szPath, "rb");
	fileRead(pSampleFile, pSample->pData, lLength);
	fileClose(pSampleFile);
	logBlockEnd("sampleCreateFromFile()");
	systemUnuse();
	return pSample;
}

void sampleDestroy(tSample *pSample) {
	memFree(pSample->pData, pSample->uwLength);
	memFree(pSample, sizeof(tSample));
}
