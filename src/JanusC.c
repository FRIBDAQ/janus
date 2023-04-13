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

#include "JanusC.h"
#include "plot.h"
#include "console.h"
#include "FERSutils.h"
#include "paramparser.h"
#include "configure.h"
#include "outputfiles.h"
#include "Statistics.h"
#include "FERSlib.h"

// ******************************************************************************************
// Global Variables
// ******************************************************************************************
Config_t WDcfg;					// struct with all parameters
RunVars_t RunVars;				// struct containing run time variables
ServEvent_t sEvt[MAX_NBRD];		// struct containing the service information of each boards
int SockConsole = 0;			// 0=stdio, 1=socket
int AcqStatus = 0;				// Acquisition Status (running, stopped, fail, etc...)
int HoldScan_newrun = 0;
int handle[MAX_NBRD];			// Board handles
int cnc_handle[MAX_NCNC];		// Concentrator handles
int UsingCnc = 0;				// Using concentrator
int Freeze = 0;					// stop plot
int OneShot = 0;				// Single Plot 
int DisableDataAnalysys = 0;	// Disable data analysis (read and flush data, for debugging)
int StatMode = 0;				// Stats monitor: 0=per board, 1=all baords
int StatIntegral = 0;			// Stats caclulation: 0=istantaneous, 1=integral
int Quit = 0;					// Quit readout loop
int RestartAcq = 0;				// Force restart acq (reconfigure HW)
int RestartAll = 0;				// Restart from the beginning (recovery after error)
int SkipConfig = 0;				// Skip board configuration
double build_time_us = 0;		// Time of the event building (in us)
int En_HVstatus_Update = 0;		// Enable periodic update of HV status
int HV_status[MAX_NBRD] = { 0 };	// HV status; bit 0 = ON/OFF, bit 1 = Over current/voltage, bit 2 = ramping up/down
char ErrorMsg[250];				// Error Message
FILE* MsgLog = NULL;			// message log output file

int jobrun = 0;
uint8_t stop_sw = 0; // Used to disable automatic start during jobs

int ServerDead = 0;


// ******************************************************************************************
// Save/Load run time variables, utility functions...
// ******************************************************************************************
int LoadRunVariables(RunVars_t* RunVars)
{
	char str[100];
	int i;
	FILE* ps = fopen(RUNVARS_FILENAME, "r");
	//char plot_name[50];	// name of histofile.
	int mline = 0;

	// set defaults
	RunVars->PlotType = PLOT_E_SPEC_HG;
	RunVars->SMonType = SMON_CHTRG_RATE;
	RunVars->RunNumber = 0;
	RunVars->ActiveCh = 0;
	RunVars->ActiveBrd = 0;
	for (i = 0; i < 4; i++) RunVars->StaircaseCfg[i] = 0;
	for (i = 0; i < 4; i++) RunVars->HoldDelayScanCfg[i] = 0;
	for (int i = 0; i < 8; i++)
		sprintf(RunVars->PlotTraces[i], "");
	if (ps == NULL) return -1;
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
			if ((v1 >= 0) && (v1 < 8) && (v2 >= 0) && (v2 < MAX_NBRD) && (v3 >= 0) && (v3 < MAX_NCH) && (c1[0] == 'B' || c1[0] == 'F' || c1[0] == 'S'))
				sprintf(RunVars->PlotTraces[v1], "%d %d %s", v2, v3, c1);
			else
				continue;
			//char tmp_var[50];
			//strcpy(tmp_var, Stats.H1_PHA_LG[v2][v3].title);
			if (Stats.H1_File[0].H_data == NULL) // Check if statistics have been created
				continue;

			Stats.offline_bin = -1; // Reset offline binning

			if ((c1[0] == 'F' || c1[0] == 'S') && (RunVars->PlotType < 4 || RunVars->PlotType == 9))
				Histo1D_Offline(v1, v2, v3, c1, RunVars->PlotType);
			else
				continue;
		}
		else if (strcmp(str, "Staircase") == 0) {
			for (i = 0; i < 4; i++)
				fscanf(ps, "%d", &RunVars->StaircaseCfg[i]);
		}
		else if (strcmp(str, "HoldDelayScan") == 0) {
			for (i = 0; i < 4; i++)
				fscanf(ps, "%d", &RunVars->HoldDelayScanCfg[i]);
		}
	}
	if (RunVars->ActiveBrd >= WDcfg.NumBrd) RunVars->ActiveBrd = 0;
	fclose(ps);
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

	if (RunVars.StaircaseCfg[3] > 0)
		fprintf(ps, "Staircase     %d %d %d %d\n", RunVars.StaircaseCfg[0], RunVars.StaircaseCfg[1], RunVars.StaircaseCfg[2], RunVars.StaircaseCfg[3]);
	if (RunVars.HoldDelayScanCfg[3] > 0)
		fprintf(ps, "HoldDelayScan %d %d %d %d\n", RunVars.HoldDelayScanCfg[0], RunVars.HoldDelayScanCfg[1], RunVars.HoldDelayScanCfg[2], RunVars.HoldDelayScanCfg[3]);
	fclose(ps);
	return 0;
}

