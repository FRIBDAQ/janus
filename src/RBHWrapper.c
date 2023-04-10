
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

void RBH_writeToRing(const void *ptr, size_t size, size_t num) {
    RingBufferHandler::getInstance() -> writeToRing(ptr, size, num);
}

#ifdef __cplusplus
}
#endif