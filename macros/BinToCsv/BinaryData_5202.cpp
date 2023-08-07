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

#include "BinaryData_5202.h"

// Read (jump) the header, 
// till 3.0:
//      16 bits (data format version) + 24 bits (software) + 8 bits (Acq Mode) + 64 bits (Start Acquisition)
// after 3.1:
//      16 bits (data format version) + 24 bits (software) + 8 bits (Acq Mode) + 16 bits (EnHisto bin) + 8 bits (Time in LSB or ns) + 32 bits ( value of LSB in ns) + 64 bits (Start Acquisition)
 

t_BinaryData::t_BinaryData(uint8_t force_ns, uint8_t mode) {
    t_BinaryData::Init(force_ns, mode);
}

t_BinaryData::t_BinaryData(uint8_t mode, std::ofstream& csvfile, uint8_t force_ns, uint8_t format_version, uint16_t en_bin, uint8_t toa_bin) { // When header is read in the main
    // The other parameters read in the main can be passed as a map. To be implemented
    t_BinaryData::Init(force_ns, mode);
    t_BinaryData::WriteCsvHeader(csvfile);
}

t_BinaryData::t_BinaryData(std::ifstream& binfile, std::ofstream& csvfile, uint8_t force_ns) {
    // binfile is already opened
    // Which values need to be set?
    t_BinaryData::Init(force_ns, 0);    // 
    t_BinaryData::ReadHeaderBinfile(binfile);
    t_BinaryData::ComputeBinfileSize(binfile);
    t_BinaryData::WriteCsvHeader(csvfile);
}

void t_BinaryData::Init(uint8_t force_ns, uint8_t mode) {
    t_BinaryData::t_force_ns = force_ns;
    t_BinaryData::t_LSB_ns = 0.5;
    t_BinaryData::t_data_format = 0;
    t_BinaryData::t_acq_mode = mode;
    t_BinaryData::t_evt_size = 0;
    t_BinaryData::t_brd = 0;
    t_BinaryData::t_tstamp = 0;
    t_BinaryData::t_trigger_ID = 0;
    t_BinaryData::t_ch_mask = 0;
    t_BinaryData::t_num_of_hit = 0;
}

void t_BinaryData::ReadHeaderBinfile(std::ifstream& binfile) {
    uint8_t tmp_8 = 0;
    uint16_t tmp_16 = 0;
    uint64_t tmp_64 = 0;
    float tmp_f = 0;

    // Skip BinFile Header size
    //binfile.seekg(1, std::ios::cur);

    // Read Data Format Version
    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryData::t_s_data_version = std::to_string(tmp_8) + ".";
    t_BinaryData::t_data_format = 10 * tmp_8;
    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryData::t_data_format += tmp_8;
    t_BinaryData::t_s_data_version += std::to_string(tmp_8);

    //binfile.seekg(3, std::ios::cur); // Skip software version (3 bytes)
    for (int i = 0; i < 3; ++i) {
        binfile.read((char*)&tmp_8, sizeof(tmp_8));
        if (i != 0) t_BinaryData::t_s_sw_version += ".";
        t_BinaryData::t_s_sw_version += std::to_string(tmp_8);
    }

    if (t_BinaryData::t_data_format >= 32) { // Skip Board version (2 bytes)
        binfile.read((char*)&tmp_16, sizeof(tmp_16));
        t_BinaryData::t_brd_ver = tmp_16;
    }

    if (t_BinaryData::t_data_format >= 32) {
        binfile.read((char*)&tmp_16, sizeof(tmp_16));
        t_BinaryData::t_run_num = tmp_16;
    }

    binfile.read((char*)&tmp_8, sizeof(tmp_8)); // Read acq_mode
    t_BinaryData::t_acq_mode = tmp_8;

    if (t_BinaryData::t_data_format >= 31) { // From V3.1 the header include OutFileUnit(8bits), EN_BIN (16bits), LSB_ns (32bits, float) 
        binfile.read((char*)&tmp_16, sizeof(tmp_16)); // EnBin
        t_BinaryData::t_en_bin = tmp_16;
        binfile.read((char*)&tmp_8, sizeof(tmp_8)); // OutFileUnit - Time in LSB or ns
        t_BinaryData::t_time_unit = tmp_8;
        binfile.read((char*)&tmp_f, sizeof(tmp_f)); // Value of LSB in ns
        t_BinaryData::t_LSB_ns = tmp_f;
    }
    binfile.read((char*)&tmp_64, sizeof(tmp_64));
    t_BinaryData::t_start_run = tmp_64;
    //binfile.seekg(8, std::ios::cur); // Skip 64 bits of the Start acq timestamp, here in all the version
}

