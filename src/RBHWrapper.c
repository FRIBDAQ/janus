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

#include "RingBufferHandler.h"
#include "RBHWrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

void RBH_setSourceID(int sourceid) {
    RingBufferHandler::getInstance() -> setSourceID(sourceid);
}

int RBH_getSourceID() {
    return RingBufferHandler::getInstance() -> getSourceID();
}

void RBH_setRingname(char *ringname) {
    RingBufferHandler::getInstance() -> setRingname(ringname);
}

const char *RBH_getRingname() {
    return RingBufferHandler::getInstance() -> getRingname();
}

void RBH_addToBuffer(const void *ptr, size_t size, size_t num) {
    RingBufferHandler::getInstance() -> addToBuffer(ptr, size, num);
}

void RBH_writeToRing() {
    RingBufferHandler::getInstance() -> writeToRing();
}

#ifdef __cplusplus
}
#endif