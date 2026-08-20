/***************************************************************************//**
* \note TERMS OF USE :
*This program is free software; you can redistribute itand /or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation.This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.The user relies on the
* software, documentationand results solely at his own risk.
*******************************************************************************/
/*
*  FERSlibDemo.cpp : FERSlib demo for running acquisition on FERS 5203 module
*
*       Author: Daniele Ninci (CAEN)
*       Date: 17/06/2025
*/


#include "Demo5203.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <sys\timeb.h>
#else
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

//#include "MultiPlatform.h"

#define DEFAULT_CONFIG_FILENAME  "Demo5203_Config.txt"

// Acquisition Status Bits
#define ACQSTATUS_SOCK_CONNECTED		1	// GUI connected through socket
#define ACQSTATUS_HW_CONNECTED			2	// Hardware connected
#define ACQSTATUS_READY					3	// ready to start (HW connected, memory allocated and initialized)
#define ACQSTATUS_RUNNING				4	// acquisition running (data taking)
#define ACQSTATUS_RESTARTING			5	// Restarting acquisition
#define ACQSTATUS_EMPTYING				6	// Acquiring data still in the boards buffers after the software stop
#define ACQSTATUS_STAIRCASE				10	// Running Staircase
#define ACQSTATUS_RAMPING_HV			11	// Switching HV ON or OFF
#define ACQSTATUS_UPGRADING_FW			12	// Upgrading the FW
#define ACQSTATUS_HOLD_SCAN				13	// Running Scan Hold
#define ACQSTATUS_ERROR					-1	// Error

// Temperatures
#define TEMP_BOARD						0
#define TEMP_FPGA						1
#define TEMP_TDC0					    2   
#define TEMP_TDC1					    3

// Stats Monitor
#define SMON_HIT_RATE					0
#define SMON_HIT_CNT					1
#define SMON_FULL_RATE					2	// Takes into account all the events read by TDC, no software 
#define SMON_FULL_CNT					3	
#define SMON_LEAD_MEAN					4	
#define SMON_LEAD_RMS					5	
#define SMON_ToT_MEAN					6	
#define SMON_ToT_RMS					7

// Plot type
#define PLOT_LEAD_SPEC					0
#define PLOT_TOT_SPEC					1
#define PLOT_TOT_COUNTS					2

#ifdef linux
#define myscanf     _scanf
#else
#define myscanf     scanf
#endif

//#define GNUPLOTEXE  "pgnuplot"
#ifdef linux
#define GNUPLOTEXE  "gnuplot"   // Or '/usr/bin/gnuplot'
#define NULL_PATH   "/dev/null"
#define popen	popen
#define pclose	pclose
#else
#define GNUPLOTEXE  "..\\..\\..\\gnuplot\\pgnuplot.exe"
#define NULL_PATH   "nul"
#define popen  _popen    /* redefine POSIX 'deprecated' popen as _popen */
#define pclose  _pclose  /* redefine POSIX 'deprecated' pclose as _pclose */
#endif



typedef struct Demo_t {
    char brd_path[16][500];
    int num_brd;
    int acq_status;
    int sync_mode;
    int Quit;
//    int EHistoNbin;
    int AdapterType;
	int InvertEdgePolarity[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5203];	// Invert Edge Polarity
    int TestMode; 
    float FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES];

    // 5203
	int MeasMode;				// Measurement Mode (LEAD_TRAIL, LEAD_TOT8, LEAD_TOT11)
	int ToTRebin;				// Rebinning factor for ToT histogram
	int LeadTrailRebin;			// Rebinning factor for Lead/Trail histogram
	int ToTHistoMin;				// Minimum ToT value for histogram
	int LeadHistoMin;			// Minimum Lead/Trail value for histogram
	int ToTHistoMax;				// Maximum ToT value for histogram
	int LeadHistoMax;			// Maximum Lead/Trail value for histogram
    int ToT_LSB;				// ToT LSB 
    int LeadTrail_LSB;		    // Lead/Trail LSB 
    int ToT_LSB_ns;				// ToT LSB in ns
	int LeadTrail_LSB_ns;		// Lead/Trail LSB in ns
	int ToT_Rescale;			// ToT Rescale factor
	int LeadTrail_Rescale;		// Lead/Trail Rescale factor

	int GateWidth;			// Gate Width in ns
} Demo_t;


/**************************************************************/
/*                  GLOBAL PARAMETERS                         */
/**************************************************************/
char description[1024];
int handle[FERSLIB_MAX_NBRD];
int cnc_handle[FERSLIB_MAX_NCNC];
int HistoCreatedE[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH_5202] = { 0 };
int stats_brd = 0, stats_ch = 0;
int stats_mon = 0, stats_plot = 0;
int first_sEvt = 0;				// Skip check on the first service event in Update_Service_Event

int ValidParameterName = 0;
int ValidParameterValue = 0;
int ValidUnits = 0;

uint32_t StatusReg[FERSLIB_MAX_NBRD];	// Acquisition Status Register
float BrdTemp[FERSLIB_MAX_NBRD][4];
uint64_t CurrentTime, PrevKbTime, PrevRateTime, ElapsedTime, StartTime, StopTime;
Stats5203_t stats;
ServEvent5203_t sEvt[FERSLIB_MAX_NBRD];
/*************************************************************/


int print_menu()
{
    printf("**************************************************************\n");
    printf("\n\t\tFERSlibDemo for 5203 module\n\n");
    printf("\nWith this demo, you can perform the following:\n");
    printf(" - Open and configure FERS module\n");
    printf(" - Read an event\n");
    printf(" - Collect statistics for Timing common start/stop, trg_matching and streaming mode for both ToA (LEAD) and ToT");
    printf("\nFor more specific functionalities, please refer to the Janus software version related to the FERS module you use\n");
    printf("**************************************************************\n");

    printf("\n\n-FERSlib_demo.exe:\tFERSlib demo executable\n");
    printf("-Option:\tConfigFile.txt\n");
    printf("\nIf no config file is provided, the default Demo5203_Config.txt in the same .exe folder will be used\n");
    printf("\n\nExample:\n./FERSlib_demo.exe my_config.txt\n./FERSlib_demo.exe\n");

    return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Local Functions
// --------------------------------------------------------------------------------------------------------- 
int f_getch()
{
#ifdef _WIN32
    return _getch();
#else
    struct termios oldattr;
    if (tcgetattr(STDIN_FILENO, &oldattr) == -1) perror(NULL);
    struct termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VTIME] = 0;
    newattr.c_cc[VMIN] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) perror(NULL);
    const int ch = getchar();
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldattr) == -1) perror(NULL);
    return ch;
#endif
    //	unsigned char temp;
    //	raw();
    //    /* stdin = fd 0 */
    //	if(read(0, &temp, 1) != 1)
    //		return 0;
    //	return temp;
    //#endif
}

int f_kbhit()
{
#ifdef _WIN32
    return _kbhit();
#else
    struct termios oldattr;
    if (tcgetattr(STDIN_FILENO, &oldattr) == -1) perror(NULL);
    struct termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VTIME] = 0;
    newattr.c_cc[VMIN] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) perror(NULL);
    /* check stdin (fd 0) for activity */
    fd_set read_handles;
    FD_ZERO(&read_handles);
    FD_SET(0, &read_handles);
    struct timeval timeout;
    timeout.tv_sec = timeout.tv_usec = 0;
    int status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldattr) == -1) perror(NULL);
    if (status < 0) {
        printf("select() failed in kbhit()\n");
        exit(1);
    }
    return status;
#endif
}

#ifdef linux
// ----------------------------------------------------
//
// ----------------------------------------------------
int _scanf(char* fmt, ...) {
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vscanf(fmt, args);
    va_end(args);
    return ret;
}
#endif


uint32_t ToTbin(uint32_t ToT, Demo_t mcfg) {
    float fbin = (ToT - (mcfg.ToTHistoMin / mcfg.ToT_LSB_ns)) / mcfg.ToTRebin;
    uint32_t bin = (fbin > 0) ? (uint32_t)fbin : 0; // At the moment it will be discharged as overflow.
    return bin;
}
uint32_t Leadbin(uint32_t ToA, Demo_t mcfg) {
    float fbin = (ToA - (mcfg.LeadHistoMin / mcfg.LeadTrail_LSB_ns)) / mcfg.LeadTrailRebin;
    uint32_t bin = (fbin > 0) ? (uint32_t)fbin : 0;
    return bin;
}


/* Get time in milliseconds from the computer internal clock */
uint64_t demo_get_time()
{
    uint64_t time_ms;
#ifdef WIN32
    struct _timeb timebuffer;
    _ftime(&timebuffer);
    time_ms = (uint64_t)timebuffer.time * 1000 + (uint64_t)timebuffer.millitm;
#else
    struct timeval t1;
    struct timezone tz;
    gettimeofday(&t1, &tz);
    time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif
    return time_ms;
}

