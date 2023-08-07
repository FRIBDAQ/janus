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
#include "BinaryDataFERS.h"

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

    std::cout << "Files to be converted: \n";
    while (lfile >> tmp) {
        std::cout << tmp << "\n";
        list.push_back(tmp);
    }

    return 0;
}

void usage() {
    std::cout << "BinToCsv version " << VERSION << ", usage:" << std::endl;
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
    uint8_t next_p = 1;
    uint8_t force_ns = 0;
    std::streampos begin, end, mb;
    std::streamoff totsize, read_size;
    int8_t res = 0;

    std::vector<std::string> filenames;
    std::vector<std::string> not_converted;

    //Check input parameters - getopt is a possible improvement
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
            std::cout << "Files to be converted: \n";
            while (++p < argc) {
                arg = argv[p];
                if (arg.find("--") != std::string::npos) {
                    --p;
                    break;
                }
                std::cout << arg << "\n";
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
        t_BinaryDataFERS mdata(to_convert, f_converted, force_ns);
        //BinaryData_5203 mdata(to_convert, f_converted, force_ns);

        totsize = mdata.GetEventsSize();
        read_size = mdata.GetEventsBegin();
        uint64_t onepercent = (uint32_t)(totsize / 100.);
        std::cout << "File Size: " << totsize << " Bytes\n";

        while (!to_convert.eof() && read_size < totsize) { // DNIN: is it possible to save each event in a queue or a vector
            mdata.ReadTmpEvtFERS(to_convert);
            mdata.WriteTmpEvtFERS(f_converted);

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


// For Debug
//const int default_argc = 8;
//std::string const argv[] = { "-", "-ns", "--bfile", "Run1_list.dat", "Run11_list.dat", "--ns", "--lfile", "unaprovola.list"};
//int argc = default_argc;

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
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run10_list.dat");
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run11_list.dat");
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run12_list.dat");
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run13_list.dat");
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run14_list.dat");
//filenames.push_back("C:\\Users\\dninci\\source\\repos\\BinToCsv\\test\\Run15_list.dat");


    //auto start = std::chrono::high_resolution_clock::now();
    ////auto stop = std::chrono::high_resolution_clock::now();

    ////// Get duration. Substart timepoints to 
    ////// get durarion. To cast it to proper unit
    ////// use duration cast method
    ////auto duration = std::chrono::duration_cast<std::chrono::seconds>(stop - start);

    ////std::cout << "Time taken by function: "
    ////    << duration.count() << " seconds" << std::endl;