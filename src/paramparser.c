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


#include <stdio.h>
#include <ctype.h>
#include "paramparser.h"
#include "JanusC.h"
#include "console.h"
//#include "FERSlib.h"


#define SETBIT(r, m, b)  (((r) & ~(m)) | ((m) * (b)))

int ch=-1, brd=-1;
int ValidParameterName = 0;
int ValidParameterValue = 0;
int ValidUnits = 0;

// ---------------------------------------------------------------------------------
// Description: compare two strings
// Inputs:		str1, str2: strings to compare
// Outputs:		-
// Return:		1=equal, 0=not equal
// ---------------------------------------------------------------------------------
int streq(char *str1, char *str2)
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
// Description: check if the directory exists, if not it is created
// Inputs:		J_cfg, Cfg file
// Outputs:		-
// Return:		void
// ---------------------------------------------------------------------------------
int f_mkdir(const char* path) {	// taken from CAENMultiplatform.c (https://gitlab.caen.it/software/utility/-/blob/develop/src/CAENMultiplatform.c#L216)
	int32_t ret = 0;
#ifdef _WIN32
	DWORD r = (CreateDirectoryA(path, NULL) != 0) ? 0 : GetLastError();
	switch (r) {
	case 0:
	case ERROR_ALREADY_EXISTS:
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}
#else
	int r = mkdir(path, ACCESSPERMS) == 0 ? 0 : errno;
	switch (r) {
	case 0:
	case EEXIST:
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}
#endif
	return ret;
}

void GetDatapath(char* value, Janus_Config_t* J_cfg) {

	if (access(value, 0) == 0) { // taken from https://stackoverflow.com/questions/6218325/
		struct stat status;
		stat(value, &status);
		int myb = status.st_mode & S_IFDIR;
		if (myb == 0) {
			Con_printf("LCSw", "WARNING: DataFilePath: %s is not a valid directory. Default .DataFiles folder is used\n", value);
			strcpy(J_cfg->DataFilePath, "DataFiles");
		}
		else
			strcpy(J_cfg->DataFilePath, value);
	}
	else {
		int ret = f_mkdir(value);
		if (ret == 0)
			strcpy(J_cfg->DataFilePath, value);
		else {
			Con_printf("LCSw", "WARNING: DataFilePath: %s cannot be created, default .DataFiles folder is used\n", value);
			strcpy(J_cfg->DataFilePath, "DataFiles");
		}
	}
}

// ---------------------------------------------------------------------------------
// Description: Read an integer (decimal) from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file / Set ValidParameterValue=0 if the string is not an integer
// ---------------------------------------------------------------------------------
static uint32_t GetInt(char* val) {
	uint32_t ret = 0;
	int num = sscanf(val, "%d", &ret);
	if (ret < 0 || num < 1) ValidParameterValue = 0;
	else ValidParameterValue = 1;
	return ret;
}


// ---------------------------------------------------------------------------------
// Description: Read number of bins for amn histogram
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file
// ---------------------------------------------------------------------------------
static int GetNbin(char* str) {
	int i;
	for (i = 0; i < (int)strlen(str); ++i)
		str[i] = (char)toupper(str[i]);
	if ((streq(str, "DISABLED")) || (streq(str, "0")))	return 0;
	else if (streq(str, "256"))							return 256;
	else if (streq(str, "512"))							return 512;
	else if ((streq(str, "1024") || streq(str, "1K")))	return 1024;
	else if ((streq(str, "2048") || streq(str, "2K")))	return 2048;
	else if ((streq(str, "4096") || streq(str, "4K")))	return 4096;
	else if ((streq(str, "8192") || streq(str, "8K")))	return 8192;
	else if ((streq(str, "16384") || streq(str, "16K")))return 16384;
	else {
		ValidParameterValue = 0;
		return 1024;  // assign a default value on error
	}
}