int get_brd_path_from_file(FILE* f_ini, Demo_t* cfg) {
    char tstr[1000], parval[1000], str1[1000];
    int brd;  // target board defined as ParamName[b][ch]
    int num_brd = 0;
    //int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD b] [CHANNEL ch])

    //read config file and assign parameters 
    while (fgets(tstr, sizeof(tstr), f_ini)) {

        if (strstr(tstr, "Open") != NULL) {
            sscanf(tstr, "%s %s", str1, parval);
            char* str = strtok(str1, "[]"); // Param name with [brd][ch]
            char* token = strtok(NULL, "[]");
            if (token != NULL) {
                sscanf(token, "%d", &brd);
            }
            sprintf(cfg->brd_path[brd], "%s", parval);
            ++num_brd;
        }
        if (strstr(tstr, "SyncMode") != NULL) {
            sscanf(tstr, "%s %s", str1, parval);
            if (strstr(parval, "MANUAL") != NULL)				    cfg->sync_mode = STARTRUN_ASYNC;  // keep "MANUAL" option for backward compatibility
            else if (strstr(parval, "ASYNC") != NULL)			    cfg->sync_mode = STARTRUN_ASYNC;
            else if (strstr(parval, "CHAIN_T0") != NULL)		    cfg->sync_mode = STARTRUN_CHAIN_T0;
            else if (strstr(parval, "CHAIN_T1") != NULL)		    cfg->sync_mode = STARTRUN_CHAIN_T1;
            else if (strstr(parval, "TDL") != NULL)				    cfg->sync_mode = STARTRUN_TDL;
            else if (strstr(parval, "TDL_EXTRUN") != NULL)		    cfg->sync_mode = STARTRUN_TDL_EXTRUN;
            else if (strstr(parval, "TDL_EXTRUN_EXTCLK") != NULL)	cfg->sync_mode = STARTRUN_TDL_EXTRUN_EXTCLK;
            else if (strstr(parval, "TDL_GPS") != NULL)			    cfg->sync_mode = STARTRUN_TDL_GPS;
        }
    }
    return num_brd;
}

int get_delayfiber_from_file(FILE* f_ini, Demo_t* cfg) {
    int ret = -1;
    char tstr[1000], parval[1000], str1[1000];
    int cnc = -1, node = -1, brd = -1;  // target board defined as ParamName[b][ch]
    int num_brd = 0;
    //int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD b] [CHANNEL ch])

    //read config file and assign parameters 
    while (fgets(tstr, sizeof(tstr), f_ini)) {
        if (strstr(tstr, "FiberDelayAdjust") != NULL) {
            sscanf(tstr, "%s %s", str1, parval);
            char* str = strtok(str1, "[]"); // Param name with [brd][ch]
            char* token = strtok(NULL, "[]");
            if (token != NULL) {
                sscanf(token, "%d", &cnc);
                if ((token = strtok(NULL, "[]")) != NULL) {
                    sscanf(token, "%d", &node);
                    if ((token = strtok(NULL, "[]")) != NULL) {
                        sscanf(token, "%d", &brd);
                    }
                }
            }
            ret = 0;
            int cm, cM, nm, nM, bm, bM;
            if (brd == -1) {
                bm = 0;
                bM = FERSLIB_MAX_NBRD;
            } else {
                bm = brd;
                bM = brd + 1;
            }
            if (node == -1) {
                nm = 0;
                nM = FERSLIB_MAX_NTDL;
            } else {
                nm = node;
                nM = node + 1;
            }
            if (cnc == -1) {
                cm = 0;
                cM = FERSLIB_MAX_NNODES;
            } else {
                cm = cnc;
                cM = cnc + 1;
            }
            for (int b = bm; b < bM; ++b) {
                for (int n = nm; n < nM; ++n) {
                    for (int c = cm; c < cM; ++c) {
                        float val;
                        sscanf(parval, "%f", &val);
                        cfg->FiberDelayAdjust[c][n][b] = val;
                    }
                }
            }
        }
    }
    return ret;
}


// ---------------------------------------------------------------------------------
// Description: compare two strings
// Inputs:		str1, str2: strings to compare
// Outputs:		-
// Return:		1=equal, 0=not equal
// ---------------------------------------------------------------------------------
int streq(char* str1, const char* str2)
{
    if (strcmp(str1, str2) == 0) {
        ValidParameterName = 1;
        return 1;
    } else {
        return 0;
    }
}


// ---------------------------------------------------------------------------------
// Description: Trim a string left and right
// Inputs:		string to be trimmed
// Outputs:		string trimmed
// Return:		string trimmed
// ---------------------------------------------------------------------------------
char* ltrim(char* s) {
    while (isspace(*s)) s++;
    return s;
}

char* rtrim(char* s) {
    char* back = s + strlen(s) - 1;
    while (isspace(*back)) --back;
    *(back + 1) = '\0';
    return s;
}

char* trim(char* s) {
    return rtrim(ltrim(s));
}


// ---------------------------------------------------------------------------------
// Description: Read an integer (decimal) from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file / Set ValidParameterValue=0 if the string is not an integer
// ---------------------------------------------------------------------------------
static uint32_t GetInt(char* val)
{
    uint32_t ret;
    int num = sscanf(val, "%d", &ret);
    if (ret < 0 || num < 1) ValidParameterValue = 0;
    else ValidParameterValue = 1;
    return ret;
}


int ParseCfgFile(FILE* f_ini,  Demo_t* cfg, int mode) {
    int ret = -1;
    int cnc = -1, node = -1, brd = -1;  // target board defined as ParamName[b][ch]
    int num_brd = 0;
    int ch = -1;  // target board/ch defined as ParamName[b][ch] (-1 = all)
    int brd_l, brd_h;  // low/high index for multiboard settings
    char tstr[1000], str1[1000], * parval, * tparval, * parname, * token;
    //int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD b] [CHANNEL ch])

    //read config file and assign parameters 
    while (fgets(tstr, sizeof(tstr), f_ini)) {
        // Read a line from the file
        fgets(tstr, sizeof(tstr), f_ini);
        if (strlen(tstr) < 2) continue;

        // skip comments
        if (tstr[0] == '#' || strlen(tstr) <= 2) continue;
        if (strstr(tstr, "#") != NULL) tparval = strtok(tstr, "#");
        else tparval = tstr;

        // Get param name (str) and values (parval)
        sscanf(tparval, "%s", str1);
        tparval += strlen(str1);
        parval = trim(tparval);

        // Search for boards and channels
        parname = strtok(trim(str1), "[]"); // Param name with [brd][ch]
        token = strtok(NULL, "[]");
        if (token != NULL) {
            sscanf(token, "%d", &brd);
            if ((token = strtok(NULL, "[]")) != NULL)
                sscanf(token, "%d", &ch);
        }
        if (brd != -1) {
            brd_l = brd;
            brd_h = brd + 1;
        } else {
            brd_l = 0;
            brd_h = (cfg->num_brd > 0) ? cfg->num_brd : 16;
        }

        if (mode == PARSE_CONN) {
            if (streq(parname, "Open")) {
                if (brd < 0 || brd >= FERSLIB_MAX_NBRD) {
                    printf("ERROR: Board index %d out of range\n", brd);
                    return -1;
                }
                strcpy(cfg->brd_path[brd], parval);
                ++cfg->num_brd;
                ret = 0;
            }
            if (streq(parname, "FiberDelayAdjust")) {
                int np, cnc, chain, node;
                float length; // length expressed in m
                np = sscanf(parval, "%d %d %d %f", &cnc, &chain, &node, &length);
                if ((np == 4) && (cnc >= 0) && (cnc < FERSLIB_MAX_NCNC) && (chain >= 0) && (chain < 8) && (node >= 0) && (node < 16))
                    cfg->FiberDelayAdjust[cnc][chain][node] = length;
            }
        } else if (mode == PARSE_CFG) {
            // Example showing how to parse and set parameters for the main program and the library
            if (streq(parname, "ToTRebin"))          cfg->ToTRebin = GetInt(parval);
            else if (streq(parname, "LeadTrailRebin"))    cfg->LeadTrailRebin = GetInt(parval);
            else if (streq(parname, "ToTHistoMin"))       cfg->ToTHistoMin = GetInt(parval);
            else if (streq(parname, "LeadHistoMin"))      cfg->LeadHistoMin = GetInt(parval);
            else if (streq(parname, "ToTHistoMax"))       cfg->ToTHistoMax = GetInt(parval);
            else if (streq(parname, "LeadHistoMax"))      cfg->LeadHistoMax = GetInt(parval);
            else if (streq(parname, "ToT_LSB"))           cfg->ToT_LSB = GetInt(parval);
            else if (streq(parname, "LeadTrail_LSB"))     cfg->LeadTrail_LSB = GetInt(parval);
            else if (streq(parname, "ToT_Rescale"))       cfg->ToT_Rescale = GetInt(parval);
            else if (streq(parname, "LeadTrail_Rescale")) cfg->LeadTrail_Rescale = GetInt(parval);
            //if (streq(parname, "MeasMode")) { // This should be passed before to lib and then retrieved with GetParam
            //    if (streq(parval, "LEAD_ONLY"))			cfg->MeasMode = MEASMODE_LEAD_ONLY;
            //    else if (streq(parval, "LEAD_TRAIL"))	cfg->MeasMode = MEASMODE_LEAD_TRAIL;
            //    else if (streq(parval, "LEAD_TOT8"))	cfg->MeasMode = MEASMODE_LEAD_TOT8;
            //    else if (streq(parval, "LEAD_TOT11"))	cfg->MeasMode = MEASMODE_LEAD_TOT11;
            //    else ValidParameterValue = 0;
            else {
                char tmp_name[100] = "";
                if (ch >= 0) {
                    sprintf(tmp_name, "%.98s[%d]", parname, ch);
                    sprintf(parname, "%s", tmp_name);
                }
                //! [ParseFile]
                for (int b = brd_l; b < brd_h; b++) {
                    //printf("%s %s\n", parname, parval);
                    ret = FERS_SetParam(handle[b], parname, parval);
                }
                //! [ParseFile]
            }
        }
	}
    
    char tmpVal[16];
    FERS_GetParam(handle[0], "MeasMode", tmpVal);
    int ret2 = sscanf(tmpVal, "%d", &cfg->MeasMode);

	// Post processing of the parameters
    if (MEASMODE_OWLT(cfg->MeasMode)) {
        cfg->LeadTrail_Rescale = 0;
        cfg->ToT_Rescale = 0;
    } else {
        cfg->LeadTrail_Rescale = cfg->LeadTrail_LSB;
        cfg->ToT_Rescale = cfg->ToT_LSB;
    }
    cfg->LeadTrail_LSB_ns = (float)(TDC_LSB_ps * (1 << cfg->LeadTrail_LSB)) / 1000.;
    cfg->ToT_LSB_ns = (float)(TDC_LSB_ps * (1 << cfg->ToT_LSB)) / 1000.;

    return 0;
}

