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

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void RBH_setSourceID(int sourceid);
void RBH_setRingname(char *ringname);
void RBH_setRunNumber(int runNo);
void RBH_setTitle(char *title);

int RBH_getSourceID();
const char *RBH_getRingname();
int RBH_getRunNumber();
const char *RBH_getTitle();

void RBH_addToBuffer(const void *ptr, size_t size, size_t num);
void RBH_writeToRing(bool isHeader = false);
void RBH_emitStateChangeToRing(bool isBegin, bool useBarrier);

#ifdef __cplusplus
}
#endif