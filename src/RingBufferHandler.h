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

#ifndef RINGBUFFERHANDLER_H
#define RINGBUFFERHANDLER_H

#include <string>
#include <memory>

#include "CRingBuffer.h"

class RingBufferHandler {
    public:
        RingBufferHandler();
        ~RingBufferHandler() {};

        static RingBufferHandler *getInstance();

        void setSourceID(int sourceid);
        int getSourceID();

        void setRingname(std::string ringname);
        const char *getRingname();

        void writeToRing(const void *ptr, size_t size, size_t num);

    private:
        int m_SourceId;
        std::string m_RingName;

        std::unique_ptr<CRingBuffer> m_RingBuffer;
};

#endif