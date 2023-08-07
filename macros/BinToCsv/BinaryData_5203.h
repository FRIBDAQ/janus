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

#ifdef Win32
#pragma once
#endif

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

#ifdef _WIN32
#include <windows.h>
#define my_sprintf sprintf_s
#else
#define  my_sprintf sprintf
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif

class t_BinaryData_5203
{
    private:
        std::streampos              t_begin, end, mb;   
        std::streamoff              t_totsize;      // dimension of the binary file in input
        std::array<std::string, 2>  t_unit = { "LSB", "ns" };    // time unit in the csv file header
        std::array<std::string, 2>  t_unit_tstamp = { "LSB", "us" };  // time unit in the csv file header for timestamp
        uint8_t                     t_force_ns;     // force the conversion of time in ns (if is in LSB)
        uint8_t                     t_time_unit;    // ToA or ToT written as int (LSB) or float (ns). 0: LSB, 1: ns
        float                       t_ToA_LSB_ns;   // ToA LSB value in ns
        float                       t_ToT_LSB_ns;   // ToT LSB value in ns
        float                       t_Tstamp_LSB_ns;    // Tstamp LSB value in ns
        uint8_t                     t_data_format;      // Version of Data format
        std::string                 t_s_data_version;   // Version of Data format as string
        std::string                 t_s_sw_version;     // Version of Janus Release
        uint16_t                    t_run_num;      // Run number of the binary file
        uint16_t                    t_brd_ver;      // Type of FERS board (5202, 5203 ...)
        uint16_t                    t_acq_mode;     // Acquisition mode
        uint8_t                     t_meas_mode;    // Measurement mode
        uint64_t                    t_start_run;    // Start of acquisition (epoch ms)
        uint16_t                    t_evt_size;     // Size of each event of binary file. It is used to guide the conversion
        uint8_t                     t_brd;          // NOT USED IN 5203
        double                      t_tstamp_d;     // Timestamp as double
        uint64_t                    t_tstamp_64;    // Timestamp as uint64
        uint64_t                    t_trigger_ID;   //
        uint16_t                    t_num_of_hit;   // Number of hit in the event, can be used as second level check
        std::vector<uint8_t>        t_brd_id;
        std::vector<uint8_t>        t_ch_id;
        std::vector<uint8_t>        t_edge;
        std::vector<uint32_t>       t_ToA_i;        // ToA as uint32
        std::vector<uint64_t>       t_ToA_64;       // ToA as uint64 (Streaming mode only)
        std::vector<uint16_t>       t_ToT_i;        // ToT as in uint16
        std::vector<double>         t_ToA_d;        // ToA as double (Streaming mode only)
        std::vector<float>          t_ToA_f;        // ToA as float
        std::vector<float>          t_ToT_f;        // ToT as float

        void Init(uint8_t force_ns, uint8_t mode);
        std::string WriteMeasMode();
        std::string WriteAcqMode();
    public:
        t_BinaryData_5203() {};
        t_BinaryData_5203(uint8_t force_ns, uint8_t mode); // constructors
        t_BinaryData_5203(std::ifstream & binfile, std::ofstream & csvfile, uint8_t force_ns);
        t_BinaryData_5203(uint8_t mode, std::ofstream & csvfile, uint8_t force_ns, uint8_t format_version, uint16_t en_bin, uint8_t toa_bin); // In the case the header is read from main
        ~t_BinaryData_5203() {}; // distructor

        void ReadHeaderBinfile(std::ifstream & binfile);
        void ComputeBinfileSize(std::ifstream & binfile);
        std::streamoff GetEventsSize() { return t_BinaryData_5203::t_totsize; };  // Return the size of the bin file containing the Events
        std::streamoff GetEventsBegin() { return std::streamoff(t_BinaryData_5203::t_begin); };  // Return the file position where the Events start
        void WriteCsvHeader(std::ofstream & csvfile);

        uint16_t ReadEvtHeader(std::ifstream & binfile);
        void ReadTmpEvt(std::ifstream & binfile);
        uint16_t ReadStreamingEvent(std::ifstream & binfile);
        uint16_t ReadCStartTMatchEvent(std::ifstream & binfile);
        void WriteTmpEvt(std::ofstream & csvfile);
        void ResetValues5203();
};

