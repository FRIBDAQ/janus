/*******************************************************************************
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

#include "FERSutils.h"
#include "Statistics.h"
#include "JanusC.h"

#include "plot.h"
#include "console.h"
#include "paramparser.h"
//#include "configure.h"
#include "outputfiles.h"

#include <stdlib.h>
#include <time.h>

//#include "FERSlib.h"

// ******************************************************************************************
// Global Variables
// ******************************************************************************************
Janus_Config_t J_cfg;					// struct with all parameters
RunVars_t RunVars;						// struct containing run time variables
ServEvent_t sEvt[MAX_NBRD];		// struct containing the service information of each boards
int SockConsole = 0;					// 0=stdio, 1=socket
int AcqStatus = 0;						// Acquisition Status (running, stopped, fail, etc...)
int HoldScan_newrun = 0;
int handle[MAX_NBRD];			// Board handles
int cnc_handle[FERSLIB_MAX_NCNC];		// Concentrator handles
int UsingCnc = 0;						// Using concentrator
int Freeze = 0;							// stop plot
int OneShot = 0;						// Single Plot 
int StatMode = 0;						// Stats monitor: 0=per board, 1=all baords
int StatIntegral = 0;					// Stats caclulation: 0=istantaneous, 1=integral
int Quit = 0;							// Quit readout loop
int RestartAcq = 0;						// Force restart acq (reconfigure HW)
int RestartAll = 0;						// Restart from the beginning (recovery after error)
int SkipConfig = 0;						// Skip board configuration
int first_sEvt = 0;						// Skip check on the first service event in Update_Service_Event
int offline_conn = 0;					// Offline connection for raw data reprocessing
int plot_changed = 0;					// Active when the plot type is changed
uint8_t offline_plot = 0;				// Offline plot in RunVars. Enable plot visualization when boards are stopped
double build_time_us = 0;				// Time of the event building (in us)
int En_HVstatus_Update = 0;				// Enable periodic update of HV status

// Service Event Variable
float BrdTemp[MAX_NBRD][4] = { 0. };		// Board Temperatures (near PIC/FPGA)
float HVMon[MAX_NBRD][2] = { 0. };		// V and I monitor from HV module
uint32_t StatusReg[MAX_NBRD];	// Acquisition Status Register
int HV_status[MAX_NBRD] = { 0 };// HV status; bit 0 = ON/OFF, bit 1 = Over current/voltage, bit 2 = ramping up/down
char ErrorMsg[250];						// Error Message
FILE* MsgLog = NULL;					// message log output file

int jobrun = 0;
uint8_t stop_sw = 0; // Used to disable automatic start during jobs

int ServerDead = 0;
uint8_t sEvt_missing = 0;
uint8_t wMsg_sent = 0, eMsg_sent = 0, is_running = 0;
int brdInFail[MAX_NBRD] = { 0 };

float TrefWindow = 0;

int MAX_NBRD_JANUS = 0;				// Max number of boards supported by Janus, 16 in GUI mode, 128 in console mode

// ******************************************************************************************
// Save/Load run time variables, utility functions...
// ******************************************************************************************
int LoadRunVariables(RunVars_t* RunVars)
{
	char str[100];
	int i;
	FILE* ps = fopen(RUNVARS_FILENAME, "r");
	//char plot_name[50];	// name of histofile.

	// set defaults
	offline_plot = 0;
	plot_changed = 0;
	int plotType = RunVars->PlotType;
	RunVars->PlotType = PLOT_E_SPEC_HG;
	RunVars->SMonType = SMON_CHTRG_RATE;
	RunVars->RunNumber = 0;
	RunVars->ActiveCh = 0;
	RunVars->ActiveBrd = 0;
	for (i = 0; i < 4; i++) RunVars->StaircaseCfg[i] = 0;
	for (i = 0; i < 4; i++) RunVars->HoldDelayScanCfg[i] = 0;
	for (int i = 0; i < 8; i++)
		RunVars->PlotTraces[i][0] = '\0';
	if (ps == NULL) return -1;
	//for (i = 0; i < MAX_NTRACES; ++i)  Stats.offline_bin[i] = -1; // Reset offline binning
	while (!feof(ps)) {
		fscanf(ps, "%s", str);

		if (strcmp(str, "ActiveBrd") == 0)	fscanf(ps, "%d", &RunVars->ActiveBrd);
		else if (strcmp(str, "ActiveCh") == 0)	fscanf(ps, "%d", &RunVars->ActiveCh);
		else if (strcmp(str, "PlotType") == 0)	fscanf(ps, "%d", &RunVars->PlotType);
		else if (strcmp(str, "SMonType") == 0)	fscanf(ps, "%d", &RunVars->SMonType);
		else if (strcmp(str, "RunNumber") == 0)	fscanf(ps, "%d", &RunVars->RunNumber);
		else if (strcmp(str, "Xcalib") == 0)	fscanf(ps, "%d", &RunVars->Xcalib);
		else if (strcmp(str, "PlotTraces") == 0) {	// DNIN: if you set board > connectedBoard you see the trace of the last connected board
			int v1, v2, v3, nr;
			char c1[100];
			nr = fscanf(ps, "%d", &v1);
			if (nr < 1) break;
			fscanf(ps, "%d %d %s", &v2, &v3, c1);
			if ((v1 >= 0) && (v1 < 8) && (v2 >= 0) && (v2 < MAX_NBRD_JANUS) && (v3 >= 0) && (v3 < MAX_NCH) && (c1[0] == 'B' || c1[0] == 'F' || c1[0] == 'S'))
				sprintf(RunVars->PlotTraces[v1], "%d %d %s", v2, v3, c1);
			else
				continue;
			//char tmp_var[50];
			//strcpy(tmp_var, Stats.H1_PHA_LG[v2][v3].title);
			if (Stats.H1_File[0].H_data == NULL) // Check if statistics have been created
				continue;

			Stats.offline_bin = -1; // Reset offline binning

			if ((c1[0] == 'F' || c1[0] == 'S') && (RunVars->PlotType < 4 || RunVars->PlotType == 9)) {
				offline_plot = 1;
				Histo1D_Offline(v1, v2, v3, c1, RunVars->PlotType);
			}
			else
				continue;
		}
		else if (strcmp(str, "Staircase") == 0) {
			for (i = 0; i < 5; i++)
				fscanf(ps, "%d", &RunVars->StaircaseCfg[i]);
		}
		else if (strcmp(str, "HoldDelayScan") == 0) {
			for (i = 0; i < 5; i++)
				fscanf(ps, "%d", &RunVars->HoldDelayScanCfg[i]);
		}
	}
	if (RunVars->ActiveBrd >= J_cfg.NumBrd) RunVars->ActiveBrd = 0;
	fclose(ps);

	if (plotType != RunVars->PlotType)
		plot_changed = 1;

	return 0;
}

int SaveRunVariables(RunVars_t RunVars)
{
	bool no_traces = 1;
	FILE* ps = fopen(RUNVARS_FILENAME, "w");
	if (ps == NULL) return -1;
	fprintf(ps, "ActiveBrd     %d\n", RunVars.ActiveBrd);
	fprintf(ps, "ActiveCh      %d\n", RunVars.ActiveCh);
	fprintf(ps, "PlotType      %d\n", RunVars.PlotType);
	fprintf(ps, "SMonType      %d\n", RunVars.SMonType);
	fprintf(ps, "RunNumber     %d\n", RunVars.RunNumber);
	fprintf(ps, "Xcalib        %d\n", RunVars.Xcalib);
	for (int i = 0; i < 8; i++) {
		if (strlen(RunVars.PlotTraces[i]) > 0) {
			no_traces = 0;
			fprintf(ps, "PlotTraces		%d %s  \n", i, RunVars.PlotTraces[i]);
		}
	}
	if (no_traces) {	// default 
		fprintf(ps, "PlotTraces		0 0 0 B\n");
	}

	if (RunVars.StaircaseCfg[4] > 0)
		fprintf(ps, "Staircase     %d %d %d %d %d\n", RunVars.StaircaseCfg[0], RunVars.StaircaseCfg[1], RunVars.StaircaseCfg[2], RunVars.StaircaseCfg[3], RunVars.StaircaseCfg[4]);
	if (RunVars.HoldDelayScanCfg[4] > 0)
		fprintf(ps, "HoldDelayScan %d %d %d %d %d\n", RunVars.HoldDelayScanCfg[0], RunVars.HoldDelayScanCfg[1], RunVars.HoldDelayScanCfg[2], RunVars.HoldDelayScanCfg[3], RunVars.HoldDelayScanCfg[4]);
	fclose(ps);
	return 0;
}


void plot_histogram()
{
	if (((RunVars.PlotType == PLOT_E_SPEC_LG) ||
		(RunVars.PlotType == PLOT_E_SPEC_HG) ||
		(RunVars.PlotType == PLOT_TOA_SPEC) ||
		(RunVars.PlotType == PLOT_TOT_SPEC) ||
		(RunVars.PlotType == PLOT_MCS_TIME)) &&
		(offline_plot || (AcqStatus == ACQSTATUS_RUNNING) || plot_changed)) {
		PlotSpectrum();
		plot_changed = 0;
	} else if (((RunVars.PlotType == PLOT_2D_CNT_RATE) ||
		(RunVars.PlotType == PLOT_2D_CHARGE_LG) ||
		(RunVars.PlotType == PLOT_2D_CHARGE_HG)) &&
		(offline_plot || (AcqStatus == ACQSTATUS_RUNNING) || plot_changed)) {
		Plot2Dmap(StatIntegral);
		plot_changed = 0;
	} else if ((RunVars.PlotType == PLOT_CHTRG_RATE) &&
		(offline_plot || (AcqStatus == ACQSTATUS_RUNNING) || plot_changed)) {
		PlotCntHisto();
		plot_changed = 0;
	}
}

// Parse the config file of a run during a job. If the corresponding cfg file is not found, the default config is parsed
void increase_job_run_number() {
	if (jobrun < J_cfg.JobLastRun) jobrun++;
	else jobrun = J_cfg.JobFirstRun;
	RunVars.RunNumber = jobrun;
	SaveRunVariables(RunVars);
	stop_sw = 1;
}

void job_read_parse()
{
	FILE* cfg;
	if (J_cfg.EnableJobs) {  // When running a job, load the specific config file for this run number
		int TmpJRun = J_cfg.JobFirstRun;
		char fname[530];
		sprintf(fname, "%sJanus_Config_Run%d.txt", J_cfg.DataFilePath, jobrun);
		cfg = fopen(fname, "r");
		if (cfg != NULL) {
			ParseConfigFile(cfg, &J_cfg, PARSEMODE_PARSE_ALL | PARSEMODE_FIRST_CALL);
			fclose(cfg);
			J_cfg.JobFirstRun = TmpJRun;	// The first run cannot be overwrite from the config file
		} else {
			sprintf(fname, "Janus_Config.txt");
			cfg = fopen(fname, "r");
			if (cfg != NULL) {
				ParseConfigFile(cfg, &J_cfg, PARSEMODE_PARSE_ALL | PARSEMODE_FIRST_CALL);
				fclose(cfg);
			}
		}
	}
}


// Convert a double in a string with unit (k, M, G)
void double2str(double f, int space, char* s)
{
	if (!space) {
		if (f <= 999.999)			sprintf(s, "%7.3f ", f);
		else if (f <= 999999)		sprintf(s, "%7.3fk", f / 1e3);
		else if (f <= 999999000)	sprintf(s, "%7.3fM", f / 1e6);
		else						sprintf(s, "%7.3fG", f / 1e9);
	}
	else {
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

void SendAcqStatusMsg(char* fmt, ...)
{
	char msg[1000];
	va_list args;

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	if (SockConsole) Con_printf("Sa", "%02d%s", AcqStatus, msg);
	else {
		if (strstr(msg, "ERROR"))
			Con_printf("Ce", "%-70s\n", msg);
		else
			Con_printf("C", "%-70s\n", msg);
	}
}


// Update service event info
int Update_Service_Info(int handle) {
	int brd = FERS_INDEX(handle);
	int b_on = -1, s_on = -1, ovc = -1, ovv = -1, ramp = -1, ret = 0;
	static int first_call = 1;
	uint64_t now = j_get_time();

	if (first_sEvt >= 1 && first_sEvt < 5) {	// Skip the check on the first event service missing
		++first_sEvt;
		return 0;
	} else if (first_sEvt == 5) {
		first_sEvt = 0;
		return 0;
	}

	if (sEvt[brd].update_time > (now - 2000) && J_cfg.EnableServiceEvent) {
		HVMon[brd][HV_VMON] = sEvt[brd].hv_Vmon;
		HVMon[brd][HV_IMON] = sEvt[brd].hv_Imon;
		BrdTemp[brd][TEMP_DETECTOR] = sEvt[brd].tempDetector;
		BrdTemp[brd][TEMP_HV] = sEvt[brd].tempHV;
		BrdTemp[brd][TEMP_FPGA] = sEvt[brd].tempFPGA;
		BrdTemp[brd][TEMP_BOARD] = sEvt[brd].tempBoard;
		b_on = sEvt[brd].hv_status_on;
		ramp = sEvt[brd].hv_status_ramp;
		ovc = sEvt[brd].hv_status_ovc;
		ovv = sEvt[brd].hv_status_ovv;
		StatusReg[brd] = sEvt[brd].Status;
		WriteTempHV(now, sEvt);
		//BrdTemp[brd][TEMP_BOARD] = sEvt[brd].tempBoard;
		//BrdTemp[brd][TEMP_FPGA] = sEvt[brd].tempFPGA;
		
	} else {
		ret |= FERS_HV_Get_Vmon(handle, &HVMon[brd][HV_VMON]);
		ret |= FERS_HV_Get_Imon(handle, &HVMon[brd][HV_IMON]);
		ret |= FERS_HV_Get_DetectorTemp(handle, &BrdTemp[brd][TEMP_DETECTOR]);
		ret |= FERS_HV_Get_IntTemp(handle, &BrdTemp[brd][TEMP_HV]);
		ret |= FERS_HV_Get_Status(handle, &b_on, &ramp, &ovc, &ovv);
		ret |= FERS_Get_FPGA_Temp(handle, &BrdTemp[brd][TEMP_FPGA]);
		ret |= FERS_Get_Board_Temp(handle, &BrdTemp[brd][TEMP_BOARD]);
		//ret |= FERS_Get_FPGA_Temp(handle, &BrdTemp[brd][TEMP_FPGA]);
		//ret |= FERS_Get_Board_Temp(handle, &BrdTemp[brd][TEMP_BOARD]);
		ret |= FERS_ReadRegister(handle, a_acq_status, &StatusReg[brd]);
		if (AcqStatus == ACQSTATUS_RUNNING && J_cfg.EnableServiceEvent && !sEvt_missing) {
			Con_printf("LCSp", "Brd %d Service Event Missing. AcqStatus = 0x%08X (ret = %d)\n", FERS_INDEX(handle), StatusReg[brd], ret);	// WARNING
			sEvt_missing = 1;
		}
	}

	float vset = J_cfg.HV_Vbias[brd];
	if (first_call)
		HV_status[brd] = b_on | ((ovc | ovv) << 1);
	else if (b_on == 0) // HV is OFF
		HV_status[brd] = (ovc | ovv) << 1;
	else if (s_on == 0) // HV is ON, but OFF command has been sent => ramping down
		HV_status[brd] = ((ovc | ovv) << 1) | 0x4;
	else if (HVMon[brd][HV_VMON] < (0.95 * vset)) // HV is ON, but Vmon < (0.95*Vset) => ramping up
		HV_status[brd] = 0x1 | ((ovc | ovv) << 1) | 0x4;
	else // HV is ON and Vmon = Vset
		HV_status[brd] = 0x1 | ((ovc | ovv) << 1);

	if (brd == J_cfg.NumBrd - 1) first_call = 0; // when GUI re-connect found for all the board the correct status, elsewhere HV_status for brd!=0 is 4

	return ret;
}


// Send HV info to socket
void Send_HV_Info(int call_update_service)
{
	char tmp_brdhv[1024] = "";
//	float vmon = -1, imon = -1, dtemp = -1, itemp = -1, fpga_temp = -1, pcb_temp = -1;
	int ret = 0;
	static int first_call = 1;
	//int brd = FERS_INDEX(handle);
	//uint64_t now = j_get_time();

	for (int brd = 0; brd < J_cfg.NumBrd; ++brd) {
		//float vset = J_cfg.HV_Vbias[brd];
		//if (sEvt[brd].update_time > (now - 2000)) {
		//	vmon = sEvt[brd].hv_Vmon;
		//	imon = sEvt[brd].hv_Imon;
		//	dtemp = sEvt[brd].tempDetector;
		//	itemp = sEvt[brd].tempHV;
		//	fpga_temp = sEvt[brd].tempFPGA;
		//	pcb_temp = sEvt[brd].tempBoard;
		//	b_on = sEvt[brd].hv_status_on;
		//	ramp = sEvt[brd].hv_status_ramp;
		//	ovc = sEvt[brd].hv_status_ovc;
		//	ovv = sEvt[brd].hv_status_ovv;
		//	status = sEvt[brd].Status;
		//	WriteTempHV(now, sEvt);
		//} else {
		//	ret |= FERS_HV_Get_Vmon(handle[brd], &vmon);
		//	ret |= FERS_HV_Get_Imon(handle[brd], &imon);
		//	ret |= FERS_HV_Get_DetectorTemp(handle[brd], &dtemp);
		//	ret |= FERS_HV_Get_IntTemp(handle[brd], &itemp);
		//	ret |= FERS_HV_Get_Status(handle[brd], &b_on, &ramp, &ovc, &ovv);
		//	ret |= FERS_Get_FPGA_Temp(handle[brd], &fpga_temp);
		//	ret |= FERS_Get_Board_Temp(handle[brd], &pcb_temp);
		//}
		//s_on = HV_status[brd] & 1; // s_on = "on" from local status; b_on = "on" from board
		//if (first_call)
		//	HV_status[brd] = b_on | ((ovc | ovv) << 1);
		//else if (b_on == 0) // HV is OFF
		//	HV_status[brd] = (ovc | ovv) << 1;
		//else if (s_on == 0) // HV is ON, but OFF command has been sent => ramping down
		//	HV_status[brd] = ((ovc | ovv) << 1) | 0x4;
		//else if (vmon < (0.95 * vset)) // HV is ON, but Vmon < (0.95*Vset) => ramping up
		//	HV_status[brd] = 0x1 | ((ovc | ovv) << 1) | 0x4;
		//else // HV is ON and Vmon = Vset
		//	HV_status[brd] = 0x1 | ((ovc | ovv) << 1);
		if (call_update_service)
			ret |= Update_Service_Info(handle[brd]);

		if (brd == J_cfg.NumBrd - 1) first_call = 0; // when GUI re-connect found for all the board the correct status, elsewhere HV_status for brd!=0 is 4
		if (brd > 0) strcat(tmp_brdhv, " |");
		char tmp_line[100];
		snprintf(tmp_line, sizeof(tmp_line), " %d %d %6.3f %6.3f %5.1f %5.1f %5.1f %5.1f", brd, HV_status[brd], HVMon[brd][HV_VMON], HVMon[brd][HV_IMON], BrdTemp[brd][TEMP_DETECTOR], BrdTemp[brd][TEMP_HV], BrdTemp[brd][TEMP_FPGA], BrdTemp[brd][TEMP_BOARD]);
		strcat(tmp_brdhv, tmp_line);
		//if (fpga_temp<200) sprintf(tmp_brdhv, "%s %d %d %6.3f %6.3f %5.1f %5.1f %5.1f %5.1", tmp_brdhv, brd, HV_status[brd], vmon, imon, dtemp, itemp, fpga_temp, pcb_temp);
		//else sprintf(tmp_brdhv, "%s %d %d %6.3f %6.3f %5.1f %5.1f -1", tmp_brdhv, brd, HV_status[brd], vmon, imon, dtemp, itemp);
	}
	Con_printf("Sh", "%s\n", tmp_brdhv);
}

// HV set ON/OFF and control ramp
int HV_Switch_OnOff(int handle, int onoff)
{
	int Err = 0, pstat = AcqStatus, b_on, ramp, ovc, ovv, hv_status;
	int brd = FERS_INDEX(handle);
	float vmon, imon, vset, imax;
	float dtemp, itemp, fpga_temp, pcb_temp;
	
	vset = J_cfg.HV_Vbias[brd];
	imax = FERS_GetParam_float(handle, "HV_Imax");

	FERS_HV_Get_DetectorTemp(handle, &dtemp);
	FERS_HV_Get_IntTemp(handle, &itemp);
	FERS_Get_FPGA_Temp(handle, &fpga_temp);
	FERS_Get_Board_Temp(handle, &pcb_temp);

	if (onoff == (HV_status[brd] & 1)) return 0;
	HV_status[brd] = (HV_status[brd] & 0xFE) | onoff;
	AcqStatus = ACQSTATUS_RAMPING_HV;
	FERS_HV_Set_Vbias(handle, vset);
	FERS_HV_Set_Imax(handle, imax);
	FERS_HV_Set_OnOff(handle, onoff);
	//for (i = 0; i < 50 && !Err; i++) 
	while (!Err) {
		for (int j = 0; j < 10; j++) {
			FERS_HV_Get_Vmon(handle, &vmon);
			FERS_HV_Get_Imon(handle, &imon);
			if ((vmon > 0) && (vmon < 85) && (imon >= 0) && (imon < 11)) break; // patch for occasional wrong reading
			Sleep(100);
		}
		FERS_HV_Get_Status(handle, &b_on, &ramp, &ovc, &ovv);
		hv_status = (ovc | ovv) ? 0x2 | 0x4 : onoff | 0x4;
		Con_printf("Sh", " %d %d %6.3f %6.3f %5.1f %5.1f %5.1f %5.1f\n", FERS_INDEX(handle), hv_status, vmon, imon, dtemp, itemp, fpga_temp, pcb_temp);
		if (ovc) {
			SendAcqStatusMsg("HV Brd %d Over Current! Shutting down...", brd);
			Con_printf("LCSe", "ERROR: failed to set HV of Brd %d for Over Current\n", brd);
			Err = 1;
		}
		else if (ovv) {
			SendAcqStatusMsg("HV Brd %d Over Voltage! Shutting down...", brd);
			Con_printf("LCSe", "ERROR: failed to set HV of Brd %d for Over Voltage\n", brd);
			Err = 1;
		}
		else if (onoff == 0) {
			SendAcqStatusMsg("HV Brd %d Ramping down: %.3f V, %.3f mA", brd, vmon, imon);
			if (vmon < (vset - 5)) break;
		}
		else {
			SendAcqStatusMsg("HV Brd %d Ramping up: %.3f V, %.3f mA", brd, vmon, imon);
			if (fabs(vmon - vset) < 1) break;
		}
		Sleep(200);
	}
	if (Err) {
		Sleep(1500);
	} else {
		Sleep(300);
		FERS_HV_Get_Vmon(handle, &vmon);
		FERS_HV_Get_Imon(handle, &imon);
		if (onoff) {
			Con_printf("LCSm", "HV of Brd %d is ON. Vset = %.3f V, Vmon = %.3f V, Imon = %.3f mA\n", brd, vset, vmon, imon);
			// set a global ON/OFF HV status
		}
		else Con_printf("LCSm", "HV of Brd %d is turning OFF (Vmon = %.3f V)\n", brd, vmon);	// DNIN: Lorenzo suggests to remove Vmon infos
	}
	Send_HV_Info(1);
	AcqStatus = pstat;
	return 0;
}


void HVLimitCheck(int *handle) 
{
	char w_msg[1024] = "";
	char w_val[1000] = "HV_Vbias:";
	for (int b = 0; b < J_cfg.NumBrd; ++b) {
		float Vbias = J_cfg.HV_Vbias[b];
		if (Vbias < 20) {
			sprintf(w_msg, "%sWARNING: HV Vbias : HV Vbias board %d = %.2f V out of lower bound (20 V, 85 V). HV Vbias value set to 20 V\n", w_msg, b, Vbias);
			Vbias = 20;
			FERS_SetParam(handle[b], "HV_Vbias", "20");
			sprintf(w_val, "%s%d %2.0f,", w_val, b, Vbias);
			//if (SockConsole) Con_printf("SM", "HV_Vbias:%f", Vbias);
		} else if (Vbias > 85) {
			sprintf(w_msg, "%sWARNING: HV Vbias : HV Vbias board %d = %.2f V out of lower bound (20 V, 85 V). HV Vbias value set to 85 V\n", w_msg, b, Vbias);
			Vbias = 85;
			FERS_SetParam(handle[b], "HV_Vbias", "85");
			sprintf(w_val, "%s%d %2.0f,", w_val, b, Vbias);
		}
	}
	if (strlen(w_msg) > 0) {
		Con_printf("LCSw", "%s", w_msg);
		w_msg[strlen(w_msg) - 1] = '\0';
		if (SockConsole) Con_printf("SM", "%s\n", w_val);
	}
}


void reportProgress(char* msg, int progress)
{
	char fwmsg[512];
	if (progress >= 0) sprintf(fwmsg, "Brd%s Progress: %3d %%", msg, progress);
	else strcpy(fwmsg, msg);
	SendAcqStatusMsg(fwmsg);
}

void CheckHVBeforeClosing() {
	int bnf = 0, rmp, ovc, ovv, ret;
	int brd_on[MAX_NBRD] = {};
	if (!J_cfg.AskHVShutDownOnExit) {
		if (SockConsole) Con_printf("LCSm", "Quitting ...\n");
		return;
	}
	FERS_BoardInfo_t BrdInfo;

	for (int mb = 0; mb < J_cfg.NumBrd; ++mb) {	// Check if any boards is still ON
		ret = FERS_GetBoardInfo(handle[mb], &BrdInfo);	// Return if there were connection issues
		if (ret != 0) {
			if (SockConsole) Con_printf("LCSm", "Quitting ...\n");
			return;
		}
		int ttmp;
		ret = FERS_HV_Get_Status(handle[mb], &ttmp, &rmp, &ovc, &ovv);
		if (ret < 0) return; // can't read HV Status, maybe the communication is lost. No way to control the HV... just quit
		bnf |= ttmp;
		brd_on[mb] = ttmp;
	}
	if (bnf == 1) {
		ret = Con_printf("LCSw", "WARNING: HV is still ON, do you want to turn it off? [y] [n]\n");
		if (ServerDead != 0) {
			Con_printf("LCSm", "Turning HV off ...\n");
			for (int mb = 0; mb < J_cfg.NumBrd; ++mb) {
				if (brd_on[mb]) {
					for (int j = 0; j < 10; ++j) { // Sometimes it seems to fail, just a patch
						ret = FERS_HV_Set_OnOff(handle[mb], 0); // Switching off all the boards (only the indexes of the boards 'ON' can be saved)
						if (!ret) break;
						Sleep(100);
					}
					for (int j = 0; j < 10; j++) { // Turning off without sending info to GUI
						float vmon;
						FERS_HV_Get_Vmon(handle[mb], &vmon);
						if (vmon < 20) break;
						Sleep(100);
					}
				}
			}
			return;
		}
		int c;
		while (!(c = Con_getch()));
		if (c == 'y') {
			Con_printf("LCSm", "Turning HV off ...\n");
			for (int mb = 0; mb < J_cfg.NumBrd; ++mb) {
				if (brd_on[mb]) {
					//HV_Switch_OnOff(handle[mb], 0); 
					FERS_HV_Set_OnOff(handle[mb], 0); // Switching off all the boards (only the indexes of the boards 'ON' can be saved)
					for (int j = 0; j < 10; j++) {
						float vmon;
						FERS_HV_Get_Vmon(handle[mb], &vmon);
						if (vmon < 20) break; 
						Sleep(100);
					}
				}
			}
			//if (SockConsole) {
			//	Con_printf("LCSm", "HV correctly turned off, quitting ...\n");
			//}
		}
		else {
			Con_printf("LCSm", "HV still on ...\n");
		}
	}
	else {
		if (SockConsole) 
			Con_printf("LCSm", "Quitting ...\n");
	}
}

// ******************************************************************************************
// Run Control functions
// ******************************************************************************************
// Start Run (starts acq in all boards)
int StartRun() {
	int ret = 0, b, tdl = 1;
	if (AcqStatus == ACQSTATUS_RUNNING) return 0;
	OpenOutputFiles(RunVars.RunNumber);

	for (b = 0; b < J_cfg.NumBrd; b++) {
		if (FERS_CONNECTIONTYPE(handle[b]) != FERS_CONNECTIONTYPE_TDL) tdl = 0;
		brdInFail[b] = 0;
	}

	if (!tdl && (J_cfg.StartRunMode == STARTRUN_TDL)) {
		J_cfg.StartRunMode = STARTRUN_ASYNC;
		Con_printf("LCSw", "WARNING: StartRunMode: can't start run in TDL mode; switching to Async mode\n");
		if (SockConsole) Con_printf("SM", "StartRunMode:%d", J_cfg.StartRunMode);
		for (b = 0; b < J_cfg.NumBrd; ++b) {
			FERS_SetParam(handle[b], "StartRunMode", "0");
			FERS_configure(handle[b], CFG_SOFT);
			Con_printf("LCSm", "Brd%d Start mode: Async\n", b);
		}
	}

	wMsg_sent = 0;
	sEvt_missing = 0;
	first_sEvt = 1;

	ret = FERS_StartAcquisition(handle, J_cfg.NumBrd, J_cfg.StartRunMode, RunVars.RunNumber);

	Stats.start_time = j_get_time();
	time(&Stats.time_of_start);
	build_time_us = 0;
	Stats.stop_time = 0;
	int OldStatus = AcqStatus;
	AcqStatus = ACQSTATUS_RUNNING;
	if (OldStatus != ACQSTATUS_RESTARTING) 
		SendAcqStatusMsg("Run #%d started\n", RunVars.RunNumber);

	Con_printf("LSm", "Run #%02d started\n", RunVars.RunNumber);

	WriteListfileHeader(); // Write the file header for ASCII or Binary

	stop_sw = 0; // Set at 0, used to prevent automatic start during jobs after a sw stop
	is_running = 1;

	return ret;
}


// Stop Run
int StopRun() {
	int ret = 0;
	if (!is_running) return 0; // Don't do stop routine if FERS was not in running (to avoid message when Janus quits with error)
	if (Stats.stop_time == 0) Stats.stop_time = j_get_time();

	if (AcqStatus == ACQSTATUS_ERROR) {
		for (int i = 0; i < J_cfg.NumBrd; i++) {
			uint32_t acq_stat;
			int ret1;
			ret1 = FERS_ReadRegister(handle[i], a_acq_status, &acq_stat);
			if (ret1 < 0)
				Con_printf("LCSm", "Brd %02d: Can't read status registers\n", i);
			else
				Con_printf("LCSm", "Brd %02d: AcqStatus = %08X\n", i, acq_stat);
		}
		Con_printf("LCSm", "Run #%02d killed at time = %.2f s\n", RunVars.RunNumber, (float)(Stats.stop_time - Stats.start_time) / 1000);
	}

	ret = FERS_StopAcquisition(handle, J_cfg.NumBrd, J_cfg.StartRunMode, RunVars.RunNumber);
	
	SaveHistos();
	if ((J_cfg.OutFileEnableMask & OUTFILE_RUN_INFO) && AcqStatus != ACQSTATUS_READY) SaveRunInfo();	
	CloseOutputFiles();
	Con_printf("LCSm", "Output file(s) saved in '%s' folder\n", J_cfg.DataFilePath);

	if (AcqStatus == ACQSTATUS_RUNNING) {
		Con_printf("LCSm", "Run #%02d stopped. Elapsed Time = %.2f s\n", RunVars.RunNumber, (float)(Stats.stop_time - Stats.start_time) / 1000);
		if ((J_cfg.RunNumber_AutoIncr) && (!J_cfg.EnableJobs)) {  //
			++RunVars.RunNumber; 
			SaveRunVariables(RunVars);
		}		
	} 

	if (AcqStatus == ACQSTATUS_RUNNING || AcqStatus == ACQSTATUS_ERROR) AcqStatus = ACQSTATUS_READY;
	is_running = 0;

	return ret;
}


// Check if the config files have been changed; ret values: 0=no change, 1=changed (no acq restart needed), 2=changed (acq restart needed)
int CheckFileUpdate() {
	static uint64_t CfgUpdateTime, RunVarsUpdateTime;
	uint64_t CurrentTime;
	static int first = 1;
	int ret = 0;
	FILE* cfg;

	GetFileUpdateTime(CONFIG_FILENAME, &CurrentTime);
	if ((CurrentTime > CfgUpdateTime) && !first) {
		const Janus_Config_t J_cfg_1 = J_cfg;
		uint32_t DebugLogMask1 = FERS_GetParam_hex(handle[0], "DebugLogMask");
		//memcpy(&J_cfg_1, &J_cfg, sizeof(Config_t));

		cfg = fopen(CONFIG_FILENAME, "r");
		ParseConfigFile(cfg, &J_cfg, PARSEMODE_PARSE_ALL | PARSEMODE_FIRST_CALL);
		fclose(cfg);
		HVLimitCheck(handle);
		Con_printf("LCSm", "Config file reloaded\n");
		if ((J_cfg_1.NumBrd != J_cfg.NumBrd) ||
			(J_cfg_1.NumCh != J_cfg.NumCh) ||
			(J_cfg_1.EnableJobs != J_cfg.EnableJobs) ||
			(J_cfg_1.JobFirstRun != J_cfg.JobFirstRun) ||
			(J_cfg_1.JobLastRun != J_cfg.JobLastRun) ||
			(J_cfg_1.TstampCoincWindow != J_cfg.TstampCoincWindow) ||
			(J_cfg_1.EventBuildingMode != J_cfg.EventBuildingMode))	ret = 3;
		else if ((J_cfg_1.AcquisitionMode != J_cfg.AcquisitionMode) ||
			(J_cfg_1.OutFileEnableMask != J_cfg.OutFileEnableMask) ||
			(J_cfg_1.ToAHistoMin != J_cfg.ToAHistoMin) ||
			(J_cfg_1.ToARebin != J_cfg.ToARebin) ||
			(J_cfg_1.ToAHistoNbin != J_cfg.ToAHistoNbin) ||
			(J_cfg_1.MCSHistoNbin != J_cfg.MCSHistoNbin) ||
			(J_cfg_1.EHistoNbin != J_cfg.EHistoNbin) ||
			(J_cfg_1.PtrgPeriod != J_cfg.PtrgPeriod))		ret = 2;
		else												ret = 1;
		if (DebugLogMask1 != FERS_GetParam_hex(handle[0], "DebugLogMask")) {
			//FERS_SetDebugLogs(DebugLogMask1);
			Con_printf("LCSw", "DebugLogMask cannot be changed while Janus is running. Please, set the mask up before launching Janus\n");
		}
		TrefWindow = FERS_GetParam_float(handle[0], "TrefWindow");

		// FERS_SetEnergyBitsRange(J_cfg.Range_14bit);
		// Dump configuration if selected (known by Lib)
		for (int bb = 0; bb < J_cfg.NumBrd; ++bb) {
			FERS_DumpCfgSaved(handle[bb]);
		}
	}
	CfgUpdateTime = CurrentTime;

	/*GetFileUpdateTime("weerocGUI.txt", &CurrentTime);
	if ((CurrentTime > CfgUpdateTime) && !first) ret = 1;*/

	GetFileUpdateTime(RUNVARS_FILENAME, &CurrentTime);
	if ((CurrentTime > RunVarsUpdateTime) && !first) {
		RunVars_t RunVars1;
		memcpy(&RunVars1, &RunVars, sizeof(RunVars_t));
		LoadRunVariables(&RunVars);
		if (J_cfg.EnableJobs) {
			RunVars.RunNumber = RunVars1.RunNumber;
			SaveRunVariables(RunVars);
		}
		if (RunVars1.RunNumber != RunVars.RunNumber) {
			ret = 1;
		}
	}
	RunVarsUpdateTime = CurrentTime;
	first = 0;
	return ret;
}


