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

#ifndef _JanusC_H
#define _JanusC_H                    // Protect against multiple inclusion

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "MultiPlatform.h"

#ifdef _WIN32
	#define popen _popen
	#define DEFAULT_GNUPLOT_PATH "..\\gnuplot\\gnuplot.exe"
	#define GNUPLOT_TERMINAL_TYPE "wxt"
	#define PATH_SEPARATOR "\\"
	#define CONFIG_FILE_PATH ""
	#define DATA_FILE_PATH "."
	#define WORKING_DIR ""
	#define EXE_NAME "JanusC.exe"
#else
    #include <unistd.h>
    #include <stdint.h>   /* C99 compliant compilers: uint64_t */
    #include <ctype.h>    /* toupper() */
    #include <sys/time.h>
	#define DEFAULT_GNUPLOT_PATH	"gnuplot"
	#define GNUPLOT_TERMINAL_TYPE "x11"
	#define PATH_SEPARATOR "/"
	#define EXE_NAME "JanusC"
	#ifndef Sleep
		#define Sleep(x) usleep((x)*1000)
	#endif
	#ifdef _ROOT_
		#define CONFIG_FILE_PATH _ROOT_"/Janus/config/"
		#define DATA_FILE_PATH _ROOT_"/Janus/"
		#define WORKING_DIR _ROOT_"/Janus/"
	#else
		#define CONFIG_FILE_PATH ""
		#define DATA_FILE_PATH "./DataFiles"
		#define WORKING_DIR ""
	#endif
#endif

#define SW_RELEASE_NUM			"3.2.4"
#define SW_RELEASE_DATE			"22/08/2023"
#define FILE_LIST_VER			"3.2"

#ifdef _WIN32
#define CONFIG_FILENAME			"Janus_Config.txt"
#define RUNVARS_FILENAME		"RunVars.txt"
#define PIXMAP_FILENAME			"pixel_map.txt"
#else	// with eclipse the Debug run from Janus folder, and the executable in Janus/Debug
#define CONFIG_FILENAME			"Janus_Config.txt"
#define RUNVARS_FILENAME		"RunVars.txt"
#define PIXMAP_FILENAME			"pixel_map.txt"
#endif

//****************************************************************************
// Definition of limits and sizes
//****************************************************************************
#define MAX_NBRD						16	// max. number of boards 
#define MAX_NCNC						8	// max. number of concentrators
#define MAX_NCH							64	// max. number of channels 
#define MAX_GW							20	// max. number of generic write commads
#define MAX_NTRACES						8	// Max num traces in the plot

// *****************************************************************
// Parameter options
// *****************************************************************
#define OUTFILE_RAW_DATA_BIN			0x0001
#define OUTFILE_RAW_DATA_ASCII			0x0002
#define OUTFILE_LIST_BIN				0x0004
#define OUTFILE_LIST_ASCII				0x0008
#define OUTFILE_SPECT_HISTO				0x0010
#define OUTFILE_ToA_HISTO				0x0020
#define OUTFILE_TOT_HISTO				0x0040
#define OUTFILE_STAIRCASE				0x0080
#define OUTFILE_RUN_INFO				0x0100
#define OUTFILE_MCS_HISTO				0x0200
#define OUTFILE_SYNC					0x0400

#define PLOT_E_SPEC_LG					0
#define PLOT_E_SPEC_HG					1
#define PLOT_TOA_SPEC					2
#define PLOT_TOT_SPEC					3
#define PLOT_CHTRG_RATE					4
#define PLOT_WAVE						5
#define PLOT_2D_CNT_RATE				6
#define PLOT_2D_CHARGE_LG				7
#define PLOT_2D_CHARGE_HG				8
#define PLOT_SCAN_THR					9
#define PLOT_SCAN_HOLD_DELAY			10
#define PLOT_MCS_TIME					11	// DNIN those value have to be sorted later

