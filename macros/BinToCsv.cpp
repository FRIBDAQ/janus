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
* software, documentationand results solely at his own risk.
****************************************************************************** */

//#include <stdio.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <string.h>
//#include <stdbool.h>
//#include <stdarg.h>
//#include <ctype.h>
#include <cstdint>
#include <cstdio>
#include <cinttypes>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define my_sprintf sprintf_s
#else
#define  my_sprintf sprintf
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#endif


/*********************************************************************************
*                           Class definition
**********************************************************************************/
/* 
* The Class can read the output binary file(s) from FERS A5202, and it will
* developed to interface with the output of all the 5200 board family.
* 
*/

// Read (jump) the header, 
// till 3.0:
//      16 bits (data format version) + 24 bits (software) + 8 bits (Acq Mode) + 64 bits (Start Acquisition)
// after 3.1:
//      16 bits (data format version) + 24 bits (software) + 8 bits (Acq Mode) + 16 bits (EnHisto bin) + 8 bits (Time in LSB or ns) + 32 bits ( value of LSB in ns) + 64 bits (Start Acquisition)

class t_BinaryData
{
private:
    std::streampos          t_begin, end, mb;
    std::streamoff          t_totsize;
    std::string             t_unit[2] = { "LSB", "ns" };
    uint8_t                 t_force_ns;
    uint16_t                t_en_bin;
    uint8_t                 t_time_unit;    // ToA or ToT written as int (LSB) or float (ns)
    float                   t_LSB_ns;
    uint8_t                 t_data_format;  // Version of Data format
    uint8_t                 t_acq_mode;
    uint16_t                t_evt_size;
    uint8_t                 t_brd;
    double                  t_tstamp;
    uint64_t                t_trigger_ID;
    uint64_t                t_ch_mask;
    uint16_t                t_num_of_hit;
    std::vector<uint8_t>    t_ch_id;
    std::vector<uint8_t>    t_data_type;
    std::vector<uint16_t>   t_PHA_LG;
    std::vector<uint16_t>   t_PHA_HG;
    std::vector<uint32_t>   t_ToA_i;
    std::vector<uint16_t>   t_ToT_i;
    std::vector<float>      t_ToA_f;
    std::vector<float>      t_ToT_f;
    std::vector<uint32_t>   t_counts;

    void Init(uint8_t force_ns, uint8_t mode);
public:
    t_BinaryData() {};
    t_BinaryData(uint8_t force_ns, uint8_t mode); // constructors
    t_BinaryData(std::ifstream& binfile, std::ofstream& csvfile, uint8_t force_ns);
    t_BinaryData(uint8_t mode, std::ofstream& csvfile, uint8_t force_ns, uint8_t format_version, uint16_t en_bin, uint8_t toa_bin); // In the case the header is read from main
    ~t_BinaryData() {}; // distructor
    
    void ReadHeaderBinfile(std::ifstream& binfile);
    void ComputeBinfileSize(std::ifstream& binfile);
    std::streamoff GetEventsSize() { return t_BinaryData::t_totsize; };  // Return the size of the bin file containing the Events
    std::streamoff GetEventsBegin() { return std::streamoff(t_BinaryData::t_begin); };  // Return the file position where the Events start
    void WriteCsvHeader(std::ofstream& csvfile);

