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

#ifndef _FRIB_H
#define _FRIB_H

#include <stddef.h>

#include "JanusC.h"

// Config file parameters (RingBuffer, SourceID, RingBufferName, RunTitle).
// Returns 1 if parname was a FRIB parameter (and marks it as a valid name).
int FRIB_ParseParam(char *parname, char *parval, Janus_Config_t *cfg);

// Runtime console commands ([u], [i], [B], [L], [N]).
// Returns 1 if the command was handled.
int FRIB_RunTimeCmd(int c);
void FRIB_PrintRunTimeMenu(void);

// Ring buffer output lifecycle. All of these are no-ops unless
// OUTFILE_RAW_DATA_RINGBUFFER is set in J_cfg.OutFileEnableMask.
void FRIB_RingOpen(int RunNumber);      // attach to the ring, latch run metadata
void FRIB_RingClose(void);              // emit END_RUN state change and detach
void FRIB_RingBegin(void);              // emit BEGIN_RUN state change
int  FRIB_RingActive(void);
void FRIB_RingWrite(const void *ptr, size_t size, size_t num);  // buffer event bytes
void FRIB_RingCommit(int isHeader);     // commit buffered bytes as one ring item

#endif
