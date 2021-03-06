/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ace/managers/log.h>
#include <string.h>
#include <ace/macros.h>
#include <ace/managers/system.h>
#ifdef ACE_DEBUG

#ifndef LOG_FILE_NAME
#define LOG_FILE_NAME "game.log"
#endif

/* Globals */
tLogManager g_sLogManager = {0};

/* Functions */

/**
 * Base debug functions
 */

void _logOpen(void) {
	g_sLogManager.pFile = fileOpen(LOG_FILE_NAME, "w");
	g_sLogManager.ubIndent = 0;
	g_sLogManager.wasLastInline = 0;
	g_sLogManager.ubBlockEmpty = 1;
	g_sLogManager.ubShutUp = 0;
}

void _logPushIndent(void) {
	++g_sLogManager.ubIndent;
}

void _logPopIndent(void) {
	--g_sLogManager.ubIndent;
}

void _logWrite(char *szFormat, ...) {
	if(g_sLogManager.ubShutUp) {
		return;
	}
	if (!g_sLogManager.pFile) {
		return;
	}

	g_sLogManager.ubBlockEmpty = 0;
	if (!g_sLogManager.wasLastInline) {
		UBYTE ubLogIndent = g_sLogManager.ubIndent;
		while (ubLogIndent--) {
			fileWrite(g_sLogManager.pFile, "\t", 1);
		}
	}

	g_sLogManager.wasLastInline = szFormat[strlen(szFormat) - 1] != '\n';

	va_list vaArgs;
	va_start(vaArgs, szFormat);
	fileVaPrintf(g_sLogManager.pFile, szFormat, vaArgs);
	va_end(vaArgs);

	fileFlush(g_sLogManager.pFile);
}

void _logClose(void) {
	logWrite("Log closed successfully");
	if (g_sLogManager.pFile) {
		fileClose(g_sLogManager.pFile);
	}
	g_sLogManager.pFile = 0;
}

/**
 * Extended debug functions
 */

// Log blocks
void _logBlockBegin(char *szBlockName, ...) {
	if(g_sLogManager.ubShutUp) {
		return;
	}
	systemUse();
	char szFmtBfr[512];
	char szStrBfr[1024];
	// make format string
	strcpy(szFmtBfr, "Block begin: ");
	strcat(szFmtBfr, szBlockName);
	strcat(szFmtBfr, "\n");

	va_list vaArgs;
	va_start(vaArgs, szBlockName);
	vsprintf(szStrBfr,szFmtBfr,vaArgs);
	va_end(vaArgs);

	logWrite(szStrBfr);
	g_sLogManager.pTimeStack[g_sLogManager.ubIndent] = timerGetPrec();
	logPushIndent();
	g_sLogManager.ubBlockEmpty = 1;
	systemUnuse();
}

void _logBlockEnd(char *szBlockName) {
	if(g_sLogManager.ubShutUp) {
		return;
	}
	systemUse();
	logPopIndent();
	timerFormatPrec(
		g_sLogManager.szTimeBfr,
		timerGetDelta(
			g_sLogManager.pTimeStack[g_sLogManager.ubIndent],
			timerGetPrec()
		)
	);
	if(g_sLogManager.ubBlockEmpty) {
		// empty block - collapse to single line
		g_sLogManager.wasLastInline = 1;
		fileSeek(g_sLogManager.pFile, -1, SEEK_CUR);
		logWrite("...OK, time: %s\n", g_sLogManager.szTimeBfr);
	}
	else {
		logWrite("Block end: %s, time: %s\n", szBlockName, g_sLogManager.szTimeBfr);
	}
	g_sLogManager.ubBlockEmpty = 0;
	systemUnuse();
}

// Average logging
/**
 *
 */
tAvg *_logAvgCreate(char *szName, UWORD uwAllocCount) {
	tAvg *pAvg = memAllocFast(sizeof(tAvg));
	pAvg->szName = szName;
	pAvg->uwAllocCount = uwAllocCount;
	pAvg->uwCurrDelta = 0;
	pAvg->uwUsedCount = 0;
	pAvg->pDeltas = memAllocFast(uwAllocCount*sizeof(ULONG));
	pAvg->ulMin = 0xFFFFFFFF;
	pAvg->ulMax = 0;
	return pAvg;
}

/**
 *
 */
void _logAvgDestroy(tAvg *pAvg) {
	logAvgWrite(pAvg);
	memFree(pAvg->pDeltas, pAvg->uwAllocCount*sizeof(ULONG));
	memFree(pAvg, sizeof(tAvg));
}

/**
 *
 */
void _logAvgBegin(tAvg *pAvg) {
	pAvg->ulStartTime = timerGetPrec();
}

/**
 *
 */
void _logAvgEnd(tAvg *pAvg) {
	// Calculate timestamp
	pAvg->pDeltas[pAvg->uwCurrDelta] = timerGetDelta(pAvg->ulStartTime, timerGetPrec());
	// Update min/max
	if(pAvg->pDeltas[pAvg->uwCurrDelta] > pAvg->ulMax) {
		pAvg->ulMax = pAvg->pDeltas[pAvg->uwCurrDelta];
	}
	if(pAvg->pDeltas[pAvg->uwCurrDelta] < pAvg->ulMin) {
		pAvg->ulMin = pAvg->pDeltas[pAvg->uwCurrDelta];
	}
	++pAvg->uwCurrDelta;
	// Roll
	if(pAvg->uwCurrDelta >= pAvg->uwAllocCount) {
		pAvg->uwCurrDelta = 0;
	}
	pAvg->uwUsedCount = MIN(pAvg->uwAllocCount, pAvg->uwUsedCount + 1);
}

/**
 *
 */
void _logAvgWrite(tAvg *pAvg) {
	ULONG ulAvg = 0;
	char szAvg[15];
	char szMin[15];
	char szMax[15];

	if(!pAvg->uwUsedCount) {
		logWrite("Avg %s: No measures taken!\n", pAvg->szName);
		return;
	}
	// Calculate average time
	for(UWORD i = pAvg->uwUsedCount; i--;) {
		ulAvg += pAvg->pDeltas[i];
	}
	ulAvg /= pAvg->uwUsedCount;

	// Display info
	timerFormatPrec(szAvg, ulAvg);
	timerFormatPrec(szMin, pAvg->ulMin);
	timerFormatPrec(szMax, pAvg->ulMax);
	logWrite("Avg %s: %s, min: %s, max: %s\n", pAvg->szName, szAvg, szMin, szMax);
}

#endif // ACE_DEBUG