// ******************************************************************************************
// RunTime commands menu
// ******************************************************************************************
int RunTimeCmd(int c)
{
	int b, reload_cfg = 0;
	//static int CfgDataAnalysis = -1;
	int bb = 0;
	int cc = 0;
	for (int m = 0; m < 8; m++) {
		if (strlen(RunVars.PlotTraces[m]) != 0) {
			sscanf(RunVars.PlotTraces[m], "%d %d", &bb, &cc);
			break;
		}
	}
	//sscanf(RunVars.PlotTraces[0], "%d %d", &bb, &cc);
	if (c == 'q') {
		// Check if HV is ON
		//CheckHVBeforeClosing(); 
		if (SockConsole) Con_GetInt(&ServerDead);
		Quit = 1;
	}
	if (c == 't' && !offline_conn) {
		for (b = 0; b < J_cfg.NumBrd; b++)
			FERS_SendCommand(handle[b], CMD_TRG);	// SW trg
	}
	if ((c == 's') && (AcqStatus == ACQSTATUS_READY)) {
		ResetStatistics();
		StartRun();
	}
	if ((c == 'S') && (AcqStatus == ACQSTATUS_RUNNING)) {
		StopRun();
		if (J_cfg.EnableJobs) {	// DNIN: In case of stop from keyboard the jobrun is increased
			increase_job_run_number();
			job_read_parse();
		}

	}
	if (c == 'b') {
		int new_brd;
		if (!SockConsole) {
			printf("Current Active Board = %d\n", RunVars.ActiveBrd);
			printf("New Active Board = ");
			scanf("%d", &new_brd);
		} else {
			Con_GetInt(&new_brd);
		}
		if ((new_brd >= 0) && (new_brd < J_cfg.NumBrd)) {
			RunVars.ActiveBrd = new_brd;
			if (!SockConsole) sprintf(RunVars.PlotTraces[0], "%d %d B", RunVars.ActiveBrd, cc);
			else Con_printf("Sm", "Active Board = %d\n", RunVars.ActiveBrd);
		}
		SaveRunVariables(RunVars);
	}
	if (c == 'c') {
		int new_ch;
		if (!SockConsole) {
			char ch_list[100], chs[8][10];
			int i, nt;
			printf("New Active Channels (ch# or pixel, space separ. list) = ");
			fgets(ch_list, 100, stdin);
			nt = sscanf(ch_list, "%s %s %s %s %s %s %s %s", chs[0], chs[1], chs[2], chs[3], chs[4], chs[5], chs[6], chs[7]);
			for (i = 0; i < MAX_NTRACES; i++) {
				if (i < nt) {
					if (isdigit(chs[i][0])) sscanf(chs[i], "%d", &new_ch);
					else new_ch = xy2ch(toupper(chs[i][0]) - 'A', chs[i][1] - '1');
					if ((new_ch >= 0) && (new_ch < MAX_NCH)) {
						//RunVars.ActiveCh = new_ch;
						sprintf(RunVars.PlotTraces[i], "%d %d B", RunVars.ActiveBrd, new_ch);
						ConfigureProbe(handle[RunVars.ActiveBrd]);
					}
				} else {
					RunVars.PlotTraces[i][0] = '\0';
				}
			}
			SaveRunVariables(RunVars);
		}
	}
	if ((c == 'm') && !SockConsole && !offline_conn)
		ManualController(handle[RunVars.ActiveBrd]);
	if (c == 'h' && !offline_conn) {
		if (SockConsole) {
			int b;
			Con_GetInt(&b);
			if ((b >= 0) && (b < J_cfg.NumBrd)) Send_HV_Info(1);
		}
		else HVControlPanel(handle[RunVars.ActiveBrd]);
	}
	if ((c == 'H') && (SockConsole) && !offline_conn) {
		int b;
		int onoff = (Con_getch() - '0') & 0x1;
		Con_GetInt(&b);
		HV_Switch_OnOff(handle[b], onoff);
	}
	if (c == 'T') {
		if (!SockConsole) {
			int thr;
			printf("Enter threshold ");
			scanf("%d", &thr);
			thr = min(thr, 4095);
			FERS_WriteRegister(handle[0], a_td_coarse_thr, thr);	// Discr Threshold 
			FERS_WriteRegister(handle[0], a_scbs_ctrl, 0x000);  // set citiroc index = 0
			FERS_SendCommand(handle[0], CMD_CFG_ASIC);
			FERS_WriteRegister(handle[0], a_scbs_ctrl, 0x200);  // set citiroc index = 1
			FERS_SendCommand(handle[0], CMD_CFG_ASIC);
		}
	}
	if (c == 'V') {
		if (SockConsole)
			En_HVstatus_Update = Con_getch() - '0';
		else
			En_HVstatus_Update ^= 1;
	}
	if ((c == 'M') && !SockConsole && !offline_conn)
		CitirocControlPanel(handle[RunVars.ActiveBrd]);
	if (c == 'r') {
		SaveRunVariables(RunVars);
		AcqStatus = ACQSTATUS_RESTARTING;
		SkipConfig = 1;
		RestartAcq = 1;
	}
	if (c == 'j' &&!offline_conn) {
		if (J_cfg.EnableJobs) {
			if(AcqStatus == ACQSTATUS_RUNNING) StopRun();
			jobrun = J_cfg.JobFirstRun;
			RunVars.RunNumber = jobrun;
			SaveRunVariables(RunVars);
			return 100; // To let the main loop know that 'j' was pressed.
		}
	}
	if ((c == 'R') && (SockConsole) && !offline_conn) {
		uint32_t addr, data;
		char str[10];
		char rw = Con_getch();
		if (rw == 'r') {
			Con_GetString(str, 8);
			sscanf(str, "%x", &addr);
			FERS_ReadRegister(handle[RunVars.ActiveBrd], addr, &data);
			Con_printf("Sm", "Read Reg: ADDR = %08X, DATA = %08X\n", addr, data);
			Con_printf("Sr", "%08X", data);
		}
		else if (rw == 'w') {
			Con_GetString(str, 8);
			sscanf(str, "%x", &addr);
			Con_GetString(str, 8);
			sscanf(str, "%x", &data);
			FERS_WriteRegister(handle[RunVars.ActiveBrd], addr, data);
			Con_printf("Sm", "Write Reg: ADDR = %08X, DATA = %08X\n", addr, data);
		}
	}
	if (c == 'f') {
		if (!SockConsole) Freeze ^= 1;
		else Freeze = (Con_getch() - '0') & 0x1;
	}
	if (c == 'o') {
		Freeze = 1;
		OneShot = 1;
	}
	if (c == 'F') {
		if (SockConsole) return -1;
		printf("\n\n");
		printf("Enter Data Analysis level:\n");
		printf("0 = DISABLED\n");
		printf("1 = CNT_ONLY\n");
		printf("2 = ALL\n");
		printf("q = return\n");

		c = Con_getch() - '0';
		if (c == 0) J_cfg.DataAnalysis = 0;
		if (c == 1) J_cfg.DataAnalysis = DATA_ANALYSIS_CNT;
		if (c == 2) J_cfg.DataAnalysis = DATA_ANALYSIS_CNT | DATA_ANALYSIS_HISTO | DATA_ANALYSIS_MEAS;
		//if (CfgDataAnalysis == -1) {
		//	CfgDataAnalysis = J_cfg.DataAnalysis;
		//	J_cfg.DataAnalysis = 0;
		//	Con_printf("L", "Data Analysis disabled\n");
		//} else {
		//	J_cfg.DataAnalysis = CfgDataAnalysis;
		//	CfgDataAnalysis = -1;
		//}
	}
	if (c == 'C') {
		if (!SockConsole) {
			printf("\n\n");
			printf("0 = ChTrg Rate\n");
			printf("1 = ChTrg Cnt\n");
			printf("2 = Tstamp Hit Rate\n");
			printf("3 = Tstamp Hit Cnt\n");
			printf("4 = PHA Rate\n");
			printf("5 = PHA Cnt\n");
		}
		c = Con_getch() - '0';
		if ((c >= 0) && (c <= 5)) RunVars.SMonType = c;
		SaveRunVariables(RunVars);
	}
	if (c == 'P') {
		if (!SockConsole) {
			printf("\n\n");
			printf("0 = Spect Low Gain\n");
			printf("1 = Spect High Gain\n");
			printf("2 = Spect ToA\n");
			printf("3 = Spect ToT\n");
			printf("4 = Counts\n");
			printf("5 = MuxOut Waveforms\n");
			printf("6 = 2D Count Rates\n");
			printf("7 = 2D Charge LG\n");
			printf("8 = 2D Charge HG\n");
			printf("9 = ScanThr (StairCase)\n");
			printf("a = ScanDelay\n");
			printf("b = MultiCh Scaler\n");
		}
		c = tolower(Con_getch());
		if ((c >= '0') && (c <= '9')) RunVars.PlotType = c - '0';
		else if (c == 'a') RunVars.PlotType = 10;
		else if (c == 'b') RunVars.PlotType = 11;
		SaveRunVariables(RunVars);
	}
	if (c == 'y' && !offline_conn) {	// staircase
		int nstep;
		if (!SockConsole) {
			while (1) {
				ClearScreen();
				printf("[1] Min Threshold = %d\n", RunVars.StaircaseCfg[SCPARAM_MIN]);
				printf("[2] Max Threshold = %d\n", RunVars.StaircaseCfg[SCPARAM_MAX]);
				printf("[3] Step = %d\n", RunVars.StaircaseCfg[SCPARAM_STEP]);
				printf("[4] Dwell Time (ms) = %d\n", RunVars.StaircaseCfg[SCPARAM_DWELL]);
				printf("[5] Board = %d\n", RunVars.StaircaseCfg[SCPARAM_BRD]);
				printf("[0] Start Scan\n");
				printf("[r] Return to main menu\n");
				c = getch();
				if (c == '0' || c == 'r') break;
				printf("Enter new value: ");
				if (c == '1') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_MIN]);
				if (c == '2') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_MAX]);
				if (c == '3') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_STEP]);
				if (c == '4') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_DWELL]);
				if (c == '5') {
					int tmpBrd = 0;
					scanf("%d", &tmpBrd);
					if (tmpBrd > J_cfg.NumBrd || tmpBrd < 0)
						Con_printf("CSm", "Board index out of range\n");
					else
						RunVars.StaircaseCfg[SCPARAM_BRD] = tmpBrd;
				}
			}
			SaveRunVariables(RunVars);
		} else {
			LoadRunVariables(&RunVars);
		}
		if (RunVars.StaircaseCfg[SCPARAM_STEP] > 0 && c != 'r') {
			nstep = (RunVars.StaircaseCfg[SCPARAM_MAX] - RunVars.StaircaseCfg[SCPARAM_MIN]) / RunVars.StaircaseCfg[SCPARAM_STEP] + 1;
			if (nstep > STAIRCASE_NBIN) {
				Con_printf("LCSe", "ERROR: too many steps in scan threshold. Staircase aborted\n");
			} else {
				b = RunVars.StaircaseCfg[SCPARAM_BRD];
				if ((b >= 0) && (b < J_cfg.NumBrd)) {
					int rawdata = FERS_GetParam_int(handle[b], "OF_RawData");
					rawdata = FERS_GetParam_int(handle[b], "OF_RawData");
					if (rawdata) {	// Disable raw data for special Run
						FERS_SetParam(handle[b], "OF_RawData", "0");
					}
					ScanThreshold(handle[b]);
					FERS_configure(handle[b], CFG_HARD);
					rawdata = FERS_GetParam_int(handle[b], "OF_RawData");
					if (rawdata) {
						FERS_SetParam(handle[b], "OF_RawData", "1");
					}
				}
				SaveHistos();
				int newRV = RunVars.RunNumber;
				if ((J_cfg.RunNumber_AutoIncr) && (!J_cfg.EnableJobs))
					newRV++;
				LoadRunVariables(&RunVars);
				RunVars.RunNumber = newRV;
				SaveRunVariables(RunVars);
			}
			//SaveHistos();
			//RestartAcq = 1;
		}
	}
	if (c == 'Y' && !offline_conn) {	// Hold-Delay
		int rawdata = 0;
		if (!SockConsole) {
			while (1) {
				ClearScreen();
				printf("[1] Min Delay (ns) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MIN]);
				printf("[2] Max Delay (ns) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MAX]);
				printf("[3] Step (ns, multiple of 8) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_STEP]);
				printf("[4] Nmean = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN]);
				printf("[5] Board = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_BRD]);
				printf("[0] Start Scan\n");
				printf("[r] Return to main menu\n");
				c = getch();
				if (c == '0' || c == 'r') break;
				printf("Enter new value: ");
				if (c == '1') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_MIN]);
				if (c == '2') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_MAX]);
				if (c == '3') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_STEP]);
				if (c == '4') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN]);
				if (c == '5') {
					int tmpBrd = 0;
					scanf("%d", &tmpBrd);
					if (tmpBrd > J_cfg.NumBrd || tmpBrd < 0)
						Con_printf("CSm", "Board index out of range\n");
					else
						RunVars.StaircaseCfg[HDSPARAM_BRD] = tmpBrd;
				}
				RunVars.HoldDelayScanCfg[HDSPARAM_STEP] &= 0xFFF8;
			}
			SaveRunVariables(RunVars);
		} else {
			LoadRunVariables(&RunVars);
		}
		if (RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN] > 9 && c != 'r') {
			b = RunVars.HoldDelayScanCfg[HDSPARAM_BRD] ;
			if ((b >= 0) && (b < J_cfg.NumBrd)) { 
				rawdata = FERS_GetParam_int(handle[b], "OF_RawData");
				if (rawdata) {
					FERS_SetParam(handle[b], "OF_RawData", "0");
				}
				ScanHoldDelay(handle[b]);
				FERS_configure(handle[b], CFG_HARD);
				HoldScan_newrun = 1;
				if (rawdata) {
					FERS_SetParam(handle[b], "OF_RawData", "1");
				}
			}
			RestartAcq = 1;
		}
	}
	if (c == 'p' && !offline_conn) {
		char str[6][10] = { "OFF", "Fast", "Slow LG", "Slow HG", "Preamp LG", "Preamp HG" };
		if (!SockConsole) printf("0=OFF, 1=Fast, 2=SlowLG, 3=SlowHG, 4=PreampLG, 5=PreampHG\n");
		uint32_t AnalogProbe[2];
		AnalogProbe[0] = Con_getch() - '0';
		if ((AnalogProbe[0] < 0) || (AnalogProbe[0] > 5)) AnalogProbe[0] = 0;
		Con_printf("CSm", "Citiroc Probe = %s\n", str[AnalogProbe[0]]);
		AnalogProbe[1] = AnalogProbe[0];
		
		for (int b = 0; b < J_cfg.NumBrd; b++) {
			FERS_SetParam(handle[b], "AnalogProbe", str[AnalogProbe[0]]);
			ConfigureProbe(handle[b]);
		}

	}
	if (c == 'x') {
		if (SockConsole) RunVars.Xcalib = (Con_getch() - '0') & 0x1;
		else RunVars.Xcalib ^= 1;
		SaveRunVariables(RunVars);
	}
	if (c == '-') {
		if (bb > 0) {
			bb--;
			sprintf(RunVars.PlotTraces[0], "%d %d B", 0, bb);
			if (!offline_conn) ConfigureProbe(handle[RunVars.ActiveBrd]);
		}
	}
	if (c == '+') {
		if (bb < (MAX_NCH - 1)) {
			bb++;
			sprintf(RunVars.PlotTraces[0], "%d %d B", 0, bb);
			if (!offline_conn) ConfigureProbe(handle[RunVars.ActiveBrd]);
		}
	}
	if (c == 'A' && !offline_conn) {
		RegAccessTest(handle, J_cfg.NumBrd);
	}
	if (c == 'D' && !offline_conn) {
		uint16_t PedHG[MAX_NCH], PedLG[MAX_NCH];
		int c;
		Con_printf("LCSm", "Running Pedestal Calibration\n");
		AcquirePedestals(handle[RunVars.ActiveBrd], PedLG, PedHG);
		Con_printf("C", "Do you want to save pedestal calibration into flash mem [y/n]?\n");
		c = tolower(getch());
		if (c == 'y') {
			FERS_WritePedestals(handle[RunVars.ActiveBrd], PedLG, PedHG, NULL);
			Con_printf("LCSm", "Pedestal saved to flash\n");
		}
		FERS_configure(handle[RunVars.ActiveBrd], CFG_HARD);  // restore configuration
	}
	if (c == 'U' && !offline_conn) {	// Firmware upgrade
		int as = AcqStatus;
		int numBrdToUpg = 0, BrdToUpg[MAX_NBRD] = {};
		char fname[500] = "";
		int tdl_upgraded = 0;
		FILE* fp;

		char tmp_brd[50];
		char* token;
		AcqStatus = ACQSTATUS_UPGRADING_FW;

		if (!SockConsole) Con_printf("Cm", "Insert the board(s) index to be upgraded separated by ',' (i.e. '1' or '0,2,5'): ");
		if (!SockConsole) {
			Con_GetString(tmp_brd, 50);
		} else {
			while (!Con_GetString(tmp_brd, 50)) {
				Sleep(10);
				continue;
			}
		}
		token = strtok(tmp_brd, ",");
		while (token != NULL) {
			BrdToUpg[numBrdToUpg] = atoi(token);
			if (BrdToUpg[numBrdToUpg] < 0 || BrdToUpg[numBrdToUpg] >= MAX_NBRD_JANUS) {
				Con_printf("LCSw", "WARNING: Cannot upgrade brd %d. Index out of range [0 - %d].\n", numBrdToUpg, MAX_NBRD_JANUS);
				continue;
			}
			if (FERS_CONNECTIONTYPE(handle[BrdToUpg[numBrdToUpg]]) == FERS_CONNECTIONTYPE_TDL) {
				tdl_upgraded = 1;
				if (FERS_FPGA_FW_MajorRev(handle[BrdToUpg[numBrdToUpg]]) < 5) { //
					AcqStatus = as;
					Sleep(1);
					Con_printf("LCSw", "WARNING: FERS Firwmare cannot be upgraded via TDlink. This function will be available with then next FW release.\n");
					return 0;
				}
			}
			++numBrdToUpg;
			token = strtok(NULL, ",");
		}
		if (!SockConsole) Con_printf("Cm", "Insert the firmware file name:");
		while (!Con_GetString(fname, 500)) {
			Sleep(10);
			continue;
		}
		fname[strcspn(fname, "\n")] = 0; // In console mode GetString append \n at the end

		char UpgSummary[1024];
		sprintf(UpgSummary, "\n******************************\nUpgrade summary:\n");
		fp = fopen(fname, "rb");
		// Check if the upgrade file exists
		// Check if the upgrade file exist
		if (!fp) {
			SendAcqStatusMsg("Failed to open file %s\n", fname);
			AcqStatus = as;
		} else {
			fclose(fp);
			int ret = 0;
			for (int i = 0; i < numBrdToUpg; ++i) {
				SendAcqStatusMsg("Upgrading board %d ...\n", BrdToUpg[i]);
				Con_printf("Su", "LED%d-1", BrdToUpg[i]);	// For GUI: Upgrade start
				int tret = FERS_FirmwareUpgrade(handle[BrdToUpg[i]], fname, reportProgress);
				if (tret < 0) {
					char des[1024];
					FERS_GetLastError(des);
					//Con_printf("LCSe", "ERROR: Brd%d: %s\n", BrdToUpg[i], des);
					sprintf(UpgSummary, "%sBrd %02d: upgrade failed (%s)\n", UpgSummary, BrdToUpg[i], des);
				} else {
					Con_printf("LCSm", "Firmware upgrade for board %d succesfully completed\n", BrdToUpg[i]);
					sprintf(UpgSummary, "%sBrd %02d: upgrade completed succesfully\n", UpgSummary, BrdToUpg[i]);
				}
				Con_printf("Su", "LED%d-%d", BrdToUpg[i], tret);	// For GUI: Upgrade stop
				ret |= tret;
			}

			//Con_printf("Su", "END");	// For GUI: End of upgrade procedure

			if (!SockConsole && numBrdToUpg > 1) {
				Con_printf("LCSm", "%s\n******************************\n", UpgSummary);
			}

			//Con_printf("CSm", "Restart Janus to complete the upgrade");
			if (ret == 0) {
				Con_printf("CSuEND", "Firmware upgrade procedure finished successfully\n");
				Con_printf("Cm", "\nPress any key to reboot the boards from the application and quit Janus\n");
			} else {
				Con_printf("CSuEND", "\nFirmware upgrade procedure finished with error(s). Exit Janus and re-upgrade the board(s) that failed the firmware upgrade "
					"wihtout turing them off, as they have no firmware installed\n");
				Con_printf("Cm", "Press any key to quit Janus\n");
			}
			//if (!SockConsole)

			while (!Con_getch());
			if (tdl_upgraded && ret == 0)
				FERS_FirmwareBootApplication_tdl(handle);

			Quit = 1;
			Con_printf("SQ", "Qq\n");
			return 101; // Means quitting
		}
	}

	if (c == '!' && !offline_conn) {
		int brd = 0, ret;
		if (J_cfg.NumBrd > 1) {
			if (!SockConsole) printf("Enter board index: ");
			Con_GetInt(&brd);
		}
		if ((brd >= 0) && (brd < J_cfg.NumBrd) && (FERS_CONNECTIONTYPE(handle[brd]) == FERS_CONNECTIONTYPE_USB)) {
			ret = FERS_Reset_IPaddress(handle[brd]);
			if (ret == 0) {
				Con_printf("LCSm", "Default IP address (192.168.50.3) has been restored\n");
			}
			else {
				Con_printf("CSw", "Failed to reset IP address\n");
			}
		} else {
			Con_printf("LCSm", "The IP address can be restored via USB connection only\n");
		}
	}
	if (c == '\t') {
		if (!SockConsole)
			StatMode ^= 1;
		else {
			Con_GetInt(&StatMode);
		}
	}
	if (c == 'I') {
		if (!SockConsole)
			StatIntegral ^= 1;
		else {
			Con_GetInt(&StatIntegral);
		}
	}

	if (c == '#') PrintMap();

	if (c == 'L') {
		memset(WDcfg.RunTitle, 0, 81);
		printf("\nEnter title for this run (Max. 80 characters, type \"empty\" for blank title):\n");
		int strlen = 0;
		if (SockConsole) {
			strlen = Con_GetString(WDcfg.RunTitle, 80);
			WDcfg.RunTitle[strcspn(WDcfg.RunTitle, "\r")] = 0;
		} else
			myscanf("%80[^\n]s", WDcfg.RunTitle);
		if (strcmp(WDcfg.RunTitle, "empty") == 0) {
			printf("Title will be empty!\n");
			memset(WDcfg.RunTitle, 0, 81);
		}
	}

	if (c == 'N') {
		printf("\nEnter a run number:\n");
		int runNumber = -1;
		if (!Con_GetInt(&runNumber) && runNumber > -1) {
			RunVars.RunNumber = runNumber;
			SaveRunVariables(RunVars);
		} else {
			printf("Wrong input received! No change made!\n");
			while (getchar() != '\n');
		}
	}

	if (c == 'i') {
		printf("\nEnter a source ID:\n");
		int sourceID = -1;
		if (!Con_GetInt(&sourceID) && sourceID > -1) {
			WDcfg.SourceID = sourceID;
		} else {
			printf("Wrong input received! No change made!\n");
			while (getchar() != '\n');
		}
	}

	if (c == 'B') {
		memset(WDcfg.RingBufferName, 0, 50);
		printf("\nEnter output RingBuffer name (Max. 49 characters, Default 'janus'):\n");
		int strlen = 0;
		if (SockConsole) {
			strlen = Con_GetString(WDcfg.RingBufferName, 49);
			WDcfg.RingBufferName[strcspn(WDcfg.RingBufferName, "\r")] = 0;
		} else
			strlen = myscanf("%49[^\n]s", WDcfg.RingBufferName);
		if (strlen == 0) {
			printf("Default RingBuffer name is used: janus\n");
			strncpy(WDcfg.RingBufferName, "janus", 50);
		}
	}

	if (c == 'u') {
		bool isEnabled = (WDcfg.OutFileEnableMask&0x8000) >> 15;
		if (isEnabled) {
			printf("\nTurning OFF using RingBuffer output\n");
		} else {
			printf("\nTurning ON using RingBuffer output\n");
		}
		WDcfg.OutFileEnableMask ^= 0x8000;
	}

	if ((c == ' ') && !SockConsole) {
		if (!offline_conn) {
			Con_printf("Cm", "[q] Quit\n");
			Con_printf("Cm", "[s] Start\n");
			Con_printf("Cm", "[S] Stop\n");
			Con_printf("Cm", "[t] SW trigger\n");
			Con_printf("Cm", "[C] Set Stats Monitor Type\n");
			Con_printf("Cm", "[P] Set Plot Mode\n");
			Con_printf("Cm", "[x] Enable/Disable X calibration\n");
			Con_printf("Cm", "[c] Change channel\n");
			Con_printf("Cm", "[b] Change board\n");
			Con_printf("Cm", "[f] Freeze plot\n");
			Con_printf("Cm", "[o] One shot plot\n");
			Con_printf("Cm", "[r] Reset histograms\n");
			Con_printf("Cm", "[u] Toggle use RingBuffer output (FRIB)\n");
			Con_printf("Cm", "[i] Set source ID (FRIB)\n");
			Con_printf("Cm", "[B] Set output RingBuffer name (FRIB)\n");
			Con_printf("Cm", "[L] Set title for RingStateChangeItem (FRIB)\n");
			Con_printf("Cm", "[N] Set run number (FRIB)\n");
			Con_printf("Cm", "[j] Reset jobs (when enabled)\n");
			Con_printf("Cm", "[y] Scan Thresholds\n");
			Con_printf("Cm", "[A] Register Access Test\n");
			Con_printf("Cm", "[D] Run Pedestal Calibration\n");
			Con_printf("Cm", "[Y] Hold Delay scan\n");
			Con_printf("Cm", "[F] Select Data analysis level\n");
			Con_printf("Cm", "[h] HV Controller\n");
			Con_printf("Cm", "[M] Citiroc Controller\n");
			Con_printf("Cm", "[p] Set Citiroc probe\n");
			Con_printf("Cm", "[m] Register Manual Controller\n");
			//printf("[w] Set waveform probe\n");
			Con_printf("Cm", "[U] Upgrade firmware\n");
			Con_printf("Cm", "[!] Reset IP address\n");
			Con_printf("Cm", "[#] Print Pixel Map\n");
			Con_printf("Cm", "[T] Set Discriminator Threshold\n");
			Con_printf("Cm", "[e] Exit this menu\n");
		} else {
			Con_printf("Cm", "[q] Quit\n");
			Con_printf("Cm", "[s] Start\n");
			Con_printf("Cm", "[S] Stop\n");
			Con_printf("Cm", "[C] Set Stats Monitor Type\n");
			Con_printf("Cm", "[P] Set Plot Mode\n");
			Con_printf("Cm", "[x] Enable/Disable X calibration\n");
			Con_printf("Cm", "[c] Change channel\n");
			Con_printf("Cm", "[b] Change board\n");
			Con_printf("Cm", "[f] Freeze plot\n");
			Con_printf("Cm", "[o] One shot plot\n");
			Con_printf("Cm", "[r] Reset histograms\n");
			Con_printf("Cm", "[u] Toggle use RingBuffer output (FRIB)\n");
			Con_printf("Cm", "[i] Set source ID (FRIB)\n");
			Con_printf("Cm", "[B] Set output RingBuffer name (FRIB)\n");
			Con_printf("Cm", "[L] Set title for RingStateChangeItem (FRIB)\n");
			Con_printf("Cm", "[N] Set run number (FRIB)\n");
			Con_printf("Cm", "[e] Exit this menu\n");
		}
		c = Con_getch(); 
		if (c == 'e') return 0;
		RunTimeCmd(c);
	}


	if (reload_cfg && !offline_conn) {
		for (b = 0; b < J_cfg.NumBrd; b++) {
			FERS_WriteRegister(handle[b], a_scbs_ctrl, 0x000);  // set citiroc index = 0
			FERS_SendCommand(handle[b], CMD_CFG_ASIC);
			Sleep(10);
			FERS_WriteRegister(handle[b], a_scbs_ctrl, 0x200);  // set citiroc index = 1
			FERS_SendCommand(handle[b], CMD_CFG_ASIC);
			Sleep(10);
		}
	}
	return 0;
}