// ---------------------------------------------------------------------------------
// Description: Read an integer (hexadecimal) from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file / Set ValidParameterValue = 0 if the value is not in HEX format
// ---------------------------------------------------------------------------------
static uint32_t GetHex32(char* str) {
	uint32_t ret;
	ValidParameterValue = 1;
	if ((str[1] == 'x') || (str[1] == 'X')) {
		sscanf(str + 2, "%x", &ret);
		if (str[0] != '0') ValidParameterValue = 0;	// Rise a warning for wrong HEX format 0x
		for (uint8_t i = 2; i < strlen(str); ++i) {
			if (!isxdigit(str[i])) {
				ValidParameterValue = 0;
				break;
			}
		}
	}
	else {
		sscanf(str, "%x", &ret);
		for (uint8_t i = 0; i < strlen(str); ++i) {	// Rise a warning for wrong HEX format
			if (!isxdigit(str[i])) {
				ValidParameterValue = 0;
				break;
			}
		}
	}
	return ret;
}

// ---------------------------------------------------------------------------------
// Description: Read a 64 bit mask (hexadecimal) from the conig file (not used)
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		mask value read from the file / Set ValidParameterValue = 0 if the value is not in HEX format
// ---------------------------------------------------------------------------------
//static uint64_t GetHex64(char* str) {
//	uint64_t ret;
//	ValidParameterValue = 1;
//	if ((str[1] == 'x') || (str[1] == 'X')) {
//		sscanf(str + 2, "%" SCNx64, &ret);
//		if (str[0] != '0') ValidParameterValue = 0;	// Rise a warning for wrong HEX format 0x
//		for (uint8_t i = 2; i < strlen(str); ++i) {
//			if (!isxdigit(str[i])) {
//				ValidParameterValue = 0;
//				break;
//			}
//		}
//	}
//	else {
//		sscanf(str, "%" SCNx64, &ret);
//		for (uint8_t i = 0; i < strlen(str); ++i) {	// Rise a warning for wrong HEX format
//			if (!isxdigit(str[i])) {
//				ValidParameterValue = 0;
//				break;
//			}
//		}
//	}
//	return ret;
//}

// ---------------------------------------------------------------------------------
// Description: Read a float from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		float value read from the file / 
// ---------------------------------------------------------------------------------
float GetFloat(char* str)
{
	float ret;
	int i;
	ValidParameterValue = 1;
	// replaces ',' with '.' (decimal separator)
	for (i = 0; i < (int)strlen(str); i++)
		if (str[i] == ',') str[i] = '.';
	sscanf(str, "%f", &ret);
	return ret;
}


// ---------------------------------------------------------------------------------
// Description: Read a value from the conig file followed by an optional time unit (ps, ns, us, ms, s)
//              and convert it in a time expressed in ns as a float 
// Inputs:		f_ini: config file
//				tu: time unit of the returned time value
// Outputs:		-
// Return:		time value (in ns) read from the file / Set ValidParamName/Value=0 if the expected format is not matched
// ---------------------------------------------------------------------------------
float GetTime(char* val, char *tu)
{
	double timev=-1;
	double ns;
	char str[100];

	int element = sscanf(val, "%lf %s", &timev, str);

	ValidUnits = 1;
	if (streq(str, "ps"))		ns = timev * 1e-3;
	else if (streq(str, "ns"))	ns = timev;
	else if (streq(str, "us"))	ns = timev * 1e3;
	else if (streq(str, "ms"))	ns = timev * 1e6;
	else if (streq(str, "s"))	ns = timev * 1e9;
	else if (streq(str, "m"))	ns = timev * 60e9;
	else if (streq(str, "h"))	ns = timev * 3600e9;
	else if (streq(str, "d"))	ns = timev * 24 * (3600e9);
	else if (element == 1 || streq(str, "#")) return (float)timev;  // No units
	else {
		ValidUnits = 0;
		return (float)timev;  // no time unit specified in the config file; assuming equal to the requested one (ns of default)
	}

	if (streq(tu, "ps"))		return (float)(ns*1e3);
	else if (streq(tu, "ns"))	return (float)(ns);
	else if (streq(tu, "us"))	return (float)(ns/1e3);
	else if (streq(tu, "ms"))	return (float)(ns/1e6);
	else if (streq(tu, "s") )	return (float)(ns/1e9);
	else if (streq(tu, "m") )	return (float)(ns/60e9);
	else if (streq(tu, "h") )	return (float)(ns/3600e9);
	else if (streq(tu, "d") )	return (float)(ns/(24*3600e9));
	else return (float)timev;
}


