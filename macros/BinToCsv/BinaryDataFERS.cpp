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

#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <vector>
#include <chrono>
#include <ctime>

#include "BinaryDataFERS.h"

t_BinaryDataFERS::t_BinaryDataFERS(uint8_t force_ns, uint8_t mode) {
    t_BinaryDataFERS::InitFERS(force_ns, mode);
}

//t_BinaryDataFERS::t_BinaryDataFERS(uint8_t mode, std::ofstream& csvfile, uint8_t force_ns, uint8_t format_version, uint16_t en_bin, uint8_t toa_bin) { // When header is read in the main
//    // The other parameters read in the main can be passed as a map. To be implemented
//    t_BinaryDataFERS::Init(force_ns, mode);
//    t_BinaryDataFERS::WriteCsvHeader(csvfile);
//}

t_BinaryDataFERS::t_BinaryDataFERS(std::ifstream& binfile, std::ofstream& csvfile, uint8_t force_ns) {
    // binfile is already opened
    // Which values need to be set?
    t_BinaryDataFERS::InitFERS(force_ns, 0);    // 
    t_BinaryDataFERS::ComputeBinfileSizeFERS(binfile);
    t_BinaryDataFERS::ReadHeaderBinfileFERS(binfile);
    binfile.seekg(t_BinaryDataFERS::t_begin, std::ios::beg);
    if (t_brd_ver == 5202)
        t_data_5202 = t_BinaryData(binfile, csvfile, force_ns);
    else if (t_brd_ver == 5203)
        t_data_5203 = t_BinaryData_5203(binfile, csvfile, force_ns);
}

void t_BinaryDataFERS::InitFERS(uint8_t force_ns, uint8_t mode) {
    t_BinaryDataFERS::t_s_data_version = "";
    t_BinaryDataFERS::t_s_sw_version = "";
    t_brd_ver = 0;
    t_data_format = 0;
}

void t_BinaryDataFERS::ComputeBinfileSizeFERS(std::ifstream& binfile) {
    t_BinaryDataFERS::t_begin = binfile.tellg();    // Position of the reading pointer after the binfile Header
    binfile.seekg(0, std::ios::end);
    end = binfile.tellg();
    binfile.seekg(t_BinaryDataFERS::t_begin, std::ios::beg);
    t_BinaryDataFERS::t_totsize = end - t_BinaryDataFERS::t_begin;
    //t_read_size = t_begin;
}

void t_BinaryDataFERS::ReadHeaderBinfileFERS(std::ifstream& binfile) {
    uint8_t tmp_8 = 0;
    uint16_t tmp_16 = 0;

    // Read Data Format Version
    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryDataFERS::t_s_data_version = std::to_string(tmp_8) + ".";
    t_BinaryDataFERS::t_data_format = 10 * tmp_8;

    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryDataFERS::t_s_data_version += std::to_string(tmp_8);
    t_BinaryDataFERS::t_data_format += tmp_8;

    //binfile.seekg(3, std::ios::cur); // Skip software version (3 bytes)
    for (int i = 0; i < 3; ++i) {   // Get software version on 3 bytes
        binfile.read((char*)&tmp_8, sizeof(tmp_8));
        if (i != 0) t_BinaryDataFERS::t_s_sw_version += ".";
        t_BinaryDataFERS::t_s_sw_version += std::to_string(tmp_8);
    }

    if (t_BinaryDataFERS::t_data_format >= 32) { // Skip Board version (2 bytes)
        binfile.read((char*)&tmp_16, sizeof(tmp_16));
        t_BinaryDataFERS::t_brd_ver = tmp_16;
    } else
        t_brd_ver = 5202;
}


void t_BinaryDataFERS::ReadTmpEvtFERS(std::ifstream& binfile) {
    if (t_brd_ver == 5202)
        t_data_5202.ReadTmpEvt(binfile);
    else if (t_brd_ver == 5203)
        t_data_5203.ReadTmpEvt(binfile);
}

void t_BinaryDataFERS::WriteTmpEvtFERS(std::ofstream& csvfile) {
    if (t_brd_ver == 5202)
        t_data_5202.WriteTmpEvt(csvfile);
    else if (t_brd_ver == 5203)
        t_data_5203.WriteTmpEvt(csvfile);
}