    void ReadEvtHeader(std::ifstream& binfile);
    void ReadTmpEvt(std::ifstream& binfile);
    uint16_t ReadSpectTime(std::ifstream& binfile);
    //uint16_t ReadTime(std::ifstream& binfile);
    uint16_t ReadCnts(std::ifstream& binfile);
    void WriteTmpEvt(std::ofstream& csvfile);
    void ResetValues();
};

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
    uint32_t tmp_32 = 0;
    float tmp_f = 0;
    // Read Data Format Version
    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryData::t_data_format = 10 * tmp_8;
    binfile.read((char*)&tmp_8, sizeof(tmp_8));
    t_BinaryData::t_data_format += tmp_8;
    
    binfile.seekg(3, std::ios::cur); // Skip software version (3 bytes)

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
    binfile.seekg(8, std::ios::cur); // Skip 64 bits of the Start acq timestamp, here in all the version
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
    if (t_BinaryData::t_acq_mode == 1) {
        csvfile << "Spect mode\n";
        if (t_BinaryData::t_data_format >= 31)
            csvfile << "PHA Histo Bin Number = " << t_en_bin << "\n";
        csvfile << "Board_Id,TStamp,Trg_Id,ChannelMask,CH_Id,data_type,PHA_LG,PHA_HG\n";
    }
    if (t_BinaryData::t_acq_mode == 2) {
        csvfile << "Timing mode\n";
        if (t_BinaryData::t_data_format >= 31) {
            csvfile << "ToA/ToT LSB value = " << t_LSB_ns << " ns \n";
            csvfile << "Time unit = " << t_BinaryData::t_unit[t_force_ns] << " \n";
        }
        csvfile << "Board_Id,TStamp,num_hits,CH_Id,data_type,ToA_" << t_BinaryData::t_unit[t_force_ns] << ",ToT_" << t_BinaryData::t_unit[t_force_ns] << " \n";
    }
    if (t_BinaryData::t_acq_mode == 3) {
        csvfile << "Spect-Timing mode\n";
        if (t_BinaryData::t_data_format >= 31) {
            csvfile << "En_Histo_Num_Bin = " << t_en_bin << "\n";
            csvfile << "ToA/ToT LSB value = " << t_LSB_ns << " ns \n";
            csvfile << "Time unit = " << t_BinaryData::t_unit[t_force_ns] << " \n";
        }

        csvfile << "Board_Id,TStamp,Trg_Id,ChannelMask,CH_Id,data_type,PHA_LG,PHA_HG,ToA_" << t_BinaryData::t_unit[t_force_ns] << ",ToT_" << t_BinaryData::t_unit[t_force_ns] << " \n";
    }
    if (t_BinaryData::t_acq_mode == 4) {
        csvfile << "Counting mode\n";
        csvfile << "Board_Id,TStamp,Trg_Id,ChannelMask,CH_Id,Counts\n";
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
    }
    else {
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
        }
        else {
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
        }
        else {
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

    std::string evt_header = std::to_string(t_brd) + "," + std::to_string(t_tstamp);
    if (t_acq_mode != 2) {
        evt_header += "," + std::to_string(t_trigger_ID) + ","; // +std::to_string(ch_mask);
        char tmp[50]; // print mask
        my_sprintf(tmp, "0x%" PRIx64, t_ch_mask);
        evt_header += tmp;
    }
    else {
        evt_header += "," + std::to_string(t_BinaryData::t_num_of_hit);
    }
    csvfile << evt_header;

    for (uint32_t i = 0; i < t_ch_id.size(); ++i) {
        //if (!((ch_mask >> i) & 0x1)) 
        //    continue;
        char tmp[50];
        if (t_acq_mode & 0x03) { // Spect (b01) Or Time (b10)
            my_sprintf(tmp, "0x%" PRIx8, t_data_type.at(i));
            csvfile << "," << (int)t_ch_id.at(i) << "," << tmp; //  (int)data_type[i];
            if (t_data_type.at(i) & 0x01)
                csvfile << "," << t_PHA_LG.at(i);
            if (t_data_type.at(i) & 0x02)
                csvfile << "," << t_PHA_HG.at(i);
            if (t_data_type.at(i) & 0x10) {
                if (t_time_unit)
                    csvfile << "," << t_ToA_f.at(i);
                else
                    csvfile << "," << time_factor * t_ToA_i.at(i);
            }
            if (t_data_type.at(i) & 0x20) {
                if (t_time_unit)
                    csvfile << "," << t_ToT_f.at(i);
                else
                    csvfile << "," << time_factor * t_ToT_i.at(i);
            }
        }
        if (t_acq_mode == 4)  // Count mode
            csvfile << "," << (int)t_ch_id.at(i) << "," << (int)t_counts.at(i);
    }
    csvfile << "\n";
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


/*************************************************************************************
*                               FUNCTIONs
**************************************************************************************/
//template <class T> void getevent(T t, std::ifstream& binfile, std::ofstream& csvfile) {
//    t.ReadEvt(binfile);
//    t.WriteCSV(csvfile);
//}

int FileList(std::string flist, std::vector<std::string>& list) {   // Get the filenames from a list file
    std::string tmp;
    std::ifstream lfile;
    lfile.open(flist);
    if (!lfile.is_open()) {
        std::cout << flist << " file not found. Exiting ...\n";
        return -1;
    }

    while (lfile >> tmp)
        list.push_back(tmp);

    return 0;
}

void usage() {
    std::cout << "BinToCsv usage:" << std::endl;
    std::cout << "\t -h or --help: print this message and quit" << std::endl;
    std::cout << "\t --ns: force the ToA/ToT unit to 'ns'" << std::endl;
    std::cout << "\t --lfile: file containing a list of binary files to convert" << std::endl;
    std::cout << "\t --bfile: list of the binary files to convert. I.e.: --bfile Run1_list.dat Run2_list.dat Run2_list.dat ..." << std::endl;
    std::cout << std::endl << std::endl;
    //std::cout << "If no filename are provided, you are asked to provide it manually."
}

/*************************************************************************************
*                               MAIN
**************************************************************************************/
int main(int argc, char* argv[]) 
{
    std::string file_name;
    std::string csv_file_name;
    uint8_t tmp_version = 0;
    uint8_t data_version = 0;
    uint8_t next_p = 1;
    uint8_t force_ns = 0;
    uint16_t en_histo_bin = 0;
    std::streampos begin, end, mb;
    std::streamoff totsize, read_size;
    int8_t res = 0;

    std::vector<std::string> filenames;
    std::vector<std::string> not_converted;

    // For Debug
    //const int default_argc = 8;
    //std::string const argv[] = { "-", "-ns", "--bfile", "Run1_list.dat", "Run11_list.dat", "--ns", "--lfile", "unaprovola.list"};
    //int argc = default_argc;
 
    // Check input parameters - getopt is a possible improvement
    for (int p = 1; p < argc; ++p) {
        std::string arg = argv[p];
        if (arg == "-h" || arg == "--help") {
            usage();
            exit(-1);
        }
        if (arg == "--ns") {
            force_ns = 1;
        }
        if (arg == "--lfile") {
            std::string list_file = argv[p + 1];
            res = FileList(list_file, filenames);
        }
        if (arg == "--bfile") {
            arg = "";
            //++p;
            while (++p < argc) {
                arg = argv[p];
                if (arg.find("--") != std::string::npos) {
                    --p;
                    break;
                }
                filenames.push_back(arg);
                //++p;
            }
        }
    }
 
    if (res == -1)  // File list not found, since only 1 is provided the program stopd
        exit(-1);
    if (filenames.size() == 0 || argc <= 1) { // Print the help menu if no file is given
        usage();
        exit(-1);
    }

    // For Debug
    // Ask for file(s) to convert if no binfile is provided - It is useful for debug
    //if (filenames.size() == 0){
    //    std::string s_force_ns = "";
    //    while (true) {
    //        std::cout << "Do you want to force the ToA/ToT unit to ns? [y][n]: ";
    //        std::cin >> s_force_ns;
    //        if (s_force_ns == "y") {
    //            force_ns = 1;
    //            break;
    //        }
    //        else if (s_force_ns == "n")
    //            break;
    //    }
    //    std::cout << "How many file(s) do you want to convert?: ";
    //    int nfile = 0;
    //    std::cin >> nfile;
    //    std::cout << "\nInsert the binary file(s) to convert (-Lfilename if file is a list)\n";
    //    for (int i = 0; i < nfile; ++i) {
    //        std::cout << "\tFile " << i + 1 << ":";
    //        std::cin >> file_name;
    //        if (file_name.find("-L") == 0)
    //            res |= FileList(file_name, filenames);
    //        else
    //            filenames.push_back(file_name);
    //    }
    //}

    // Debug
    //filenames.push_back("C:\\Users\\dninci\\source\\repos\\janus\\bin\\DataFiles\\Run10_list.dat");
    //filenames.push_back("C:\\Users\\dninci\\source\\repos\\janus\\bin\\DataFiles\\Run11_list.dat");
    //filenames.push_back("C:\\Users\\dninci\\source\\repos\\janus\\bin\\DataFiles\\Run12_list.dat");
    //filenames.push_back("C:\\Users\\dninci\\source\\repos\\janus\\bin\\DataFiles\\Run18_list.dat");
    //filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run101_list.dat");

    // Define the binfile to convert and the csvfile
    std::ifstream to_convert;
    std::ofstream f_converted;

    for (uint32_t i = 0; i < filenames.size(); ++i) {  
        to_convert.open(filenames.at(i), std::ios::binary);
        if (!to_convert.is_open()) {    // Skipping to the next file if any
            std::cout << "File " << filenames.at(i) << " not found!\nMove to the next one ..." << std::endl; // std::flush;;
            not_converted.push_back(filenames.at(i));
            res |= -1;
            continue;
        }
        else
            std::cout << "Opening file " << filenames.at(i) << std::endl; // "\n" << std::flush;;

        csv_file_name = filenames.at(i).substr(0, filenames.at(i).find_last_of('.')) + ".csv";   // The converted file will be saved in the same folder of the binfile
        f_converted.open(csv_file_name);
        if (!f_converted.is_open()) {
            std::cout << "File " << csv_file_name << " cannot be created!\nMove to the next one ...\n";
            not_converted.push_back(filenames.at(i));
            res = -1;
            continue;
        }

        // Class initialization. Should be destroyed? It is not a pointer, so no ... correct?
        t_BinaryData mdata(to_convert, f_converted, force_ns);

        totsize = mdata.GetEventsSize();
        read_size = mdata.GetEventsBegin();
        uint64_t onepercent = (uint32_t)(totsize / 100.);
        std::cout << "File Size: " << totsize << " Bytes\n";

        while (!to_convert.eof() && read_size < totsize) { // DNIN: is it possible to save each event in a queue or a vector
            mdata.ReadTmpEvt(to_convert);
            mdata.WriteTmpEvt(f_converted);

            mb = to_convert.tellg();
            read_size = mb - begin;
            if ((uint32_t)read_size > next_p * onepercent) {
                std::cout << "\r" << "---> Processing: " << read_size << "/" << totsize << "(" << 100 * (read_size) / totsize << "%)" << std::flush;    // DNIN: Why it does not flush with > 1 file?
                ++next_p;
            }
        }

        std::cout << "\r" << "---> Processing: " << totsize << "/" << totsize << "(" << 100 << "%)\n" << std::flush;
        std::cout << "File " << filenames.at(i) << " converted correctly to " << csv_file_name << " \n" << std::endl;

        to_convert.close();
        to_convert.clear();
        f_converted.close();
        f_converted.clear();
        next_p = 1;

    }
    
    //auto start = std::chrono::high_resolution_clock::now();
    ////auto stop = std::chrono::high_resolution_clock::now();

    ////// Get duration. Substart timepoints to 
    ////// get durarion. To cast it to proper unit
    ////// use duration cast method
    ////auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

    ////std::cout << "Time taken by function: "
    ////    << duration.count() << " seconds" << std::endl;

    if (res != 0) {
        std::cout << "File(s) not converted: \n";
        for (uint16_t i = 0; i < not_converted.size(); ++i)
            std::cout << not_converted.at(i) << "\n";
    }
    else
        std::cout << "Conversion(s) finished!! Exiting ...\n";
    
    Sleep(2000);
    return 0;

}

