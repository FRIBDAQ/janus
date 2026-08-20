/*******************************************************************************
*
* CAEN SpA - Software Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
*\note TERMS OF USE :
*This program is free software; you can redistribute itand /or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.The user relies on the
* software, documentation and results solely at his own risk.
****************************************************************************** */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <vector>
#include <array>

// Acquisition Mode 5202
#define ACQMODE_SPECT		0x01  // Spectroscopy Mode (Energy)
#define ACQMODE_TIMING		0x02  // Timing Mode 
#define ACQMODE_TSPECT		0x03  // Spectroscopy + Timing Mode (Energy + Tstamp)
#define ACQMODE_COUNT		0x04  // Counting Mode (MCS)
#define ACQMODE_WAVE		0x08  // Waveform Mode

// Data Qualifier 5202
#define DTQ_SPECT			0x01  // Spectroscopy Mode (Energy)
#define DTQ_TIMING			0x02  // Timing Mode 
#define DTQ_COUNT 			0x04  // Counting Mode (MCS)
#define DTQ_WAVE			0x08  // Waveform Mode
#define DTQ_TSPECT			0x03  // Spectroscopy + Timing Mode (Energy + Tstamp)
#define DTQ_RTSTAMP         0x80  // Relative Timestamp

// Data Type 5202
#define LG                  0x01
#define HG                  0x02
#define TOA                 0x10
#define TOT                 0x20

class t_BinaryData
{
private:
    std::streampos                      t_begin, end, mb;
    std::streamoff                      t_evts_size;
    std::array<std::string, 2>          t_unit;         // = { "LSB", "ns" };
    std::array<std::string, 2>          t_unit_tstamp;  // = { "LSB", "us" };  // time unit in the csv file header for timestamp
    std::string                         t_s_sw_version;
    std::string                         t_s_data_version;   // Version of Data format as string
    uint8_t                             t_force_ns;
    uint16_t                            t_en_bin;
    uint8_t                             t_time_unit;    // ToA or ToT written as int (LSB) or float (ns)
    double                              t_LSB_ns;
    uint8_t                             t_data_format;  // Version of Data format
    uint8_t                             t_acq_mode;
    uint16_t                            t_run_num;      // Run number of the binary file
    uint16_t                            t_evt_size;
    uint16_t                            t_brd_ver;      // Type of FERS board (5202, 5203 ...)
    uint8_t                             t_brd;
    double                              t_tstamp;
    double                              t_rel_tstamp;   // Relative Timestamp of an external trigger
	double                              t_tref_tstamp;  // Tref Timestamp with 0.5 ns resolution for SpectTiming mode  
    uint64_t                            t_trigger_ID;
    uint64_t                            t_ch_mask;
    uint16_t                            t_num_of_hit;
    uint64_t                            t_start_run;    // Start of acquisition (epoch ms)
    std::vector<uint8_t>                t_ch_id;
    std::vector<uint8_t>                t_data_type;
    std::vector<uint16_t>               t_PHA_LG;
    std::vector<uint16_t>               t_PHA_HG;
    std::vector<uint32_t>               t_ToA_i;
    std::vector<int16_t>                t_ToT_i;
    std::vector<float>                  t_ToA_f;
    std::vector<float>                  t_ToT_f;
    std::vector<uint64_t>               t_counts;

    void Init(uint8_t force_ns, uint8_t mode);
public:
    t_BinaryData() {};
    t_BinaryData(uint8_t force_ns, uint8_t mode); // constructors
    t_BinaryData(std::ifstream& binfile, std::ofstream& csvfile, uint8_t force_ns);
    t_BinaryData(uint8_t mode, std::ofstream& csvfile, uint8_t force_ns, uint8_t format_version, uint16_t en_bin, uint8_t toa_bin); // In the case the header is read from main
    ~t_BinaryData() {}; // distructor

    void ReadHeaderBinfile(std::ifstream& binfile);
    void ComputeBinfileSize(std::ifstream& binfile);
    std::streamoff GetEventsSize() { return t_BinaryData::t_evts_size; };  // Return the size of the bin file containing the Events
    std::streamoff GetEventsBegin() { return std::streamoff(t_BinaryData::t_begin); };  // Return the file position where the Events start
    void WriteCsvHeader(std::ofstream& csvfile);

    uint16_t ReadEvtHeader(std::ifstream& binfile);
    void ReadTmpEvt(std::ifstream& binfile);
    uint16_t ReadSpectTime(std::ifstream& binfile);
    //uint16_t ReadTime(std::ifstream& binfile);
    uint16_t ReadCnts(std::ifstream& binfile);
    void WriteTmpEvt(std::ofstream& csvfile);
    void ResetValues();
};