int CreateStatistics(int nb, int nch, int Nbin) {
    for (int brd = 0; brd < nb; brd++) {
        for (int ch = 0; ch < nch; ch++) {
            stats.H1_Lead[brd][ch].H_data = (uint32_t*)malloc(Nbin * sizeof(uint32_t));
            memset(stats.H1_Lead[brd][ch].H_data, 0, Nbin * sizeof(uint32_t));
            stats.H1_Lead[brd][ch].Nbin = Nbin;
            stats.H1_Lead[brd][ch].H_cnt = 0;
            stats.H1_Lead[brd][ch].Ovf_cnt = 0;
            stats.H1_Lead[brd][ch].Unf_cnt = 0;

            stats.H1_ToT[brd][ch].H_data = (uint32_t*)malloc(Nbin * sizeof(uint32_t));
            memset(stats.H1_ToT[brd][ch].H_data, 0, Nbin * sizeof(uint32_t));
            stats.H1_ToT[brd][ch].Nbin = Nbin;
            stats.H1_ToT[brd][ch].H_cnt = 0;
            stats.H1_ToT[brd][ch].Ovf_cnt = 0;
            stats.H1_ToT[brd][ch].Unf_cnt = 0;
        }
    }
    return 0;
}

int ResetStatistics() {
    stats.start_time = demo_get_time();
    stats.current_time = stats.start_time;
    stats.previous_time = stats.start_time;
    for (int b = 0; b < FERSLIB_MAX_NBRD; b++) {
        stats.current_trgid[b] = 0;
        stats.previous_trgid[b] = 0;
        stats.current_tstamp_us[b] = 0;
        stats.previous_tstamp_us[b] = 0;
        stats.LostTrgPerc[b] = 0;
        stats.ZeroSupprTrgPerc[b] = 0;
        stats.ProcTrgPerc[b] = 0;
        stats.BuildPerc[b] = 0;
        memset(&stats.TotTrgCnt[b], 0, sizeof(Counter_t));
        memset(&stats.LostTrgCnt[b], 0, sizeof(Counter_t));
        memset(&stats.SupprTrgCnt[b], 0, sizeof(Counter_t));
        memset(&stats.ReadTrgCnt[b], 0, sizeof(Counter_t));
        memset(&stats.ByteCnt[b], 0, sizeof(Counter_t));
        for (int ch = 0; ch < FERSLIB_MAX_NCH_5203; ch++) {
            memset(&stats.ReadHitCnt[b][ch], 0, sizeof(Counter_t));
            memset(&stats.MatchHitCnt[b][ch], 0, sizeof(Counter_t));
            memset(&stats.LeadMeas[b][ch], 0, sizeof(MeasStat_t));
            memset(&stats.TrailMeas[b][ch], 0, sizeof(MeasStat_t));
            memset(&stats.ToTMeas[b][ch], 0, sizeof(MeasStat_t));
        }
        //    memset(stats.H1_PHA_LG[b][ch].H_data, 0, stats.H1_PHA_LG[b][ch].Nbin * sizeof(uint32_t));
        //    stats.H1_PHA_LG[b][ch].H_cnt = 0;
        //    stats.H1_PHA_LG[b][ch].H_p_cnt = 0;
        //    stats.H1_PHA_LG[b][ch].Ovf_cnt = 0;
        //    stats.H1_PHA_LG[b][ch].Unf_cnt = 0;

        //    memset(stats.H1_PHA_HG[b][ch].H_data, 0, stats.H1_PHA_HG[b][ch].Nbin * sizeof(uint32_t));
        //    stats.H1_PHA_HG[b][ch].H_cnt = 0;
        //    stats.H1_PHA_HG[b][ch].H_p_cnt = 0;
        //    stats.H1_PHA_HG[b][ch].Ovf_cnt = 0;
        //    stats.H1_PHA_HG[b][ch].Unf_cnt = 0;
        //    HistoCreatedE[b][ch] = 1;
        //}
    }
    return 0;
}

int DestroyHistogram1D(Histogram1D_t Histo) {
    if (Histo.H_data != NULL)
        free(Histo.H_data);	// DNIN error in memory access
    return 0;
}

int DestroyStatistics() {
    int b, ch;
    for (b = 0; b < FERSLIB_MAX_NBRD; b++) {
        for (ch = 0; ch < FERSLIB_MAX_NCH_5202; ch++) {
            if (HistoCreatedE[b][ch]) {
                DestroyHistogram1D(stats.H1_Lead[b][ch]);
                DestroyHistogram1D(stats.H1_ToT[b][ch]);
                HistoCreatedE[b][ch] = 0;
            }
        }
    }
    return 0;
}

#define MEAN(m, ns)		((ns) > 0 ? (m)/(ns) : 0)
#define RMS(r, m, ns)	((ns) > 0 ? sqrt((r)/(ns) - (m)*(m)) : 0)

void AddMeasure(MeasStat_t* mstat, double value) {
    mstat->sum += value;
    mstat->ssum += value * value;
    mstat->ncnt++;
}

void UpdateCntRate(Counter_t* Counter, double elapsed_time_us, int RateMode) {
    if (elapsed_time_us <= 0) {
        //Counter->rate = 0;
        return;
    } else if (RateMode == 1)
        Counter->rate = Counter->cnt / (elapsed_time_us * 1e-6);
    else
        Counter->rate = (Counter->cnt - Counter->pcnt) / (elapsed_time_us * 1e-6);
    Counter->pcnt = Counter->cnt;
}

void UpdateMeasure(MeasStat_t* mstat) {
    mstat->psum = mstat->sum;
    mstat->pssum = mstat->ssum;
    mstat->pncnt = mstat->ncnt;
    mstat->mean = MEAN(mstat->sum, mstat->ncnt);
    mstat->rms = RMS(mstat->ssum, mstat->mean, mstat->ncnt);
}