// ---------------------------------------------------------------------------------
// Description: Read a value from the conig file followed by an optional byte unit (B, MB, GB)
//              and convert it in Bytes
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		memory size value expressed in Bytes. Minimum value allowed 1 kB
// ---------------------------------------------------------------------------------
static float GetBytes(char* val)
{
	char van[50];
	float var;
	char str[100];
	float minSize = 1e3; // 1 kB

	int element0 = sscanf(val, "%s %s", van, str);
	int element1 = sscanf(van, "%f", &var);
	if (element1 > 0) ValidParameterValue = 1;
	else ValidParameterValue = 0;

	ValidUnits = 1;
	if (streq(str, "TB"))		return (var * 1e12 > minSize) ? var * (float)1e12 : minSize;
	else if (streq(str, "GB"))	return (var * 1e9 > minSize) ? var * (float)1e9 : minSize;
	else if (streq(str, "MB"))	return (var * 1e6 > minSize) ? var * (float)1e6 : minSize;
	else if (streq(str, "kB"))	return (var * 1e3 > minSize) ? var * (float)1e3 : minSize;
	else if (streq(str, "B"))	return (var > minSize) ? var : minSize;
	else if (element1 * element0 == 1 || streq(str, "#")) {	// no units, assumed Byte
		return (var > 1e6) ? var : minSize;
	}
	else {	// wrong units, raise warning
		ValidUnits = 0;
		return (var > minSize) ? var : minSize;  // no units specified in the config file; assuming bytes
	}
}


static void LoadMacro(char* parval, Janus_Config_t* J_cfg2, int ParsingMode) {
	FILE* n_cfg;
	n_cfg = fopen(parval, "r");
	if (n_cfg != NULL) {
		Con_printf("LCSm", "Loading Additional config file \"%s\"\n", parval);
		ParseConfigFile(n_cfg, J_cfg2, ParsingMode & 0xE);
		fclose(n_cfg);
	} else {
		Con_printf("LCSw", "WARNING: Loading Macro: Macro file \"%s\" not found\n", parval);
		ValidParameterValue = 0;
	}
}


// ---------------------------------------------------------------------------------
// Description: check return value of FERS_SetParam
// Inputs:      parameter name
//              return value of FERS_SetParam
// ---------------------------------------------------------------------------------
static void CheckSetParamStatus(int ret, char* parname, char* parval) {

	ValidParameterName = (ret == FERSLIB_ERR_INVALID_PARAM) ? 0 : 1;
	ValidParameterValue = (ret == FERSLIB_ERR_INVALID_PARAM_VALUE) ? 0 : 1;
	ValidUnits = (ret == FERSLIB_ERR_INVALID_PARAM_UNIT) ? 0 : 1;


	if (!ValidParameterName)
		Con_printf("LCSw", "WARNING: %s: unkwown parameter\n", parname);
	else if (!ValidParameterValue)
		Con_printf("LCSw", "WARNING: %s: unkwown value '%s'\n", parname, parval);
	else if (!ValidUnits)
		Con_printf("LCSw", "WARNING: %s: unkwown units. Janus use as default V, mA, ns\n", parname);

}


// ---------------------------------------------------------------------------------
// Description: Mapping Trigger:Tref value for SpectTiming mode
// Inputs:		TriggerMask as returned from GetParam
// Outputs:		
// Return:		TrefMask value pointing to the same source of trigger, or -1 if the input is incorrect
// ---------------------------------------------------------------------------------
static uint32_t TriggerTrefMap(uint32_t TrgMask)
{
	if (TrgMask == 0x3)  return 0x2;  // T1-In  Trg=0x3:  Tref=0x2
	else if (TrgMask == 0x5)  return 0x4;  // Q-OR   Trg=0x5:  Tref=0x4
	else if (TrgMask == 0x9)  return 0x8;  // T-OR   Trg=0x9:  Tref=0x8
	else if (TrgMask == 0x11) return 0x1;  // T0-In  Trg=0x11: Tref=0x1
	else if (TrgMask == 0x21) return 0x10; // PTRG   Trg=0x21: Tref=0x10
	else if (TrgMask == 0x41) return 0x40; // TLOGIC Trg=0x41: Tref=0x40
	else {
		Con_printf("LCSw", "Trigger Source Mask 0x%X unknown. Cannot set Tref Source properly\n", TrgMask);
		return -1;
	}
}