void t_BinaryData::ComputeBinfileSize(std::ifstream& binfile) {
    t_begin = binfile.tellg();    // Position of the reading pointer after the binfile Header
    binfile.seekg(0, std::ios::end);
    end = binfile.tellg();
    binfile.seekg(t_begin, std::ios::beg);
    t_totsize = end - t_begin;
    //t_read_size = t_begin;
}

void t_BinaryData::WriteCsvHeader(std::ofstream& csvfile) {
    auto tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(t_BinaryData::t_start_run));
    // convert time point to local time structure
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::string date = std::asctime(std::localtime(&tt));

    date.erase(std::remove(date.begin(), date.end(), '\n'), date.cend());

    uint8_t time_unit = t_time_unit || t_force_ns;
    if (t_BinaryData::t_data_format >= 32) 
        csvfile << "Janus " << t_brd_ver << " Release " << t_s_sw_version << "\n";

    if (t_BinaryData::t_acq_mode == ACQMODE_SPECT) {
        csvfile << "Spect mode\n";
        if (t_BinaryData::t_data_format >= 31)
            csvfile << "Energy Histo Bin Number = " << t_en_bin << "\n";
        if (t_BinaryData::t_data_format >=32) 
            csvfile << "Run " << t_run_num << " started at " << date << "\n";

        csvfile << "TStamp,Trg_Id,Board_Id,ChannelMask,CH_Id,Data_type,PHA_LG,PHA_HG\n";
    }
    if (t_BinaryData::t_acq_mode == ACQMODE_TIMING) {
        csvfile << "Timing mode\n";
        if (t_BinaryData::t_data_format >= 31) {
            csvfile << "ToA/ToT LSB value = " << t_LSB_ns << " ns \n";
            csvfile << "Time unit = " << t_BinaryData::t_unit[t_time_unit] << " \n";
        }
        if (t_BinaryData::t_data_format >= 32)
            csvfile << "Run " << t_run_num << " started at " << date << "\n";

        csvfile << "TStamp,Board_Id,Num_hits,CH_Id,Data_type,ToA_" << t_BinaryData::t_unit[t_time_unit] << ",ToT_" << t_BinaryData::t_unit[t_time_unit] << " \n";
    }
    if (t_BinaryData::t_acq_mode == ACQMODE_TSPECT) {
        csvfile << "Spect-Timing mode\n";
        if (t_BinaryData::t_data_format >= 31) {
            csvfile << "Energy Histo Num Bin = " << t_en_bin << "\n";
            csvfile << "ToA/ToT LSB value = " << t_LSB_ns << " ns \n";
            csvfile << "Time unit = " << t_BinaryData::t_unit[t_time_unit] << " \n";
        }
        if (t_BinaryData::t_data_format >= 32)
            csvfile << "Run " << t_run_num << " started at " << date << "\n";

        csvfile << "TStamp,Trg_Id,Board_Id,ChannelMask,CH_Id,Data_type,PHA_LG,PHA_HG,ToA_" << t_BinaryData::t_unit[t_time_unit] << ",ToT_" << t_BinaryData::t_unit[t_time_unit] << " \n";
    }
    if (t_BinaryData::t_acq_mode == ACQMODE_COUNT) {
        csvfile << "Counting mode\n";
        if (t_BinaryData::t_data_format >= 32)
            csvfile << "Run " << t_run_num << " started at " << date << "\n";

        csvfile << "TStamp,Trg_Id,Board_Id,ChannelMask,CH_Id,Counts\n";
    }
}