int UpdateStatistics(int RateMode) {
    double pc_elapstime = (RateMode == 1) ? 1e3 * (stats.current_time - stats.start_time) : 1e3 * (stats.current_time - stats.previous_time);  // us
    stats.previous_time = stats.current_time;

    for (int b = 0; b < FERSLIB_MAX_NBRD; b++) {
        double brd_elapstime = (RateMode == 1) ? stats.current_tstamp_us[b] : stats.current_tstamp_us[b] - stats.previous_tstamp_us[b];  // - Stats.start_time 
        double elapstime_serv = (RateMode == 1) ? stats.last_serv_tstamp_us[b] : stats.last_serv_tstamp_us[b] - stats.prev_serv_tstamp_us[b];
        double elapstime = (brd_elapstime > 0) ? brd_elapstime : pc_elapstime;
        stats.previous_tstamp_us[b] = stats.current_tstamp_us[b];
		stats.prev_serv_tstamp_us[b] = stats.last_serv_tstamp_us[b];
        for (int ch = 0; ch < FERSLIB_MAX_NCH_5203; ch++) {
            UpdateMeasure(&stats.LeadMeas[b][ch]);
            UpdateMeasure(&stats.TrailMeas[b][ch]);
            UpdateMeasure(&stats.ToTMeas[b][ch]);
            UpdateCntRate(&stats.ReadHitCnt[b][ch], elapstime, RateMode);
            UpdateCntRate(&stats.MatchHitCnt[b][ch], elapstime, RateMode);
        }
        if (stats.TotTrgCnt[b].cnt == 0) {
            stats.LostTrgPerc[b] = 0;
            stats.ProcTrgPerc[b] = 0;
            stats.ZeroSupprTrgPerc[b] = 0;
        } else if (RateMode == 1) {
            stats.LostTrgPerc[b] = (float)(100.0 * stats.LostTrgCnt[b].cnt / stats.TotTrgCnt[b].cnt);
            stats.ProcTrgPerc[b] = (float)(100.0 * stats.ReadTrgCnt[b].cnt / stats.TotTrgCnt[b].cnt);
            stats.ZeroSupprTrgPerc[b] = (float)(100.0 * stats.SupprTrgCnt[b].cnt / stats.TotTrgCnt[b].cnt);
        } else if (stats.TotTrgCnt[b].cnt > stats.TotTrgCnt[b].pcnt) {
            stats.LostTrgPerc[b] = (float)(100.0 * (stats.LostTrgCnt[b].cnt - stats.LostTrgCnt[b].pcnt) / (stats.TotTrgCnt[b].cnt - stats.TotTrgCnt[b].pcnt));
            stats.ProcTrgPerc[b] = (float)(100.0 * (stats.ReadTrgCnt[b].cnt - stats.ReadTrgCnt[b].pcnt) / (stats.TotTrgCnt[b].cnt - stats.TotTrgCnt[b].pcnt));
            stats.ZeroSupprTrgPerc[b] = (float)(100.0 * (stats.SupprTrgCnt[b].cnt - stats.SupprTrgCnt[b].pcnt) / (stats.TotTrgCnt[b].cnt - stats.TotTrgCnt[b].pcnt));
        } else {
            stats.LostTrgPerc[b] = 0;
            stats.ProcTrgPerc[b] = 0;
            stats.ZeroSupprTrgPerc[b] = 0;
        }
        if (stats.LostTrgPerc[b] > 100) stats.LostTrgPerc[b] = 100;
        if (stats.ProcTrgPerc[b] > 100) stats.ProcTrgPerc[b] = 100;
        if (stats.ZeroSupprTrgPerc[b] > 100) stats.ZeroSupprTrgPerc[b] = 100;
        stats.previous_trgid_p[b] = stats.current_trgid[b];

        double etime = elapstime_serv > 0 ? elapstime_serv : elapstime;
        UpdateCntRate(&stats.TotTrgCnt[b], etime, RateMode);
        UpdateCntRate(&stats.LostTrgCnt[b], etime, RateMode);
        UpdateCntRate(&stats.SupprTrgCnt[b], etime, RateMode);
        UpdateCntRate(&stats.ReadTrgCnt[b], elapstime, RateMode);
        UpdateCntRate(&stats.BoardHitCnt[b], elapstime, RateMode);
        UpdateCntRate(&stats.ByteCnt[b], elapstime, RateMode);
        UpdateCntRate(&stats.ByteCnt[b], pc_elapstime, RateMode);
    }
    return 0;
}

int AddCount(Histogram1D_t* Histo, int Bin) {
    if (Bin < 0) {
        Histo->Unf_cnt++;
        return -1;
    } else if (Bin >= (int)(Histo->Nbin - 1)) {
        Histo->Ovf_cnt++;
        return -1;
    }
    Histo->H_data[Bin]++;
    Histo->H_cnt++;
    Histo->mean += (double)Bin;
    Histo->rms += (double)Bin * (double)Bin;
    return 0;
}

// Update service event info
int Update_Service_Info(int handle) {
    int brd = FERS_INDEX(handle);
    int ret = 0, fail = 0;
    uint64_t now = demo_get_time();

    //if (first_sEvt >= 1 && first_sEvt < 10) {	// Skip the check on the first event service missing
    //    ++first_sEvt;
    //    return 0;
    //} else if (first_sEvt == 10) {
    //    first_sEvt = 0;
    //    return 0;
    //}

    if (sEvt[brd].update_time > (now - 2000)) {
        BrdTemp[brd][TEMP_BOARD] = sEvt[brd].tempBoard;
        BrdTemp[brd][TEMP_FPGA] = sEvt[brd].tempFPGA;
        BrdTemp[brd][TEMP_TDC0] = sEvt[brd].tempTDC[0];
        BrdTemp[brd][TEMP_TDC1] = sEvt[brd].tempTDC[1];
        StatusReg[brd] = sEvt[brd].Status;
    } else {
        ret |= FERS_Get_FPGA_Temp(handle, &BrdTemp[brd][TEMP_FPGA]);
        ret |= FERS_Get_Board_Temp(handle, &BrdTemp[brd][TEMP_BOARD]);
        ret |= FERS_Get_TDC0_Temp(handle, &BrdTemp[brd][TEMP_TDC0]);
        ret |= FERS_Get_TDC1_Temp(handle, &BrdTemp[brd][TEMP_TDC1]);
        ret |= FERS_ReadRegister(handle, a_acq_status, &StatusReg[brd]);
    }
    if (ret < 0) fail = 1;
    return ret;
}


// Convert a double in a string with unit (k, M, G)
void double2str(double f, int space, char* s)
{
    if (!space) {
        if (f <= 999.999)			sprintf(s, "%7.3f ", f);
        else if (f <= 999999)		sprintf(s, "%7.3fk", f / 1e3);
        else if (f <= 999999000)	sprintf(s, "%7.3fM", f / 1e6);
        else						sprintf(s, "%7.3fG", f / 1e9);
    } else {
        if (f <= 999.999)			sprintf(s, "%7.3f ", f);
        else if (f <= 999999)		sprintf(s, "%7.3f k", f / 1e3);
        else if (f <= 999999000)	sprintf(s, "%7.3f M", f / 1e6);
        else						sprintf(s, "%7.3f G", f / 1e9);
    }
}

void cnt2str(uint64_t c, char* s)
{
    if (c <= 9999999)			sprintf(s, "%7d ", (uint32_t)c);
    else if (c <= 9999999999)	sprintf(s, "%7dk", (uint32_t)(c / 1000));
    else						sprintf(s, "%7dM", (uint32_t)(c / 1000000));
}


void ClearScreen()
{
    printf("\033[2J");                // ClearScreen
    printf("%c[%d;%df", 0x1B, 0, 0);
}

void gotoxy(int x, int y)
{
    printf("%c[%d;%df", 0x1B, y, x);  // goto(x,y)
}

// ******************************************************************************************
// Run Control functions
// ******************************************************************************************
// Start Run (starts acq in all boards)
int StartRun(Demo_t* tcfg) {
    int ret = 0, b, tdl = 1;
    char parvalue[1024];
    char parname[1024] = "";
    strcpy(parname, "StartRunMode");
    if (tcfg->acq_status == ACQSTATUS_RUNNING) return 0;

    for (b = 0; b < tcfg->num_brd; ++b) {
        if (FERS_CONNECTIONTYPE(handle[b]) != FERS_CONNECTIONTYPE_TDL) tdl = 0;
    }

    // GetStartRun Mode
    //STARTRUN_ASYNC
    //STARTARUN_CHAIN_T0
    //STARTARUN_CHAIN_T1
    //STARTRUN_TDL : this mode can be used only for TDL connection

    //! [GetParam]
    ret = FERS_GetParam(handle[0], parname, parvalue);
    //! [GetParam]
    sscanf(parvalue, "%d", &b);
    if (!tdl && (b == STARTRUN_TDL)) {
        printf("WARNING: StartRunMode: can't start run in TDL mode; switching to Async mode\n");
        for (b = 0; b < tcfg->num_brd; ++b) {
            sprintf(parvalue, "%d", STARTRUN_ASYNC);
            //! [SetParam]
            FERS_SetParam(handle[b], parname, parvalue);
            //! [SetParam]	
            FERS_configure(handle[b], CFG_SOFT);
            printf("Brd%d Start mode Async\n", b);
        }
    }

    ret = FERS_StartAcquisition(handle, tcfg->num_brd, STARTRUN_ASYNC, 0);

    stats.start_time = demo_get_time();
    stats.stop_time = 0;
    first_sEvt = 1;

    tcfg->acq_status = ACQSTATUS_RUNNING;
    return ret;
}

// Stop Run
int StopRun(Demo_t* tcfg) {
    int ret = 0;

    ret = FERS_StopAcquisition(handle, tcfg->num_brd, STARTRUN_ASYNC, 0);
    if (stats.stop_time == 0) stats.stop_time = demo_get_time();

    if (tcfg->acq_status == ACQSTATUS_RUNNING) {
        //printf("Run #%d stopped. Elapsed Time = %.2f\n", (float)(Stats.stop_time - Stats.start_time) / 1000);
        tcfg->acq_status = ACQSTATUS_READY;
    }
    return ret;
}