// ---------------------------------------------------------------------------------
// Description: Read a config file, parse the parameters and set the relevant fields in the J_cfg structure
// Inputs:		f_ini: config file pinter
// Outputs:		J_cfg: struct with all parameters
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------
int ParseConfigFile(FILE* f_ini, Janus_Config_t* J_cfg, int ParseMode)
{
	int i, b;
	int brd, ch;  // target board/ch defined as ParamName[b][ch] (-1 = all)
	int brd_l, brd_h;  // low/high index for multiboard settings
	int idx = 0;
	int RawDataVal = 0;
	char tstr[1000], str1[1000], * parval, * tparval, * parname, * token;

	if ((ParseMode & PARSEMODE_PARSE_CONNECTION) & PARSEMODE_RESET) 
		memset(J_cfg, 0, sizeof(Janus_Config_t));
		//J_cfg->NumBrd = 0;
	
	if (ParseMode & PARSEMODE_FIRST_CALL) { // initialize J_cfg when it is call by Janus. No when it is call for overwrite parameters 
		int num_brd = J_cfg->NumBrd;
		char mpath[MAX_NBRD][200];
		for (int i = 0; i < MAX_NBRD_JANUS; ++i) 
			sprintf(mpath[i], "%s", J_cfg->ConnPath[i]);

		memset(J_cfg, 0, sizeof(Janus_Config_t));
		/* Default settings */
		strcpy(J_cfg->DataFilePath, "DataFiles");
		J_cfg->NumBrd = num_brd;	
		for (int i = 0; i < MAX_NBRD_JANUS; ++i) 
			sprintf(J_cfg->ConnPath[i], "%s", mpath[i]);

		J_cfg->NumCh = 64;
		J_cfg->EHistoNbin = 4096;
		J_cfg->ToAHistoNbin = 4096;
		J_cfg->ToARebin = 1;
		J_cfg->ToAHistoMin = 0;
		J_cfg->ToTHistoNbin = 4096;
		J_cfg->MCSHistoNbin = 4096;
		J_cfg->AcquisitionMode = 0;
		J_cfg->EnableToT = 1;
		J_cfg->TriggerMask = 0;
		J_cfg->PtrgPeriod = 0;
		//J_cfg->TD_CoarseThreshold = 0;
		J_cfg->EnLiveParamChange = 1;
		J_cfg->AskHVShutDownOnExit = 1;
		J_cfg->MaxOutFileSize = 1e9; // 1 GB
		J_cfg->EnableRawDataRead = 0;
		J_cfg->EnableMaxFileSize = 0;
		J_cfg->EnableListZeroSuppr = 0;

	}
	
	//read config file and assign parameters 
	while(!feof(f_ini)) {
		brd = -1;
		ch = -1;
		ValidParameterName = 0;
		ValidParameterValue = 1;
		ValidUnits = 1;

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
		}
		else {
			brd_l = 0;
			brd_h = (J_cfg->NumBrd > 0) ? J_cfg->NumBrd : MAX_NBRD_JANUS;
		}

		if (brd_l < 0 || brd_l > (MAX_NBRD_JANUS - 1)) { // brd_l cannot be larger than MAX_NBRD - 1
			Con_printf("LCSw", "%s: brd %d index out of range\n", parname, brd_l);
			continue;
		}

		// Some name replacement for back compatibility
		if (streq(parname, "TriggerSource"))		sprintf(parname, "BunchTrgSource");
		if (streq(parname, "DwellTime"))			sprintf(parname, "PtrgPeriod");
		if (streq(parname, "TrgTimeWindow"))		sprintf(parname, "TstampCoincWindow");
		if (streq(parname, "Hit_HoldOff"))			sprintf(parname, "Trg_HoldOff");
		if (streq(parname, "PairedCnt_CoincWin"))	sprintf(parname, "ChTrg_Width");

		// START PARAM PARSER
		if (ParseMode & PARSEMODE_PARSE_CONNECTION) {
			if (streq(parname, "Open")) {
				if (brd == -1) {
					Con_printf("LCSw", "%s: cannot be a common setting (must be in a [BOARD] section)\n", parname);
				}
				else {
					if (J_cfg->NumBrd >= MAX_NBRD_JANUS) {
						Con_printf("LCSw", "WARNING: Maximum number of supported boards reached (NumBoards = %d)\n", J_cfg->NumBrd);
						continue;
					}
					if (streq(J_cfg->ConnPath[brd], "")) {
						strcpy(J_cfg->ConnPath[brd], parval);
						J_cfg->NumBrd++;
					} else {
						Con_printf("LCSw", "WARNING: Board %d already has a connection path assigned. Please, check the Janus_Config.txt file\n", brd);
					}
					
					if (streq(J_cfg->ConnPath[brd], ""))
						Con_printf("LCSw", "%s: connection path cannot be empty\n", parval);
				}
			}
			// Other parameters can be read and used in Lib before do the complete parser
			if (streq(parname, "Load"))
				LoadMacro(parval, J_cfg, ParseMode);
			if (streq(parname, "FiberDelayAdjust")) {
				int np, cnc, chain, node;
				float length; // length expressed in m
				np = sscanf(parval, "%d %d %d %f", &cnc, &chain, &node, &length);
				if ((np == 4) && (cnc >= 0) && (cnc < FERSLIB_MAX_NCNC) && (chain >= 0) && (chain < 8) && (node >= 0) && (node < 16))
					J_cfg->FiberDelayAdjust[cnc][chain][node] = length;
			}
			if (streq(parname, "DebugLogMask")) {
				FERS_SetParam(0, "DebugLogMask", parval);
			}

			// These parameters must be set for synchronizing conncetrator with other devices - Not TRUE ANYMORE, can be removed
			if (streq(parname, "StartRunMode")) {
				int ret = FERS_SetParam(0, "StartRunMode", parval);
				if		(streq(parval, "ASYNC"))				J_cfg->StartRunMode = STARTRUN_ASYNC;
				else if (streq(parval, "CHAIN_T0"))				J_cfg->StartRunMode = STARTRUN_CHAIN_T0;
				else if (streq(parval, "CHAIN_T1"))				J_cfg->StartRunMode = STARTRUN_CHAIN_T1;
				else if (streq(parval, "TDL"))					J_cfg->StartRunMode = STARTRUN_TDL;
				else if (streq(parval, "TDL_EXTRUN"))			J_cfg->StartRunMode = STARTRUN_TDL_EXTRUN;
				else if (streq(parval, "TDL_GPS"))				J_cfg->StartRunMode = STARTRUN_TDL_GPS;
				if (ret < 0) ValidParameterValue = 0;
			}

			//if (strstr(parname, "CncProbe"))
			continue; // Skip the rest of the code
		} 

		if (streq(parname, "Open")) continue; // Skip Open param when not parse connection

		if (streq(parname, "EventBuildingMode")) {
			if		(streq(parval, "DISABLED"))		J_cfg->EventBuildingMode = EVBLD_DISABLED;
			else if	(streq(parval, "TRGTIME_SORTING"))J_cfg->EventBuildingMode = EVBLD_TRGTIME_SORTING;
			else if	(streq(parval, "TRGID_SORTING"))	J_cfg->EventBuildingMode = EVBLD_TRGID_SORTING;
			else 	ValidParameterValue = 0;
		}
		if (streq(parname, "DataAnalysis")) {
			if (streq(parval, "NONE") || streq(parval, "DISABLED"))	J_cfg->DataAnalysis = 0;
			else if (streq(parval, "CNT_ONLY"))		J_cfg->DataAnalysis = DATA_ANALYSIS_CNT;
			else if (streq(parval, "ALL"))			J_cfg->DataAnalysis = DATA_ANALYSIS_CNT | DATA_ANALYSIS_HISTO | DATA_ANALYSIS_MEAS;
			else if (strstr(parval, "MASK"))			J_cfg->DataAnalysis = GetHex32(trim(strtok(parval, "MASK")));
			else 	ValidParameterValue = 0;
		}
		if (streq(parname, "OF_OutFileUnit")) {
			if (streq(parval, "LSB"))					J_cfg->OutFileUnit = OF_UNIT_LSB;
			else if (streq(parval, "ns"))				J_cfg->OutFileUnit = OF_UNIT_NS;
			else 	ValidParameterValue = 0;
		}
		if (streq(parname, "CitirocCfgMode")) {
			if		(streq(parval, "FROM_FILE"))		J_cfg->CitirocCfgMode = CITIROC_CFG_FROM_FILE;
			else if	(streq(parval, "FROM_REGS"))		J_cfg->CitirocCfgMode = CITIROC_CFG_FROM_REGS;
			else 	ValidParameterValue = 0;
		}

		if (streq(parname, "DataFilePath"))				GetDatapath(parval, J_cfg);
		if (streq(parname, "EHistoNbin"))				J_cfg->EHistoNbin			= GetNbin(parval);
		if (streq(parname, "ToAHistoNbin"))				J_cfg->ToAHistoNbin			= GetNbin(parval);
		if (streq(parname, "ToARebin"))					J_cfg->ToARebin				= GetInt(parval);
		if (streq(parname, "ToAHistoMin"))				J_cfg->ToAHistoMin			= GetTime(parval, "ns");
		if (streq(parname, "ToTHistoNbin"))				J_cfg->ToTHistoNbin			= GetNbin(parval);
		if (streq(parname, "MCSHistoNbin"))				J_cfg->MCSHistoNbin			= GetNbin(parval);
		if (streq(parname, "JobFirstRun"))				J_cfg->JobFirstRun			= GetInt(parval);
		if (streq(parname, "JobLastRun"))				J_cfg->JobLastRun			= GetInt(parval);
		if (streq(parname, "RunSleep"))					J_cfg->RunSleep				= GetTime(parval, "s");
		if (streq(parname, "EnableJobs"))				J_cfg->EnableJobs			= GetInt(parval);
		if (streq(parname, "EnLiveParamChange"))		J_cfg->EnLiveParamChange	= GetInt(parval);
		if (streq(parname, "OutFileEnableMask"))		J_cfg->OutFileEnableMask	= GetHex32(parval);
		if (streq(parname, "OF_EnMaxSize"))				J_cfg->EnableMaxFileSize	= GetInt(parval);
		if (streq(parname, "OF_MaxSize"))				J_cfg->MaxOutFileSize		= GetBytes(parval);
		if (streq(parname, "EnableRawDataRead"))		J_cfg->EnableRawDataRead	= GetInt(parval);
		if (streq(parname, "OF_ListLL"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_RAW_LL, GetInt(parval));
		if (streq(parname, "OF_ListBin"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_LIST_BIN, GetInt(parval));
		if (streq(parname, "OF_ListCSV"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_LIST_CSV, GetInt(parval));
		if (streq(parname, "OF_ListAscii"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_LIST_ASCII, GetInt(parval));
		if (streq(parname, "OF_Sync"))					J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_SYNC, GetInt(parval));
		if (streq(parname, "OF_SpectHisto"))			J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_SPECT_HISTO, GetInt(parval));
		if (streq(parname, "OF_ToAHisto"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_ToA_HISTO, GetInt(parval));
		if (streq(parname, "OF_ToTHisto"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_TOT_HISTO, GetInt(parval));
		if (streq(parname, "OF_Staircase"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_STAIRCASE, GetInt(parval));
		if (streq(parname, "OF_RunInfo"))				J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_RUN_INFO, GetInt(parval));
		if (streq(parname, "OF_ServiceInfo"))			J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_SERVICE_INFO, GetInt(parval));
		if (streq(parname, "OF_MCS"))					J_cfg->OutFileEnableMask	= SETBIT(J_cfg->OutFileEnableMask, OUTFILE_MCS_HISTO, GetInt(parval));
		if (streq(parname, "TstampCoincWindow"))		J_cfg->TstampCoincWindow	= (uint32_t)GetTime(parval, "ns");
		if (streq(parname, "PresetTime"))				J_cfg->PresetTime			= GetTime(parval, "s");
		if (streq(parname, "PresetCounts"))				J_cfg->PresetCounts			= GetInt(parval);
		if (streq(parname, "RunNumber_AutoIncr"))		J_cfg->RunNumber_AutoIncr	= GetInt(parval);
		if (streq(parname, "AskHVShutDownOnExit"))		J_cfg->AskHVShutDownOnExit  = GetInt(parval);
		if (streq(parname, "EnableListZeroSuppr"))		J_cfg->EnableListZeroSuppr	= GetInt(parval);	

			
		if (streq(parname, "Load")) {
			FILE* n_cfg;
			n_cfg = fopen(parval, "r");
			if (n_cfg != NULL) {
				Con_printf("LCSm", "Loading Additional config file \"%s\"\n", parval);
				ParseConfigFile(n_cfg, J_cfg, ParseMode & 0xE);
				fclose(n_cfg);
			} else {
				Con_printf("LCSw", "WARNING: Loading Macro: Macro file \"%s\" not found\n", parval);
				ValidParameterValue = 0;
			}
		}

		// if exists, load the extra settings contained in PostConfig.txt
		static int PostConfigDone = 0;
		if (!PostConfigDone) {
			FILE* pcfg = fopen("PostConfig.txt", "r");
			PostConfigDone = 1;
			if (pcfg != NULL) {
				Con_printf("LCSm", "Reading additional configuration file PostConfig.txt\n");
				ParseConfigFile(pcfg, J_cfg, ParseMode);
				fclose(pcfg);
				PostConfigDone = 0;
			}
		}

		//if (!ValidParameterName) IsJanusParam = 0;

		// Param name not find in Janus param list. Can be a param of the lib. Call the set_param
		if (ParseMode & PARSEMODE_PARSE_ALL) {
			if (!ValidParameterName) {
				// Append the '[ch]' if ch != 0
				char tmp_name[100] = "";
				if (ch >= 0) {
					sprintf(tmp_name, "%.98s[%d]", parname, ch);
					sprintf(parname, "%s", tmp_name);
				}
				char desc_err[100][1024];
				if (streq(parname, "OF_RawData")) RawDataVal = GetInt(parval);
				if (RawDataVal && !strstr(J_cfg->ConnPath[0], "offline"))
					J_cfg->OutFileEnableMask = SETBIT(J_cfg->OutFileEnableMask, OUTFILE_RUN_INFO, 1);

				for (b = brd_l; b < brd_h; b++) {
					int ret;
					//printf("%s %s\n", parname, parval);
					ret = FERS_SetParam(handle[b], parname, parval);
					if (ret < 0) {
						char desc_err[1024];
						FERS_GetLastError(desc_err);
						Con_printf("LCSw", "%s\n", desc_err);
					}
					ValidParameterName = (ret == FERSLIB_ERR_INVALID_PARAM) ? 0 : 1;
					ValidParameterValue = (ret == FERSLIB_ERR_INVALID_PARAM_VALUE) ? 0 : 1;
					ValidUnits = (ret == FERSLIB_ERR_INVALID_PARAM_UNIT) ? 0 : 1;
				}
			}
		}

		if (!ValidParameterName)
			Con_printf("LCSw", "WARNING: %s: unkwown parameter\n", parname);
		else if (!ValidParameterValue)
			Con_printf("LCSw", "WARNING: %s: unkwown value '%s'\n", parname, parval);
		else if (!ValidUnits)
			Con_printf("LCSw", "WARNING: %s: unkwown units. Janus use as default V, mA, ns\n", parname);
	}

	if (!(ParseMode & PARSEMODE_FIRST_CALL)) return 0; // The code below must be executed just on the first call


	if (J_cfg->EHistoNbin > (1 << ENERGY_NBIT))	J_cfg->EHistoNbin = (1 << ENERGY_NBIT);
	if (J_cfg->ToAHistoNbin > (1 << TOA_NBIT))	J_cfg->ToAHistoNbin = (1 << TOA_NBIT);	// DNIN: misleading. This is just for plot visualization
	if (J_cfg->ToTHistoNbin > (1 << TOT_NBIT))	J_cfg->ToTHistoNbin = (1 << TOT_NBIT);
	
	
	J_cfg->AcquisitionMode = FERS_GetParam_int(handle[0], "AcquisitionMode");
	J_cfg->StartRunMode = FERS_GetParam_int(handle[0], "StartRunMode"); // Performed during connection
	J_cfg->StopRunMode = FERS_GetParam_int(handle[0], "StopRunMode");

	J_cfg->PtrgPeriod = FERS_GetParam_float(handle[0], "PtrgPeriod");
	for (int b = 0; b < J_cfg->NumBrd; ++b) {
		J_cfg->HV_Vbias[b] = FERS_GetParam_float(handle[b], "HV_Vbias");
	}
	J_cfg->TriggerMask = FERS_GetParam_uint32(handle[0], "TriggerMask");
	J_cfg->EnableServiceEvent = FERS_GetParam_int(handle[0], "EnableServiceEvents");

	// If Spect_Timing force Tref = Ptrg
	if (J_cfg->AcquisitionMode == ACQMODE_TSPECT) {
		char mypar[20];
		uint32_t maskVal = TriggerTrefMap(FERS_GetParam_uint32(handle[0], "BunchTrgSource"));
		uint32_t TrefMask = FERS_GetParam_uint32(handle[0], "TrefSource");
		if (maskVal != TrefMask) {
			Con_printf("LCSw", "WARNING:In SpectTiming mode Tref source must match Trigger source. Overwriting TrefSource\n");
			sprintf(mypar, "MASK 0x%x", maskVal);
			for (int j = 0; j < J_cfg->NumBrd; ++j)
				FERS_SetParam(handle[j], "TrefSource", mypar);
			Con_printf("SM", "TrefSource:0x%X", maskVal);
		}
	}		
#ifdef linux
	if (J_cfg->DataFilePath[strlen(J_cfg->DataFilePath)-1] != '/')	strcat(J_cfg->DataFilePath, "/");
#else
	if (J_cfg->DataFilePath[strlen(J_cfg->DataFilePath)-1] != '\\')	strcat(J_cfg->DataFilePath, "\\");
#endif


	// If AcquisitionMode is WaveForm, file saving disabled
	if (J_cfg->AcquisitionMode == ACQMODE_WAVE) {
		Con_printf("LCSw", "WARNING:Waveform acquisition mode is selected. Output files saving is disabled\n");
		J_cfg->OutFileEnableMask = 0;
		for (int i = 0; i < J_cfg->NumBrd; ++i)
			FERS_SetParam(handle[i], "OF_RawData", "0");
	}

	// Force options when connection is offline
	for (i = 0; i < J_cfg->NumBrd; ++i) {
		if (strstr(J_cfg->ConnPath[i], "offline") != NULL) {
			if (J_cfg->StopRunMode != STOPRUN_MANUAL) {
				Con_printf("LCSw", "WARNING:Manual Stop Run must be set when Raw Data are processed\nSetting MANUAL option ...\n");
				J_cfg->StopRunMode = STOPRUN_MANUAL;
				Con_printf("SM", "StopRunMode:%d", STOPRUN_MANUAL);
			}
			if (J_cfg->StartRunMode != STARTRUN_ASYNC) {
				Con_printf("LCSw", "Offline connection need ASYNC StartRunMode. Switching to ASYNC\n");
				J_cfg->StartRunMode = STARTRUN_ASYNC;
				Con_printf("SM", "StartRunMode:%d", J_cfg->StartRunMode);
			}
			if (J_cfg->EnableJobs != 0) {
				Con_printf("LCSw", "WARNING:Jobs are not permitted when Raw Data are processed\nDisabling Jobs ...\n");
				J_cfg->EnableJobs = 0;
				Con_printf("SM", "EnableJobs:%d", J_cfg->EnableJobs);
			}
			if (RawDataVal) {	// FERSlib already unset RawData saving if offline mode is on.
				Con_printf("LCSw", "WARNING:Cannot save Raw Data file when Raw Data are processed\nDisabling Raw Data saving...\n");
				Con_printf("SM", "OF_RawData:0");
				RawDataVal = 0;
			}
			break;
		}
	}

	// SetParam for RawData saving
	char buffLS[50];
	char buffMS[50];
	sprintf(buffLS, "%" PRIu8, J_cfg->EnableMaxFileSize);
	sprintf(buffMS, "%f", J_cfg->MaxOutFileSize);
	int ret = 0;
	for (int i = 0; i < J_cfg->NumBrd; ++i) {
		ret = FERS_SetParam(handle[i], "OF_RawDataPath", J_cfg->DataFilePath);
		CheckSetParamStatus(ret, "OF_RawDataPath", J_cfg->DataFilePath);
		ret = FERS_SetParam(handle[i], "OF_LimitedSize", buffLS);
		CheckSetParamStatus(ret, "OF_LimitedSize", buffLS);
		ret = FERS_SetParam(handle[i], "MaxSizeDataOutputFile", buffMS);
		CheckSetParamStatus(ret, "MaxSizeDataOutputFile", buffMS);	}

	return 0;
}