int report_firmware_notfound(int b, int as) {
	int c;
	// sprintf(ErrorMsg, "The firmware of the FPGA in board %d cannot be loaded.\nPlease, try to re-load the firmware running on shell the command\n'./%s -u %s firmware_filename.ffu'\n", b, EXE_NAME, J_cfg.ConnPath[b]);
	Con_printf("LCSF", "Do you want to reload the FPGA ffu firmware for board %d ? [y][n]\n", b);

	while(!(c = Con_getch()));
	if (c == 'y') {
		AcqStatus = ACQSTATUS_UPGRADING_FW;
		// Get file and call FERSFWUpgrade
		if (!SockConsole) {
			Con_printf("LCSm", "Please, insert the path of the ffu firmware you want to load (i.e: Firmware\\a5202.ffu:\n");
			AcqStatus = ACQSTATUS_UPGRADING_FW;
		}
		char fname[500] = "";
		//if (SockConsole) while (!(c = Con_getch()));
		while (!Con_GetString(fname, 500));
		fname[strcspn(fname, "\n")] = 0; // In console mode GetString append \n at the end

		FILE* fp = fopen(fname, "rb");
		if (!fp) {
//			Con_printf("CSm", "Failed to open file %s\n", fname);
			sprintf(ErrorMsg, "Failed to open FW file %s\n", fname);
			return -1;
		}
		int ret = FERS_FirmwareUpgrade(handle[b], fname, reportProgress);
		if (ret == 0) {
			Con_printf("LCSuEND", "Firmware upgrade completed\n");
			Con_printf("C", "Press any keys to quit\n");
			if (FERS_CONNECTIONTYPE(handle[b]) == FERS_CONNECTIONTYPE_TDL) FERS_FirmwareBootApplication_tdl(handle);
		} else {
			Con_printf("LCSuEND", "Firmware upgrade failed\n");
			Con_printf("C", "Press any keys to quit\n");
		}

		while(!(c = Con_getch()));
		AcqStatus = as;
		return ret;
	} else
		return -2;

}





