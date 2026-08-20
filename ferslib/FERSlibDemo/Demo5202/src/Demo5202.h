/***************************************************************************//**
* \note TERMS OF USE :
*This program is free software; you can redistribute itand /or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.The user relies on the
* software, documentationand results solely at his own risk.
*******************************************************************************/

#ifndef _STATISTICS_H
#define _STATISTICS_H              // Protect against multiple inclusion

#include <stdint.h>
#ifdef linux
#include <termios.h>
#endif
//! [IncludeHeader]
#include "FERSlib.h"
//! [IncludeHeader]

#define DEMO_RELEASE_NUM   "1.1.0"


#define PARSE_CONN		0
#define PARSE_CFG		1

//****************************************************************************
// Counter Data Structure
//****************************************************************************
typedef struct
{
	uint64_t cnt;		// Current Counter value
	uint64_t pcnt;		// Previous Counter value (last update)
	uint64_t ptime;		// Last update time (ms from start time)
	double rate;		// Rate (Hz)
} Counter_t;


typedef struct
{
	char spectrum_name[100];	// spectrum name
	char title[20];				// plot title
	char x_unit[20];			// X axis unit name (e.g. keV)
	char y_unit[20];			// Y axis unit name (e.g. counts)
	uint32_t* H_data;			// pointer to the histogram data
	uint32_t Nbin;				// Number of bins (channels) in the histogram
	uint32_t H_cnt;				// Number of entries (sum of the counts in each bin)
	uint32_t H_p_cnt;			// Previous Number of entries 
	uint32_t Ovf_cnt;			// Overflow counter
	uint32_t Unf_cnt;			// Underflow counter
	uint32_t Bin_set;			// Last bin set (for MCS popruse)
	double rms;					// rms
	double mean;				// mean
	double p_rms;				// previous rms
	double p_mean;				// previous mean
	double real_time;			// real time in s
	double live_time;			// live time in s

	// calibration coefficients
	double A[4];

} Histogram1D_t;


typedef struct Stats_t {
	int offline_bin;											// Number of bin for offline histograms
	time_t time_of_start;										// Start Time in UnixEpoch (as time_t type)
	uint64_t start_time;										// Start Time from computer (epoch ms)
	uint64_t stop_time;											// Stop Time from computer (epoch ms)
	uint64_t current_time;										// Current Time from computer (epoch ms)
	uint64_t previous_time;										// Previous Time from computer (epoch ms)
	Counter_t BuiltEventCnt;									// Counter of Built events
	double current_tstamp_us[FERSLIB_MAX_NBRD];					// Current Time from board (oldest time stamp in us)
	double previous_tstamp_us[FERSLIB_MAX_NBRD];					// Previous Time from board (used to calculate elapsed time => rates)
	uint64_t current_trgid[FERSLIB_MAX_NBRD];						// Current Trigger ID
	uint64_t previous_trgid[FERSLIB_MAX_NBRD];					// Previous Trigger ID (used to calculate lost triggers)
	uint64_t previous_trgid_p[FERSLIB_MAX_NBRD];					// Previous Trigger ID (used to calculate percent of lost triggers)
	double trgcnt_update_us[FERSLIB_MAX_NBRD];					// update time of the trg counters
	double previous_trgcnt_update_us[FERSLIB_MAX_NBRD];			// Previous update time of the trg counters
	Counter_t ReadHitCnt[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];		// Channel Hit Counter (total number of hit read from a picoTDC channel)
	Counter_t ChTrgCnt[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];			// Channel Trigger counter from fast discriminators (when counting mode is enabled)
	Counter_t HitCnt[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];			// Recorded Hit counter (when timing mode is enabled)
	Counter_t PHACnt[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];			// PHA event counter HG or LG (when spectroscopy mode is enabled)
	Counter_t T_OR_Cnt[FERSLIB_MAX_NBRD];							// T-OR counter
	Counter_t Q_OR_Cnt[FERSLIB_MAX_NBRD];							// Q-OR counter
	Counter_t GlobalTrgCnt[FERSLIB_MAX_NBRD];						// Global Trigger Counter
	Counter_t LostTrg[FERSLIB_MAX_NBRD];							// Lost Trigger
	Counter_t ByteCnt[FERSLIB_MAX_NBRD];							// Byte counter (read data)
	float LostTrgPerc[FERSLIB_MAX_NBRD];							// Percent of lost triggers
	float BuildPerc[FERSLIB_MAX_NBRD];							// Percent of built events
	Histogram1D_t H1_PHA_HG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5202];		// Energy Histograms (High Gain)
	Histogram1D_t H1_PHA_LG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5202];		// Energy Histograms (Low Gain)
	Histogram1D_t H1_ToA[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];		// ToA Histograms 
	Histogram1D_t H1_ToT[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];		// ToT Histograms 
	Histogram1D_t H1_MCS[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];		// MultiChannel Scaler Histograms
	//float* Staircase_file[MAX_NTRACES];						// Staircase read from file (offline)
} Stats_t;

#endif