#define SMON_CHTRG_RATE					0
#define SMON_CHTRG_CNT					1
#define SMON_HIT_RATE					2
#define SMON_HIT_CNT					3
#define SMON_PHA_RATE					4
#define SMON_PHA_CNT					5

#define STOPRUN_MANUAL					0
#define STOPRUN_PRESET_TIME				1
#define STOPRUN_PRESET_COUNTS			2

#define EVBLD_DISABLED					0
#define EVBLD_TRGTIME_SORTING			1
#define EVBLD_TRGID_SORTING				2

#define TEST_PULSE_DEST_ALL 			-1
#define TEST_PULSE_DEST_EVEN			-2
#define TEST_PULSE_DEST_ODD				-3
#define TEST_PULSE_DEST_NONE			-4

#define CITIROC_CFG_FROM_REGS			0
#define CITIROC_CFG_FROM_FILE			1

#define SCPARAM_MIN						0
#define SCPARAM_MAX						1
#define SCPARAM_STEP					2
#define SCPARAM_DWELL					3
										
#define HDSPARAM_MIN					0
#define HDSPARAM_MAX					1
#define HDSPARAM_STEP					2
#define HDSPARAM_NMEAN					3


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


//****************************************************************************
// struct that contains the configuration parameters (HW and SW)
//****************************************************************************
typedef struct Config_t {

	// System info 
	char ConnPath[MAX_NBRD][40];	// IP address of the board
	int FERSmodel;					// Type of FERS board
	int NumBrd;                     // Tot number of connected boards
	int NumCh;						// Number of channels 
	int SerialNumber;				// Board serial number

	int DebugLogMask;				// Enable debug messages or log files
	int EnLiveParamChange;			// Enable param change while running (when disabled, Janus will stops and restarts the acq. when a param changes)
	int AskHVShutDownOnExit;		// Ask if the HV must be shut down before quitting
	int OutFileEnableMask;			// Enable/Disable output files 
	char DataFilePath[500];			// Output file data path
	uint8_t OutFileUnit;			// Unit for time measurement in output files (0 = LSB, 1 = ns)
	int EnableJobs;					// Enable Jobs
	int JobFirstRun;				// First Run Number of the job
	int JobLastRun;					// Last Run Number of the job
	float RunSleep;					// Wait time between runs of one job
	int StartRunMode;				// Start Mode (this is a HW setting that defines how to start/stop the acquisition in the boards)
	int StopRunMode;				// Stop Mode (unlike the start mode, this is a SW setting that deicdes the stop criteria)
	int RunNumber_AutoIncr;			// auto increment run number after stop
	float PresetTime;				// Preset Time (Real Time in s) for auto stop
	int PresetCounts;				// Preset Number of counts (triggers)
	int EventBuildingMode;			// Event Building Mode
	int TstampCoincWindow;			// Time window (ns) in event buiding based on trigger Tstamp

	int EHistoNbin;					// Number of channels (bins) in the Energy Histogram
	int ToAHistoNbin;				// Number of channels (bins) in the ToA Histogram
	int8_t ToARebin;				// Rebin factor for ToA histogram. New bin = 0.5*Rebin ns
	float ToAHistoMin;				// Minimum time value for ToA Histogram. Maximum is Min+0.5*Rebin*Nbin
	int ToTHistoNbin;				// Number of channels (bins) in the ToT Histogram
	int MCSHistoNbin;				// Number of channels (bins) in the MCS Histogram

	int CitirocCfgMode;				// 0=from regs, 1=from file
	uint16_t Pedestal;				// Common pedestal added to all channels

	//                                                                       
	// Acquisition Setup (HW settings)
	//                                                                       
	// Board Settings
	uint32_t AcquisitionMode;						// ACQMODE_COUNT, ACQMODE_SPECT, ACQMODE_TIMING, ACQMODE_WAVE
	uint32_t EnableToT;								// Enable readout of ToT (time over threshold)
	uint32_t TimingMode;							// COMMON_START, COMMON_STOP, STREAMING
	uint32_t EnableServiceEvents;					// Enable service events. 0=disabled, 1=enabled HV, 2=enabled counters, 3=enabled all
	uint32_t EnableCntZeroSuppr;					// Enable zero suppression in Counting Mode (1 by default)
	uint32_t SupprZeroCntListFile;		    			// 
	uint32_t TriggerMask;							// Bunch Trigger mask
	uint32_t TriggerLogic;							// Trigger Logic Definition
	uint32_t T0_outMask[MAX_NBRD];					// T0-OUT mask
	uint32_t T1_outMask[MAX_NBRD];					// T1-OUT mask
	uint32_t Tref_Mask;								// Tref mask
	uint32_t Veto_Mask;								// Veto mask
	uint32_t Validation_Mask;						// Validation mask
	uint32_t Validation_Mode;						// Validation Mode: 0=disabled, 1=positive (accept), 2=negative (reject)
	uint32_t Counting_Mode;							// Counting Mode (Singles, Paired_AND)
	uint32_t TrefWindow;							// Tref Windows in ns (Common start/stop)
	float TrefDelay;								// Tref delay in ns (can be negative)
	uint32_t PairedCnt_CoincWin;					// Self Trg Width in ns => Coinc window for the paired counting
	uint32_t MajorityLevel;							// Majority Level
	uint32_t PtrgPeriod;							// period in ns of the internal periodic trigger (dwell time)
	uint32_t TestPulseSource;						// EXT, INT_T0, INT_T1, INT_PTRG, INT_SW
	uint32_t TestPulseDestination;					// -1=ALL, -2=EVEN, -3=ODD or channel number (0 to 63) for single channel pulsing
	uint32_t TestPulsePreamp;						// 1=LG, 2=HG, 3=BOTH
	uint32_t TestPulseAmplitude;					// DAC setting for the internal test pulser (12 bit). Meaningless for TestPulseSource=EXT 
	uint32_t WaveformLength;						// Num of samples in the waveform
	uint32_t WaveformSource;						// LG0, HG0, LG1, HG1 (High/Low Gain, chip 0/1)
	uint32_t AnalogProbe[2];						// Analog probe in Citiroc (Preamp LG/HG, Slow Shaper HG/LG, Fast Shaper)
	uint32_t DigitalProbe[2];						// Citiroc digital probe (peak Sens HG/LG) or other FPGA signal (start_conv, data_commit...)
	uint32_t ProbeChannel[2];						// Probing channel
	uint32_t Range_14bit;							// Use full 14 bit range for the A/D conversion
	uint32_t TrgIdMode;								// Trigger ID: 0 = trigger counter, 1 = validation counter

	uint32_t ChEnableMask0[MAX_NBRD];				// Channel enable mask of Citiroc 0
	uint32_t ChEnableMask1[MAX_NBRD];				// Channel enable mask of Citiroc 1
	uint32_t Q_DiscrMask0[MAX_NBRD];				// Charge Discriminator mask of Citiroc 0
	uint32_t Q_DiscrMask1[MAX_NBRD];				// Charge Discriminator mask of Citiroc 1
	uint32_t Tlogic_Mask0[MAX_NBRD];				// Trigger Logic mask of Citiroc 0
	uint32_t Tlogic_Mask1[MAX_NBRD];				// Trigger Logic mask of Citiroc 1

    uint32_t QD_CoarseThreshold;					// Coarse Threshold for Citiroc charge discriminator
    uint32_t TD_CoarseThreshold[MAX_NBRD];			// Coarse Threshold for Citiroc time discriminator
    uint32_t HG_ShapingTime;						// Shaping Time of the High Gain preamp
    uint32_t LG_ShapingTime;						// Shaping Time of the Low Gain preamp
	uint32_t Enable_HV_Adjust;						// Enable input DAC for HV fine adjust
	uint32_t HV_Adjust_Range;						// HV adj DAC range (reference): 0 = 2.5V, 1 = 4.5V
	uint32_t MuxClkPeriod;							// Period of the Mux Clock
	uint32_t MuxNSmean;								// Num of samples for the Mux mean: 0: 4 samples, 1: 16 samples
	uint32_t HoldDelay;								// Time between Trigger and Hold
	uint32_t GainSelect;							// Select gain between High/Low/Auto
	uint32_t PeakDetectorMode;						// Peaking Mode: 0 = Peak Stretcher, 1 = Track&Hold
	uint32_t EnableQdiscrLatch;						// Q-dicr mode: 1 = Latched, 0 = Direct
	uint32_t EnableChannelTrgout;					// 0 = Channel Trgout Disabled, 1 = Enabled
	uint32_t FastShaperInput;						// Fast Shaper (Tdiscr) connection: 0 = High Gain PA, 1 = Low Gain PA
	uint32_t Trg_HoldOff;							// Trigger hold off (applied to channel triggers)

	float FiberDelayAdjust[MAX_NCNC][8][16];		// Fiber length (in meters) for individual tuning of the propagation delay along the TDL daisy chains

	float HV_Vbias[MAX_NBRD];						// Voltage setting for HV
	float HV_Imax[MAX_NBRD];						// Imax for HV
	float TempSensCoeff[3];							// Temperature Sensor Coefficients (2=quad, 1=lin, 0=offset)
	float TempFeedbackCoeff;						// Temperature Feedback Coeff: Vout = Vset - k * (T-25)
	int EnableTempFeedback;							// Enable Temp Feedback

	// Channel Settings 
    uint16_t ZS_Threshold_LG[MAX_NBRD][MAX_NCH];	// Low Threshold for zero suppression (LG)
    uint16_t ZS_Threshold_HG[MAX_NBRD][MAX_NCH];	// Low Threshold for zero suppression (HG)
    uint16_t QD_FineThreshold[MAX_NBRD][MAX_NCH];	// Fine Threshold for Citiroc charge discriminator
    uint16_t TD_FineThreshold[MAX_NBRD][MAX_NCH];	// Fine Threshold for Citiroc time discriminator
    uint16_t HG_Gain[MAX_NBRD][MAX_NCH];			// Gain fo the High Gain Preamp
    uint16_t LG_Gain[MAX_NBRD][MAX_NCH];			// Gain fo the Low Gain Preamp
	uint16_t HV_IndivAdj[MAX_NBRD][MAX_NCH];		// HV individual bias adjust (Citiroc 8bit input DAC)

	// Generic write accesses 
	int GWn;
    uint32_t GWbrd[MAX_GW];						// Board Number (-1 = all)
    uint32_t GWaddr[MAX_GW];					// Register Address
    uint32_t GWdata[MAX_GW];					// Data to write
    uint32_t GWmask[MAX_GW];					// Bit Mask
 
} Config_t;


typedef struct RunVars_t {
	int ActiveBrd;				// Active board
	int ActiveCh;				// Active channel
	int PlotType;				// Plot Type
	int SMonType;				// Statistics Monitor Type
	int Xcalib;					// X-axis calibration
	int RunNumber;				// Run Number (used in output file name; -1 => no run number)
	char PlotTraces[8][100];	// Plot Traces (format: "0 3 X" => Board 0, Channel 3, From X[B board - F offline - S from file)
	int StaircaseCfg[4];		// Staircase Params: MinThr Maxthr Step Dwell
	int HoldDelayScanCfg[4];	// Hold Delay Scan Params: MinDelay MaxDelay Step Nmean
} RunVars_t;




//****************************************************************************
// Global Variables
//****************************************************************************
extern Config_t	WDcfg;				// struct containing all acquisition parameters
extern RunVars_t RunVars;			// struct containing run time variables
extern int handle[MAX_NBRD];		// board handles
//extern int ActiveCh, ActiveBrd;		// Board and Channel active in the plot
extern int AcqRun;					// Acquisition running
extern int AcqStatus;				// Acquisition Status
extern int SockConsole;				// 0: use stdio console, 1: use socket console
extern char ErrorMsg[250];			// Error Message

#endif