// ******************************************************************************************
// MAIN
// ******************************************************************************************
int main(int argc, char* argv[])
{
	int i = 0, ret = 0, clrscr = 0, dtq, ch, b, cnc, rdymsg; // jobrun = 0, 
	int PresetReached = 0;
	int nb = 0;
	double tstamp_us, curr_tstamp_us = 0;
	int MajorFWrev = 255;
	uint64_t kb_time, curr_time, print_time, wave_time;
	char ConfigFileName[500] = CONFIG_FILENAME;
	char fwupg_fname[500] = "", fwupg_conn_path[20] = "";
	char PixelMapFileName[500] = PIXMAP_FILENAME; //  CTIN: get name from config file
	void* Event;
	float elapsedPC_s;
	float elapsedBRD_s;
	FILE* cfg;
	int a1, a2, AllocSize;
	int ROmode, brdInWarning[MAX_NBRD] = {}, crcBrdError[MAX_NBRD], crcCncError = 0;
	uint32_t CrcErrorLevel = 1;
	char port[6] = "";
	bool devnull = false;

	char description[1024];

	// Get command line options
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i] + 1, "g") == 0) SockConsole = 1;
			if (argv[i][1] == 'p') strcpy(port, &argv[i][2]);
			if (argv[i][1] == 'd') devnull = true;
			if (argv[i][1] == 'c') strcpy(ConfigFileName, &argv[i][2]);
			if (argv[i][1] == 'u') {
				if (argc > (i + 2)) {
					strcpy(fwupg_conn_path, argv[i + 1]);
					strcpy(fwupg_fname, argv[i + 2]);
					i += 2;
				}
			}
		}
	}

	MsgLog = fopen("MsgLog.txt", "w");
	ret = InitConsole(SockConsole, MsgLog, port);
	if (ret) {
		printf(FONT_STYLE_BOLD COLOR_RED "ERROR: init console failed\n" COLOR_RESET);
		exit(0);
	}
	MAX_NBRD_JANUS = SockConsole ? MAX_NBRD_GUI : MAX_NBRD;

	AcqStatus = ACQSTATUS_SOCK_CONNECTED;
	if (SockConsole) SendAcqStatusMsg("JanusC connected. Release %s (%s).", SW_RELEASE_NUM, SW_RELEASE_DATE);
	Con_printf("LCSm", "*************************************************\n");
	Con_printf("LCSm", "JanusC Rev %s (%s)\n", SW_RELEASE_NUM, SW_RELEASE_DATE);
	Con_printf("LCSm", "FERSlib Rev %s (%s)\n", FERS_GetLibReleaseNum(), FERS_GetLibReleaseDate());
	Con_printf("LCSm", "Readout Software for CAEN FERS-5200\n");
	Con_printf("LCSm", "*************************************************\n");

	// Check if a FW upgrade has been required
	if ((strlen(fwupg_fname) > 0) && (strlen(fwupg_conn_path) > 0)) {
		FILE* fp = fopen(fwupg_fname, "rb");
		if (!fp) {
			Con_printf("CSm", "Failed to open file %s\n", fwupg_fname);
			return -1;
		}
		fclose(fp);
		ret = FERS_OpenDevice(fwupg_conn_path, &handle[0]);
		if (ret == FERSLIB_ERR_INVALID_FW || ret == 0) {
			Con_printf("LCSm", "\nUpgrading board %d with FW file %s\n", FERS_INDEX(handle[0]), fwupg_fname);
		} else if (ret < 0) {
			Con_printf("CSe", "\nERROR: can't open FERS at %s\n", fwupg_conn_path);
			return -1;
		}
		if ((FERS_CONNECTIONTYPE(handle[0]) == FERS_CONNECTIONTYPE_TDL) && (FERS_FPGA_FW_MajorRev(handle[0]) < 6)){
			Sleep(1);
			Con_printf("LCSw", "WARNING: FERS Firwmare cannot be upgraded via TDLink with a Firmware Major Rev < 6\n");
			Sleep(10000);
			return 0;
		}

		ret = FERS_FirmwareUpgrade(handle[0], fwupg_fname, reportProgress);
		fclose(fp);
		return 0;
	}


	//if (SockConsole) {
	//	Con_printf("Sd", "Debug\n");
	//	while (!Con_getch()) {
	//		continue;
	//	}
	//}

