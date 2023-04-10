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

#define RING_BUFFER_SIZE 64*1024

typedef union _timestamp {
    double value;
    uint8_t element[8];
} timestamp;

class RingBufferHandler {
    public:
        RingBufferHandler();
        ~RingBufferHandler() {};

        static RingBufferHandler *getInstance();

        void setSourceID(int sourceid);
        int getSourceID();

        void setRingname(std::string ringname);
        const char *getRingname();

        void clearBuffer();
        void addToBuffer(const void *ptr, size_t size, size_t num);
        void writeToRing(bool isHeader = false);

    private:
        int m_SourceId;
        std::string m_RingName;

        uint8_t m_Buffer[RING_BUFFER_SIZE];
        uint16_t m_SizeToWrite;
        std::unique_ptr<CRingBuffer> m_RingBuffer;
};

#endif