void t_BinaryData::ReadEvtHeader(std::ifstream& binfile) {
    binfile.read((char*)&t_evt_size, sizeof(t_evt_size));
    binfile.read((char*)&t_brd, sizeof(t_brd));
    binfile.read((char*)&t_tstamp, sizeof(t_tstamp));
}

void t_BinaryData::ReadTmpEvt(std::ifstream& binfile) {
    ReadEvtHeader(binfile);
    uint16_t myrsize = t_evt_size;

    if (t_acq_mode != 2) {
        binfile.read((char*)&t_trigger_ID, sizeof(t_trigger_ID));
        binfile.read((char*)&t_ch_mask, sizeof(t_ch_mask));
        myrsize -= 216 / 8;  // 64*3 + 16 + 8
    } else {
        binfile.read((char*)&t_num_of_hit, sizeof(uint16_t));
        myrsize -= 104 / 8;  // 64 + 16*2 + 8
    }

    while (myrsize > 0) {
        uint16_t msize = 0;
        if (t_acq_mode & 0x03) // Spect Or Time
            msize = t_BinaryData::ReadSpectTime(binfile);
        if (t_acq_mode == 4) // Count mode
            msize = t_BinaryData::ReadCnts(binfile);
        if (t_acq_mode != 2) // Number of hits (Chs firing) read in Spect and Count mode, like in time mode (DNIN: is it useful?)
            ++t_num_of_hit;
        myrsize -= msize;
    }
}

uint16_t t_BinaryData::ReadSpectTime(std::ifstream& binfile) {
    uint8_t  tmp_u8;
    uint16_t tmp_u16;
    uint32_t tmp_u32;
    float tmp_f;

    binfile.read((char*)&tmp_u8, sizeof(uint8_t));
    t_ch_id.push_back(tmp_u8);
    binfile.read((char*)&tmp_u8, sizeof(uint8_t));
    t_data_type.push_back(tmp_u8);

    uint16_t mysize = 2;
    if (t_data_type.back() & 0x01) {
        binfile.read((char*)&tmp_u16, sizeof(uint16_t));
        t_PHA_LG.push_back(tmp_u16);
        mysize += 2;
    } else t_PHA_LG.push_back(0);

    if (t_data_type.back() & 0x02) {
        binfile.read((char*)&tmp_u16, sizeof(uint16_t));
        t_PHA_HG.push_back(tmp_u16);
        mysize += 2;
    } else t_PHA_HG.push_back(0);

    if (t_data_type.back() & 0x10) {
        if (t_BinaryData::t_time_unit) { // Default is 0. If ver > 3.1 it can be 1, that means time is given as float
            binfile.read((char*)&tmp_f, sizeof(tmp_f));
            t_ToA_f.push_back(tmp_f);
            mysize += 4;
        } else {
            binfile.read((char*)&tmp_u32, sizeof(uint32_t));
            t_ToA_i.push_back(tmp_u32);
            mysize += 4;
        }
    } else {    // Set to 0 both vectors instead of do another 'if'
        t_ToA_i.push_back(0);
        t_ToA_f.push_back(0);
    }

    if (t_data_type.back() & 0x20) {
        if (t_BinaryData::t_time_unit) { // Default is 0. If ver > 3.1 it can be 1, that means time is given as float
            binfile.read((char*)&tmp_f, sizeof(tmp_f));
            t_ToT_f.push_back(tmp_f);
            mysize += 4;
        } else {
            binfile.read((char*)&tmp_u16, sizeof(uint16_t));
            t_ToT_i.push_back(tmp_u16);
            mysize += 2;
        }
    } else {
        t_ToT_i.push_back(0);
        t_ToT_f.push_back(0);
    }

    // Due to a bug in JanusC, in timing mode the size of the event
    // always summed ToA and ToT till data format version 3.0, so here the size was 'size+=4+2' always.
    // The correct size is computed from data format version 3.1
    if (t_data_format < 31 && ((t_acq_mode & 0x0F) == 0x02))    // Patch for back compatibility
        mysize = 8;

    return mysize;
}

