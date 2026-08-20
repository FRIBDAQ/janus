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
#define STATS_MAX_NBRD				128
#define STATS_MAX_NCH				128

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

//****************************************************************************
// Histogram Data Structure (1 dimension)
//****************************************************************************
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


//****************************************************************************
// Time Measurement statistics
//****************************************************************************
typedef struct
{
	double mean;		// mean
	double rms;			// mean
	double sum;			// sum
	double psum;		// previous sum
	double ssum;		// sum of squares
	double pssum;		// previous sum of squares
	uint32_t ncnt;		// num of entries
	uint32_t pncnt;		// prev num of entries
} MeasStat_t;


//****************************************************************************
// struct containig the statistics amd histograms of each channel
//****************************************************************************
typedef struct Stats_t {
	int offline_bin;											// Number of bin for offline histograms
	time_t time_of_start;										// Start Time in UnixEpoch (as time_t type)
	uint64_t start_time;										// Start Time from computer (epoch ms)
	uint64_t stop_time;											// Stop Time from computer (epoch ms)
	uint64_t current_time;										// Current Time from computer (epoch ms)
	uint64_t previous_time;										// Previous Time from computer (epoch ms)
	Counter_t BuiltEventCnt;									// Counter of Built events
	double current_tstamp_us[STATS_MAX_NBRD];					// Current Time from board (oldest time stamp in us)
	double previous_tstamp_us[STATS_MAX_NBRD];					// Previous Time from board (used to calculate elapsed time => rates)
	double last_serv_tstamp_us[STATS_MAX_NBRD];					// Time stamp of the last service event
	double prev_serv_tstamp_us[STATS_MAX_NBRD];					// Time stamp of the last service event (previous value)
	uint64_t current_trgid[STATS_MAX_NBRD];						// Current Trigger ID
	uint64_t previous_trgid[STATS_MAX_NBRD];					// Previous Trigger ID (used to calculate lost triggers)
	uint64_t previous_trgid_p[STATS_MAX_NBRD];					// Previous Trigger ID (used to calculate percent of lost triggers)
	Counter_t ReadHitCnt[STATS_MAX_NBRD][STATS_MAX_NCH];		// Channel Hit Counter (total number of hit read from a picoTDC channel)
	Counter_t MatchHitCnt[STATS_MAX_NBRD][STATS_MAX_NCH];		// Channel Hit Counter (only matching hits)
	Counter_t BoardHitCnt[STATS_MAX_NBRD];						// Total Hit Counter (total number of hit read from a board)
	Counter_t TotTrgCnt[STATS_MAX_NBRD];						// Total Trigger Counter (counted in HW)
	Counter_t LostTrgCnt[STATS_MAX_NBRD];						// Lost Trigger Counter (counted in HW)
	Counter_t SupprTrgCnt[STATS_MAX_NBRD];						// Zero Suppressed Trigger Counter (counted in HW)
	Counter_t ReadTrgCnt[STATS_MAX_NBRD];						// Read Trigger Counter (counted in SW)
	Counter_t ByteCnt[STATS_MAX_NBRD];							// Byte counter (read data)
	float ProcTrgPerc[STATS_MAX_NBRD];							// Percent of processed triggers
	float LostTrgPerc[STATS_MAX_NBRD];							// Percent of lost triggers
	float ZeroSupprTrgPerc[STATS_MAX_NBRD];						// Percent of suppressed triggers (empty events)
	float BuildPerc[STATS_MAX_NBRD];							// Percent of built events
	MeasStat_t LeadMeas[STATS_MAX_NBRD][STATS_MAX_NCH];			// leading edge measurement statistics
	MeasStat_t TrailMeas[STATS_MAX_NBRD][STATS_MAX_NCH];		// trailing edge measurement statistics
	MeasStat_t ToTMeas[STATS_MAX_NBRD][STATS_MAX_NCH];			// ToT measurement statistics
	Histogram1D_t H1_Lead[STATS_MAX_NBRD][STATS_MAX_NCH];		// Leading edge time Histograms 
	Histogram1D_t H1_Trail[STATS_MAX_NBRD][STATS_MAX_NCH];		// Trailing edge time Histograms 
	Histogram1D_t H1_ToT[STATS_MAX_NBRD][STATS_MAX_NCH];		// ToT Histograms 
	//float* Staircase_file[MAX_NTRACES];						// Staircase read from file (offline)
} Stats5203_t;

#endif
