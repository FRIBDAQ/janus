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

#include <cstring>

#include "CPhysicsEventItem.h"
#include "RingBufferHandler.h"

static RingBufferHandler *ringbufferHandler = NULL;

RingBufferHandler::RingBufferHandler()
: m_SourceId(-1), m_RingName("")
{
    clearBuffer();
}

RingBufferHandler *RingBufferHandler::getInstance() {
    if (ringbufferHandler == NULL) {
        ringbufferHandler = new RingBufferHandler();
    }

    return ringbufferHandler;
}

void RingBufferHandler::setSourceID(int sourceid) {
    m_SourceId = sourceid;
}

int RingBufferHandler::getSourceID() {
    return m_SourceId;
}

void RingBufferHandler::setRingname(std::string ringname) {
    m_RingName = ringname;

    CRingBuffer *pNewRing = CRingBuffer::createAndProduce(m_RingName, RING_BUFFER_SIZE);
    m_RingBuffer.reset(pNewRing);
}

const char* RingBufferHandler::getRingname() {
    return m_RingName.c_str();
}

void RingBufferHandler::clearBuffer() {
    memset(m_Buffer, 0, RING_BUFFER_SIZE);
    m_SizeToWrite = 0;
}

void RingBufferHandler::addToBuffer(const void *buf, size_t size, size_t num) {
    memcpy(m_Buffer + m_SizeToWrite, buf, size*num);
    m_SizeToWrite += size*num;
}

void RingBufferHandler::writeToRing() {
    CPhysicsEventItem item(0, m_SourceId, 0, m_SizeToWrite + 1024);
    void *dest = item.getBodyCursor();
    memcpy(dest, m_Buffer, m_SizeToWrite);
    dest = static_cast<void *>(static_cast<uint8_t *>(dest) + m_SizeToWrite);
    item.setBodyCursor(dest);
    item.updateSize();
    
    CRingBuffer* pR = m_RingBuffer.get();
    item.commitToRing(*pR);

    clearBuffer();
}
