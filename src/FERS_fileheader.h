/******************************************************************************
    This software is Copyright by the Board of Trustees of Michigan
    State University (c) Copyright 2023.

    You may use this software under the terms of the GNU public license
    (GPL).  The terms of this license are described at:

     http://www.gnu.org/licenses/gpl.txt

     Authors:
          Genie Jhang <changj@frib.msu.edu>

          Facility for Rare Isotope Beams
          Michigan State University
          East Lansing, MI 48824-1321
******************************************************************************/

#ifndef _FERS_FILEHEADER_H_
#define _FERS_FILEHEADER_H_

typedef struct __attribute__((__packed__)) {
	uint8_t dataformat_major;
	uint8_t dataformat_minor;
	uint8_t software_major;
	uint8_t software_minor;
	uint8_t software_patch;
	uint16_t boardtype;
	uint16_t runnumber;
	uint8_t acqmode;
	uint16_t energyNCH;
	uint8_t timeunit;
	float timeconversion;
	uint64_t startacq_ms;
} FileHeader_t;

#endif
