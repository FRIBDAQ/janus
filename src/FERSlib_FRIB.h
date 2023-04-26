/******************************************************************************
* 
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the 
* software, documentation and results solely at his own risk.
******************************************************************************/

/******************************************************************************
  Extracted from the original FERSlib.h included in the Janus package.

  Original package: Janus 3.0.3
  Extractor:
          Genie Jhang <changj@frib.msu.edu>

          Facility for Rare Isotope Beams
          Michigan State University
          East Lansing, MI 48824-1321
******************************************************************************/

#include <cstdint>

#ifndef _FERSLIB_FRIB_H_
#define _FERSLIB_FRIB_H_			// Protect against multiple inclusion

#define MAX_LIST_SIZE				512

//******************************************************************
// Event Data Structures
//******************************************************************
// Handling for file header
typedef struct __attribute__((__packed__)) {
	uint8_t dataformat_major;
	uint8_t dataformat_minor;
	uint8_t software_major;
	uint8_t software_minor;
	uint8_t software_patch;
	uint8_t acqmode;
	uint8_t timeunit;
	uint16_t energyNCH;
	float timeconversion;
	uint64_t startacq_ms;
} FileHeader_t;

// Spectroscopy Event (with or without timing)
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint64_t chmask;
	uint64_t qdmask;
	uint16_t energyHG[64];
	uint16_t energyLG[64];
	uint32_t tstamp[64];	// used in TSPEC mode only
	uint16_t ToT[64];		// used in TSPEC mode only
} SpectEvent_t;

// Counting Event
typedef struct {
	double tstamp_us;
	uint64_t trigger_id;
	uint64_t chmask;
	uint32_t counts[64];
	uint32_t t_or_counts;
	uint32_t q_or_counts;
} CountingEvent_t;

// List Event (timing mode only)
typedef struct {
	uint16_t nhits;
	uint8_t  channel[MAX_LIST_SIZE];
	uint32_t tstamp[MAX_LIST_SIZE];
	uint16_t ToT[MAX_LIST_SIZE];
} ListEvent_t;

#endif