ReadCfg:
	memset(handle, -1, sizeof(handle));
	memset(cnc_handle, -1, sizeof(cnc_handle));

	// -----------------------------------------------------
	// Parse config file
	// -----------------------------------------------------
	cfg = fopen(ConfigFileName, "r");
	if (cfg == NULL) {
		sprintf(ErrorMsg, "Can't open configuration file %s\n", ConfigFileName);
		goto ManageError;
	}
//	Con_printf("LCSm", "Reading configuration file %s\n", ConfigFileName);
	ret = ParseConfigFile(cfg, &J_cfg, PARSEMODE_PARSE_CONNECTION);
	fclose(cfg);

	// -----------------------------------------------------
	// Read pixel map (channel (0:63) to pixel[x][y] 
	// -----------------------------------------------------
	Con_printf("LCSm", "Reading Pixel Map %s\n", PixelMapFileName);
	if (Read_ch2xy_Map(PixelMapFileName) < 0)
		Con_printf("LCSw", "WARNING: Map File not found. Sequential mapping will be used\n");

	// -----------------------------------------------------
	// Connect To Boards
	// -----------------------------------------------------
	cnc = 0;
	for (b = 0; b < J_cfg.NumBrd; b++) {
		FERS_BoardInfo_t BoardInfo;
		char* cc, cpath[100];
		if (((cc = strstr(J_cfg.ConnPath[b], "offline")) != NULL)) {	// Open an 'offline' connection just for Raw Data reprocessing
			Con_printf("Ss", "offline\n");
			Con_printf("LCSm", "\n--------------- OFFLINE ----------------\n", cnc);
			int ret = FERS_OpenDevice(J_cfg.ConnPath[b], handle);
			if (ret != 0) {
				FERS_GetLastError(description);
				sprintf(ErrorMsg, "%s (ret = %d)\n", description, ret);
				goto ManageError;
			}

			// If TDL connection, 1 file for all the boards
			// So, all the board info should be retreived and update the J_cfg.NumBrd
			
			if (FERS_CONNECTIONTYPE(handle[0]) == FERS_CONNECTIONTYPE_TDL) {
				Con_printf("LCSm", "\n--------------- Concentrator ----------------\n");
				FERS_CncInfo_t CncInfo;
				FERS_GetCncInfo(handle[0], &CncInfo);
				Con_printf("LCSm", "FPGA FW revision = %s\n", CncInfo.FPGA_FWrev);
				Con_printf("LCSm", "SW revision = %s\n", CncInfo.SW_rev);
				Con_printf("LCSm", "PID = %d\n", CncInfo.pid);
			}
			int brd_opened = FERS_GetNumBrdConnected();
			for (int k = b; k < brd_opened; ++k) {
				FERS_GetBoardInfo(handle[k], &BoardInfo);
				if (BoardInfo.FERSCode != 5202) {
					sprintf(ErrorMsg, "Cannot be proccessed data from FERS_%" PRIu16 ", because this Janus version can support only FERS_5202 boards. Please, download from www.caen.it the Janus version for the FERS_5202 board version\n", BoardInfo.FERSCode);
					goto ManageError;
				}

				Con_printf("LCSm", "\n------------------ Board %2d --------------------\n", k);
				char fver[100];
				if (BoardInfo.FPGA_FWrev == 0) sprintf(fver, "BootLoader");
				else sprintf(fver, "%d.%d (Build = %04X)", (BoardInfo.FPGA_FWrev >> 8) & 0xFF, BoardInfo.FPGA_FWrev & 0xFF, (BoardInfo.FPGA_FWrev >> 16) & 0xFFFF);
				MajorFWrev = min((int)(BoardInfo.FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
				Con_printf("LCSm", "FPGA FW revision = %s\n", fver);
				if (strstr(J_cfg.ConnPath[b], "cnc") == NULL)
					Con_printf("LCSm", "uC FW revision = %08X\n", BoardInfo.uC_FWrev);
				Con_printf("LCSm", "PID = %d\n", BoardInfo.pid);
				if (SockConsole) {
					if (strstr(J_cfg.ConnPath[b], "cnc") == NULL) Con_printf("Si", "%d;%d;%s;%s;%08X", k, BoardInfo.pid, BoardInfo.ModelName, fver, BoardInfo.uC_FWrev); // ModelName for firmware upgrade
					else Con_printf("Si", "%d;%d;%s;%s;N.A.", k, BoardInfo.pid, BoardInfo.ModelName, fver);
				}

			}
			
			offline_conn = 1;
			if (brd_opened > J_cfg.NumBrd) {
				J_cfg.NumBrd = brd_opened;
				break;
			}

		} else {
			Con_printf("Ss", "online\n");
			if (((cc = strstr(J_cfg.ConnPath[b], "tdl")) != NULL)) {  // TDlink used => Open connection to concentrator (this is not mandatory, it is done for reading information about the concentrator)
				UsingCnc = 1;
				FERS_Get_CncPath(J_cfg.ConnPath[b], cpath);
				if (!FERS_IsOpen(cpath)) {
					FERS_CncInfo_t CncInfo;
					Con_printf("LCSm", "\n--------------- Concentrator %2d ----------------\n", cnc);
					Con_printf("LCSm", "Opening connection to %s\n", cpath);
					ret = FERS_OpenDevice(cpath, &cnc_handle[cnc]);
					if (ret == 0) {
						Con_printf("LCSm", "Connected to Concentrator %s\n", cpath);
					} else {
						sprintf(ErrorMsg, "Can't open concentrator at %s\n", cpath);
						goto ManageError;
					}
					if (!FERS_TDLchainsInitialized(cnc_handle[cnc])) {
						Con_printf("LCSm", "Initializing TDL chains. This will take a few seconds...\n", cpath);
						if (SockConsole) SendAcqStatusMsg("Initializing TDL chains. This may take a few seconds...");
						//ret = FERS_InitTDLchains(cnc_handle[cnc], J_cfg.FiberDelayAdjust[cnc]);
					}
					ret = FERS_InitTDLchains(cnc_handle[cnc], J_cfg.FiberDelayAdjust[cnc]);
					if (ret != 0) {
						sprintf(ErrorMsg, "Failure in TDL chain init\n");
						goto ManageError;
					}

					ret |= FERS_ReadConcentratorInfo(cnc_handle[cnc], &CncInfo);
					if (ret == 0) {
						Con_printf("LCSm", "FPGA FW revision = %s\n", CncInfo.FPGA_FWrev);
						Con_printf("LCSm", "SW revision = %s\n", CncInfo.SW_rev);
						Con_printf("LCSm", "PID = %d\n", CncInfo.pid);
						if (CncInfo.ChainInfo[0].BoardCount == 0) { 	// Rising error if no board is connected to link 0
							sprintf(ErrorMsg, "No board connected to link 0\n");
							goto ManageError;
						}
						for (i = 0; i < 8; i++) {
							if (CncInfo.ChainInfo[i].BoardCount > 0)
								Con_printf("LCSm", "Found %d board(s) connected to TDlink n. %d\n", CncInfo.ChainInfo[i].BoardCount, i);
						}
					} else {
						sprintf(ErrorMsg, "Can't read concentrator info\n");
						goto ManageError;
					}
					cnc++;
				}
			}
			if ((J_cfg.NumBrd > 1) || (cnc > 0)) Con_printf("LCSm", "\n------------------ Board %2d --------------------\n", b);
			Con_printf("LCSm", "Opening connection to %s\n", J_cfg.ConnPath[b]);
			ret = FERS_OpenDevice(J_cfg.ConnPath[b], &handle[b]);
			if (ret == 0) {
				Con_printf("LCSm", "Connected to %s\n", J_cfg.ConnPath[b]);
			} else if (ret == FERSLIB_ERR_INVALID_FW) {
				sprintf(ErrorMsg, "The board %d is not running a valid FW\n", b);
				int ret_rep = report_firmware_notfound(b, AcqStatus);
				if (ret_rep == 0) {
					Quit = 1;
					Con_printf("SQ", "Qq\n");
					goto ExitPoint;
				} else
					//if (!SockConsole) 
					goto ManageError;
			} else {
				char desc_err[1024];
				FERS_GetLastError(desc_err);
				sprintf(ErrorMsg, "Can't open board %d at %s\n", b, J_cfg.ConnPath[b]);
				sprintf(ErrorMsg, "%s%s (ret=%d)\n", ErrorMsg, desc_err, ret);
				goto ManageError;
			}

			// Check SW compatibility (available from FW 5.1)
			uint32_t sw_comp;
			ret = FERS_ReadRegister(handle[b], a_sw_compatib, &sw_comp);
			if ((ret == 0) && ((sw_comp & 0xFFFF) != 0xFFFF)) {
				int r2, r1, r0, f2, f1;
				sscanf(SW_RELEASE_NUM, "%d.%d.%d", &r2, &r1, &r0);
				f2 = (sw_comp >> 8) & 0xFF;
				f1 = sw_comp & 0xFF;
				if ((r2 < f2) || ((r2 == f2) && (r1 < f1))) {
					Con_printf("LCSw", "WARNING: the FW in the board requires at least Janus Rev. %d.%d.0. Please update!\n", f2, f1);
				}
			}
			
			ret = FERS_GetBoardInfo(handle[b], &BoardInfo);
			if (ret != 0) {
				sprintf(ErrorMsg, "Can't read board info\n");
				goto ManageError;
			}
			if (BoardInfo.FERSCode != 5202) {
				sprintf(ErrorMsg, "Cannot open FERS_%" PRIu16 ", because this Janus version can support only FERS_5202 boards. Please download the Janus version for the FERS_5202 board\n", BoardInfo.FERSCode);
				goto ManageError;
			}
			char fver[100];
			sprintf(fver, "%d.%d (Build = %04X)", (BoardInfo.FPGA_FWrev >> 8) & 0xFF, BoardInfo.FPGA_FWrev & 0xFF, (BoardInfo.FPGA_FWrev >> 16) & 0xFFFF);
			MajorFWrev = min((int)(BoardInfo.FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
			Con_printf("LCSm", "FPGA FW revision = %s\n", fver);
			if (strstr(J_cfg.ConnPath[b], "tdl") == NULL)
				Con_printf("LCSm", "uC FW revision = %08X\n", BoardInfo.uC_FWrev);
			Con_printf("LCSm", "PID = %d\n", BoardInfo.pid);
			if (SockConsole) {
				if (strstr(J_cfg.ConnPath[b], "tdl") == NULL) Con_printf("Si", "%d;%d;%s;%s;%08X", b, BoardInfo.pid, BoardInfo.ModelName, fver, BoardInfo.uC_FWrev); // ModelName for firmware upgrade
				else Con_printf("Si", "%d;%d;%s;%s;N.A.", b, BoardInfo.pid, BoardInfo.ModelName, fver);
			}
			if ((BoardInfo.FPGA_FWrev > 0) && ((BoardInfo.FPGA_FWrev & 0xFF00) < 1) && (BoardInfo.FERSCode == 5202)) {
				sprintf(ErrorMsg, "Your FW revision is %d.%d; must be 1.X or higher\n", (BoardInfo.FPGA_FWrev >> 8) & 0xFF, BoardInfo.FPGA_FWrev & 0xFF);
				goto ManageError;
			}
		}
	}
	if ((J_cfg.NumBrd > 1) || (cnc > 0))  Con_printf("LCSm", "\n");
	if (AcqStatus != ACQSTATUS_RESTARTING) {
		AcqStatus = ACQSTATUS_HW_CONNECTED;
		SendAcqStatusMsg("Num of connected boards = %d", J_cfg.NumBrd);
	}

	// Second pass of the config file parser. Now the boards are open, it is possible to read the board parameters
	cfg = fopen(ConfigFileName, "r");
	Con_printf("LCSm", "Reading configuration file %s\n", ConfigFileName);
	ret = ParseConfigFile(cfg, &J_cfg, PARSEMODE_PARSE_ALL | PARSEMODE_FIRST_CALL);
	HVLimitCheck(handle);
	fclose(cfg);

	// Dump configuration if selected (known by Lib)
	for (int bb = 0; bb < J_cfg.NumBrd; ++bb) {
		FERS_DumpCfgSaved(handle[bb]);
	}

	LoadRunVariables(&RunVars);
	if (J_cfg.EnableJobs) {
		jobrun = J_cfg.JobFirstRun;
		RunVars.RunNumber = jobrun;
		SaveRunVariables(RunVars);
	}

	// In USB connection live parameter change is not allowed, is automatically performed stop/configure/start
	for (int bd = 0; bd < J_cfg.NumBrd; bd++) {
		if (FERS_CONNECTIONTYPE(handle[bd]) == FERS_CONNECTIONTYPE_USB) {
			J_cfg.EnLiveParamChange = 0;
			//Con_printf("LCSw", "WARNING: Live parameter change is not allowed in USB connection mode. It will be disabled.\n");
			break;
		}
	}

	//FERS_SetDebugLogs(J_cfg.DebugLogMask);  CTIN1 perch\E9 la setto dal main? deve saperlo la lib
	//FERS_SetEnergyBitsRange(J_cfg.Range_14bit);

	
	// -----------------------------------------------------
	// Allocate memory buffers and histograms, open files
	// -----------------------------------------------------
	ROmode = (J_cfg.EventBuildingMode != 0) ? 1 : 0;
	for (b = 0; b < J_cfg.NumBrd; b++) {
		FERS_InitReadout(handle[b], ROmode, &a1);
		memset(&sEvt[b], 0, sizeof(ServEvent_t));
	}
	CreateStatistics(J_cfg.NumBrd, FERSLIB_MAX_NCH_5202, &a2);
	AllocSize = a2 + FERS_TotalAllocatedMemory();
	Con_printf("LCSm", "Total allocated memory = %.2f MB\n", (float)AllocSize / (1024 * 1024));

	// -----------------------------------------------------
	// Open plotter
	// -----------------------------------------------------
	OpenPlotter(devnull);

// +++++++++++++++++++++++++++++++++++++++++++++++++++++
Restart:  // when config file changes or a new run of the job is scheduled, the acquisition restarts here // BUG: it does not restart when job is enabled, just when preset time or count is active
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
	if (!PresetReached)
		ResetStatistics();
	else
		PresetReached = 0;

	LoadRunVariables(&RunVars);
	if (J_cfg.EnableJobs)
		job_read_parse();

	// -----------------------------------------------------
	// Configure Boards
	// -----------------------------------------------------
	if (!SkipConfig && !offline_conn) {
		Con_printf("LCSm", "Configuring Boards ... \n");
		for (b = 0; b < J_cfg.NumBrd; b++) {
			ret = FERS_configure(handle[b], CFG_HARD);
			if (ret < 0) {
				Con_printf("LCSe", "Failed!!!\n");
				FERS_GetLastError(description);
				sprintf(ErrorMsg, "%s\n", description);
				goto ManageError;
			} else {
				Con_printf("LCSm", "Board %d configured.\n", b);
			}
		}
		Con_printf("LCSm", "Done.\n");
	}
	SkipConfig = 0;

	//Con_printf("Cm", "Dumping FERSlib Register ... \n");
	//FERS_DumpBoardRegister(handle[0], "Board_Reg.txt");

	// Send some info to GUI
	if (SockConsole) {
		//for (int b = 0; b < J_cfg.NumBrd; b++) 
		if (!offline_conn) Send_HV_Info(1);
		Con_printf("SSG0", "Time Stamp");
		Con_printf("SSG1", "Trigger-ID");
		Con_printf("SSG2", "Trg Rate");
		Con_printf("SSG3", "Trg Reject");
		Con_printf("SSG4", "Tot Lost Trg");
		Con_printf("SSG5", "Event Build");
		Con_printf("SSG6", "Readout Rate");
		Con_printf("SSG7", "T-OR Rate");		//Con_printf("Sp", "%d", RunVars.PlotType);
		Con_printf("SR", "%d", RunVars.RunNumber);
	}

	// ###########################################################################################
	// Readout Loop
	// ###########################################################################################
	// Start Acquisition
	if (((AcqStatus == ACQSTATUS_RESTARTING) || ((J_cfg.EnableJobs && (jobrun > J_cfg.JobFirstRun) && (jobrun <= J_cfg.JobLastRun)))) && !stop_sw) {
		if (J_cfg.EnableJobs) Sleep((int)(J_cfg.RunSleep * 1000));
		StartRun();
		if (!SockConsole) ClearScreen();
	}
	else if (AcqStatus != ACQSTATUS_RUNNING) AcqStatus = ACQSTATUS_READY;

	curr_time = j_get_time();
	print_time = curr_time - 2000;  // force 1st print with no delay
	wave_time = curr_time;
	kb_time = curr_time;
	rdymsg = 1;
	PresetReached = 0;
	build_time_us = 0;

	// DNIN: These vars are set to -1 in Release?
	Quit = 0;
	RestartAll = 0;
	Freeze = 0;

	En_HVstatus_Update = 1; // DEBUG
	while (!Quit && !RestartAcq && !PresetReached && !RestartAll) {

		curr_time = j_get_time();
		Stats.current_time = curr_time;
		nb = 0;

		// ---------------------------------------------------
		// Check for commands from console or changes in cfg files
		// ---------------------------------------------------
		if ((curr_time - kb_time) > 200) {
			int upd = CheckFileUpdate();
			if (upd == 1) {
				if (!SockConsole) clrscr = 1;
				rdymsg = 1;
				//int curr_status = AcqStatus;
				if (!offline_conn) {
					if (J_cfg.EnLiveParamChange == 0 && AcqStatus == ACQSTATUS_RUNNING) {
						StopRun();
						for (b = 0; b < J_cfg.NumBrd; b++) {
							FERS_configure(handle[b], CFG_SOFT);
							FERS_FlushData(handle[b]);
						}
						StartRun();
					} else {
						Con_printf("LCSm", "Configuring Boards ... \n");
						for (b = 0; b < J_cfg.NumBrd; b++) {
							ret = FERS_configure(handle[b], CFG_SOFT);
							if (ret < 0) {
								Con_printf("LCSe", "Failed!!!\n");
								Con_printf("LCSe", "%s", ErrorMsg);
								goto ManageError;
							}
						}
						Con_printf("LCSm", "Done.\n");
					}
				}
			} else if (upd == 2) {
				int size;
				DestroyStatistics();
				CreateStatistics(J_cfg.NumBrd, FERSLIB_MAX_NCH_5202, &size);
				RestartAcq = 1;
			} else if (upd == 3) {
				RestartAll = 1;
			}
			if (Con_kbhit()) {
				int tchar = Con_getch();
#ifdef _WIN32
				if (tchar == 224)
					Con_getch();
#endif			
				int tret = RunTimeCmd(tchar);  // Con_getch());
				clrscr = 1;
				rdymsg = 1;
				if (tret == 100) {
					kb_time = curr_time;
					goto Restart;
				} else if (tret == 101)
					goto ExitPoint;
			}
			kb_time = curr_time;
		}

		// ---------------------------------------------------
		// Read Data from the boards
		// ---------------------------------------------------
		if (AcqStatus == ACQSTATUS_RUNNING) {
			ret = FERS_GetEvent(handle, &b, &dtq, &tstamp_us, &Event, &nb);
			if (nb > 0) curr_tstamp_us = tstamp_us;
			if (ret < 0) {
				AcqStatus = ACQSTATUS_ERROR;
				StopRun();
				char eMsg[200];
				sprintf(eMsg, "Readout failure (ret = %d)!", ret);
				Con_printf("L", "ERROR: %s\n", eMsg);
				SendAcqStatusMsg("ERROR: %s\n", eMsg);
				if (SockConsole) rdymsg = 0;
				AcqStatus = ACQSTATUS_READY;
			}
			if (ret == RAWDATA_REPROCESS_FINISHED) {// 4 means end of offline reprocessing
				plot_histogram();
				StopRun();
			}
			elapsedPC_s = (Stats.current_time > Stats.start_time) ? ((float)(Stats.current_time - Stats.start_time)) / 1000 : 0;
			elapsedBRD_s = (float)(Stats.current_tstamp_us[0] * 1e-6);
			if ((J_cfg.StopRunMode == STOPRUN_PRESET_TIME) && ((elapsedBRD_s > J_cfg.PresetTime) || (elapsedPC_s > (J_cfg.PresetTime + 1)))) {
				Stats.stop_time = Stats.start_time + (uint64_t)(J_cfg.PresetTime * 1000);
				StopRun();
				PresetReached = 1; // Preset time reached; quit readout loop
			} else if ((J_cfg.StopRunMode == STOPRUN_PRESET_COUNTS) && (Stats.GlobalTrgCnt[0].cnt >= (uint32_t)J_cfg.PresetCounts)) {  // Stop on board 0 counts
				StopRun();
				PresetReached = 1; // Preset counts reached; quit readout loop
			}
		}
		if (AcqStatus == ACQSTATUS_EMPTYING) {
			ret = FERS_GetEvent(handle, &b, &dtq, &tstamp_us, &Event, &nb);
			if (nb > 0) curr_tstamp_us = tstamp_us;
			else if (nb == 0) StopRun();
			if (ret < 0) {
				sprintf(ErrorMsg, "Readout failure (ret = %d)!\n", ret);
				goto ManageError;
			}
		}
		if ((nb > 0) && !PresetReached) {
			Stats.current_tstamp_us[b] = curr_tstamp_us;
			Stats.ByteCnt[b].cnt += (uint32_t)nb;
			Stats.GlobalTrgCnt[b].cnt++;
			if ((curr_tstamp_us > (build_time_us + 0.001 * J_cfg.TstampCoincWindow)) && (J_cfg.EventBuildingMode != EVBLD_DISABLED)) {
				if (build_time_us > 0) Stats.BuiltEventCnt.cnt++;
				build_time_us = curr_tstamp_us;
			}

			// ---------------------------------------------------
			// update statistics and spectra and save list files 
			// ---------------------------------------------------
			if (((dtq & 0xF) == DTQ_SPECT) || ((dtq & 0xF) == DTQ_TSPECT)) {
				SpectEvent_t* Ev = (SpectEvent_t*)Event;
				Stats.current_trgid[b] = Ev->trigger_id;
				if (J_cfg.DataAnalysis & DATA_ANALYSIS_HISTO) {
					for (ch = 0; ch < 64; ch++) {
						uint16_t ediv = 1;
						if (J_cfg.EHistoNbin > 0) ediv = FERS_GetParam_int(handle[b], "Range_14bit") ? ((1 << 14) / J_cfg.EHistoNbin) : ((1 << 13) / J_cfg.EHistoNbin);
						if (ediv < 1) ediv = 1;
						uint16_t EbinLG = Ev->energyLG[ch] / ediv;
						uint16_t EbinHG = Ev->energyHG[ch] / ediv;
						uint32_t Hmin = (uint32_t)(J_cfg.ToAHistoMin / TOA_LSB_ns);
						uint32_t ToAbin = Ev->tstamp[ch];
						ToAbin = (uint32_t)((ToAbin - Hmin) / J_cfg.ToARebin); // Shift and Rebin ToA histogram
						uint32_t ToTbin = Ev->ToT[ch];
						if (J_cfg.EHistoNbin > 0) {
							if (EbinLG > 0) Histo1D_AddCount(&Stats.H1_PHA_LG[b][ch], EbinLG);
							if (EbinHG > 0) Histo1D_AddCount(&Stats.H1_PHA_HG[b][ch], EbinHG);
						}
						if (J_cfg.ToAHistoNbin > 0) {
							if (ToAbin > 0) Histo1D_AddCount(&Stats.H1_ToA[b][ch], ToAbin);
							if (ToTbin > 0) Histo1D_AddCount(&Stats.H1_ToT[b][ch], ToTbin);
						}
						if ((EbinLG > 0) || (EbinHG > 0)) Stats.PHACnt[b][ch].cnt++;
						if (ToAbin > 0) Stats.HitCnt[b][ch].cnt++;
					}
				}
				if ((J_cfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_BIN) 
					|| (J_cfg.OutFileEnableMask & OUTFILE_SYNC) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_CSV)
					|| (J_cfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}

			} else if ((dtq & 0x0F) == DTQ_TIMING) {
				uint32_t Hmin = (uint32_t)(J_cfg.ToAHistoMin/TOA_LSB_ns);
				ListEvent_t* Ev = (ListEvent_t*)Event;
				if (J_cfg.DataAnalysis & DATA_ANALYSIS_HISTO) {
					for (i = 0; i < Ev->nhits; i++) {
						uint32_t ToAbin = Ev->tstamp[i];
						uint16_t ToTbin = Ev->ToT[i];
						if (J_cfg.ToAHistoNbin == 0) break;
						ch = Ev->channel[i];
						ToAbin = (uint32_t)((ToAbin - Hmin) / J_cfg.ToARebin); // Shift and Rebin ToA histogram
						if (J_cfg.AcquisitionMode == ACQMODE_TIMING_CSTOP) {
							ToAbin = (uint32_t)TrefWindow - ToAbin;
						}
						if (ToAbin > 0) {
							Histo1D_AddCount(&Stats.H1_ToA[b][ch], ToAbin);
							Stats.HitCnt[b][ch].cnt++;
						}
						if (ToTbin > 0) Histo1D_AddCount(&Stats.H1_ToT[b][ch], ToTbin);
					}
				}
				if ((J_cfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_BIN) 
					|| (J_cfg.OutFileEnableMask & OUTFILE_SYNC) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_CSV)
					|| (J_cfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}

			} else if ((dtq & 0x0F) == DTQ_COUNT)  {
				CountingEvent_t* Ev = (CountingEvent_t*)Event;
				Stats.current_trgid[b] = Ev->trigger_id;
				if (J_cfg.DataAnalysis != 0) {
					for (i = 0; i < MAX_NCH; i++) {
						Stats.ChTrgCnt[b][i].cnt += Ev->counts[i];
						if ((J_cfg.MCSHistoNbin > 0) && (J_cfg.DataAnalysis & DATA_ANALYSIS_HISTO))
							Histo1D_SetCount(&Stats.H1_MCS[b][i], Ev->counts[i]);
					}
					Stats.T_OR_Cnt[b].cnt += Ev->t_or_counts;
					Stats.Q_OR_Cnt[b].cnt += Ev->q_or_counts;
					Stats.trgcnt_update_us[b] = curr_tstamp_us;
				}
				if ((J_cfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_BIN) 
					|| (J_cfg.OutFileEnableMask & OUTFILE_SYNC) || (J_cfg.OutFileEnableMask & OUTFILE_LIST_CSV)
					|| (J_cfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}
			} else if (dtq == DTQ_WAVE) {
				// Plot waveform
				if (((curr_time - wave_time) > 300) && (RunVars.PlotType == PLOT_WAVE) && (!Freeze || OneShot)) {
					WaveEvent_t* Ev;
					int brd, ch;
					sscanf(RunVars.PlotTraces[0], "%d %d", &brd, &ch);
					if ((brd >= 0) && (brd < J_cfg.NumBrd) && (ch >= 0) && (ch < J_cfg.NumCh) && (brd == b)) {
						Ev = (WaveEvent_t*)Event;
						PlotWave(Ev, brd, ch);
						OneShot = 0;
						wave_time = curr_time;
					}
				}
			} else if (dtq == DTQ_SERVICE) {
				ServEvent_t* Ev = (ServEvent_t*)Event;
				memcpy(&sEvt[b], Ev, sizeof(ServEvent_t));
				if (J_cfg.AcquisitionMode != ACQMODE_COUNT) {	// DNIN: in counting mode the counters are already incremented
					if (J_cfg.DataAnalysis != 0) {
						for (ch = 0; ch < MAX_NCH; ch++)
							Stats.ChTrgCnt[b][ch].cnt += sEvt[b].ch_trg_cnt[ch];
					}
					Stats.T_OR_Cnt[b].cnt += sEvt[b].t_or_cnt;
					Stats.Q_OR_Cnt[b].cnt += sEvt[b].q_or_cnt;
					Stats.trgcnt_update_us[b] = (double)sEvt[b].update_time * 1000;
				}
			}

			// Count lost triggers (per board)
			if (dtq != DTQ_SERVICE) {
				if ((Stats.current_trgid[b] > 0) && (Stats.current_trgid[b] > (Stats.previous_trgid[b] + 1)))
					Stats.LostTrg[b].cnt += ((uint32_t)Stats.current_trgid[b] - (uint32_t)Stats.previous_trgid[b] - 1);
				Stats.previous_trgid[b] = Stats.current_trgid[b];
			}
		}

		// ---------------------------------------------------
		// print stats to console
		// ---------------------------------------------------
		if ((((curr_time - print_time) > 1000) && (!Freeze || OneShot)) || PresetReached) {
			char rinfo[100] = "", ror[20], totror[20], trr[20], ss2gui[1024] = "", ss[MAX_NCH][10], torr[100];
			//double lostp[MAX_NBRD], BldPerc[MAX_NBRD];			
			float rtime, tp;
			static char stitle[6][20] = { "ChTrg Rate (cps)", "ChTrg Counts", "Tstamp Rate (cps)", "Tstamp Counts", "PHA Rate (cps)", "PHA Counts" };
			char w_msg[1024] = "";
			char e_msg[1024] = "";
			char es_msg[1024] = "";
			char tmp_brdhv[2048] = "";

			// brdInWarning: 0->NoWarning, 1->WarningNotified,
			if (!offline_conn) {
				for (b = 0; b < J_cfg.NumBrd; b++) {
					ret = Update_Service_Info(handle[b]);
					int brd = b;
					sprintf(tmp_brdhv, "%s %d %d %6.3f %6.3f %5.1f %5.1f %5.1f %5.1f", tmp_brdhv, brd, HV_status[brd], HVMon[brd][HV_VMON], HVMon[brd][HV_IMON], BrdTemp[brd][TEMP_DETECTOR], BrdTemp[brd][TEMP_HV], BrdTemp[brd][TEMP_FPGA], BrdTemp[brd][TEMP_BOARD]);

					if (ret < 0) {	// Most probably lost connection with FERS card
						sprintf(es_msg, "%sLost Connection to board %d\n", es_msg, b);
					} else if (brdInWarning[b] == 1 && BrdTemp[b][TEMP_FPGA] < 70) {
						brdInWarning[b] = 0;
					} else if (BrdTemp[b][TEMP_FPGA] > 82 && BrdTemp[b][TEMP_FPGA] < 200) { // CTIN: add check of board and ASIC temp
						//char wmsg[200] = "";
						if (brdInWarning[b] == 0) { // Warning to be sent
							sprintf(w_msg, "%sWARNING: Board %d is OVERHEATING (T=%2.2f degC). Please provide ventilation to prevent from permanent damages\n", w_msg, b, BrdTemp[b][TEMP_FPGA]);
							wMsg_sent = 0;
							brdInWarning[b] = 1;
						}
					}
					// Check failure error in FERS cards
					if (StatusReg[b] & STATUS_FAIL) {
						if (brdInFail[b] == 0) {
							sprintf(e_msg, "%sERROR: Board Failure (brd=%d, AcqStatus=%08X)\n", e_msg, b, StatusReg[b]);
							eMsg_sent = 0;
							brdInFail[b] = 1;
						}
					}
					// Check CRC errors in FERS cards
					if (StatusReg[b] & STATUS_CRC_ERROR) {
						if (crcBrdError[b] == 0) { // Warning to be sent
							sprintf(w_msg, "%sWARNING: CRC Error (brd=%d, AcqStatus=%08X)\n", w_msg, b, StatusReg[b]);
							wMsg_sent = 0;
							crcBrdError[b] = 1;
						}
					}
					// Check CRC errors in concentrator 
					if ((FERS_CONNECTIONTYPE(handle[0]) == FERS_CONNECTIONTYPE_TDL) && (b == 0)) {
						uint32_t errcnt;
						FERS_GetCrcErrorCnt(FERS_CNCINDEX(handle[0]), &errcnt);
						if ((errcnt >= CrcErrorLevel) && (crcCncError == 1)) {
							sprintf(w_msg, "%sWARNING: CRC Errors in Concentrator (num errors = %d)\n", w_msg, errcnt);
							CrcErrorLevel *= 100;
							wMsg_sent = 0;
							crcCncError = 1;
						}
					}

				}

				if (En_HVstatus_Update)
					Con_printf("Sh", "%s\n", tmp_brdhv);
					//Send_HV_Info(0);

				// Manage warning message, Janus does not quit
				if (strlen(w_msg) > 0 && !wMsg_sent) {
					Con_printf("LCSw", "%s\n", w_msg);
					Con_printf("L", "Board(s) status:\n");
					for (b = 0; b < J_cfg.NumBrd; b++) {  // DNIN: maybe too much infos
						uint32_t acq_stat;
						int ret1;
						ret1 = FERS_ReadRegister(handle[b], a_acq_status, &acq_stat);
						if (ret1 < 0)
							Con_printf("L", "Brd %d: Can't read status registers\n", b);
						else if (BrdTemp[b][TEMP_BOARD] != INVALID_TEMP)
							Con_printf("L", "Brd %d: AcqStatus = %08X, TempFPGA = %2.2f, TempBrd = %2.2f\n", b, acq_stat, BrdTemp[b][TEMP_FPGA], BrdTemp[b][TEMP_BOARD]);
						else
							Con_printf("L", "Brd %d: AcqStatus = %08X, TempFPGA = %2.2f, TempBrd = N.A. \n", b, acq_stat, BrdTemp[b][TEMP_FPGA]);
					}
					wMsg_sent = 1;
				}

				// Manage failure messages in FERS card status register, Janus does not quit
				if (strlen(e_msg) > 0 && !eMsg_sent) {
					int current_status = AcqStatus;
					AcqStatus = ACQSTATUS_ERROR;
					Con_printf("L", "%s\n", e_msg);
					if (current_status != ACQSTATUS_RUNNING) {
						Con_printf("L", "Board(s) status:\n");
						for (b = 0; b < J_cfg.NumBrd; b++) {
							uint32_t acq_stat;
							int ret1;
							ret1 = FERS_ReadRegister(handle[b], a_acq_status, &acq_stat);
							if (ret1 < 0)
								Con_printf("L", "Brd %d: Can't read status registers\n", b);
							else if (BrdTemp[b][TEMP_BOARD] != INVALID_TEMP)
								Con_printf("L", "Brd %d: AcqStatus = %08X, TempFPGA = %2.2f, TempBrd = %2.2f\n", b, acq_stat, BrdTemp[b][TEMP_FPGA], BrdTemp[b][TEMP_BOARD]);  // Add Service Event Info [Temperatures]
							else
								Con_printf("L", "Brd %d: AcqStatus = %08X, TempFPGA = %2.2f, TempBrd = N.A. \n", b, acq_stat, BrdTemp[b][TEMP_FPGA]);
						}
					}
					SendAcqStatusMsg("%s", e_msg);
					if (SockConsole) rdymsg = 0;
					if (current_status == ACQSTATUS_RUNNING) StopRun();
					eMsg_sent = 1;
				}

				// Lost connection
				if (strlen(es_msg) > 0) { // At the moment, lost connection cannot be recovered
					if (AcqStatus == ACQSTATUS_RUNNING) StopRun();
					strcpy(ErrorMsg, es_msg);
					goto ManageError;
				}

				Send_HV_Info(0);

			}

			if (J_cfg.StopRunMode == STOPRUN_PRESET_TIME) {
				if (J_cfg.EnableJobs) sprintf(rinfo, "(%d of %d, Preset = %.2f s)", jobrun - J_cfg.JobFirstRun + 1, J_cfg.JobLastRun - J_cfg.JobFirstRun + 1, J_cfg.PresetTime);
				else sprintf(rinfo, "(Preset = %.2f s)", J_cfg.PresetTime);
			} else if (J_cfg.StopRunMode == STOPRUN_PRESET_COUNTS) {
				if (J_cfg.EnableJobs) sprintf(rinfo, "(%d of %d, Preset = %d cnts)", jobrun - J_cfg.JobFirstRun + 1, J_cfg.JobLastRun - J_cfg.JobFirstRun + 1, J_cfg.PresetCounts);
				else sprintf(rinfo, "(Preset = %d cnts)", J_cfg.PresetCounts);
			} else {
				if (J_cfg.EnableJobs) sprintf(rinfo, "(%d of %d)", jobrun - J_cfg.JobFirstRun + 1, J_cfg.JobLastRun - J_cfg.JobFirstRun + 1);
				else rinfo[0] = '\0';
			}

			//if ((En_HVstatus_Update && ((curr_time - print_time) > 3500)) && !offline_conn) {// DNIN: all the boards should have the same firmware   FERS_FPGA_FW_MajorRev(handle[0]) >= 4 
			//	Send_HV_Info();
			//}

			if ((AcqStatus == ACQSTATUS_READY) && rdymsg && !PresetReached) {
				SendAcqStatusMsg("Ready to start Run #%d %s", RunVars.RunNumber, rinfo);
				Con_printf("C", "Press [s] to start, [q] to quit, [SPACE] to enter the menu\n");
				rdymsg = 0;
			} else if (AcqStatus == ACQSTATUS_RUNNING) {
				if (!SockConsole) {
					if (clrscr) ClearScreen();
					clrscr = 0;
					gotoxy(1, 1);
				}

				UpdateStatistics(StatIntegral);

				// Calculate Total data throughput
				double totrate = 0;
				for (i = 0; i < J_cfg.NumBrd; ++i) {
					totrate += Stats.ByteCnt[i].rate;
					double2str(totrate, 1, totror);
				}

				if (PresetReached) {
					if (J_cfg.StopRunMode == STOPRUN_PRESET_TIME) rtime = J_cfg.PresetTime;
					tp = 100;
				} else {
					rtime = (float)(curr_time - Stats.start_time) / 1000;
					if (J_cfg.StopRunMode == STOPRUN_PRESET_TIME) tp = J_cfg.PresetTime > 0 ? 100 * rtime / J_cfg.PresetTime : 0;
					else tp = J_cfg.PresetCounts > 0 ? 100 * (float)Stats.GlobalTrgCnt[0].cnt / J_cfg.PresetCounts : 0;
				}

				if (J_cfg.StopRunMode == STOPRUN_PRESET_TIME) SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f = %.2f%% %s", RunVars.RunNumber, rtime, tp, rinfo);
				else if (J_cfg.StopRunMode == STOPRUN_PRESET_COUNTS) SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f (%.2f%%) %s", RunVars.RunNumber, rtime, tp, rinfo);
				else {
					if (SockConsole) SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f s. Readout = %sB/s", RunVars.RunNumber, rtime, totror);
					else SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f s", RunVars.RunNumber, rtime);
				}

				if (StatMode == 0) {  // Single boards statistics
					int ab = RunVars.ActiveBrd;
					double2str(Stats.ByteCnt[ab].rate, 1, ror);
					double2str(Stats.GlobalTrgCnt[ab].rate, 1, trr);
					double2str(Stats.T_OR_Cnt[ab].rate, 1, torr);
					for (i = 0; i < MAX_NCH; i++) {
						if (RunVars.SMonType == SMON_CHTRG_RATE) {
							if ((MajorFWrev >= 3) || (J_cfg.AcquisitionMode == ACQMODE_COUNT)) double2str(Stats.ChTrgCnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_CHTRG_CNT) {
							if ((MajorFWrev >= 3) || (J_cfg.AcquisitionMode == ACQMODE_COUNT)) cnt2str(Stats.ChTrgCnt[ab][i].cnt, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_HIT_RATE) {
							if (J_cfg.AcquisitionMode & 0x2) double2str(Stats.HitCnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_HIT_CNT) {
							if (J_cfg.AcquisitionMode & 0x2) cnt2str(Stats.HitCnt[ab][i].cnt, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_PHA_RATE) {
							if (J_cfg.AcquisitionMode & 0x1) double2str(Stats.PHACnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_PHA_CNT) {
							if (J_cfg.AcquisitionMode & 0x1) cnt2str(Stats.PHACnt[ab][i].cnt, ss[i]); 
							else sprintf(ss[i], "N/A     ");
						}
					}

					if (SockConsole) {
						Con_printf("CSSb", "%d\n", ab);
						char sg2gui[1024];
						sprintf(sg2gui, "%s\t%.3lf s\t%" PRIu64 "\t%scps\t%.2f%%\t%" PRIu32 "\t%.2f%%\t%sB/s\t%scps", 
							stitle[RunVars.SMonType], 
							Stats.current_tstamp_us[ab] / 1e6, 
							Stats.current_trgid[ab], 
							trr, 
							Stats.LostTrgPerc[ab], 
							(uint32_t)Stats.LostTrg[ab].cnt, 
							Stats.BuildPerc[ab],
							ror, 
							torr);
						Con_printf("CSSg", "%s\n", sg2gui);
						for (i = 0; i < MAX_NCH; i++) {
							char tmp_ss2gui[256] = "";
							sprintf(tmp_ss2gui, "%s", ss[i]);
							strcat(ss2gui, tmp_ss2gui);
						}
						Con_printf("SS", "c%s", ss2gui);
					} else {
						if (J_cfg.NumBrd > 1) Con_printf("C", "Board n. %d (press [b] to change active board)\n", ab);
						Con_printf("C", "Time Stamp:   %10.3lf s                \n", Stats.current_tstamp_us[ab] / 1e6);
						Con_printf("C", "Trigger-ID:   %10lld                   \n", Stats.current_trgid[ab]);
						Con_printf("C", "Trg Rate:        %scps                 \n", trr);
						Con_printf("C", "Trg Reject:   %10.2lf %%               \n", Stats.LostTrgPerc[ab]);
						Con_printf("C", "Tot Lost Trg: %10" PRIu32 "            \n", Stats.LostTrg[ab].cnt);
						Con_printf("C", "EvBuild:      %10.2lf %%               \n", Stats.BuildPerc[ab]);
						Con_printf("C", "T-OR Rate:       %scps                 \n", torr);
						if (J_cfg.NumBrd > 1)
							Con_printf("C", "Readout Rate:    %sB/s (Tot: %sB/s)             \n", ror, totror);
						else
							Con_printf("C", "Readout Rate:    %sB/s                   \n", ror);
						if (BrdTemp[ab][TEMP_BOARD] != INVALID_TEMP)
							Con_printf("C", "Temp (degC):     FPGA=%4.1f PCB=%4.1f              \n", BrdTemp[ab][TEMP_FPGA], BrdTemp[ab][TEMP_BOARD]);
						else
							Con_printf("C", "Temp (degC):     FPGA=%4.1f PCB=N.A.               \n", BrdTemp[ab][TEMP_FPGA]);
						Con_printf("C", "\n");
						if (StatIntegral) Con_printf("C", "Statistics averaging: Integral (press [I] for Updating mode)\n");
						else Con_printf("C", "Statistics averaging: Updating (press [I] for Integral mode)\n");
						Con_printf("C", "Press [tab] to view statistics of all boards\n\n");
						for (i = 0; i < MAX_NCH; i++) {
							Con_printf("C", "%02d: %s     ", i, ss[i]);
							if ((i & 0x3) == 0x3) Con_printf("C", "\n");
						}
						Con_printf("C", "\n");
					}
				} else { // Multi boards statistics
					if (SockConsole) {
						char tmp_brdstat[256] = "";
						char full_brdstat[2048] = "";
						Con_printf("CSSt", "%s\n", stitle[RunVars.SMonType]);
						for (i = 0; i < J_cfg.NumBrd; ++i) {
							sprintf(tmp_brdstat, " %3d %12.2lf %12" PRIu64 " %12.5lf %12.5lf %12.2lf %12.5lf", i, Stats.current_tstamp_us[i] / 1e6, Stats.current_trgid[i], Stats.GlobalTrgCnt[i].rate / 1000, Stats.LostTrgPerc[i], Stats.BuildPerc[i], Stats.ByteCnt[i].rate / (1024 * 1024));
							strcat(full_brdstat, tmp_brdstat);
						}
						Con_printf("CSSB", "%s\n", full_brdstat);
					} else {
						Con_printf("C", "\n");
						if (StatIntegral) Con_printf("C", "Statistics averaging: Integral (press [I] for Updating mode)\n");
						else Con_printf("C", "Statistics averaging: Updating (press [I] for Integral mode)\n");
						Con_printf("C", "Press [tab] to view statistics of single boards\n\n");
						Con_printf("C", "%3s %10s %10s %10s %10s %10s %10s\n", "Brd", "TStamp", "Trg-ID", "TrgRate", "LostTrg", "Build", "DtRate");
						Con_printf("C", "%3s %10s %10s %10s %10s %10s %10s\n\n", "", "[s]", "[cnt]", "[KHz]", "[%]", "[%]", "[MB/s]");
						for (i = 0; i < J_cfg.NumBrd; i++)
							Con_printf("C", "%3d %10.2lf %10" PRIu64 " %10.5lf %10.5lf %10.2lf %10.5lf\n", i, Stats.current_tstamp_us[i] / 1e6, Stats.current_trgid[i], Stats.GlobalTrgCnt[i].rate / 1000, Stats.LostTrgPerc[i], Stats.BuildPerc[i], Stats.ByteCnt[i].rate / (1024 * 1024));
					}
				}
			}

			// ---------------------------------------------------
			// plot histograms
			// ---------------------------------------------------
			if ((J_cfg.DataAnalysis & DATA_ANALYSIS_HISTO)) {
				plot_histogram();
			}
			if (RunVars.PlotType == PLOT_SCAN_THR) 			PlotStaircase();
			else if (RunVars.PlotType == PLOT_SCAN_HOLD_DELAY)		PlotScanHoldDelay(&HoldScan_newrun);
			OneShot = 0;
			print_time = curr_time;
		}
	}

ExitPoint:
	if (RestartAll || Quit) {
		StopRun();
		//if (RestartAll) {
		//	for (b = 0; b < J_cfg.NumBrd; b++) {	// DNIN: configure the board after each run stop, to avoid block of the readout in the next run
		//		int ret = ConfigureFERS(handle[b], CFG_HARD);
		//		if (ret < 0) {
		//			Con_printf("LCSm", "Configuration Boards Failed!!!\n");
		//			Con_printf("LCSm", "%s", ErrorMsg);
		//			return -1;
		//		}
		//	}
		//}
		if (Quit && !offline_conn) CheckHVBeforeClosing(); // DNIN: This have to be done before closing the comm with Boards
		for (b = 0; b < J_cfg.NumBrd; b++) {
			if (handle[b] < 0) break;
			FERS_CloseReadout(handle[b]);
			FERS_CloseDevice(handle[b]);
		}
		for (b = 0; b < MAX_NCNC; b++) {
			if (cnc_handle[b] < 0) break;
			FERS_CloseDevice(cnc_handle[b]);
		}
		DestroyStatistics();
		ClosePlotter();
		if (RestartAll) {
			if (AcqStatus == ACQSTATUS_RUNNING) AcqStatus = ACQSTATUS_RESTARTING;
			RestartAll = 0;
			Quit = 0;
			goto ReadCfg;
		} else {
			Con_printf("LCSm", "Quitting...\n");
			fclose(MsgLog);
			return 0;
		}
	} else if (RestartAcq) {
		if (AcqStatus == ACQSTATUS_RUNNING) {
			StopRun();
			AcqStatus = ACQSTATUS_RESTARTING;
		}
		RestartAcq = 0;
		goto Restart;
	}

	if (J_cfg.EnableJobs) {
		PresetReached = 0;
		Con_printf("LCSm", "Run %d closed\n", jobrun);
		if (jobrun < J_cfg.JobLastRun) {
			jobrun++;
			SendAcqStatusMsg("Getting ready for Run #%d of the Job...", jobrun);
		}
		else jobrun = J_cfg.JobFirstRun;
		RunVars.RunNumber = jobrun;
		SaveRunVariables(RunVars);
		if (!SockConsole) ClearScreen();
	}
	goto Restart;

ManageError:
	AcqStatus = ACQSTATUS_ERROR;
	Con_printf("Le", "ERROR: %s", ErrorMsg);
	SendAcqStatusMsg("ERROR: %s", ErrorMsg);
	if (SockConsole) {
		while (1) {
			if (Con_kbhit()) {
				int c = Con_getch();
				if (c == 'q') {
					Sleep(100);
					Con_printf("LCSm", "Quitting...\n");
					Sleep(100);
					break;
				}
			}
			int upd = CheckFileUpdate();	
			if (upd > 0) {
				RestartAll = 1;
				goto ExitPoint;
			}
			Sleep(100);
		}
		//goto ExitPoint;
	}
	else {
		Quit = 1;
		Con_getch();
		goto ExitPoint;
	}
}