// Parse the config file of a run during a job. If the corresponding cfg file is not found, the default config is parsed
void job_read_parse() {
	FILE* cfg;
	if (WDcfg.EnableJobs) {  // When running a job, load the specific config file for this run number
		int TmpJRun = WDcfg.JobFirstRun;
		char fname[200];
		sprintf(fname, "%sJanus_Config_Run%d.txt", WDcfg.DataFilePath, jobrun);
		cfg = fopen(fname, "r");
		if (cfg != NULL) {
			ParseConfigFile(cfg, &WDcfg, 1);
			fclose(cfg);
			WDcfg.JobFirstRun = TmpJRun;	// The first run cannot be overwrite from the config file
		} else {
			sprintf(fname, "Janus_Config.txt");
			cfg = fopen(fname, "r");
			if (cfg != NULL) {
				ParseConfigFile(cfg, &WDcfg, 1);
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
	else Con_printf("C", "%-70s\n", msg);
}


// Send HV info to socket
void Send_HV_Info()
{
	char tmp_brdhv[1024] = "";
	float vmon, imon, dtemp, itemp, fpga_temp;
	int b_on, s_on, ovc, ovv, ramp;
	//int brd = FERS_INDEX(handle);
	static int first_call = 1;
	uint64_t now = get_time();

	for (int brd = 0; brd < WDcfg.NumBrd; ++brd) {
		if (sEvt[brd].update_time > (now - 2000)) {
			vmon = sEvt[brd].hv_Vmon;
			imon = sEvt[brd].hv_Imon;
			dtemp = sEvt[brd].tempDetector;
			itemp = sEvt[brd].tempHV;
			fpga_temp = sEvt[brd].tempFPGA;
			b_on = sEvt[brd].hv_status_on;
			ramp = sEvt[brd].hv_status_ramp;
			ovc = sEvt[brd].hv_status_ovc;
			ovv = sEvt[brd].hv_status_ovv;
		} else {
			HV_Get_Vmon(handle[brd], &vmon);
			HV_Get_Imon(handle[brd], &imon);
			HV_Get_DetectorTemp(handle[brd], &dtemp);
			HV_Get_IntTemp(handle[brd], &itemp);
			HV_Get_Status(handle[brd], &b_on, &ramp, &ovc, &ovv);
			FERS_Get_FPGA_Temp(handle[brd], &fpga_temp);
		}
		s_on = HV_status[brd] & 1; // s_on = "on" from local status; b_on = "on" from board
		if (first_call)
			HV_status[brd] = b_on | ((ovc | ovv) << 1);
		else if (b_on == 0) // HV is OFF
			HV_status[brd] = (ovc | ovv) << 1;
		else if (s_on == 0) // HV is ON, but OFF command has been sent => ramping down
			HV_status[brd] = ((ovc | ovv) << 1) | 0x4;
		else if (vmon < (0.95 * WDcfg.HV_Vbias[brd])) // HV is ON, but Vmon < (0.95*Vset) => ramping up
			HV_status[brd] = 0x1 | ((ovc | ovv) << 1) | 0x4;
		else // HV is ON and Vmon = Vset
			HV_status[brd] = 0x1 | ((ovc | ovv) << 1);

		if (brd == WDcfg.NumBrd - 1) first_call = 0; // when GUI re-connect found for all the board the correct status, elsewhere HV_status for brd!=0 is 4
		if (fpga_temp < 200) sprintf(tmp_brdhv, "%s %d %d %6.3f %6.3f %5.1f %5.1f %5.1f", tmp_brdhv, brd, HV_status[brd], vmon, imon, dtemp, itemp, fpga_temp);
		else sprintf(tmp_brdhv, "%s %d %d %6.3f %6.3f %5.1f %5.1f -1", tmp_brdhv, brd, HV_status[brd], vmon, imon, dtemp, itemp);
	}
	Con_printf("Sh", "%s", tmp_brdhv);
		//if (fpga_temp < 200) Con_printf("Sh", "%d %d %6.3f %6.3f %5.1f %5.1f %5.1f", brd, HV_status[brd], vmon, imon, dtemp, itemp, fpga_temp);
	//else Con_printf("Sh", "%d %d %6.3f %6.3f %5.1f %5.1f -1", brd, HV_status[brd], vmon, imon, dtemp, itemp);
}

// HV set ON/OFF and control ramp
int HV_Switch_OnOff(int handle, int onoff)
{
	int i, Err = 0, pstat = AcqStatus, b_on, ramp, ovc, ovv, hv_status;
	int brd = FERS_INDEX(handle);
	float vmon, imon, vset = WDcfg.HV_Vbias[brd];

	if (onoff == (HV_status[brd] & 1)) return 0;
	HV_status[brd] = (HV_status[brd] & 0xFE) | onoff;
	AcqStatus = ACQSTATUS_RAMPING_HV;
	HV_Set_Vbias(handle, WDcfg.HV_Vbias[brd]);
	HV_Set_Imax(handle, WDcfg.HV_Imax[brd]);
	HV_Set_OnOff(handle, onoff);
	for (i = 0; i < 50 && !Err; i++) {
		for (int j = 0; j < 10; j++) {
			HV_Get_Vmon(handle, &vmon);
			HV_Get_Imon(handle, &imon);
			if ((vmon > 0) && (vmon < 85) && (imon >= 0) && (imon < 11)) break; // patch for occasional wrong reading
			Sleep(100);
		}
		HV_Get_Status(handle, &b_on, &ramp, &ovc, &ovv);
		hv_status = (ovc | ovv) ? 0x2 : onoff;
		Con_printf("Sh", "%d %d %6.3f %6.3f %5.1f %5.1f %5.1f", FERS_INDEX(handle), hv_status, vmon, imon, .0, .0, .0);
		if (ovc) {
			SendAcqStatusMsg("HV Brd %d Over Current! Shutting down...", brd);
			Con_printf("LCSm", "ERROR: failed to set HV of Brd %d for Over Current\n", brd);
			Err = 1;
		}
		else if (ovv) {
			SendAcqStatusMsg("HV Brd %d Over Voltage! Shutting down...", brd);
			Con_printf("LCSm", "ERROR: failed to set HV of Brd %d for Over Voltage\n", brd);
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
		Sleep(100);
	}
	if (Err) {
		Sleep(1500);
	}
	else {
		Sleep(300);
		HV_Get_Vmon(handle, &vmon);
		HV_Get_Imon(handle, &imon);
		if (onoff) {
			Con_printf("LCSm", "HV of Brd %d is ON. Vset = %.3f V, Vmon = %.3f V, Imon = %.3f mA\n", brd, WDcfg.HV_Vbias[brd], vmon, imon);
			// set a global ON/OFF HV status
		}
		else Con_printf("LCSm", "HV of Brd %d is OFF (Vmon = %.3f V)\n", brd, vmon);	// DNIN: Lorenzo suggests to remove Vmon infos
	}
	Send_HV_Info();
	AcqStatus = pstat;
	return 0;
}

void reportProgress(char* msg, int progress)
{
	char fwmsg[100];
	if (progress > 0) sprintf(fwmsg, "Progress: %3d %%", progress);
	else strcpy(fwmsg, msg);
	SendAcqStatusMsg(fwmsg);
}

void CheckHVBeforeClosing() {
	int bnf = 0, rmp, ovc, ovv, ret;
	const int nn = WDcfg.NumBrd;
	int brd_on[MAX_NBRD] = {};
	if (!WDcfg.AskHVShutDownOnExit) {
		if (SockConsole) Con_printf("LCSm", "Quitting ...\n");
		return;
	}
	for (int mb = 0; mb < WDcfg.NumBrd; ++mb) {	// Check if any boards is still ON
		int ttmp;
		HV_Get_Status(handle[mb], &ttmp, &rmp, &ovc, &ovv);
		bnf |= ttmp;
		brd_on[mb] = ttmp;
	}
	if (bnf == 1) {
		ret = Con_printf("LCSw", "WARNING: HV is still ON, do you want to turn it off? [y] [n]\n");
		if (ServerDead != 0) {
			Con_printf("LCSm", "Turning HV off ...\n");
			for (int mb = 0; mb < WDcfg.NumBrd; ++mb) {
				if (brd_on[mb]) {
					for (int j = 0; j < 10; ++j) { // Sometimes it seems to fail, just a patch
						ret = HV_Set_OnOff(handle[mb], 0); // Switching off all the boards (only the indexes of the boards 'ON' can be saved)
						if (!ret) break;
						Sleep(100);
					}
					for (int j = 0; j < 10; j++) { // Turning off without sending info to GUI
						float vmon;
						HV_Get_Vmon(handle[mb], &vmon);
						if (vmon < 20) break;
						Sleep(100);
					}
				}
			}
		}
		else if (Con_getch() == 'y') {
			Con_printf("LCSm", "Turning HV off ...\n");
			for (int mb = 0; mb < WDcfg.NumBrd; ++mb) {
				if (brd_on[mb]) {
					//HV_Switch_OnOff(handle[mb], 0); 
					HV_Set_OnOff(handle[mb], 0); // Switching off all the boards (only the indexes of the boards 'ON' can be saved)
					for (int j = 0; j < 10; j++) {
						float vmon;
						HV_Get_Vmon(handle[mb], &vmon);
						if (vmon < 20) break; 
						Sleep(100);
					}
				}
			}
			//if (SockConsole) {
			//	Con_printf("LCSm", "HV correctly turned off, quitting ...\n");
			//}
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

	for (b = 0; b < WDcfg.NumBrd; b++) {
		if (FERS_CONNECTIONTYPE(handle[b]) != FERS_CONNECTIONTYPE_TDL) tdl = 0;
	}
	if (!tdl && (WDcfg.StartRunMode == STARTRUN_TDL)) {
		WDcfg.StartRunMode = STARTRUN_ASYNC;
		Con_printf("LCSm", "WARNING: can't start in TDL mode; switched to Async mode\n");
	}

	ret = FERS_StartAcquisition(handle, WDcfg.NumBrd, WDcfg.StartRunMode);

	Stats.start_time = get_time();
	time(&Stats.time_of_start);
	build_time_us = 0;
	Stats.stop_time = 0;
	if (AcqStatus != ACQSTATUS_RESTARTING) Con_printf("LCSm", "Run #%d started\n", RunVars.RunNumber);
	AcqStatus = ACQSTATUS_RUNNING;

	stop_sw = 0; // Set at 0, used to prevent automatic start during jobs after a sw stop

	return ret;
}

// Stop Run
int StopRun() {
	int ret = 0;

	ret = FERS_StopAcquisition(handle, WDcfg.NumBrd, WDcfg.StartRunMode);

	if (Stats.stop_time == 0) Stats.stop_time = get_time();
	SaveHistos();
	if (WDcfg.OutFileEnableMask & OUTFILE_RUN_INFO) SaveRunInfo();
	CloseOutputFiles();
	if (AcqStatus == ACQSTATUS_RUNNING) {
		Con_printf("LCSm", "Run #%d stopped. Elapsed Time = %.2f\n", RunVars.RunNumber, (float)(Stats.stop_time - Stats.start_time) / 1000);
		if ((WDcfg.RunNumber_AutoIncr) && (!WDcfg.EnableJobs)) {  //
			++RunVars.RunNumber; 
			SaveRunVariables(RunVars);
		}
		AcqStatus = ACQSTATUS_READY;
	}

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
		const Config_t WDcfg_1 = WDcfg;
		//memcpy(&WDcfg_1, &WDcfg, sizeof(Config_t));
		cfg = fopen(CONFIG_FILENAME, "r");
		ParseConfigFile(cfg, &WDcfg, 1);
		fclose(cfg);
		Con_printf("LCSm", "Config file reloaded\n");
		if ((WDcfg_1.NumBrd != WDcfg.NumBrd) ||
			(WDcfg_1.NumCh != WDcfg.NumCh) ||
			(WDcfg_1.EnableJobs != WDcfg.EnableJobs) ||
			(WDcfg_1.JobFirstRun != WDcfg.JobFirstRun) ||
			(WDcfg_1.JobLastRun != WDcfg.JobLastRun))		ret = 3;
		else if ((WDcfg_1.AcquisitionMode != WDcfg.AcquisitionMode) ||
			(WDcfg_1.OutFileEnableMask != WDcfg.OutFileEnableMask) ||
			(WDcfg_1.ToAHistoNbin != WDcfg.ToAHistoNbin) ||
			(WDcfg_1.EHistoNbin != WDcfg.EHistoNbin))		ret = 2;
		else												ret = 1;
	}
	CfgUpdateTime = CurrentTime;

	/*GetFileUpdateTime("weerocGUI.txt", &CurrentTime);
	if ((CurrentTime > CfgUpdateTime) && !first) ret = 1;*/

	GetFileUpdateTime(RUNVARS_FILENAME, &CurrentTime);
	if ((CurrentTime > RunVarsUpdateTime) && !first) {
		RunVars_t RunVars1;
		memcpy(&RunVars1, &RunVars, sizeof(RunVars_t));
		LoadRunVariables(&RunVars);
		if (WDcfg.EnableJobs) {
			RunVars.RunNumber = RunVars1.RunNumber;
			SaveRunVariables(RunVars);
		}
		if (RunVars1.RunNumber != RunVars.RunNumber) ret = 1;
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
	if (c == 't') {
		for (b = 0; b < WDcfg.NumBrd; b++)
			FERS_SendCommand(handle[b], CMD_TRG);	// SW trg
	}
	if ((c == 's') && (AcqStatus == ACQSTATUS_READY)) {
		ResetStatistics();
		StartRun();
	}
	if ((c == 'S') && (AcqStatus == ACQSTATUS_RUNNING)) {
		StopRun();
		if (WDcfg.EnableJobs) {	// DNIN: In case of stop from keyboard the jobrun is increased
			if (jobrun < WDcfg.JobLastRun) jobrun++;
			else jobrun = WDcfg.JobFirstRun;
			RunVars.RunNumber = jobrun;
			SaveRunVariables(RunVars);
			stop_sw = 1;
		}
		job_read_parse();
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
		if ((new_brd >= 0) && (new_brd < WDcfg.NumBrd)) {
			RunVars.ActiveBrd = new_brd;
			if (!SockConsole) sprintf(RunVars.PlotTraces[0], "%d %d B", RunVars.ActiveBrd, cc);
			else Con_printf("Sm", "Active Board = %d\n", RunVars.ActiveBrd);
		}
	}
	if (c == 'c') {
		int new_ch;
		if (!SockConsole) {
			char chs[10];
			printf("Current Active Channel = %d (%c%c)\n", cc, 'A' + ch2x(cc), '1' + ch2y(cc));
			printf("New Active Channel (ch# or pixel) = ");
			myscanf("%s", &chs);
			if (isdigit(chs[0])) sscanf(chs, "%d", &new_ch);
			else new_ch = xy2ch(toupper(chs[0]) - 'A', chs[1] - '1');
		} else {
			Con_GetInt(&new_ch);
		}
		if ((new_ch >= 0) && (new_ch < MAX_NCH)) {
			//RunVars.ActiveCh = new_ch;
			sprintf(RunVars.PlotTraces[0], "%d %d B", RunVars.ActiveBrd, new_ch);
			ConfigureProbe(handle[RunVars.ActiveBrd]);
		}
	}
	if ((c == 'm') && !SockConsole)
		ManualController(handle[RunVars.ActiveBrd]);
	if (c == 'h') {
		if (SockConsole) {
			int b;
			Con_GetInt(&b);
			if ((b >= 0) && (b < WDcfg.NumBrd)) Send_HV_Info();
		}
		else HVControlPanel(handle[RunVars.ActiveBrd]);
	}
	if ((c == 'H') && (SockConsole)) {
		int b;
		int onoff = (Con_getch() - '0') & 0x1;
		Con_GetInt(&b);
		HV_Switch_OnOff(handle[b], onoff);
	}
	if (c == 'V') {
		if (SockConsole)
			En_HVstatus_Update = Con_getch() - '0';
		else
			En_HVstatus_Update ^= 1;
	}
	if ((c == 'M') && !SockConsole)
		CitirocControlPanel(handle[RunVars.ActiveBrd]);
	if (c == 'r') {
		AcqStatus = ACQSTATUS_RESTARTING;
		SkipConfig = 1;
		RestartAcq = 1;
	}
	if (c == 'j') {
		if (WDcfg.EnableJobs) {
			if(AcqStatus == ACQSTATUS_RUNNING) StopRun();
			jobrun = WDcfg.JobFirstRun;
			RunVars.RunNumber = jobrun;
			SaveRunVariables(RunVars);
			return 100; // To letthe main loop know that 'j' was pressed.
		}
	}
	if ((c == 'R') && (SockConsole)) {
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
		DisableDataAnalysys ^= 1;
		if (DisableDataAnalysys)
			printf("Data Analysis disabled\n");
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
	}
	if (c == 'y') {	// staircase
		int nstep;
		if (!SockConsole) {
			while (1) {
				ClearScreen();
				printf("[1] Min Threshold = %d\n", RunVars.StaircaseCfg[SCPARAM_MIN]);
				printf("[2] Max Threshold = %d\n", RunVars.StaircaseCfg[SCPARAM_MAX]);
				printf("[3] Step = %d\n", RunVars.StaircaseCfg[SCPARAM_STEP]);
				printf("[4] Dwell Time (ms) = %d\n", RunVars.StaircaseCfg[SCPARAM_DWELL]);
				printf("[0] Start Scan\n");
				printf("[r] Return to main menu\n");
				c = getch();
				if (c == '0' || c == 'r') break;
				printf("Enter new value: ");
				if (c == '1') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_MIN]);
				if (c == '2') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_MAX]);
				if (c == '3') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_STEP]);
				if (c == '4') scanf("%d", &RunVars.StaircaseCfg[SCPARAM_DWELL]);
			}
			SaveRunVariables(RunVars);
		} else {
			LoadRunVariables(&RunVars);
		}
		if (RunVars.StaircaseCfg[SCPARAM_STEP] > 0 && c != 'r') {
			nstep = (RunVars.StaircaseCfg[SCPARAM_MAX] - RunVars.StaircaseCfg[SCPARAM_MIN]) / RunVars.StaircaseCfg[SCPARAM_STEP] + 1;
			if (nstep > STAIRCASE_NBIN) {
				Con_printf("LCSm", "ERROR: too many steps in scan threshold. Staircase aborted\n");
			} else {
				sscanf(RunVars.PlotTraces[0], "%d", &b);
				if ((b >= 0) && (b < WDcfg.NumBrd)) {
					ScanThreshold(handle[b]);
					ConfigureFERS(handle[b], CFG_HARD);
				}
				SaveHistos();
				int newRV = RunVars.RunNumber;
				if ((WDcfg.RunNumber_AutoIncr) && (!WDcfg.EnableJobs))
					newRV++;
				LoadRunVariables(&RunVars);
				RunVars.RunNumber = newRV;
				SaveRunVariables(RunVars);
			}
			//SaveHistos();
			//RestartAcq = 1;
		}
	}
	if (c == 'Y') {	// Hold-Delay
		if (!SockConsole) {
			while (1) {
				ClearScreen();
				printf("[1] Min Delay (ns) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MIN]);
				printf("[2] Max Delay (ns) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MAX]);
				printf("[3] Step (ns, multiple of 8) = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_STEP]);
				printf("[4] Nmean = %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN]);
				printf("[0] Start Scan\n");
				printf("[r] Return to main menu\n");
				c = getch();
				if (c == '0' || c == 'r') break;
				printf("Enter new value: ");
				if (c == '1') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_MIN]);
				if (c == '2') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_MAX]);
				if (c == '3') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_STEP]);
				if (c == '4') scanf("%d", &RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN]);
				RunVars.HoldDelayScanCfg[HDSPARAM_STEP] &= 0xFFF8;
			}
			SaveRunVariables(RunVars);
		} else {
			LoadRunVariables(&RunVars);
		}
		if (RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN] > 9 && c != 'r') {
			sscanf(RunVars.PlotTraces[0], "%d", &b);
			if ((b >= 0) && (b < WDcfg.NumBrd)) {
				ScanHoldDelay(handle[b]);
				ConfigureFERS(handle[b], CFG_HARD);
				HoldScan_newrun = 1;
			}
			RestartAcq = 1;
		}
	}
	if (c == 'p') {
		char str[6][10] = { "OFF", "Fast", "Slow LG", "Slow HG", "Preamp LG", "Preamp HG" };
		if (!SockConsole) printf("0=OFF, 1=Fast, 2=SlowLG, 3=SlowHG, 4=PreampLG, 5=PreampHG\n");
		WDcfg.AnalogProbe = Con_getch() - '0';
		if ((WDcfg.AnalogProbe < 0) || (WDcfg.AnalogProbe > 5)) WDcfg.AnalogProbe = 0;
		Con_printf("CSm", "Citiroc Probe = %s\n", str[WDcfg.AnalogProbe]);
		for (int b = 0; b < WDcfg.NumBrd; b++)
			ConfigureProbe(handle[b]);
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
			ConfigureProbe(handle[RunVars.ActiveBrd]);
		}
	}
	if (c == '+') {
		if (bb < (MAX_NCH - 1)) {
			bb++;
			sprintf(RunVars.PlotTraces[0], "%d %d B", 0, bb);
			ConfigureProbe(handle[RunVars.ActiveBrd]);
		}
	}
	if (c == 'D') {
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
		ConfigureFERS(handle[RunVars.ActiveBrd], CFG_HARD);  // restore configuration
	}
	if (c == 'U') {	// Firmware upgrade
		int brd, as = AcqStatus;
		char fname[500] = "";
		FILE* fp;
		AcqStatus = ACQSTATUS_UPGRADING_FW;
		// if is in console mode use command input
		if (!SockConsole) printf("Insert the board num followed by firmware file name (2new_firmware.ffu): ");
		Con_GetInt(&brd);
		//if (!SockConsole) printf("Insert the firmware filename: ");
		Con_GetString(fname, 500);
		fname[strcspn(fname, "\n")] = 0; // In console mode GetString append \n at the end
		fp = fopen(fname, "rb");
		if (!fp) {
			SendAcqStatusMsg("Failed to open file %s\n", fname);
			AcqStatus = as;
		} else {
			if ((brd >= 0) && (brd < WDcfg.NumBrd)) {
				int ret = FERS_FirmwareUpgrade(handle[brd], fp, reportProgress);
				if (ret == FERSLIB_ERR_INVALID_FWFILE) Con_printf("Sw", "Invalid Firmware Upgrade File");
				fclose(fp);
				RestartAll = 1;
			} else {
				Con_printf("Sw", "Invalid Board Number %d", brd);
				fclose(fp);
				AcqStatus = as;
			}
		}
	}
	if (c == '!') {
		int brd = 0, ret;
		if (WDcfg.NumBrd > 1) {
			if (!SockConsole) printf("Enter board index: ");
			Con_GetInt(&brd);
		}
		if ((brd >= 0) && (brd < WDcfg.NumBrd) && (FERS_CONNECTIONTYPE(handle[brd]) == FERS_CONNECTIONTYPE_USB)) {
			ret = FERS_Reset_IPaddress(handle[brd]);
			if (ret == 0) {
				Con_printf("LCSm", "Default IP address (192.168.50.3) has been restored\n");
			}
			else {
				Con_printf("CSw", "Failed to reset IP address\n");
			}
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

	if (c == 'T') {
		memset(WDcfg.RunTitle, 0, 81);
		printf("\nEnter title for this run (Max. 80 characters):\n");
		scanf("%80s", WDcfg.RunTitle);
	}
	if (c == 'R') {
		printf("\nEnter the run number:\n");
		int runNumber = -1;
		if (scanf("%d", &runNumber) || runNumber > -1) {
			RunVars.RunNumber = runNumber;
			SaveRunVariables(RunVars);
		} else {
			printf("Wrong input received! No change made!\n");
			while (getchar() != '\n');
		}
    }

	if ((c == ' ') && !SockConsole) {
		printf("[q] Quit\n");
		printf("[m] Register Manual Controller\n");
		printf("[M] Citiroc Controller\n");
		printf("[h] HV Controller\n");
		printf("[s] Start\n");
		printf("[S] Stop\n");
		printf("[t] SW trigger\n");
		printf("[c] Change channel\n");
		printf("[b] Change board\n");
		printf("[x] Enable/Disable X calibration\n");
		printf("[p] Set Citiroc probe\n");
		printf("[C] Set Stats Monitor Type\n");
		printf("[P] Set Plot Mode\n");
		printf("[f] Freeze plot\n");
		printf("[o] One shot plot\n");
		//printf("[w] Set waveform probe\n");
		printf("[y] Scan Thresholds\n");
		printf("[Y] Hold Delay scan\n");
		printf("[D] Run Pedestal Calibration\n");
		printf("[U] Upgrade firmware\n");
		printf("[r] Reset histograms\n");
		printf("[j] Reset jobs (when enabled)\n");
		printf("[!] Reset IP address\n");
		printf("[#] Print Pixel Map\n");
		printf("[T] Set title for RingStateChangeItem (FRIB)\n");
		printf("[R] Set run number\n");
		c = Con_getch();
		RunTimeCmd(c);
	}


	if (reload_cfg) {
		for (b = 0; b < WDcfg.NumBrd; b++) {
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

void report_firmware_notfound(int b) {
	sprintf(ErrorMsg, "The firmware of the FPGA in board %d cannot be loaded.\nPlease, try to re-load the firmware, running on shell the command\n'./%s -u %s firmware_filename.ffu'\n", b, EXE_NAME, WDcfg.ConnPath[b]);
}




// ******************************************************************************************
// MAIN
// ******************************************************************************************
int main(int argc, char* argv[])
{
	int i = 0, ret = 0, clrscr = 0, dtq, ch, b, cnc, rdymsg; // jobrun = 0, 
	int snd_fpga_warn = 0;
	int PresetReached = 0;
	int nb = 0;
	int NoData[MAX_NBRD] = { 0 };
	double tstamp_us, curr_tstamp_us = 0;
	float fpga_temp[MAX_NBRD] = { 0 };
	int MajorFWrev = 255; 
	uint64_t kb_time, curr_time, print_time, wave_time, temp_time;
	char ConfigFileName[500] = CONFIG_FILENAME;
	char fwupg_fname[500] = "", fwupg_conn_path[20] = "";
	char PixelMapFileName[500] = PIXMAP_FILENAME; //  CTIN: get name from config file
	void* Event;
	float elapsedPC_s;
	float elapsedBRD_s;
	FILE* cfg;
	int a1, a2, AllocSize;
	int ROmode;

	// Get command line options
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i] + 1, "g") == 0) SockConsole = 1;
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
	ret = InitConsole(SockConsole, MsgLog);
	if (ret) {
		printf("ERROR: init console failed\n");
		exit(0);
	}

	// Check if a FW upgrade has been required
	if ((strlen(fwupg_fname) > 0) && (strlen(fwupg_conn_path) > 0)) {
		FILE* fp = fopen(fwupg_fname, "rb");
		if (!fp) {
			Con_printf("CSm", "Failed to open file %s\n", fwupg_fname);
			return -1;
		}
		ret = FERS_OpenDevice(fwupg_conn_path, &handle[0]);
		if (ret < 0) {
			Con_printf("CSm", "ERROR: can't open FERS at %s\n", fwupg_conn_path);
			return -1;
		}
		ret = FERS_FirmwareUpgrade(handle[0], fp, reportProgress);
		fclose(fp);
		return 0;
	}

	AcqStatus = ACQSTATUS_SOCK_CONNECTED;
	if (SockConsole) SendAcqStatusMsg("JanusC connected. Release %s (%s).", SW_RELEASE_NUM, SW_RELEASE_DATE);

	Con_printf("LCSm", "*************************************************\n");
	Con_printf("LCSm", "JanusC Rev %s (%s)\n", SW_RELEASE_NUM, SW_RELEASE_DATE);
	Con_printf("LCSm", "Readout Software for CAEN FERS-5200\n");
	Con_printf("LCSm", "*************************************************\n");

ReadCfg:
	// -----------------------------------------------------
	// Parse config file
	// -----------------------------------------------------
	cfg = fopen(ConfigFileName, "r");
	if (cfg == NULL) {
		sprintf(ErrorMsg, "Can't open configuration file %s\n", ConfigFileName);
		goto ManageError;
	}
	Con_printf("LCSm", "Reading configuration file %s\n", ConfigFileName);
	ret = ParseConfigFile(cfg, &WDcfg, 1);
	fclose(cfg);
	LoadRunVariables(&RunVars);
	if (WDcfg.EnableJobs) {	
		jobrun = WDcfg.JobFirstRun;
		RunVars.RunNumber = jobrun;
		SaveRunVariables(RunVars);
	}
	FERS_SetDebugLogs(WDcfg.DebugLogMask);

	// -----------------------------------------------------
	// Read pixel map (channel (0:63) to pixel[x][y] 
	// -----------------------------------------------------
	Con_printf("LCSm", "Reading Pixel Map %s\n", PixelMapFileName);
	if (Read_ch2xy_Map(PixelMapFileName) < 0)
		Con_printf("LCSm", "WARNING: Map File not found. Sequential mapping will be used\n");

	// -----------------------------------------------------
	// Connect To Boards
	// -----------------------------------------------------
	memset(handle, -1, sizeof(*handle) * MAX_NBRD);
	memset(cnc_handle, -1, sizeof(*handle) * MAX_NCNC);
	cnc = 0;
	for (b = 0; b < WDcfg.NumBrd; b++) {
		FERS_BoardInfo_t BoardInfo;
		char* cc, cpath[100];
		if (((cc = strstr(WDcfg.ConnPath[b], "tdl")) != NULL)) {  // TDlink used => Open connection to concentrator (this is not mandatory, it is done for reading information about the concentrator)
			UsingCnc = 1;
			FERS_Get_CncPath(WDcfg.ConnPath[b], cpath);
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
					ret = FERS_InitTDLchains(cnc_handle[cnc]);
					if (ret != 0) {
						sprintf(ErrorMsg, "Failure in TDL chain init\n");
						goto ManageError;
					}
				}
				ret |= FERS_ReadConcentratorInfo(cnc_handle[cnc], &CncInfo);
				if (ret == 0) {
					Con_printf("LCSm", "FPGA FW revision = %s\n", CncInfo.FPGA_FWrev);
					Con_printf("LCSm", "SW revision = %s\n", CncInfo.SW_rev);
					Con_printf("LCSm", "PID = %d\n", CncInfo.pid);
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
		if ((WDcfg.NumBrd > 1) || (cnc > 0)) Con_printf("LCSm", "\n------------------ Board %2d --------------------\n", b);
		Con_printf("LCSm", "Opening connection to %s\n", WDcfg.ConnPath[b]);
		ret = FERS_OpenDevice(WDcfg.ConnPath[b], &handle[b]);
		if (ret == 0) {
			Con_printf("LCSm", "Connected to %s\n", WDcfg.ConnPath[b]);
			int bootL;
			uint32_t ver, rel;
			FERS_CheckBootloaderVersion(handle[b], &bootL, &ver, &rel);
			if (bootL) {
				report_firmware_notfound(b);
				goto ManageError;
			}
		} else {
			sprintf(ErrorMsg, "Can't open board %d at %s\n", b, WDcfg.ConnPath[b]);
			goto ManageError;
		}
		ret = FERS_ReadBoardInfo(handle[b], &BoardInfo);
		if (ret == 0) {
			char fver[100];
			if (BoardInfo.FPGA_FWrev == 0) sprintf(fver, "BootLoader");
			else sprintf(fver, "%d.%d (Build = %04X)", (BoardInfo.FPGA_FWrev >> 8) & 0xFF, BoardInfo.FPGA_FWrev & 0xFF, (BoardInfo.FPGA_FWrev >> 16) & 0xFFFF);
			MajorFWrev = min((int)(BoardInfo.FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
			Con_printf("LCSm", "FPGA FW revision = %s\n", fver);
			if (strstr(WDcfg.ConnPath[b], "tdl") == NULL)
				Con_printf("LCSm", "uC FW revision = %08X\n", BoardInfo.uC_FWrev);
			Con_printf("LCSm", "PID = %d\n", BoardInfo.pid);
			if (SockConsole) {
				if (strstr(WDcfg.ConnPath[b], "tdl") == NULL) Con_printf("Si", "%d;%d;%s;%s;%08X", b, BoardInfo.pid, BoardInfo.ModelName, fver, BoardInfo.uC_FWrev); // ModelName for firmware upgrade
				else Con_printf("Si", "%d;%d;%s;%s;N.A.", b, BoardInfo.pid, BoardInfo.ModelName, fver);
			}
			if ((BoardInfo.FPGA_FWrev > 0) && ((BoardInfo.FPGA_FWrev & 0xFF00) < 1) && (BoardInfo.FERSCode == 5202)) {
				sprintf(ErrorMsg, "Your FW revision is %d.%d; must be 1.X or higher\n", (BoardInfo.FPGA_FWrev >> 8) & 0xFF, BoardInfo.FPGA_FWrev & 0xFF);
				goto ManageError;
			}
		} else {
			sprintf(ErrorMsg, "Can't read board info\n");
			report_firmware_notfound(b);
			goto ManageError;
		}
	}
	if ((WDcfg.NumBrd > 1) || (cnc > 0))  Con_printf("LCSm", "\n");
	if (AcqStatus != ACQSTATUS_RESTARTING) {
		AcqStatus = ACQSTATUS_HW_CONNECTED;
		SendAcqStatusMsg("Num of connected boards = %d", WDcfg.NumBrd);
	}

	// -----------------------------------------------------
	// Allocate memory buffers and histograms, open files
	// -----------------------------------------------------
	ROmode = (WDcfg.EventBuildingMode != 0) ? 1 : 0;
	for (b = 0; b < WDcfg.NumBrd; b++) {
		FERS_InitReadout(handle[b], ROmode, &a1);
		memset(&sEvt[b], 0, sizeof(ServEvent_t));
	}
	CreateStatistics(WDcfg.NumBrd, FERSLIB_MAX_NCH, &a2);
	AllocSize = a2 + FERS_TotalAllocatedMemory();
	Con_printf("LCSm", "Total allocated memory = %.2f MB\n", (float)AllocSize / (1024 * 1024));

	// -----------------------------------------------------
	// Open plotter
	// -----------------------------------------------------
	OpenPlotter();

// +++++++++++++++++++++++++++++++++++++++++++++++++++++
Restart:  // when config file changes or a new run of the job is scheduled, the acquisition restarts here // BUG: it does not restart when job is enabled, just when preset time or count is active
// +++++++++++++++++++++++++++++++++++++++++++++++++++++
	if (!PresetReached)
		ResetStatistics();
	else
		PresetReached = 0;

	LoadRunVariables(&RunVars);
	job_read_parse();

	// -----------------------------------------------------
	// Configure Boards
	// -----------------------------------------------------
	if (!SkipConfig) {
		for (b = 0; b < WDcfg.NumBrd; b++) {
			Con_printf("LCSm", "Configuring Board %d... ", b);
			ret = ConfigureFERS(handle[b], CFG_HARD);
			if (ret < 0) {
				Con_printf("LCSm", "Failed!!!\n");
				Con_printf("LCSm", "%s", ErrorMsg);
				goto ManageError;
			}
			else Con_printf("LCSm", "Done.\n");
		}
	}
	SkipConfig = 0;

	// Send some info to GUI
	if (SockConsole) {
		//for (int b = 0; b < WDcfg.NumBrd; b++) 
		Send_HV_Info();
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
	if (((AcqStatus == ACQSTATUS_RESTARTING) || ((WDcfg.EnableJobs && (jobrun > WDcfg.JobFirstRun) && (jobrun <= WDcfg.JobLastRun)))) && !stop_sw) {
		if (WDcfg.EnableJobs) Sleep((int)(WDcfg.RunSleep * 1000));
		StartRun();
		if (!SockConsole) ClearScreen();
	}
	else if (AcqStatus != ACQSTATUS_RUNNING) AcqStatus = ACQSTATUS_READY;

	curr_time = get_time();
	print_time = curr_time - 2000;  // force 1st print with no delay
	wave_time = curr_time;
	kb_time = curr_time;
	temp_time = curr_time;
	rdymsg = 1;
	PresetReached = 0;
	build_time_us = 0;

	while (!Quit && !RestartAcq && !PresetReached && !RestartAll) {

		curr_time = get_time();
		Stats.current_time = curr_time;

		nb = 0;
		if (((curr_time - temp_time) > 1000) || (fpga_temp[0] == 0)) {
			for (b = 0; b < WDcfg.NumBrd; b++) {
				char wmsg[200];
				sprintf(wmsg, "WARNING: In board %d: FPGA is OVERHEATING (Temp > 85 degC). Please provide suitable ventilation to prevent from permanent damages", b);
				if (sEvt[b].update_time > (curr_time - 2000)) {
					fpga_temp[b] = sEvt[b].tempFPGA;
					temp_time = curr_time;
				} else if (((curr_time - temp_time) > 10000) || (fpga_temp[0] == 0)) { // read every 10 seconds from register if not available from service events
					FERS_Get_FPGA_Temp(handle[b], &fpga_temp[b]);
					temp_time = curr_time;
				}
				if (fpga_temp[b] > 83 && fpga_temp[b] < 200) {	// DNIN: more sofisticated actions can be taken
					if (snd_fpga_warn == 0) Con_printf("LCSw", "%s\n", wmsg);
					else Con_printf("LCSm", "%s\n", wmsg);
					snd_fpga_warn ^= 1;
				}
			}
		}

		// ---------------------------------------------------
		// Check for commands from console or changes in cfg files
		// ---------------------------------------------------
		if ((curr_time - kb_time) > 200) {
			int upd = CheckFileUpdate();
			if (upd == 1) {
				if (!SockConsole) clrscr = 1;
				rdymsg = 1;
				if (WDcfg.EnLiveParamChange == 0 && AcqStatus == ACQSTATUS_RUNNING) {	
					for (b = 0; b < WDcfg.NumBrd; b++) {
						StopRun();
						ConfigureFERS(handle[b], CFG_SOFT);	
						FERS_FlushData(handle[b]);
						StartRun();
					}
				} else {
					for (b = 0; b < WDcfg.NumBrd; b++) {
						ConfigureFERS(handle[b], CFG_SOFT);
					}
				}
			} else if (upd == 2) {
				int size;
				DestroyStatistics();
				CreateStatistics(WDcfg.NumBrd, FERSLIB_MAX_NCH, &size);
				RestartAcq = 1;
			} else if (upd == 3) {
				RestartAll = 1;
			}
			if (Con_kbhit()) {
				int tret = RunTimeCmd(Con_getch());
				clrscr = 1;
				rdymsg = 1;
				if (tret == 100) {
					kb_time = curr_time;
					goto Restart;
				}
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
				sprintf(ErrorMsg, "Readout failure (ret = %d)!\n", ret);
				goto ManageError;
			}
			elapsedPC_s = (Stats.current_time > Stats.start_time) ? ((float)(Stats.current_time - Stats.start_time)) / 1000 : 0;
			elapsedBRD_s = (float)(Stats.current_tstamp_us[0] * 1e-6);
			if ((WDcfg.StopRunMode == STOPRUN_PRESET_TIME) && ((elapsedBRD_s > WDcfg.PresetTime) || (elapsedPC_s > (WDcfg.PresetTime + 1)))) {
				Stats.stop_time = Stats.start_time + (uint64_t)(WDcfg.PresetTime * 1000);
				StopRun();
				PresetReached = 1; // Preset time reached; quit readout loop
			} else if ((WDcfg.StopRunMode == STOPRUN_PRESET_COUNTS) && (Stats.GlobalTrgCnt[0].cnt >= (uint32_t)WDcfg.PresetCounts)) {  // Stop on board 0 counts
				StopRun();
				PresetReached = 1; // Preset counts reached; quit readout loop
			}
		}
		if ((nb > 0) && !PresetReached && !DisableDataAnalysys) {
			NoData[b] = 0;
			Stats.current_tstamp_us[b] = curr_tstamp_us;
			Stats.ByteCnt[b].cnt += (uint32_t)nb;
			Stats.GlobalTrgCnt[b].cnt++;
			if ((curr_tstamp_us > (build_time_us + 0.001 * WDcfg.TstampCoincWindow)) && (WDcfg.EventBuildingMode != EVBLD_DISABLED)) {
				if (build_time_us > 0) Stats.BuiltEventCnt.cnt++;
				build_time_us = curr_tstamp_us;
			}

			// ---------------------------------------------------
			// update statistics and spectra and save list files 
			// ---------------------------------------------------
			if (((dtq & 0xF) == DTQ_SPECT) || ((dtq & 0xF) == DTQ_TSPECT)) {
				SpectEvent_t* Ev = (SpectEvent_t*)Event;
				Stats.current_trgid[b] = Ev->trigger_id;
				for (ch = 0; ch < 64; ch++) {
					uint16_t ediv = 1;
					if (WDcfg.EHistoNbin > 0) ediv = WDcfg.Range_14bit ? ((1 << 14) / WDcfg.EHistoNbin) : ((1 << 13) / WDcfg.EHistoNbin);
					if (ediv < 1) ediv = 1;
					uint16_t EbinLG = Ev->energyLG[ch] / ediv;
					uint16_t EbinHG = Ev->energyHG[ch] / ediv;
					uint32_t Hmin = uint32_t(WDcfg.ToAHistoMin / TOA_LSB_ns);
					uint32_t ToAbin = Ev->tstamp[ch];
					ToAbin = (uint32_t)((ToAbin - Hmin) / WDcfg.ToARebin); // Shift and Rebin ToA histogram
					uint32_t ToTbin = Ev->ToT[ch];
					if (WDcfg.EHistoNbin > 0) {
						if (EbinLG > 0) Histo1D_AddCount(&Stats.H1_PHA_LG[b][ch], EbinLG);
						if (EbinHG > 0) Histo1D_AddCount(&Stats.H1_PHA_HG[b][ch], EbinHG);
					}
					if (WDcfg.ToAHistoNbin > 0) {
						if (ToAbin > 0) Histo1D_AddCount(&Stats.H1_ToA[b][ch], ToAbin);
						if (ToTbin > 0) Histo1D_AddCount(&Stats.H1_ToT[b][ch], ToTbin);
					}
					if ((EbinLG > 0) || (EbinHG > 0)) Stats.PHACnt[b][ch].cnt++;
					if (ToAbin > 0) Stats.HitCnt[b][ch].cnt++;
				}
				//int isTSpect = (dtq & 0x2) >> 1;
				if ((WDcfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (WDcfg.OutFileEnableMask & OUTFILE_LIST_BIN) || (WDcfg.OutFileEnableMask & OUTFILE_SYNC) || (WDcfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					//SaveList_Spect(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, isTSpect);
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}

			} else if ((dtq & 0x0F) == DTQ_TIMING) {
				uint32_t Hmin = uint32_t(WDcfg.ToAHistoMin/0.5);
				ListEvent_t* Ev = (ListEvent_t*)Event;
				for (i = 0; i < Ev->nhits; i++) {
					if (WDcfg.ToAHistoNbin == 0) break;
					ch = Ev->channel[i];
					uint32_t ToAbin = Ev->tstamp[i];
					ToAbin = (uint32_t)((ToAbin - Hmin) / WDcfg.ToARebin); // Shift and Rebin ToA histogram
					uint16_t ToTbin = Ev->ToT[i];
					Stats.HitCnt[b][ch].cnt++;
					if (ToAbin > 0) Histo1D_AddCount(&Stats.H1_ToA[b][ch], ToAbin);
					if (ToTbin > 0) Histo1D_AddCount(&Stats.H1_ToT[b][ch], ToTbin);
				}
				if ((WDcfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (WDcfg.OutFileEnableMask & OUTFILE_LIST_BIN) || (WDcfg.OutFileEnableMask & OUTFILE_SYNC) || (WDcfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}

			} else if (dtq == DTQ_COUNT) {
				CountingEvent_t* Ev = (CountingEvent_t*)Event;
				Stats.current_trgid[b] = Ev->trigger_id;
				for (i = 0; i < MAX_NCH; i++) {
					Stats.ChTrgCnt[b][i].cnt += Ev->counts[i];
					if (WDcfg.MCSHistoNbin > 0)
						Histo1D_SetCount(&Stats.H1_MCS[b][i], Ev->counts[i]);
				}
				Stats.T_OR_Cnt[b].cnt += Ev->t_or_counts;
				Stats.Q_OR_Cnt[b].cnt += Ev->q_or_counts;
				Stats.trgcnt_update_us[b] = curr_tstamp_us;
				if ((WDcfg.OutFileEnableMask & OUTFILE_LIST_ASCII) || (WDcfg.OutFileEnableMask & OUTFILE_LIST_BIN) || (WDcfg.OutFileEnableMask & OUTFILE_SYNC) || (WDcfg.OutFileEnableMask & OUTFILE_RAW_DATA_RINGBUFFER)) {
					SaveList(b, Stats.current_tstamp_us[b], Stats.current_trgid[b], Ev, dtq);
				}
			} else if (dtq == DTQ_WAVE) {
				// Plot waveform
				if (((curr_time - wave_time) > 300) && (RunVars.PlotType == PLOT_WAVE) && (!Freeze || OneShot)) {
					WaveEvent_t* Ev;
					int brd, ch;
					char title[20];
					sscanf(RunVars.PlotTraces[0], "%d %d", &brd, &ch);
					if ((brd >= 0) && (brd < WDcfg.NumBrd) && (ch >= 0) && (ch < WDcfg.NumCh) && (brd == b)) {
						Ev = (WaveEvent_t*)Event;
						if (ch < 32) sprintf(title, "Brd %d, Ch [0-31]", brd);
						else sprintf(title, "Brd %d, Ch [32-63]", brd);
						PlotWave(Ev, title);
						OneShot = 0;
						wave_time = curr_time;
					}
				}
			} else if (dtq == DTQ_SERVICE) {
				ServEvent_t* Ev = (ServEvent_t*)Event;
				memcpy(&sEvt[b], Ev, sizeof(ServEvent_t));
				if (WDcfg.AcquisitionMode != ACQMODE_COUNT) {	// DNIN: in counting mode the counters are already incremented
					for (ch = 0; ch < MAX_NCH; ch++) 
						Stats.ChTrgCnt[b][ch].cnt += sEvt[b].ch_trg_cnt[ch];
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
			int ab = RunVars.ActiveBrd;

			// Check if the connection is lost
			for (b = 0; b < WDcfg.NumBrd; b++) {
				if (NoData[b]) {
					uint32_t status;
					ret = FERS_ReadRegister(handle[b], a_acq_status, &status);
					if ((ret < 0) || ((status & 0x3) != 1)) {
						if (ret == 0) sprintf(ErrorMsg, "Lost Connection to Board n. %d (stat=%04X)", b, status);
						else sprintf(ErrorMsg, "Lost Connection to Board n. %d (can't read stat)", b);
						goto ManageError;
					}
				}
				NoData[b] = 1;
			}

			if (WDcfg.StopRunMode == STOPRUN_PRESET_TIME) {
				if (WDcfg.EnableJobs) sprintf(rinfo, "(%d of %d, Preset = %.2f s)", jobrun - WDcfg.JobFirstRun + 1, WDcfg.JobLastRun - WDcfg.JobFirstRun + 1, WDcfg.PresetTime);
				else sprintf(rinfo, "(Preset = %.2f s)", WDcfg.PresetTime);
			} else if (WDcfg.StopRunMode == STOPRUN_PRESET_COUNTS) {
				if (WDcfg.EnableJobs) sprintf(rinfo, "(%d of %d, Preset = %d cnts)", jobrun - WDcfg.JobFirstRun + 1, WDcfg.JobLastRun - WDcfg.JobFirstRun + 1, WDcfg.PresetCounts);
				else sprintf(rinfo, "(Preset = %d cnts)", WDcfg.PresetCounts);
			} else {
				if (WDcfg.EnableJobs) sprintf(rinfo, "(%d of %d)", jobrun - WDcfg.JobFirstRun + 1, WDcfg.JobLastRun - WDcfg.JobFirstRun + 1);
				else sprintf(rinfo, "");
			}

			//for (b = 0; b < WDcfg.NumBrd; ++b) {
			//	if (FERS_FPGA_FW_MajorRev(handle[b]) >= 4 || En_HVstatus_Update)
			//		Send_HV_Info(handle[b]);
			//}
			if (FERS_FPGA_FW_MajorRev(handle[0]) >= 4 || En_HVstatus_Update && ((curr_time - print_time) > 2500)) // DNIN: all the boards should have the same firmware
				Send_HV_Info();

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
				for (i = 0; i < WDcfg.NumBrd; ++i) {
					totrate += Stats.ByteCnt[i].rate;
					double2str(totrate, 1, totror);
				}

				if (PresetReached) {
					if (WDcfg.StopRunMode == STOPRUN_PRESET_TIME) rtime = WDcfg.PresetTime;
					tp = 100;
				} else {
					rtime = (float)(curr_time - Stats.start_time) / 1000;
					if (WDcfg.StopRunMode == STOPRUN_PRESET_TIME) tp = WDcfg.PresetTime > 0 ? 100 * rtime / WDcfg.PresetTime : 0;
					else tp = WDcfg.PresetCounts > 0 ? 100 * (float)Stats.GlobalTrgCnt[0].cnt / WDcfg.PresetCounts : 0;
				}

				if (WDcfg.StopRunMode == STOPRUN_PRESET_TIME) SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f = %.2f%% %s", RunVars.RunNumber, rtime, tp, rinfo);
				else if (WDcfg.StopRunMode == STOPRUN_PRESET_COUNTS) SendAcqStatusMsg("Run #%d: Elapsed Time = %.2f (%.2f%%) %s", RunVars.RunNumber, rtime, tp, rinfo);
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
							if ((MajorFWrev >= 3) || (WDcfg.AcquisitionMode == ACQMODE_COUNT)) double2str(Stats.ChTrgCnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_CHTRG_CNT) {
							if ((MajorFWrev >= 3) || (WDcfg.AcquisitionMode == ACQMODE_COUNT)) cnt2str(Stats.ChTrgCnt[ab][i].cnt, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_HIT_RATE) {
							if (WDcfg.AcquisitionMode & 0x2) double2str(Stats.HitCnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_HIT_CNT) {
							if (WDcfg.AcquisitionMode & 0x2) cnt2str(Stats.HitCnt[ab][i].cnt, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_PHA_RATE) {
							if (WDcfg.AcquisitionMode & 0x1) double2str(Stats.PHACnt[ab][i].rate, 0, ss[i]);
							else sprintf(ss[i], "N/A     ");
						} else if (RunVars.SMonType == SMON_PHA_CNT) {
							if (WDcfg.AcquisitionMode & 0x1) cnt2str(Stats.PHACnt[ab][i].cnt, ss[i]); 
							else sprintf(ss[i], "N/A     ");
						}
					}

					if (SockConsole) {
						Con_printf("CSSb", "%d", ab);						
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
						Con_printf("CSSg", "%s", sg2gui);
						for (i = 0; i < MAX_NCH; i++)
							sprintf(ss2gui, "%s%s", ss2gui, ss[i]);
						Con_printf("SS", "c%s", ss2gui);
					} else {
						if (WDcfg.NumBrd > 1) Con_printf("C", "Board n. %d (press [b] to change active board)\n", ab);
						Con_printf("C", "Time Stamp:   %10.3lf s                \n", Stats.current_tstamp_us[ab] / 1e6);
						Con_printf("C", "Trigger-ID:   %10lld                   \n", Stats.current_trgid[ab]);
						Con_printf("C", "Trg Rate:        %scps                 \n", trr);
						Con_printf("C", "Trg Reject:   %10.2lf %%               \n", Stats.LostTrgPerc[ab]);
						Con_printf("C", "Tot Lost Trg: %10" PRIu32 "            \n", Stats.LostTrg[ab].cnt);
						Con_printf("C", "EvBuild:      %10.2lf %%               \n", Stats.BuildPerc[ab]);
						Con_printf("C", "T-OR Rate:       %scps                 \n", torr);
						if (WDcfg.NumBrd > 1)
							Con_printf("C", "Readout Rate:    %sB/s (Tot: %sB/s)             \n", ror, totror);
						else
							Con_printf("C", "Readout Rate:    %sB/s                   \n", ror);
						Con_printf("C", "FPGA temp:    %10.1f degC              \n", fpga_temp[ab]);
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
						char tmp_brdstat[1024] = "";
						Con_printf("CSSt", "%s", stitle[RunVars.SMonType]);
						for (i = 0; i < WDcfg.NumBrd; ++i)
							sprintf(tmp_brdstat, "%s %3d %12.2lf %12" PRIu64 " %12.5lf %12.5lf %12.2lf %12.5lf", tmp_brdstat, i, Stats.current_tstamp_us[i] / 1e6, Stats.current_trgid[i], Stats.GlobalTrgCnt[i].rate / 1000, Stats.LostTrgPerc[i], Stats.BuildPerc[i], Stats.ByteCnt[i].rate / (1024 * 1024));
						Con_printf("CSSB", "%s", tmp_brdstat);
					} else {
						Con_printf("C", "\n");
						if (StatIntegral) Con_printf("C", "Statistics averaging: Integral (press [I] for Updating mode)\n");
						else Con_printf("C", "Statistics averaging: Updating (press [I] for Integral mode)\n");
						Con_printf("C", "Press [tab] to view statistics of single boards\n\n");
						Con_printf("C", "%3s %10s %10s %10s %10s %10s %10s\n", "Brd", "TStamp", "Trg-ID", "TrgRate", "LostTrg", "Build", "DtRate");
						Con_printf("C", "%3s %10s %10s %10s %10s %10s %10s\n\n", "", "[s]", "[cnt]", "[KHz]", "[%]", "[%]", "[MB/s]");
						for (i = 0; i < WDcfg.NumBrd; i++)
							Con_printf("C", "%3d %10.2lf %10" PRIu64 " %10.5lf %10.5lf %10.2lf %10.5lf\n", i, Stats.current_tstamp_us[i] / 1e6, Stats.current_trgid[i], Stats.GlobalTrgCnt[i].rate / 1000, Stats.LostTrgPerc[i], Stats.BuildPerc[i], Stats.ByteCnt[i].rate / (1024 * 1024));
					}
				}
			}

			// ---------------------------------------------------
			// plot histograms
			// ---------------------------------------------------
			if (!DisableDataAnalysys) {
				if ((RunVars.PlotType == PLOT_E_SPEC_LG) ||
					(RunVars.PlotType == PLOT_E_SPEC_HG) ||
					(RunVars.PlotType == PLOT_TOA_SPEC)  ||
					(RunVars.PlotType == PLOT_TOT_SPEC)  ||
					(RunVars.PlotType == PLOT_MCS_TIME))				PlotSpectrum();
				else if ((RunVars.PlotType == PLOT_2D_CNT_RATE) ||
					(RunVars.PlotType == PLOT_2D_CHARGE_LG)     ||
					(RunVars.PlotType == PLOT_2D_CHARGE_HG))			Plot2Dmap(StatIntegral);
				else if (RunVars.PlotType == PLOT_SCAN_THR) 			PlotStaircase();
				else if (RunVars.PlotType == PLOT_SCAN_HOLD_DELAY)		PlotScanHoldDelay(&HoldScan_newrun);
				else if (RunVars.PlotType == PLOT_CHTRG_RATE) 			PlotCntHisto();
				OneShot = 0;
			}
			print_time = curr_time;
		}
	}

ExitPoint:
	if (RestartAll || Quit) {
		StopRun();
		if (RestartAll) {
			for (b = 0; b < WDcfg.NumBrd; b++) {	// DNIN: configure the board after each run stop, to avoid block of the readout in the next run
				int ret = ConfigureFERS(handle[b], CFG_HARD);
				if (ret < 0) {
					Con_printf("LCSm", "Configuration Boards Failed!!!\n");
					Con_printf("LCSm", "%s", ErrorMsg);
					return -1;
				}
			}
		}
		if (Quit) CheckHVBeforeClosing(); // DNIN: This have to be done before closing the comm with Boards
		for (b = 0; b < WDcfg.NumBrd; b++) {
			if (handle[b] < 0) break;
			FERS_CloseDevice(handle[b]);
			FERS_CloseReadout(handle[b]);
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
		}
		else {
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

	if (WDcfg.EnableJobs) {
		PresetReached = 0;
		Con_printf("LCSm", "Run %d closed\n", jobrun);
		if (jobrun < WDcfg.JobLastRun) {
			jobrun++;
			SendAcqStatusMsg("Getting ready for Run #%d of the Job...", jobrun);
		}
		else jobrun = WDcfg.JobFirstRun;
		RunVars.RunNumber = jobrun;
		SaveRunVariables(RunVars);
		if (!SockConsole) ClearScreen();
	}
	goto Restart;

ManageError:
	AcqStatus = ACQSTATUS_ERROR;
	Con_printf("LSm", "ERROR: %s\n", ErrorMsg);
	SendAcqStatusMsg("ERROR: %s\n", ErrorMsg);
	if (SockConsole) {
		while (1) {
			if (Con_kbhit()) {
				int c = Con_getch();
				if (c == 'q') break;
			}
			int upd = CheckFileUpdate();
			if (upd > 0) {
				RestartAll = 1;
				goto ExitPoint;
			}
			Sleep(100);
		}
	}
	else {
		Quit = 1;
		Con_getch();
		goto ExitPoint;
	}
}
