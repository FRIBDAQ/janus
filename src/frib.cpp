/*
    This software is Copyright by the Board of Trustees of Michigan
    State University (c) Copyright 2023.

    You may use this software under the terms of the GNU public license
    (GPL).  The terms of this license are described at:

     http://www.gnu.org/licenses/gpl.txt

     Author:
             Genie Jhang
       Facility for Rare Isotope Beams
       Michigan State University
       East Lansing, MI 48824-1321
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

// RingBufferHandler.h (std C++ / NSCLDAQ headers) must come before JanusC.h:
// FERSlib.h and MultiPlatform.h define macros (min, lock, ...) that break
// any standard C++ header included after them
#include "RingBufferHandler.h"

#include "JanusC.h"
#include "console.h"
#include "frib.h"

extern int ValidParameterName;              // paramparser.c
int SaveRunVariables(RunVars_t RunVars);    // JanusC.c

static int ring_active = 0;

// copy parval into a fixed buffer, dropping surrounding quotes
// (param_defs.txt defaults are quoted, e.g. "janus")
static void copy_unquoted(char *dst, size_t dstsize, const char *src)
{
	size_t len = strlen(src);
	if (len >= 2 && (src[0] == '"' || src[0] == '\'') && src[len - 1] == src[0]) {
		src += 1;
		len -= 2;
	}
	if (len > dstsize - 1) len = dstsize - 1;
	memcpy(dst, src, len);
	dst[len] = '\0';
}

// --------------------------------------------------------------------------------
// Config file parameters
// --------------------------------------------------------------------------------
int FRIB_ParseParam(char *parname, char *parval, Janus_Config_t *cfg)
{
	if (strcmp(parname, "RingBuffer") == 0) {
		if (atoi(parval)) cfg->OutFileEnableMask |= OUTFILE_RAW_DATA_RINGBUFFER;
		else cfg->OutFileEnableMask &= ~OUTFILE_RAW_DATA_RINGBUFFER;
	} else if (strcmp(parname, "SourceID") == 0) {
		cfg->SourceID = atoi(parval);
	} else if (strcmp(parname, "RingBufferName") == 0) {
		copy_unquoted(cfg->RingBufferName, sizeof(cfg->RingBufferName), parval);
	} else if (strcmp(parname, "RunTitle") == 0) {
		copy_unquoted(cfg->RunTitle, sizeof(cfg->RunTitle), parval);
	} else {
		return 0;
	}
	ValidParameterName = 1;
	return 1;
}

// --------------------------------------------------------------------------------
// Runtime console commands
// --------------------------------------------------------------------------------
int FRIB_RunTimeCmd(int c)
{
	if (c == 'L') {
		memset(J_cfg.RunTitle, 0, sizeof(J_cfg.RunTitle));
		printf("\nEnter title for this run (Max. 80 characters, type \"empty\" for blank title):\n");
		if (SockConsole) {
			Con_GetString(J_cfg.RunTitle, sizeof(J_cfg.RunTitle) - 1);
			J_cfg.RunTitle[strcspn(J_cfg.RunTitle, "\r")] = 0;
		} else
			myscanf("%80[^\n]s", J_cfg.RunTitle);
		if (strcmp(J_cfg.RunTitle, "empty") == 0) {
			printf("Title will be empty!\n");
			memset(J_cfg.RunTitle, 0, sizeof(J_cfg.RunTitle));
		}
		return 1;
	}

	if (c == 'N') {
		printf("\nEnter a run number:\n");
		int runNumber = -1;
		if (!Con_GetInt(&runNumber) && runNumber > -1) {
			RunVars.RunNumber = runNumber;
			SaveRunVariables(RunVars);
		} else {
			printf("Wrong input received! No change made!\n");
			while (getchar() != '\n');
		}
		return 1;
	}

	if (c == 'i') {
		printf("\nEnter a source ID:\n");
		int sourceID = -1;
		if (!Con_GetInt(&sourceID) && sourceID > -1) {
			J_cfg.SourceID = sourceID;
		} else {
			printf("Wrong input received! No change made!\n");
			while (getchar() != '\n');
		}
		return 1;
	}

	if (c == 'B') {
		memset(J_cfg.RingBufferName, 0, sizeof(J_cfg.RingBufferName));
		printf("\nEnter output RingBuffer name (Max. 49 characters, Default 'janus'):\n");
		int len = 0;
		if (SockConsole) {
			len = Con_GetString(J_cfg.RingBufferName, sizeof(J_cfg.RingBufferName) - 1);
			J_cfg.RingBufferName[strcspn(J_cfg.RingBufferName, "\r")] = 0;
		} else
			len = myscanf("%49[^\n]s", J_cfg.RingBufferName);
		if (len == 0) {
			printf("Default RingBuffer name is used: janus\n");
			strcpy(J_cfg.RingBufferName, "janus");
		}
		return 1;
	}

	if (c == 'u') {
		if (J_cfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER) {
			printf("\nTurning OFF using RingBuffer output\n");
		} else {
			printf("\nTurning ON using RingBuffer output\n");
		}
		J_cfg.OutFileEnableMask ^= OUTFILE_RAW_DATA_RINGBUFFER;
		return 1;
	}

	return 0;
}

void FRIB_PrintRunTimeMenu(void)
{
	Con_printf("Cm", "[u] Toggle use RingBuffer output (FRIB)\n");
	Con_printf("Cm", "[i] Set source ID (FRIB)\n");
	Con_printf("Cm", "[B] Set output RingBuffer name (FRIB)\n");
	Con_printf("Cm", "[L] Set title for RingStateChangeItem (FRIB)\n");
	Con_printf("Cm", "[N] Set run number (FRIB)\n");
}

// --------------------------------------------------------------------------------
// Ring buffer output lifecycle
// --------------------------------------------------------------------------------
void FRIB_RingOpen(int RunNumber)
{
	if (ring_active) return;
	if (!(J_cfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) return;

	RingBufferHandler *rbh = RingBufferHandler::getInstance();
	rbh->setSourceID(J_cfg.SourceID);
	rbh->setRingname(strlen(J_cfg.RingBufferName) > 0 ? J_cfg.RingBufferName : "janus");
	rbh->setTitle(J_cfg.RunTitle);
	rbh->setRunNumber(RunNumber);
	ring_active = 1;
}

void FRIB_RingClose(void)
{
	if (!ring_active) return;
	RingBufferHandler::getInstance()->emitStateChangeToRing(false, true);
	ring_active = 0;
}

void FRIB_RingBegin(void)
{
	if (!ring_active) return;
	RingBufferHandler::getInstance()->emitStateChangeToRing(true, true);
}

int FRIB_RingActive(void)
{
	return ring_active;
}

void FRIB_RingWrite(const void *ptr, size_t size, size_t num)
{
	if (!ring_active) return;
	RingBufferHandler::getInstance()->addToBuffer(ptr, size, num);
}

void FRIB_RingCommit(int isHeader)
{
	if (!ring_active) return;
	RingBufferHandler::getInstance()->writeToRing(isHeader != 0);
}