uint16_t t_BinaryData::ReadCnts(std::ifstream& binfile) {
    uint8_t  tmp_u8;
    uint32_t tmp_u32;
    uint16_t mysize;

    binfile.read((char*)&tmp_u8, sizeof(uint8_t));
    t_ch_id.push_back(tmp_u8);
    binfile.read((char*)&tmp_u32, sizeof(uint32_t));
    t_counts.push_back(tmp_u32);
    //++i;
    mysize = 5; // 40 bits->5 bytes

    return mysize;
}

void t_BinaryData::WriteTmpEvt(std::ofstream& csvfile) {
    float time_factor = 1; // Conversion factor from LSB to ns. It changes if force_ns is 1
    if (t_force_ns)
        time_factor = t_LSB_ns;

    //std::string evt_header = std::to_string(t_brd) + "," + std::to_string(t_tstamp);
    std::string evt_header = std::to_string(t_tstamp);
    if (t_acq_mode != ACQMODE_TIMING) {
        evt_header += "," + std::to_string(t_trigger_ID) + "," + std::to_string(t_brd) + ","; // +std::to_string(ch_mask);
        char tmp[50]; // print mask
        my_sprintf(tmp, "0x%" PRIx64, t_ch_mask);
        evt_header += tmp;
    } else {
        evt_header += "," + std::to_string(t_brd) + "," + std::to_string(t_BinaryData::t_num_of_hit);
    }

    for (uint32_t i = 0; i < t_ch_id.size(); ++i) {
        //if (!((ch_mask >> i) & 0x1)) 
        //    continue;
        std::string s_data;
        char tmp[50];
        if (t_acq_mode & ACQMODE_TSPECT) { // Spect (b01) Or Time (b10)
            my_sprintf(tmp, "0x%" PRIx8, t_data_type.at(i));
            s_data = "," + std::to_string(t_ch_id.at(i)) + "," + tmp;
            if (t_data_type.at(i) & LG)
                s_data += "," + std::to_string(t_PHA_LG.at(i));
            if (t_data_type.at(i) & HG)
                s_data += "," + std::to_string(t_PHA_HG.at(i));
            if (t_data_type.at(i) & TOA) {
                if (t_time_unit)
                    s_data += "," + std::to_string(t_ToA_f.at(i));
                else
                    s_data += "," + std::to_string(time_factor * t_ToA_i.at(i));
            }
            if (t_data_type.at(i) & TOT) {
                if (t_time_unit)
                    s_data += "," + std::to_string(t_ToT_f.at(i));
                else
                    s_data += "," + std::to_string(time_factor * t_ToT_i.at(i));
            }
        }
        if (t_acq_mode == ACQMODE_COUNT)  // Count mode
            s_data += "," + std::to_string((int)t_ch_id.at(i)) + "," + std::to_string((int)t_counts.at(i));
        
        csvfile << evt_header << s_data << "\n";
    }

    //********************************************************
    ResetValues();
}

void t_BinaryData::ResetValues() {
    t_num_of_hit = 0;
    if (t_ch_id.size()) t_ch_id.clear();
    if (t_data_type.size()) t_data_type.clear();
    if (t_PHA_LG.size()) t_PHA_LG.clear();
    if (t_PHA_HG.size()) t_PHA_HG.clear();
    if (t_ToA_i.size()) t_ToA_i.clear();
    if (t_ToT_i.size()) t_ToT_i.clear();
    if (t_ToA_f.size()) t_ToA_f.clear();
    if (t_ToT_f.size()) t_ToT_f.clear();
    if (t_counts.size()) t_counts.clear();
}
