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

#include <cstddef>
#include <cstring>
#include <ctime>

#include "DataFormat.h"
#include "CPhysicsEventItem.h"
#include "CRingStateChangeItem.h"
#include "RingBufferHandler.h"

#include "FERS_fileheader.h"

static RingBufferHandler *ringbufferHandler = NULL;

RingBufferHandler::RingBufferHandler()
: m_SourceId(-1), m_RingName(""), m_AcqMode(0), m_TimeUnit(0), m_Title(""), m_TitleWithFileHeader("")
{
    clearBuffer();
}

void RingBufferHandler::setSourceID(int sourceid) {
    m_SourceId = sourceid;
}

void RingBufferHandler::setRingname(std::string ringname) {
    m_RingName = ringname;

    if (m_RingBuffer.get()) {
        CRingBuffer *removed = m_RingBuffer.get();
        m_RingBuffer.release();

        delete removed;
    }

    CRingBuffer *pNewRing = CRingBuffer::createAndProduce(m_RingName, RING_BUFFER_SIZE);
    m_RingBuffer.reset(pNewRing);
}

void RingBufferHandler::setRunNumber(int runnumber) {
    m_RunNumber = runnumber;
}

void RingBufferHandler::setTitle(std::string title) {
    m_Title = title;
}

RingBufferHandler *RingBufferHandler::getInstance() {
    if (ringbufferHandler == NULL) {
        ringbufferHandler = new RingBufferHandler();
    }

    return ringbufferHandler;
}

int RingBufferHandler::getSourceID() {
    return m_SourceId;
}

const char *RingBufferHandler::getRingname() {
    return m_RingName.c_str();
}

int RingBufferHandler::getRunNumber() {
    return m_RunNumber;
}

const char *RingBufferHandler::getTitle() {
    return m_Title.c_str();
}

void RingBufferHandler::clearBuffer() {
    memset(m_Buffer, 0, RING_BUFFER_SIZE);
    m_SizeToWrite = 0;
}

void RingBufferHandler::addToBuffer(const void *buf, size_t size, size_t num) {
    memcpy(m_Buffer + m_SizeToWrite, buf, size*num);
    m_SizeToWrite += size*num;
}

void RingBufferHandler::writeToRing(bool isHeader) {
    timestamp tstamp;
    FileHeader_t *fileheader;
    uint64_t tstamp_int = 0;

    if (isHeader) {
        fileheader = reinterpret_cast<FileHeader_t *>(m_Buffer);
        m_AcqMode = fileheader -> acqmode;
        m_TimeUnit = fileheader -> timeunit;
    } else {
        for (int iByte = 0; iByte < 8; iByte++) {
            tstamp.element[iByte] = m_Buffer[3 + iByte];
        }

        tstamp_int = tstamp.value*10000;
    }

    CPhysicsEventItem item(tstamp_int, m_SourceId, 0, m_SizeToWrite + 1024);

    void *dest = item.getBodyCursor();
    if (!isHeader) {
      uint16_t metadata = (uint16_t) m_AcqMode << 8 | (uint16_t) m_TimeUnit;
      memcpy(dest, &metadata, 2);
    } else {
      memcpy(dest, &m_SizeToWrite, 2);
    }

    memcpy(static_cast<void *>(static_cast<uint8_t *>(dest) + 2), m_Buffer, m_SizeToWrite);
    dest = static_cast<void *>(static_cast<uint8_t *>(dest) + 2 + m_SizeToWrite);

    item.setBodyCursor(dest);
    item.updateSize();

    CRingBuffer* pR = m_RingBuffer.get();
    item.commitToRing(*pR);

    clearBuffer();
}

void RingBufferHandler::emitStateChangeToRing(bool isBegin, bool useBarrier) {
    CRingStateChangeItem item(time(NULL), m_SourceId, (useBarrier ? (isBegin ? 1 : 2) : 0), (isBegin ? BEGIN_RUN : END_RUN), m_RunNumber, 0, time(NULL), m_Title);
    void *dest = item.getBodyCursor();
    memcpy(dest, m_TitleWithFileHeader.c_str(), RING_TITLE_BUFFER_SIZE);
    dest = static_cast<void *>(static_cast<uint8_t *>(dest) + RING_TITLE_BUFFER_SIZE);
    item.setBodyCursor(dest);
    item.updateSize();
    
    CRingBuffer* pR = m_RingBuffer.get();
    item.commitToRing(*pR);
}