int CheckRunTimeCmd(Demo_t* tcfg)
{
    int c = 0;
    char input_name[1024], input_val[1024];
    int r = 0;
    int brd = 0;
    if (!f_kbhit())
        return 0;
    c = f_getch();

    if (c == 'q') {
        tcfg->Quit = 1;
        return 1;
    }
    if (c == 's') {
        r = StartRun(tcfg);
        return 1;
    }
    if (c == 'S') {
        r = StopRun(tcfg);
        return 1;
    }
    if (c == 'w' && tcfg->acq_status == ACQSTATUS_READY) {
        // Clear Screen
        printf("* SET PARAMETER FUNCTION\n\n");
        printf("Enter board idx [0-%d]: ", tcfg->num_brd - 1);
        r = scanf("%d", &brd);
        if (brd < 0 or brd>(tcfg->num_brd - 1)) {
            printf("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        printf("Enter parameter name: ");
        r = scanf("%s", input_name);
        printf("Enter parameter value: ");
        r = scanf("%s", input_val);
        r = FERS_SetParam(handle[brd], input_name, input_val);
        if (r != 0) {
            //! [LastError]
            char emsg[1024];
            FERS_GetLastError(emsg);
            //! [LastError]
            printf("ERROR: %s\n", emsg);
            return r;
        }
        return 1; // Parameter set, configure
    }
    if (c == 'r' && tcfg->acq_status == ACQSTATUS_READY) {
        // Get from keyboard param_name
        // Clear Screen
        printf("* GET PARAMETER FUNCTION\n\n");
        printf("Enter board idx [0-%d]: ", tcfg->num_brd - 1);
        scanf("%d", &brd);
        if (brd < 0 or brd>(tcfg->num_brd - 1)) {
            printf("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        printf("Enter parameter name: ");
        scanf("%s", input_name);
        r = FERS_GetParam(handle[brd], input_name, input_val);
        if (r != 0) {
            FERS_GetLastError(description);
            printf("ERROR: %s\n", description);
            return FERSLIB_ERR_GENERIC;
        }
        printf("\n%s = %s\n", input_name, input_val);
        return 1;
    }
    //if (c == 'h' && tcfg->acq_status == ACQSTATUS_READY) {
    //    // histograms
    //    printf("\n\nSelect histo binning\n");
    //    printf("0 = 256\n");
    //    printf("1 = 512\n");
    //    printf("2 = 1024\n");
    //    printf("3 = 2048\n");
    //    printf("4 = 4096\n");
    //    printf("5 = 8192\n");
    //    c = f_getch() - '0';
    //    if (c == '0') tcfg->EHistoNbin = 256;
    //    else if (c == '1') tcfg->EHistoNbin = 512;
    //    else if (c == '2') tcfg->EHistoNbin = 1024;
    //    else if (c == '3') tcfg->EHistoNbin = 2048;
    //    else if (c == '4') tcfg->EHistoNbin = 4096;
    //    else if (c == '5') tcfg->EHistoNbin = 8192;
    //    return 4;
    //}
    //if (c == 'p') {
    //    printf("\n\nSelect plot\n");
    //    printf("0 = Spect Low Gain\n");
    //    printf("1 = Spect High Gain\n");
    //    c = f_getch() - '0';
    //    if ((c >= 0) && (c < 2))
    //        stats_plot = c;
    //    return 1;
    //}
    if (c == 'b') {
        int new_brd;
        printf("Current Active Board = %d\n", stats_brd);
        printf("New Active Board = ");
        scanf("%d", &new_brd);

        if ((new_brd >= 0) && (new_brd < tcfg->num_brd)) {
            stats_brd = new_brd;
            printf("Active Board = %d\n", stats_brd);
        }
        return 1;
    }
    if (c == 'c') {
        int new_ch;
        char chs[10];
        printf("Current Active Channel = %d\n", stats_ch);
        printf("New Active Channel ");
        scanf("%s", chs);
        if (isdigit(chs[0])) sscanf(chs, "%d", &new_ch);
        if ((new_ch >= 0) && (new_ch < FERSLIB_MAX_NCH_5202)) {
            stats_ch = new_ch;
        }
        return 1;
    }
    if (c == 'm') {
        printf("\n\nSelect statistics monitor\n");
        printf("0 = Hit Rate\n");
        printf("1 = Hit Cnt\n");
        printf("2 = Full Rate\n");
        printf("3 = Full Cnt\n");
        printf("4 = Lead Mean\n");
        printf("5 = Lead RMS\n");
		printf("6 = ToT Mean\n");
		printf("7 = ToT RMS\n");
        c = f_getch() - '0';
        if ((c >= 0) && (c <= 5)) stats_mon = c;
        return 1;
    }
    if (c == 'C' && tcfg->acq_status == ACQSTATUS_READY) return 3;
    if (c == 'l' && tcfg->acq_status == ACQSTATUS_READY) return 2;
    if (c == ' ') {
        printf("\n");
        printf("[q] Quit\n");
        printf("[s] Start Acquisition\n");
        printf("[S] Stop Acquisition\n");
        printf("[C] Configure FERS (not available when board is in RUN)\n");
        printf("[l] Load configuration from file (not available when board is in RUN)\n");
        printf("[w] Set Param (not available when board is in RUN)\n");
        printf("[r] Get Param (not available when board is in RUN)\n");
        printf("[b] Change board\n");
        printf("[c] Change channel\n");
        printf("[m] Set StatsMonitor Type\n");
        //printf("[h] Set Histo binning (not available when board is in RUN)\n");
        //printf("[p] Set Plot Mode\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    FILE* fcfg; // , * plotter;
    Demo_t this_cfg;
    FERS_BoardInfo_t BoardInfo[FERSLIB_MAX_NBRD];
    char cfg_file[500]; // = "C:\\Users\\dninci\\source\\repos\\FERSlibDemo\\x64\\Debug\\FERSlib_Config.txt"; //DEFAULT_CONFIG_FILENAME;
    int ret = 0;
    float FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES] = { 0 };
    int cnc = 0;
    int offline_conn = 0;
    int UsingCnc = 0;
    int MajorFWrev = 255;
    int rdymsg = 1;
    int AcqMode = 0;
    uint64_t Tref = 0;
    memset(&this_cfg, 0, sizeof(Demo_t));

    // ToA (or deltaT) after event data processing (rescale + time difference). Data for output file
    // ToT after event data processing (rescale + time difference). Data for output file
    // The pointers are allocated once to avoid the overhead of the malloc
    // Common Start/Stop
    uint64_t* of_DeltaT = (uint64_t*)calloc(FERSLIB_MAX_NCH_5203, sizeof(uint64_t));
    uint16_t* of_ToTDeltaT = (uint16_t*)calloc(FERSLIB_MAX_NCH_5203, sizeof(uint16_t));
    // Trg Matching/Streaming
    uint64_t* of_ToA = (uint64_t*)calloc(MAX_LIST_SIZE, sizeof(uint64_t));
    uint16_t* of_ToT = (uint16_t*)calloc(MAX_LIST_SIZE, sizeof(uint16_t));

    int tmp_brd = 0, tmp_ch = 0;

    int i = 0, clrscr = 0, dtq, ch, b; // jobrun = 0, 
    int nb = 0;
    double tstamp_us, curr_tstamp_us = 0;
    uint64_t kb_time = 0, curr_time = 0, print_time = 0;  // , wave_time;
    float rtime;
    void* Event;

    printf("**************************************************************\n");
    printf("FERSlib Demo v%s for FERS 5203\n", DEMO_RELEASE_NUM);
    printf("FERSlib v%s\n", FERS_GetLibReleaseNum());
    printf("**************************************************************\n");

    //printf("\n\n%s\n%d\n", argv[0], argc);

    if (argc == 1) {  // No options provided
        fcfg = fopen(DEFAULT_CONFIG_FILENAME, "r");
        if (fcfg == NULL) {
            printf("No config file provided and no default one %s found. Exiting (ret=%d)\n", DEFAULT_CONFIG_FILENAME, FERSLIB_ERR_INVALID_PATH);
            print_menu();
            return FERSLIB_ERR_INVALID_PATH;
        }
        strcpy(cfg_file, DEFAULT_CONFIG_FILENAME);
    } else if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 or strcmp(argv[1], "--help") == 0) {
            print_menu();
            return 0;
        }
        fcfg = fopen(argv[1], "r");
        if (fcfg == NULL) {
            printf("No valid config file name %s provided. Exiting (ret=%d)\n", argv[1], FERSLIB_ERR_INVALID_PATH);
            return FERSLIB_ERR_INVALID_PATH;
        }
        strcpy(cfg_file, argv[1]);
    } else
        return FERSLIB_ERR_GENERIC;

    // OPEN CFG FILE
    // Parser for this_cfg, there are more prameters than in 5202 Demo
	ret = ParseCfgFile(fcfg, &this_cfg, PARSE_CONN);
    //this_cfg.num_brd = get_brd_path_from_file(fcfg, &this_cfg);
    //fseek(fcfg, 0, SEEK_SET);
    get_delayfiber_from_file(fcfg, &this_cfg);
    fclose(fcfg);
    // OPEN BOARDS
    memset(handle, -1, sizeof(*handle) * FERSLIB_MAX_NBRD);
    memset(cnc_handle, -1, sizeof(*handle) * FERSLIB_MAX_NCNC);

    for (int b = 0; b < this_cfg.num_brd; b++) {
        char* cc, cpath[100];
        if (((cc = strstr(this_cfg.brd_path[b], "tdl")) != NULL)) {  // TDlink used => Open connection to concentrator (this is not mandatory, it is done for reading information about the concentrator)
            UsingCnc = 1;
            FERS_Get_CncPath(this_cfg.brd_path[b], cpath);
            if (!FERS_IsOpen(cpath)) {
                FERS_CncInfo_t CncInfo;
                printf("\n--------------- Concentrator %2d ----------------\n", cnc);
                printf("Opening connection to %s\n", cpath);
                ret = FERS_OpenDevice(cpath, &cnc_handle[cnc]);
                if (ret == 0) {
                    printf("Connected to Concentrator %s\n", cpath);
                } else {
                    printf("Can't open concentrator at %s\n", cpath);
                    return FERSLIB_ERR_GENERIC;
                }
                if (!FERS_TDLchainsInitialized(cnc_handle[cnc])) {
                    printf("Initializing TDL chains. This will take a few seconds...\n");
                    ret = FERS_EnumTDLchains(cnc_handle[cnc], this_cfg.FiberDelayAdjust[cnc]);
                    if (ret != 0) {
                        printf("Failure in TDL chain init\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                }
                ret |= FERS_ReadConcentratorInfo(cnc_handle[cnc], &CncInfo);
                if (ret == 0) {
                    printf("FPGA FW revision = %s\n", CncInfo.FPGA_FWrev);
                    printf("SW revision = %s\n", CncInfo.SW_rev);
                    printf("PID = %d\n", CncInfo.pid);
                    if (CncInfo.ChainInfo[0].BoardCount == 0) { 	// Rising error if no board is connected to link 0
                        printf("No board connected to link 0\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                    for (int i = 0; i < 8; i++) {
                        if (CncInfo.ChainInfo[i].BoardCount > 0)
                            printf("Found %d board(s) connected to TDlink n. %d\n", CncInfo.ChainInfo[i].BoardCount, i);
                    }
                } else {
                    printf("Can't read concentrator info\n");
                    return FERSLIB_ERR_GENERIC;
                }
                cnc++;
            }
        }
        if ((this_cfg.num_brd > 1) || (cnc > 0)) printf("\n------------------ Board %2d --------------------\n", b);
        printf("Opening connection to %s\n", this_cfg.brd_path[b]);
        char BoardPath[500];
        strcpy(BoardPath, this_cfg.brd_path[b]);
        //! [LastErrorOpen]
        //! [Connect]
        ret = FERS_OpenDevice(BoardPath, &handle[b]);
        //! [Connect]
        if (ret != 0) {
            char emsg[1024];
            FERS_GetLastError(emsg);
            printf("Can't open board: %s (ret=%d)\n", emsg, ret);
        }
        //! [LastErrorOpen]
        else {
            printf("Connected to %s\n", this_cfg.brd_path[b]);
        }
        // PRINT BOARD INFO
        ret = FERS_GetBoardInfo(handle[b], &BoardInfo[b]);
        if (ret == 0) {
            if (BoardInfo[b].FERSCode != 5203) {
                printf("ERROR: This demo supports only FERS_5203 module, cannot support board FERS_%" PRIu16 " at BrdIdx %d\nExit...\n", BoardInfo[b].FERSCode);
                return FERSLIB_ERR_GENERIC;
            }
            char fver[100];
            if (BoardInfo[b].FPGA_FWrev == 0) sprintf(fver, "BootLoader");
            else sprintf(fver, "%d.%d (Build = %04X)", (BoardInfo[b].FPGA_FWrev >> 8) & 0xFF, BoardInfo[b].FPGA_FWrev & 0xFF, (BoardInfo[b].FPGA_FWrev >> 16) & 0xFFFF);
            MajorFWrev = min((int)(BoardInfo[b].FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
            printf("FPGA FW revision = %s\n", fver);
            if (strstr(this_cfg.brd_path[b], "tdl") == NULL)
                printf("uC FW revision = %08X\n", BoardInfo[b].uC_FWrev);
            printf("PID = %d\n", BoardInfo[b].pid);
        } else {
            printf("Can't read board info\n");
            return FERSLIB_ERR_GENERIC;
        }
    }
    if (FERS_CONNECTIONTYPE(handle[0] == FERS_CONNECTIONTYPE_TDL))
        FERS_SyncTDLchains(cnc_handle, this_cfg.sync_mode);

LoadConfigFERS:
	fcfg = fopen(cfg_file, "r");
    
	ret = ParseCfgFile(fcfg, &this_cfg, PARSE_CFG);
    //ret = FERS_LoadConfigFile(cfg_file);
    if (ret != 0)
        printf("Cannot load FERS configuration from file %s\n", cfg_file);

    // Get Acquisition Mode
    char AcqModeVal[32];
    char ParName[32] = "AcquisitionMode";
    ret = FERS_GetParam(handle[0], ParName, AcqModeVal);
    sscanf(AcqModeVal, "%d", &AcqMode);

    // Get Test Mode
	ret = FERS_GetParam(handle[0], (char*)"TestMode", AcqModeVal);
	sscanf(AcqModeVal, "%d", &this_cfg.TestMode);

    // Init Readout
    for (b = 0; b < this_cfg.num_brd; b++) {
        int a1;
        //! [InitReadout]
        FERS_InitReadout(handle[b], 0, &a1);
        //! [InitReadout]
        memset(&sEvt[b], 0, sizeof(ServEvent5203_t));
    }

    //CreateStatistics(this_cfg.num_brd, FERSLIB_MAX_NCH_5202, this_cfg.EHistoNbin);


    // OPEN PLOTTER
//    char sstr[200];
//    strcpy(sstr, "");
//    strcat(sstr, GNUPLOTEXE);
//
//#ifndef _WIN32
//    strcat(sstr, " 2> ");
//    strcat(sstr, NULL_PATH);
//#endif
//    plotter = popen(sstr, "w");    // Open plotter pipe (gnuplot)
//    if (plotter == NULL) printf("Can't open gnuplot at %s\n", GNUPLOTEXE);
//    fprintf(plotter, "set grid\n");
//    fprintf(plotter, "set mouse\n");

    // CONFIGURE FROM FILE
ConfigFERS:
    printf("Configuring boards ...\n");
    for (int b = 0; b < this_cfg.num_brd; ++b) {
        //! [Configure]
        ret = FERS_configure(handle[b], CFG_HARD);
        //! [Configure]
        if (ret != 0) {
            printf("Board %d failed!!!\n", b);
            return FERSLIB_ERR_GENERIC;
        } else
            printf("Board %d configured\n", b);
        ret = FERS_DumpBoardRegister(handle[b]);
        ret |= FERS_DumpCfgSaved(handle[b]);
        if (ret != 0) {
            printf("Cannot save parameter configuration");
        }
    }
    //FERS_SetEnergyBitsRange(0);

    ResetStatistics();

    this_cfg.acq_status = ACQSTATUS_READY;
    curr_time = demo_get_time();
    print_time = curr_time;  // force 1st print with no delay
    kb_time = curr_time;
    stats_mon = SMON_HIT_RATE;
    stats_plot = PLOT_LEAD_SPEC;
    stats_brd = 0;
    stats_ch = 0;

    //FERS_WriteRegister(handle[0], a_channel_mask_0, 0xFFFF0000);

    while (!this_cfg.Quit) {
        curr_time = demo_get_time();
        stats.current_time = curr_time;
        nb = 0;

        if ((curr_time - kb_time) > 200) {
            int code = CheckRunTimeCmd(&this_cfg);
            if (code > 0) {  // Cmd executed
                if (this_cfg.Quit)
                    goto QuitProgram;
                if (code == 2) {
                    clrscr = 1;
                    rdymsg = 1;
                    goto LoadConfigFERS;
                }
                if (code == 3) {
                    rdymsg = 1;
                    clrscr = 1;
                    goto ConfigFERS;
                }
                if (code == 4) {
                    clrscr = 1;
                    ResetStatistics();
                    //DestroyStatistics();
                    //CreateStatistics(this_cfg.num_brd, FERSLIB_MAX_NCH_5202, this_cfg.EHistoNbin);
                }
                if (code == 1) {
                    print_time = curr_time;
                    rdymsg = 1;
                    clrscr = 1;
                }
            }
            kb_time = curr_time;
        }

        if (this_cfg.acq_status == ACQSTATUS_READY) {
            if (rdymsg) {
                printf("Press [s] to start run, [q] to quit, [SPACE] to enter the menu\n\n");
                rdymsg = 0;
            }
            //continue;
        }

        if (this_cfg.acq_status == ACQSTATUS_RUNNING) {

            //! [GetEvent]
            Sleep(1);
            ret = FERS_GetEvent(handle, &b, &dtq, &tstamp_us, &Event, &nb);
            //! [GetEvent]
            if (nb > 0) curr_tstamp_us = tstamp_us;
            if (ret < 0) {
                printf("ERROR: Readout failure (ret = %d).\nStopping Data Acquisition ...\n", ret);
                StopRun(&this_cfg);
            }
            if (nb > 0) {
                stats.current_tstamp_us[b] = curr_tstamp_us;
                stats.ByteCnt[b].cnt += (uint64_t)nb;

                if ((dtq & 0x0F) == DTQ_TIMING) {    // 5202+5203 Need to give some statistics
                    uint32_t DTb = 0, ToTb = 0;
                    stats.ReadTrgCnt[b].cnt++;
                    ListEvent_t* Ev = (ListEvent_t*)Event;

                    if (AcqMode == ACQMODE_COMMON_START || AcqMode == ACQMODE_COMMON_STOP) {
                        int Lead[FERSLIB_MAX_NCH_5203], Trail[FERSLIB_MAX_NCH_5203];  // First (Cstart) or Last (Cstop) leading/traling edge; -1 = not found
                        memset(Lead, -1, FERSLIB_MAX_NCH_5203 * sizeof(int));
                        memset(Trail, -1, FERSLIB_MAX_NCH_5203 * sizeof(int));
                        for (int s = 0; s < Ev->nhits; s++) {
                            int i = (AcqMode == ACQMODE_COMMON_START) ? s : Ev->nhits - s - 1;  // index (CSTART = ascending, CSTOP = descending)
                            if (this_cfg.AdapterType == ADAPTER_NONE) ch = Ev->channel[i];
                            else FERS_ChIndex_tdc2ada(Ev->channel[i], &ch, FERS_INDEX(handle[b]));
                            stats.ReadHitCnt[b][ch].cnt++;
                            if (Ev->edge[i] == (EDGE_LEAD ^ this_cfg.InvertEdgePolarity[b][ch])) {
                                if ((ch == 0) && (b == 0 || this_cfg.TestMode)) Tref = Ev->tstamp_clk * CLK2LSB + Ev->ToA[i];
                                if (Lead[ch] == -1)	Lead[ch] = i;
                            } else {
                                if (Trail[ch] == -1) Trail[ch] = i;
                            }
                        }

                        for (ch = 0; ch < FERSLIB_MAX_NCH_5203; ch++) {
                            int IsChRef = ((ch == 0) && (b == 0));
                            if (Lead[ch] != -1) {
                                uint64_t abstime = Ev->tstamp_clk * CLK2LSB + Ev->ToA[Lead[ch]];
                                int tmp_DT = (AcqMode == ACQMODE_COMMON_START) ? (int)(abstime - Tref) : (int)(Tref - abstime);
                                if (tmp_DT < 0) { // Stop/Start out of the begin of the gate - (abstime < Tref && J_cfg.AcquisitionMode == ACQMODE_COMMON_START) || (abstime > Tref && J_cfg.AcquisitionMode == ACQMODE_COMMON_STOP)
                                    of_DeltaT[ch] = 0;
                                    of_ToTDeltaT[ch] = 0;
                                    continue;
                                }
                                if (IsChRef) of_DeltaT[ch] = Ev->ToA[Lead[ch]];
                                else of_DeltaT[ch] = tmp_DT;
                                of_DeltaT[ch] = of_DeltaT[ch] >> this_cfg.LeadTrail_Rescale;
                                if (IsChRef || ((of_DeltaT[ch] * this_cfg.LeadTrail_LSB_ns) <= ((uint32_t)(this_cfg.GateWidth)))) {
                                    stats.MatchHitCnt[b][ch].cnt++;
                                    if (Trail[ch] != -1) {
                                        uint32_t t_tot = (Ev->ToA[Trail[ch]] - Ev->ToA[Lead[ch]]) >> this_cfg.ToT_Rescale;
                                        of_ToTDeltaT[ch] = ((t_tot & 0xFFFF0000) >> 16 == 0) ? t_tot : 0xFFFF;
                                        stats.MatchHitCnt[b][ch].cnt++;
                                    } else {  //if (Event->ToT[Lead[ch]] > 0) {
                                        of_ToTDeltaT[ch] = Ev->ToT[Lead[ch]] >> this_cfg.ToT_Rescale;
                                        if (((this_cfg.MeasMode == MEASMODE_LEAD_TOT8) && (of_ToTDeltaT[ch] == 0xFF)) || ((this_cfg.MeasMode == MEASMODE_LEAD_TOT11) && (of_ToTDeltaT[ch] == 0x7FFF)))
                                            of_ToTDeltaT[ch] = 0xFFFF;
                                    }
                                    // Apply walk correction 
                                    // The correction can be applied to the ToT to correct the walk effect,
                                    // By appling a polynomial correction as a + b*ToT + c*ToT^2 + d*ToT^3 + e*ToT^4 + f*ToT^5 ... 
                                    // Check Janus for 5203 for more details

                                    if (!IsChRef && (of_DeltaT[ch] > 0)) AddMeasure(&stats.LeadMeas[b][ch], of_DeltaT[ch] * this_cfg.LeadTrail_LSB_ns);
                                    if (of_ToTDeltaT[ch] > 0) AddMeasure(&stats.ToTMeas[b][ch], of_ToTDeltaT[ch] * this_cfg.ToT_LSB_ns);

                                    // Histograms are not filled
                                    //if (!IsChRef) {
                                    //    DTb = Leadbin((uint32_t)of_DeltaT[ch], this_cfg);
                                    //    AddCount(&stats.H1_Lead[b][ch], DTb);
                                    //}
                                    //if (of_ToTDeltaT[ch] > 0) {
                                    //    ToTb = ToTbin(of_ToTDeltaT[ch], this_cfg);
                                    //    if (ToTb > 0) AddCount(&stats.H1_ToT[b][ch], ToTb);
                                    //}
                                }
                            } else {
                                of_DeltaT[ch] = 0;
                                of_ToTDeltaT[ch] = 0;
                            }
                        }
                    } else if (AcqMode == ACQMODE_TRG_MATCHING) {
                        uint32_t PrevL[FERSLIB_MAX_NCH_5203] = { 0 };
                        for (int i = 0; i < Ev->nhits; ++i) {
							if (this_cfg.AdapterType == ADAPTER_NONE) ch = Ev->channel[i];
							else FERS_ChIndex_tdc2ada(Ev->channel[i], &ch, FERS_INDEX(handle[b]));
                            stats.ReadHitCnt[b][ch].cnt++;
                            stats.MatchHitCnt[b][ch].cnt++;
                            if (Ev->edge[i] == EDGE_LEAD) {
                                AddMeasure(&stats.LeadMeas[b][ch], (Ev->ToA[i] >> this_cfg.LeadTrailRebin) * this_cfg.LeadTrail_LSB_ns);
                                PrevL[ch] = Ev->ToA[i];
                            }
							if (MEASMODE_OWLT(this_cfg.MeasMode)) {
								of_ToT[ch] = Ev->ToT[i] >> this_cfg.ToT_Rescale;
                                if (((this_cfg.MeasMode == MEASMODE_LEAD_TOT8) && (of_ToT[i] == 0xFF)) || ((this_cfg.MeasMode == MEASMODE_LEAD_TOT11) && (of_ToT[i] == 0x7FF)))
                                    of_ToT[i] = 0xFFFF;
                            } else if ((Ev->edge == EDGE_TRAIL) && (PrevL[ch] > 0)) {
                                uint32_t t_tot = (Ev->ToA[i] - PrevL[ch]) >> this_cfg.ToT_Rescale;
                                of_ToT[i] = ((t_tot & 0xFFFF0000) >> 16 == 0) ? t_tot : 0xFFFF; // If ToT rolls, set 0xFFFF
                            }
                            if (of_ToT[i] > 0) 
                                    AddMeasure(&stats.ToTMeas[b][ch], of_ToT[i] * this_cfg.ToT_LSB_ns);
                        }
                    } else if (AcqMode == ACQMODE_STREAMING) {
                        static uint64_t PrevL[FERSLIB_MAX_NCH_5203] = { 0 };
                        for (i = 0; i < Ev->nhits; i++) {
                            uint64_t ToA_64 = Ev->tstamp_clk * CLK2LSB + Ev->ToA[i];
                            if (this_cfg.AdapterType == ADAPTER_NONE) ch = Ev->channel[i];
                            else FERS_ChIndex_tdc2ada(Ev->channel[i], &ch, FERS_INDEX(handle[b]));
                            stats.ReadHitCnt[b][ch].cnt++;
                            stats.MatchHitCnt[b][ch].cnt++;
                            if (Ev->edge[i] == EDGE_LEAD) {
                                if (PrevL[ch] > 0) {
                                    uint64_t lead = (ToA_64 - PrevL[ch]) >> this_cfg.LeadTrail_Rescale;
                                    AddMeasure(&stats.LeadMeas[b][ch], lead * this_cfg.LeadTrail_LSB_ns);
                                }
                                PrevL[ch] = ToA_64;
                            } 
                            if (!MEASMODE_OWLT(this_cfg.MeasMode) && (Ev->edge[i] == EDGE_TRAIL) && (PrevL[ch] > 0)) {
                                uint64_t t_tot = ((ToA_64 - PrevL[ch]) >> this_cfg.ToT_Rescale);
                                of_ToT[i] = ((t_tot & 0xFFFFFFFFFFFF0000) >> 16 == 0) ? (uint16_t)t_tot : 0xFFFF;
                            } else {
                                of_ToT[i] = Ev->ToT[i] >> this_cfg.ToT_Rescale;
								// Avoid rollover in ToT
                                if (((this_cfg.MeasMode == MEASMODE_LEAD_TOT8) && (of_ToTDeltaT[i] == 0xFF)) || ((this_cfg.MeasMode == MEASMODE_LEAD_TOT11) && (of_ToTDeltaT[i] == 0x7FF)))
                                    of_ToT[i] = 0xFFFF;
                            }
                            if (of_ToT[i] > 0) {
                                    AddMeasure(&stats.ToTMeas[b][ch], of_ToT[i] * this_cfg.ToT_LSB_ns);
                            }
                        }
                    }
                } else if (dtq == DTQ_SERVICE) {
                    ServEvent5203_t* Ev = (ServEvent5203_t*)Event;
                    memcpy(&sEvt[b], Ev, sizeof(ServEvent5203_t));
                    stats.last_serv_tstamp_us[b] = Ev->tstamp_us;
                    stats.TotTrgCnt[b].cnt = Ev->TotTrg_cnt;
                    stats.LostTrgCnt[b].cnt = Ev->RejTrg_cnt;
                    stats.SupprTrgCnt[b].cnt = Ev->SupprTrg_cnt;
                }
            }
        }
        // Statistcs and Plot
        if ((curr_time - print_time) > 1000) {

            if (this_cfg.acq_status == ACQSTATUS_RUNNING) {
                if (clrscr) {
                    ClearScreen();
                    clrscr = 0;
                }
                gotoxy(1, 1);
            }

            for (int bb = 0; bb < this_cfg.num_brd; ++bb)
                Update_Service_Info(handle[bb]);

            char ss[FERSLIB_MAX_NCH_5202][10], totror[20], ror[20], trr[20], tottrr[20], hitr[20];  // torr[20], 
            rtime = (float)(curr_time - stats.start_time) / 1000;
            if (this_cfg.acq_status == ACQSTATUS_RUNNING) {
                UpdateStatistics(0);
                double totrate = 0;
                for (i = 0; i < this_cfg.num_brd; ++i) {
                    totrate += stats.ByteCnt[i].rate;
                    double2str(totrate, 1, totror);
                }
                double2str(stats.ByteCnt[stats_brd].rate, 1, ror);
                double2str(stats.ReadTrgCnt[stats_brd].rate, 1, trr);
                double2str(stats.TotTrgCnt[stats_brd].rate, 1, tottrr);
                double2str(stats.BoardHitCnt[stats_brd].rate, 1, hitr);

                printf("Elapsed Time: %10.2f s\n", rtime);

                double2str(stats.ByteCnt[0].rate, 1, ror);

                // Select Monitor Type
                for (i = 0; i < FERSLIB_MAX_NCH_5202; ++i) {
                    if (stats_mon == SMON_LEAD_MEAN) double2str(stats.LeadMeas[stats_brd][i].mean, 0, ss[i]);
					if (stats_mon == SMON_ToT_MEAN) double2str(stats.ToTMeas[stats_brd][i].mean, 0, ss[i]);

                    if (stats_mon == SMON_FULL_RATE) double2str(stats.ReadHitCnt[stats_brd][i].rate, 0, ss[i]);
                    if (stats_mon == SMON_HIT_RATE) double2str(stats.MatchHitCnt[stats_brd][i].rate, 0, ss[i]);  

                    if (stats_mon == SMON_FULL_CNT) double2str((double)stats.ReadHitCnt[stats_brd][i].cnt, 0, ss[i]);
                    if (stats_mon == SMON_HIT_CNT) double2str((double)stats.MatchHitCnt[stats_brd][i].cnt, 0, ss[i]);  
                }

                // Print Global statistics
                if (this_cfg.num_brd > 1) printf("Board n. %d (press [b] to change active board)\n", stats_brd);
                printf("Time Stamp:   %10.3lf s                \n", stats.current_tstamp_us[stats_brd] / 1e6);
                printf("Trigger-ID:   %10lld                   \n", stats.current_trgid[stats_brd]);
                printf("Trg Rate:        %scps           \n", tottrr);
                printf("  Processed:     %scps (%.2f %%)        \n", trr, stats.ProcTrgPerc[stats_brd]);
                printf("  Rejected:      %6.2lf %% (%" PRIu64 ")      \n", stats.LostTrgPerc[stats_brd], stats.LostTrgCnt[stats_brd].cnt);
                printf("  Empty Suppr:   %6.2lf %%             \n", stats.ZeroSupprTrgPerc[stats_brd]);
                printf("Brd Hit Rate:    %scps                 \n", hitr);

                if (this_cfg.num_brd > 1)
                    printf("Readout Rate:    %sB/s (Tot: %sB/s)             \n", ror, totror);
                else
                    printf("Readout Rate:    %sB/s                   \n", ror);

                if (BoardInfo[stats_brd].NumCh == 64)
                    printf("Temp (degC): FPGA=%4.1f Board=%4.1f TDC=%4.1f\n", BrdTemp[stats_brd][TEMP_FPGA], BrdTemp[stats_brd][TEMP_BOARD], BrdTemp[stats_brd][TEMP_TDC0]);
                else
                    printf("Temp (degC): FPGA=%4.1f Board=%4.1f TDC0=%4.1f TDC1=%4.1f\n", BrdTemp[stats_brd][TEMP_FPGA], BrdTemp[stats_brd][TEMP_BOARD], BrdTemp[stats_brd][TEMP_TDC0], BrdTemp[stats_brd][TEMP_TDC1]);


                printf("\n");

                if (stats_mon == SMON_LEAD_MEAN)    printf("Lead(ToA) Mean\n");
				if (stats_mon == SMON_ToT_MEAN)     printf("ToT Mean\n");

                if (stats_mon == SMON_FULL_RATE)    printf("Total Rate\n");
                if (stats_mon == SMON_HIT_RATE)     printf("Matched Hits Rate\n");  // 5203 Only

                if (stats_mon == SMON_FULL_CNT)     printf("Total Count\n");
                if (stats_mon == SMON_HIT_CNT)      printf("Matched Hits Count\n");  // 5203 Only

                // Aggiungerei la media del valore degli istogrammi

                // Print channels statistics
                for (i = 0; i < FERSLIB_MAX_NCH_5202; i++) {
                    printf("%02d: %s     ", i, ss[i]);
                    if ((i & 0x3) == 0x3) printf("\n");
                }
                printf("\n");
            }

            ////////////////////////////////////
            // Plot - only for PHA LG/HG 5202 //
            ////////////////////////////////////
            //if (this_cfg.acq_status == ACQSTATUS_RUNNING || stats_brd != tmp_brd || stats_ch != tmp_ch) {
            //    tmp_brd = stats_brd;
            //    tmp_ch = stats_ch;
            //    char ptitle[50];
            //    FILE* hist_file;
            //    hist_file = fopen("hist.txt", "w");
            //    for (int j = 0; j < this_cfg.EHistoNbin; ++j) {
            //        if (stats_plot == PLOT_E_SPEC_LG) {
            //            fprintf(hist_file, "%f\t%d\n", float(j), stats.H1_PHA_LG[stats_brd][stats_ch].H_data[j]);
            //            sprintf(ptitle, "Spect LG");
            //        } else if (stats_plot == PLOT_E_SPEC_HG) {
            //            fprintf(hist_file, "%f\t%d\n", float(j), stats.H1_PHA_HG[stats_brd][stats_ch].H_data[j]);
            //            sprintf(ptitle, "Spect HG");
            //        }
            //    }
            //    fclose(hist_file);
            //    if (plotter) {
            //        fprintf(plotter, "set xlabel 'Channels'\n");
            //        fprintf(plotter, "set ylabel 'Counts'\n");
            //        fprintf(plotter, "set title '%s'\n", ptitle);
            //        fprintf(plotter, "plot 'hist.txt' w boxes fs solid 0.7 title 'Brd:%d Ch:%d'\n", stats_brd, stats_ch);
            //        fflush(plotter);
            //    }

            //}
            print_time = curr_time;
        }

    }


QuitProgram:
    StopRun(&this_cfg);
    for (int b = 0; b < this_cfg.num_brd; ++b) {
        if (handle[b] < 0) break;
        //! [CloseReadout]
        FERS_CloseReadout(handle[b]);
        //! [CloseReadout]

        //! [Disconnect]
        FERS_CloseDevice(handle[b]);
        //! [Disconnect]
    }

    for (int b = 0; b < FERSLIB_MAX_NCNC; ++b) {
        if (cnc_handle[b] < 0) break;
        FERS_CloseDevice(cnc_handle[b]);
    }

    if (of_DeltaT != NULL) {
        free(of_DeltaT);
        free(of_ToTDeltaT);
    }
    if (of_ToA != NULL) {
        free(of_ToA);
        free(of_ToT);
    }

    //pclose(plotter);
    printf("Quitting ...\n");

    return 0;
}

