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
#include "FERSlib.h"


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
// Description: check if the directory exists, if not it is created
// Inputs:		WDcfg, Cfg file
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

void GetDatapath(FILE* f_ini, Config_t* WDcfg) {

	char mchar[200];
	fscanf(f_ini, "%s", mchar);
	if (access(mchar, 0) == 0) { // taken from https://stackoverflow.com/questions/6218325/
		struct stat status;
		stat(mchar, &status);
		int myb = status.st_mode & S_IFDIR;
		if (myb == 0) {
			Con_printf("LCSw", "WARNING: DataFilePath: %s is not a valid directory. Default .DataFiles folder is used\n", mchar);
			strcpy(WDcfg->DataFilePath, "DataFiles");
		}
		else
			strcpy(WDcfg->DataFilePath, mchar);
	}
	else {
		int ret = f_mkdir(mchar);
		if (ret == 0)
			strcpy(WDcfg->DataFilePath, mchar);
		else {
			Con_printf("LCSw", "WARNING: DataFilePath %s cannot be created, default .DataFiles folder is used\n", mchar);
			strcpy(WDcfg->DataFilePath, "DataFiles");
		}
	}
}

// ---------------------------------------------------------------------------------
// Description: Read an integer (decimal) from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file / Set ValidParameterValue=0 if the string is not an integer
// ---------------------------------------------------------------------------------
int GetInt(FILE *f_ini)
{
	int ret;
	char val[50];
	//fscanf(f_ini, "%d", &ret);
	fscanf(f_ini, "%s", val);
	//fgets(val, 100, f_ini);
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
int GetNbin(FILE *f_ini)
{
	int i;
	char str[100];
	fscanf(f_ini, "%s", str);
	for (i=0; i<(int)strlen(str); i++)
		str[i] = toupper(str[i]);
	if		((streq(str, "DISABLED")) || (streq(str, "0")))	return 0;
	else if  (streq(str, "256"))							return 256;
	else if  (streq(str, "512"))							return 512;	else if ((streq(str, "1024")  || streq(str, "1K")))		return 1024;
	else if ((streq(str, "2048")  || streq(str, "2K")))		return 2048;
	else if ((streq(str, "4096")  || streq(str, "4K")))		return 4096;
	else if ((streq(str, "8192")  || streq(str, "8K")))		return 8192;
	else if ((streq(str, "16384") || streq(str, "16K")))	return 16384;
	else { 												
		return 1024;  // assign a default value on error
		ValidParameterValue = 0;
		//Con_printf("LCSw", "WARNING: Einvalid Nbin value %s\n", str);
	}
}



// ---------------------------------------------------------------------------------
// Description: Read an integer (hexadecimal) from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		integer value read from the file / Set ValidParameterValue = 0 if the value is not in HEX format
// ---------------------------------------------------------------------------------
int GetHex(FILE *f_ini)
{ 
	int ret;
	char str[100];
	ValidParameterValue = 1;
	fscanf(f_ini, "%s", str);
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
// Description: Read a float from the conig file
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		float value read from the file / 
// ---------------------------------------------------------------------------------
float GetFloat(FILE *f_ini)
{
	float ret;
	char str[1000];
	int i;
	ValidParameterValue = 1;
	fgets(str, 1000, f_ini);
	// replaces ',' with '.' (decimal separator)
	for(i=0; i<(int)strlen(str); i++)
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
float GetTime(FILE *f_ini, char *tu)
{
	double timev=-1;
	double ns;
	char val[500];
	long fp;
	char str[100];

	int element;

	//fscanf(f_ini, "%lf", &timev);
	//fp = ftell(f_ini);
	fgets(val, 500, f_ini);
	fp = ftell(f_ini);
	element = sscanf(val, "%lf %s", &timev, str);
	//fseek(f_ini, fp, SEEK_SET);
	/*if (timev < 0) ValidParameterValue = 0;
	else ValidParameterValue = 1;*/
	// try to read the unit from the config file (string)
	//fp = ftell(f_ini);  // save current pointer before "str"
	//fscanf(f_ini, "%s", str);  // read string "str"
	ValidUnits = 1;
	if (streq(str, "ps"))		ns = timev * 1e-3;
	else if (streq(str, "ns"))	ns = timev;
	else if (streq(str, "us"))	ns = timev * 1e3;
	else if (streq(str, "ms"))	ns = timev * 1e6;
	else if (streq(str, "s"))	ns = timev * 1e9;
	else if (streq(str, "m"))	ns = timev * 60e9;
	else if (streq(str, "h"))	ns = timev * 3600e9;
	else if (streq(str, "d"))	ns = timev * 24 * (3600e9);
	else if (element == 1 || streq(str, "#")) return (float)timev;
	else {
		ValidUnits = 0;
		fseek(f_ini, fp-2, SEEK_SET); // move pointer back to beginning of "str" and use it again for next parsing
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
// Description: Read a value from the conig file followed by an optional time unit (V, mV, uV)
//              and convert it in a voltage expressed in volts 
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		voltage value expressed in volts
// ---------------------------------------------------------------------------------
float GetVoltage(FILE *f_ini)
{
	float var;
	long fp;
	char str[100];

	int val0 = fscanf(f_ini, "%f", &var);
	// try to read the unit from the config file (string)
	fp = ftell(f_ini);  // save current pointer before "str"
	int val1 = fscanf(f_ini, "%s", str);  // read string "str"
	ValidUnits = 1;
	if (streq(str, "uV"))		return var / 1000000;
	else if (streq(str, "mV"))	return var / 1000;
	else if (streq(str, "V"))	return var;
	else if (val1 != 1 || streq(str, "#")) {	// no units, assumed Voltage
		fseek(f_ini, fp, SEEK_SET); // move pointer back to beginning of "str" and use it again for next parsing
		return var;
	}
	else {	// wrong units, raise warning
		ValidUnits = 0;
		fseek(f_ini, fp, SEEK_SET); // move pointer back to beginning of "str" and use it again for next parsing
		return var;  // no voltage unit specified in the config file; assuming volts
	}
}

// ---------------------------------------------------------------------------------
// Description: Read a value from the conig file followed by an optional time unit (A, mA, uA)
//              and convert it in a current expressed in mA 
// Inputs:		f_ini: config file
// Outputs:		-
// Return:		current value expressed in mA  / Set ValidParamName/Value=0 if the expected format is not matched
// ---------------------------------------------------------------------------------
float GetCurrent(FILE *f_ini)
{
	char van[50];
	float var = 0;
	char val[500] = "";
	long fp = 0;
	char stra[100] = "";
	int element0 = 0;
	int element1 = 0;

	//fscanf(f_ini, "%f", &var);
	fgets(val, 500, f_ini);
	fp = ftell(f_ini);
	//fscanf(f_ini, "%s", val);
	//element0 = sscanf(val, "%f %s", &var, stra);
	element0 = sscanf(val, "%s %s", van, stra);
	element1 = sscanf(van, "%f", &var);
	if (element1 > 0) ValidParameterValue = 1;
	else ValidParameterValue = 0;
	
	// try to read the unit from the config file (string)
	//fp = ftell(f_ini);  // save current pointer before "str"
	//fscanf(f_ini, "%s", stra);  // read string "str"
	ValidUnits = 1;
	if (streq(stra, "uA"))		return var / 1000;
	else if (streq(stra, "mA"))	return var;
	else if (streq(stra, "A"))	return var * 1000;
	else if (element1*element0 == 1 || streq(stra, "#")) {	// No units, no warning raised
		fseek(f_ini, fp - 1, SEEK_SET); // move pointer back to beginning of "str" and use it again for next parsing
		return var;
	}
	else {	// Wrong units entered
		ValidUnits = 0;
		fseek(f_ini, fp - 1, SEEK_SET); // move pointer back to beginning of "str" and use it again for next parsing
		return var;  // wrong unit specified in the config file; assuming mA
	}
}


// ---------------------------------------------------------------------------------
// Description: Set a parameter (individual board or broadcast) to a given integer value 
// Inputs:		param: array of parameters 
//				val: value to set
// Outputs:		-
// Return:		-
// ---------------------------------------------------------------------------------
void SetBoardParam(int param[MAX_NBRD], int val)
{
	int b;
	if (brd == -1)
		for (b = 0; b < MAX_NBRD; b++)
			param[b] = val;
	else
		param[brd] = val;
}

// ---------------------------------------------------------------------------------
// Description: Set a parameter (individual board or broadcast) to a given integer value 
// Inputs:		param: array of parameters 
//				val: value to set
// Outputs:		-
// Return:		-
// ---------------------------------------------------------------------------------
void SetBoardParamFloat(float param[MAX_NBRD], float val)
{
	int b;
	if (brd == -1)
		for (b = 0; b < MAX_NBRD; b++)
			param[b] = val;
	else
		param[brd] = val;
}

// ---------------------------------------------------------------------------------
// Description: Set a parameter (individual channel or broadcast) to a given integer value 
// Inputs:		param: array of parameters 
//				val: value to set
// Outputs:		-
// Return:		-
// ---------------------------------------------------------------------------------
void SetChannelParam(uint16_t param[MAX_NBRD][MAX_NCH], int val)
{
	int i, b;
    if (brd == -1) {
        for(b=0; b<MAX_NBRD; b++)
			for(i=0; i<MAX_NCH; i++)
				param[b][i] = val;
	} else if (ch == -1) {
		for(i=0; i<MAX_NCH; i++)
			param[brd][i] = val;
	} else {
        param[brd][ch] = val;
	}
}

// ---------------------------------------------------------------------------------
// Description: Set a parameter (individual channel or broadcast) to a given float value 
// Inputs:		param: array of parameters 
//				val: value to set
// Outputs:		-
// Return:		-
// ---------------------------------------------------------------------------------
void SetChannelParamFloat(float param[MAX_NBRD][MAX_NCH], float val)
{
	int i, b;
    if (brd == -1) {
        for(b=0; b<MAX_NBRD; b++)
			for(i=0; i<MAX_NCH; i++)
				param[b][i] = val;
	} else if (ch == -1) {
		for(i=0; i<MAX_NCH; i++)
			param[brd][i] = val;
	} else {
        param[brd][ch] = val;
	}
}

// ---------------------------------------------------------------------------------
// Description: Parse WDcfg with parameter of a new configuration file
// Inputs:		param: file name, Config_t 
//				val: value to set
// Outputs:		-
// Return:		-
// ---------------------------------------------------------------------------------
void LoadExtCfgFile(FILE* f_ini, Config_t* WDcfg) {	// DNIN: The first initialization should not be done, it must be inside and if block [NEED TO BE CHECKED]
	char nfile[500];
	int mf = 0;
	mf = fscanf(f_ini, "%s", nfile);
	if (mf == 0) ValidParameterValue = 0;	// DNIN: filename missing
	else {
		FILE* n_cfg;
		n_cfg = fopen(nfile, "r");
		if (n_cfg != NULL) {
			Con_printf("LCSm", "Overwriting parameters from %s\n", nfile);
			ParseConfigFile(n_cfg, WDcfg, 0);
			fclose(n_cfg);
			ValidParameterValue = 1;
			ValidParameterName = 1;
		} else {
			Con_printf("LCSm", "Macro file \"%s\" not found.", nfile);
			ValidParameterValue = 0;
		}
	}

}

// ---------------------------------------------------------------------------------
// Description: Read a config file, parse the parameters and set the relevant fields in the WDcfg structure
// Inputs:		f_ini: config file pinter
// Outputs:		WDcfg: struct with all parameters
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------
int ParseConfigFile(FILE* f_ini, Config_t* WDcfg, bool fcall)
{
	char str[1000], str1[1000];
	int i, b, val; // , tr = -1;
	int brd1, ch1;  // target board/ch defined as ParamName[b][ch]
	int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD b] [CHANNEL ch])

	if (fcall) { // initialize WDcfg when it is call by Janus. No when it is call for overwrite parameters 
		memset(WDcfg, 0, sizeof(Config_t));
		/* Default settings */
		strcpy(WDcfg->DataFilePath, "DataFiles");
		WDcfg->NumBrd = 0;	
		WDcfg->NumCh = 64;
		WDcfg->GWn = 0;
		WDcfg->EHistoNbin = 4096;
		WDcfg->ToAHistoNbin = 4096;
		WDcfg->ToARebin = 1;
		WDcfg->ToAHistoMin = 0;
		WDcfg->ToTHistoNbin = 512;
		WDcfg->MCSHistoNbin = 4096;
		WDcfg->AcquisitionMode = 0;
		WDcfg->EnableServiceEvents = 3;  // enable service events with both HV mon and Counter
		WDcfg->EnableCntZeroSuppr = 1;
		WDcfg->SupprZeroCntListFile = 0;
		WDcfg->EnableToT = 1;
		WDcfg->TriggerMask = 0;
		WDcfg->TriggerLogic = 0;
		WDcfg->MajorityLevel = 2;
		WDcfg->Tref_Mask = 0;
		WDcfg->TrefWindow = 100;
		WDcfg->PtrgPeriod = 0;
		WDcfg->QD_CoarseThreshold = 0;
		//WDcfg->TD_CoarseThreshold = 0;
		WDcfg->HG_ShapingTime = 0;
		WDcfg->LG_ShapingTime = 0;
		WDcfg->Enable_HV_Adjust = 0;
		WDcfg->EnableChannelTrgout = 1;
		WDcfg->HV_Adjust_Range = 1;
		WDcfg->EnableQdiscrLatch = 1;
		WDcfg->GainSelect = GAIN_SEL_AUTO;
		WDcfg->WaveformLength = 800;
		WDcfg->Trg_HoldOff = 0;
		WDcfg->Pedestal = 100;
		WDcfg->TempSensCoeff[0] = 0;
		WDcfg->TempSensCoeff[1] = 50;
		WDcfg->TempSensCoeff[2] = 0;
		WDcfg->EnLiveParamChange = 1;
		WDcfg->AskHVShutDownOnExit = 1;

		for (b = 0; b < MAX_NBRD; b++) {
			WDcfg->TD_CoarseThreshold[b] = 0;	// new
			WDcfg->ChEnableMask0[b] = 0xFFFFFFFF;
			WDcfg->ChEnableMask1[b] = 0xFFFFFFFF;
			WDcfg->Q_DiscrMask0[b] = 0xFFFFFFFF;
			WDcfg->Q_DiscrMask1[b] = 0xFFFFFFFF;
			WDcfg->Tlogic_Mask0[b] = 0xFFFFFFFF;
			WDcfg->Tlogic_Mask1[b] = 0xFFFFFFFF;
			WDcfg->HV_Vbias[b] = 30;
			WDcfg->HV_Imax[b] = (float)0.01;
			// Default values for TMP37
			for (i = 0; i < MAX_NCH; i++) {
				WDcfg->ZS_Threshold_LG[b][i] = 0;
				WDcfg->ZS_Threshold_HG[b][i] = 0;
				WDcfg->QD_FineThreshold[b][i] = 0;
				WDcfg->TD_FineThreshold[b][i] = 0;
				WDcfg->HG_Gain[b][i] = 0;
				WDcfg->LG_Gain[b][i] = 0;
				WDcfg->HV_IndivAdj[b][i] = 0;
			}
		}
	}
	
	//read config file and assign parameters 
	while(!feof(f_ini)) {
		int read;
		char *cb, *cc;

		brd1 = -1;
		ch1 = -1;
		ValidParameterName = 0;
        // read a word from the file
        read = fscanf(f_ini, "%s", str);
        if( !read || (read == EOF) || !strlen(str))
			continue;

        // skip comments
        if (str[0] == '#') {
			fgets(str, 1000, f_ini);
			continue;
		}

		ValidParameterValue = 1;
		ValidUnits = 1;
        // Section (COMMON or individual channel)
		if (str[0] == '[')	{
			ValidParameterName = 1;
			if (strstr(str, "COMMON")!=NULL) {
				brd2 = -1;
				ch2 = -1;
			} else if (strstr(str, "BOARD")!=NULL) {
				ch2 = -1;
				fscanf(f_ini, "%s", str1);
				sscanf(str1, "%d", &val);
				if (val < 0 || val >= MAX_NBRD) Con_printf("LCSm", "%s: Invalid board number\n", str);
				else brd2 = val;
			} else if (strstr(str, "CHANNEL") != NULL) {
				fscanf(f_ini, "%s", str1);
				sscanf(str1, "%d", &val);
				if (val < 0 || val >= MAX_NCH) Con_printf("LCSm", "%s: Invalid channel number\n", str);
				if ((brd2 == -1) && (WDcfg->NumBrd == 1)) brd2 = 0;
				else ch2 = val;
			}
		} else if ((cb = strstr(str, "[")) != NULL) {
			char *ei;
			sscanf(cb+1, "%d", &brd1); 
			if ((ei = strstr(cb, "]")) == NULL) {
				Con_printf("LCSm", "%s: Invalid board index\n", str);
				fgets(str1, 1000, f_ini);
			}
			if ((cc = strstr(ei, "[")) != NULL) {
				sscanf(cc+1, "%d", &ch1);
				if ((ei = strstr(cc, "]")) == NULL) {
					Con_printf("LCSm", "%s: Invalid channel index\n", str);
					fgets(str1, 1000, f_ini);
				}
			}
			*cb = 0;
		}
		if ((brd1 >= 0) && (brd1 < MAX_NBRD) && (ch1 < MAX_NCH)) {
			brd = brd1;
			ch = ch1;
		} else {
			brd = brd2;
			ch = ch2;
		}

		// Some name replacement for back compatibility
		if (streq(str, "TriggerSource"))		sprintf(str, "BunchTrgSource");
		if (streq(str, "DwellTime"))			sprintf(str, "PtrgPeriod");
		if (streq(str, "TrgTimeWindow"))		sprintf(str, "TstampCoincWindow");
		if (streq(str, "Hit_HoldOff"))			sprintf(str, "Trg_HoldOff");

 		if (streq(str, "Open"))	{
			if (brd==-1) {
				Con_printf("LCSm", "%s: cannot be a common setting (must be in a [BOARD] section)\n", str); 
				fgets(str1, 1000, f_ini);
			} else {
				fscanf(f_ini, "%s", str1);
				if (streq(WDcfg->ConnPath[brd], ""))
					WDcfg->NumBrd++;
				strcpy(WDcfg->ConnPath[brd], str1);
				//WDcfg->NumBrd++;
			}
		}
		if (streq(str, "WriteRegister")) {	
			if (WDcfg->GWn < MAX_GW) {
				WDcfg->GWbrd[WDcfg->GWn]=brd;
				fscanf(f_ini, "%x", (int *)&WDcfg->GWaddr[WDcfg->GWn]);
				fscanf(f_ini, "%x", (int *)&WDcfg->GWdata[WDcfg->GWn]);
				fscanf(f_ini, "%x", (int *)&WDcfg->GWmask[WDcfg->GWn]);
				WDcfg->GWn++;
			} else {
				Con_printf("LCSw", "WARNING: MAX_GW Generic Write exceeded (%d). Change MAX_GW and recompile\n", MAX_GW);
			}
		}
		if (streq(str, "WriteRegisterBits")) {
			if (WDcfg->GWn < MAX_GW) {
				int start, stop, data;
				WDcfg->GWbrd[WDcfg->GWn]=brd;
				fscanf(f_ini, "%x", (int *)&WDcfg->GWaddr[WDcfg->GWn]);
				fscanf(f_ini, "%d", &start);
				fscanf(f_ini, "%d", &stop);
				fscanf(f_ini, "%d", &data);
				WDcfg->GWmask[WDcfg->GWn] = ((1<<(stop-start+1))-1) << start;
				WDcfg->GWdata[WDcfg->GWn] = ((uint32_t)data << start) & WDcfg->GWmask[WDcfg->GWn];
				WDcfg->GWn++;
			} else {
				Con_printf("LCSw", "WARNING: MAX_GW Generic Write exceeded (%d). Change MAX_GW and recompile\n", MAX_GW);
			}
		}
		if (streq(str, "AcquisitionMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "COUNTING"))			WDcfg->AcquisitionMode = ACQMODE_COUNT;
			else if	(streq(str1, "SPECTROSCOPY"))		WDcfg->AcquisitionMode = ACQMODE_SPECT;
			else if	(streq(str1, "SPECT_TIMING"))		WDcfg->AcquisitionMode = ACQMODE_TSPECT;
			else if	(streq(str1, "TIMING"))				WDcfg->AcquisitionMode = ACQMODE_TIMING_CSTART;
			else if	(streq(str1, "TIMING_CSTART"))		WDcfg->AcquisitionMode = ACQMODE_TIMING_CSTART;
			else if	(streq(str1, "TIMING_CSTOP"))		WDcfg->AcquisitionMode = ACQMODE_TIMING_CSTOP;
			else if	(streq(str1, "TIMING_STREAMING"))	WDcfg->AcquisitionMode = ACQMODE_TIMING_STREAMING;
			else if	(streq(str1, "WAVEFORM"))			WDcfg->AcquisitionMode = ACQMODE_WAVE;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);        
		}
		if (streq(str, "StartRunMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "MANUAL"))			WDcfg->StartRunMode = STARTRUN_ASYNC;  // keep "MANUAL" option for backward compatibility
			else if	(streq(str1, "ASYNC"))			WDcfg->StartRunMode = STARTRUN_ASYNC;  
			else if	(streq(str1, "CHAIN_T0"))		WDcfg->StartRunMode = STARTARUN_CHAIN_T0;  
			else if	(streq(str1, "CHAIN_T1"))		WDcfg->StartRunMode = STARTRUN_CHAIN_T1;  
			else if	(streq(str1, "TDL"))			WDcfg->StartRunMode = STARTRUN_TDL;  
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "StopRunMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "MANUAL"))			WDcfg->StopRunMode = STOPRUN_MANUAL;
			else if	(streq(str1, "PRESET_TIME"))	WDcfg->StopRunMode = STOPRUN_PRESET_TIME;
			else if	(streq(str1, "PRESET_COUNTS"))	WDcfg->StopRunMode = STOPRUN_PRESET_COUNTS;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "BunchTrgSource")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "SW_ONLY"))		WDcfg->TriggerMask = 0x1;
			else if	(streq(str1, "T1-IN"))			WDcfg->TriggerMask = 0x3;
			else if	(streq(str1, "Q-OR"))			WDcfg->TriggerMask = 0x5;
			else if	(streq(str1, "T-OR"))			WDcfg->TriggerMask = 0x9;
			else if	(streq(str1, "T0-IN"))			WDcfg->TriggerMask = 0x11;
			else if	(streq(str1, "PTRG"))			WDcfg->TriggerMask = 0x21;
			else if	(streq(str1, "TLOGIC"))			WDcfg->TriggerMask = 0x41;
			else if	(streq(str1, "MASK"))			fscanf(f_ini, "%x", &WDcfg->TriggerMask);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TriggerLogic")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "OR64"))			WDcfg->TriggerLogic = 0;
			else if	(streq(str1, "AND2_OR32"))		WDcfg->TriggerLogic = 1;
			else if	(streq(str1, "OR32_AND2"))		WDcfg->TriggerLogic = 2;
			else if	(streq(str1, "MAJ64"))			WDcfg->TriggerLogic = 4;
			else if	(streq(str1, "MAJ32_AND2"))		WDcfg->TriggerLogic = 5;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TrefSource")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "T0-IN"))			WDcfg->Tref_Mask = 0x1;
			else if	(streq(str1, "T1-IN"))			WDcfg->Tref_Mask = 0x2;
			else if	(streq(str1, "Q-OR"))			WDcfg->Tref_Mask = 0x4;
			else if	(streq(str1, "T-OR"))			WDcfg->Tref_Mask = 0x8;
			else if	(streq(str1, "PTRG"))			WDcfg->Tref_Mask = 0x10;
			else if	(streq(str1, "TLOGIC"))			WDcfg->Tref_Mask = 0x40;
			else if	(streq(str1, "MASK"))			fscanf(f_ini, "%x", &WDcfg->Tref_Mask);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "ValidationSource")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "SW_CMD"))			WDcfg->Validation_Mask = 0x1;
			else if	(streq(str1, "T0-IN"))			WDcfg->Validation_Mask = 0x2;
			else if	(streq(str1, "T1-IN"))			WDcfg->Validation_Mask = 0x4;
			else if	(streq(str1, "MASK"))			fscanf(f_ini, "%x", &WDcfg->Validation_Mask);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "ValidationMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "DISABLED"))		WDcfg->Validation_Mode = 0;
			else if	(streq(str1, "ACCEPT"))			WDcfg->Validation_Mode = 1;
			else if	(streq(str1, "REJECT"))			WDcfg->Validation_Mode = 2;
			else if	(streq(str1, "MASK"))			fscanf(f_ini, "%x", &WDcfg->Validation_Mask);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "CountingMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "SINGLES"))		WDcfg->Counting_Mode = 0;
			else if	(streq(str1, "PAIRED_AND"))		WDcfg->Counting_Mode = 1;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TrgIdMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "TRIGGER_CNT"))	WDcfg->TrgIdMode = 0;
			else if	(streq(str1, "VALIDATION_CNT"))	WDcfg->TrgIdMode = 1;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "VetoSource")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "DISABLED"))		WDcfg->Veto_Mask = 0x0;
			else if	(streq(str1, "SW_CMD"))			WDcfg->Veto_Mask = 0x1;
			else if	(streq(str1, "T0-IN"))			WDcfg->Veto_Mask = 0x2;
			else if	(streq(str1, "T1-IN"))			WDcfg->Veto_Mask = 0x4;
			else if	(streq(str1, "MASK"))			fscanf(f_ini, "%x", &WDcfg->Veto_Mask);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "EventBuildingMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "DISABLED"))		WDcfg->EventBuildingMode = EVBLD_DISABLED;
			else if	(streq(str1, "TRGTIME_SORTING"))WDcfg->EventBuildingMode = EVBLD_TRGTIME_SORTING;
			else if	(streq(str1, "TRGID_SORTING"))	WDcfg->EventBuildingMode = EVBLD_TRGID_SORTING;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "T0_Out")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "T0-IN"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x001);
			else if	(streq(str1, "BUNCHTRG"))		SetBoardParam((int *)WDcfg->T0_outMask, 0x002);
			else if	(streq(str1, "T-OR"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x004);
			else if	(streq(str1, "RUN"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x008);
			else if	(streq(str1, "PTRG"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x010);
			else if	(streq(str1, "BUSY"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x020);
			else if	(streq(str1, "DPROBE"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x040);
			else if	(streq(str1, "TLOGIC"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x080);
			else if	(streq(str1, "SQ_WAVE"))		SetBoardParam((int *)WDcfg->T0_outMask, 0x100);
			else if	(streq(str1, "TDL_SYNC"))		SetBoardParam((int *)WDcfg->T0_outMask, 0x200);
			else if	(streq(str1, "RUN_SYNC"))		SetBoardParam((int *)WDcfg->T0_outMask, 0x400);
			else if	(streq(str1, "ZERO"))			SetBoardParam((int *)WDcfg->T0_outMask, 0x000);
			else if (streq(str1, "MASK")) {
				int val;
				fscanf(f_ini, "%x", &val);
				SetBoardParam((int*)WDcfg->T0_outMask, val);
			}
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "T1_Out")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "T1-IN"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x001);
			else if (streq(str1, "BUNCHTRG"))		SetBoardParam((int*)WDcfg->T1_outMask, 0x002);
			else if (streq(str1, "Q-OR"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x004);
			else if (streq(str1, "RUN"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x008);
			else if (streq(str1, "PTRG"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x010);
			else if (streq(str1, "BUSY"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x020);
			else if (streq(str1, "DPROBE"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x040);
			else if (streq(str1, "TLOGIC"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x080);
			else if (streq(str1, "SQ_WAVE"))		SetBoardParam((int*)WDcfg->T1_outMask, 0x100);
			else if (streq(str1, "TDL_SYNC"))		SetBoardParam((int*)WDcfg->T1_outMask, 0x200);
			else if (streq(str1, "RUN_SYNC"))		SetBoardParam((int*)WDcfg->T1_outMask, 0x400);
			else if (streq(str1, "ZERO"))			SetBoardParam((int*)WDcfg->T1_outMask, 0x000);
			else if (streq(str1, "MASK")) {
				int val;
				fscanf(f_ini, "%x", &val);
				SetBoardParam((int*)WDcfg->T1_outMask, val);
			}
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TestPulseSource")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "OFF"))			WDcfg->TestPulseSource = -1;
			else if	(streq(str1, "EXT"))			WDcfg->TestPulseSource = TEST_PULSE_SOURCE_EXT;
			else if	(streq(str1, "T0-IN"))			WDcfg->TestPulseSource = TEST_PULSE_SOURCE_T0_IN;
			else if	(streq(str1, "T1-IN"))			WDcfg->TestPulseSource = TEST_PULSE_SOURCE_T1_IN;
			else if	(streq(str1, "PTRG"))			WDcfg->TestPulseSource = TEST_PULSE_SOURCE_PTRG;
			else if	(streq(str1, "SW-CMD"))			WDcfg->TestPulseSource = TEST_PULSE_SOURCE_SW_CMD;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TestPulseDestination")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "ALL"))			WDcfg->TestPulseDestination = TEST_PULSE_DEST_ALL;
			else if	(streq(str1, "EVEN"))			WDcfg->TestPulseDestination = TEST_PULSE_DEST_EVEN;
			else if	(streq(str1, "ODD"))			WDcfg->TestPulseDestination = TEST_PULSE_DEST_ODD;
			else if	(streq(str1, "NONE"))			WDcfg->TestPulseDestination = TEST_PULSE_DEST_NONE;
			else if	(streq(str1, "CH"))				fscanf(f_ini, "%d", &WDcfg->TestPulseDestination);
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TestPulsePreamp")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "HG"))			WDcfg->TestPulsePreamp = TEST_PULSE_PREAMP_HG;
			else if	(streq(str1, "LG"))			WDcfg->TestPulsePreamp = TEST_PULSE_PREAMP_LG;
			else if	(streq(str1, "BOTH"))		WDcfg->TestPulsePreamp = TEST_PULSE_PREAMP_BOTH;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "LG_ShapingTime")) {
			float st = GetTime(f_ini, "ns");
			if      ((st==0) || (st==87.5))		WDcfg->LG_ShapingTime = 0;
			else if ((st==1) || (st==75))		WDcfg->LG_ShapingTime = 1;
			else if ((st==2) || (st==62.5))		WDcfg->LG_ShapingTime = 2;
			else if ((st==3) || (st==50))		WDcfg->LG_ShapingTime = 3;
			else if ((st==4) || (st==37.5))		WDcfg->LG_ShapingTime = 4;
			else if ((st==5) || (st==25))		WDcfg->LG_ShapingTime = 5;
			else if ((st==6) || (st==12.5))		WDcfg->LG_ShapingTime = 6;
			else 	Con_printf("LCSw", "WARNING: Shaping Time LG: invalid setting\n");
		}
		if (streq(str, "HG_ShapingTime")) {
			float st = GetTime(f_ini, "ns");
			if      ((st==0) || (st==87.5))		WDcfg->HG_ShapingTime = 0;
			else if ((st==1) || (st==75))		WDcfg->HG_ShapingTime = 1;
			else if ((st==2) || (st==62.5))		WDcfg->HG_ShapingTime = 2;
			else if ((st==3) || (st==50))		WDcfg->HG_ShapingTime = 3;
			else if ((st==4) || (st==37.5))		WDcfg->HG_ShapingTime = 4;
			else if ((st==5) || (st==25))		WDcfg->HG_ShapingTime = 5;
			else if ((st==6) || (st==12.5))		WDcfg->HG_ShapingTime = 6;
			else 	Con_printf("LCSw", "WARNING: Shaping Time HG: invalid setting\n");
		}
		if (streq(str, "AnalogProbe") || streq(str, "AnalogProbe0") || streq(str, "AnalogProbe1")) {
			int ap = 0;
			if (streq(str, "AnalogProbe1")) ap = 1;
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "OFF"))			WDcfg->AnalogProbe[ap] = APROBE_OFF;
			else if	(streq(str1, "FAST"))			WDcfg->AnalogProbe[ap] = APROBE_FAST;
			else if	(streq(str1, "SLOW_LG"))		WDcfg->AnalogProbe[ap] = APROBE_SLOW_LG;
			else if	(streq(str1, "SLOW_HG"))		WDcfg->AnalogProbe[ap] = APROBE_SLOW_HG;
			else if	(streq(str1, "PREAMP_LG"))		WDcfg->AnalogProbe[ap] = APROBE_PREAMP_LG;
			else if	(streq(str1, "PREAMP_HG"))		WDcfg->AnalogProbe[ap] = APROBE_PREAMP_HG;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
			if (streq(str, "AnalogProbe")) WDcfg->AnalogProbe[1] = WDcfg->AnalogProbe[0];
		}
		if (streq(str, "DigitalProbe") || streq(str, "DigitalProbe0") || streq(str, "DigitalProbe1")) {
			int val;
			int dp = 0;
			if (streq(str, "DigitalProbe1")) dp = 1;
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "OFF"))			WDcfg->DigitalProbe[dp] = DPROBE_OFF;
			else if	(streq(str1, "PEAK_LG"))		WDcfg->DigitalProbe[dp] = DPROBE_PEAK_LG;
			else if	(streq(str1, "PEAK_HG"))		WDcfg->DigitalProbe[dp] = DPROBE_PEAK_HG;
			else if	(streq(str1, "HOLD"))			WDcfg->DigitalProbe[dp] = DPROBE_HOLD;
			else if	(streq(str1, "START_CONV"))		WDcfg->DigitalProbe[dp] = DPROBE_START_CONV;
			else if	(streq(str1, "DATA_COMMIT"))	WDcfg->DigitalProbe[dp] = DPROBE_DATA_COMMIT;
			else if	(streq(str1, "DATA_VALID"))		WDcfg->DigitalProbe[dp] = DPROBE_DATA_VALID;
			else if	(streq(str1, "CLK_1024"))		WDcfg->DigitalProbe[dp] = DPROBE_CLK_1024;
			else if	(streq(str1, "VAL_WINDOW"))		WDcfg->DigitalProbe[dp] = DPROBE_VAL_WINDOW;
			else if	(streq(str1, "T_OR"))			WDcfg->DigitalProbe[dp] = DPROBE_T_OR;
			else if	(streq(str1, "Q_OR"))			WDcfg->DigitalProbe[dp] = DPROBE_Q_OR;
			else if	(strstr(str1, "ACQCTRL") != NULL) {
				char *c = strchr(str1, '_');
				sscanf(c+1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80000000 | val;
			} else if	(strstr(str1, "CRIF") != NULL) {
				char *c = strchr(str1, '_');
				sscanf(c+1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80010000 | val;
			} else if	(strstr(str1, "DTBLD") != NULL) {
				char *c = strchr(str1, '_');
				sscanf(c+1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80020000 | val;
			} else if (strstr(str1, "TSTMP") != NULL) {
				char* c = strchr(str1, '_');
				sscanf(c + 1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80030000 | val;
			} else if (strstr(str1, "TDL") != NULL) {
				char* c = strchr(str1, '_');
				sscanf(c + 1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80040000 | val;
			} else if (strstr(str1, "PMP") != NULL) {
				char* c = strchr(str1, '_');
				sscanf(c + 1, "%d", &val);
				WDcfg->DigitalProbe[dp] = 0x80050000 | val;
			} else if	((str1[0]=='0') && (tolower(str1[1])=='x')) {
				sscanf(str1+2, "%x", &val);
				WDcfg->DigitalProbe[dp] = 0x80000000 | val;
			} 
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
			if (streq(str, "DigitalProbe")) WDcfg->DigitalProbe[1] = WDcfg->DigitalProbe[0];
		}
		if (streq(str, "CitirocCfgMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "FROM_FILE"))		WDcfg->CitirocCfgMode = CITIROC_CFG_FROM_FILE;
			else if	(streq(str1, "FROM_REGS"))		WDcfg->CitirocCfgMode = CITIROC_CFG_FROM_REGS;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "PeakDetectorMode")) {
			fscanf(f_ini, "%s", str1);
			if		(streq(str1, "PEAK_STRETCH"))	WDcfg->PeakDetectorMode = 0;
			else if	(streq(str1, "TRACK&HOLD"))		WDcfg->PeakDetectorMode = 1;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "FastShaperInput")) {
			fscanf(f_ini, "%s", str1);
			if		((streq(str1, "HG-PA") || streq(str1, "HG")))	WDcfg->FastShaperInput = FAST_SHAPER_INPUT_HGPA;
			else if	((streq(str1, "LG-PA") || streq(str1, "LG")))	WDcfg->FastShaperInput = FAST_SHAPER_INPUT_LGPA;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "HV_Adjust_Range")) {
			fscanf(f_ini, "%s", str1);
			if		((streq(str1, "2.5")      || streq(str1, "0")))		WDcfg->HV_Adjust_Range = 0;
			else if	((streq(str1, "4.5")      || streq(str1, "1")))		WDcfg->HV_Adjust_Range = 1;
			else if	(streq(str1, "DISABLED"))							WDcfg->HV_Adjust_Range = -1;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "MuxNSmean")) {
			fscanf(f_ini, "%s", str1);
			if		((streq(str1, "1")  || streq(str1, "0")))		WDcfg->MuxNSmean = 0;
			else if	((streq(str1, "4")  || streq(str1, "1")))		WDcfg->MuxNSmean = 1;
			else if	((streq(str1, "16") || streq(str1, "2")))		WDcfg->MuxNSmean = 2;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "GainSelect")) {
			fscanf(f_ini, "%s", str1);
			if		((streq(str1, "HIGH") || streq(str1, "HG")))	WDcfg->GainSelect = GAIN_SEL_HIGH;
			else if	((streq(str1, "LOW")  || streq(str1, "LG")))	WDcfg->GainSelect = GAIN_SEL_LOW;
			else if	(streq(str1, "AUTO"))							WDcfg->GainSelect = GAIN_SEL_AUTO;
			else if	(streq(str1, "BOTH"))							WDcfg->GainSelect = GAIN_SEL_BOTH;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "TempSensType")) {
			fscanf(f_ini, "%s", str1);
			if (streq(str1, "TMP37")) {
				WDcfg->TempSensCoeff[0] = 0;
				WDcfg->TempSensCoeff[1] = (float)50;
				WDcfg->TempSensCoeff[2] = 0;
			} else if (streq(str1, "LM94021_G11")) {
				WDcfg->TempSensCoeff[0] = (float)194.25;
				WDcfg->TempSensCoeff[1] = (float)-73.63;
				WDcfg->TempSensCoeff[2] = 0;
			} else if (streq(str1, "LM94021_G00")) {
				WDcfg->TempSensCoeff[0] = (float)188.12;
				WDcfg->TempSensCoeff[1] = (float)-181.62;
				WDcfg->TempSensCoeff[2] = 0;
			} else {
				float v0, v1, v2;
				if (sscanf(str1+1, "%f", &v0) == 1) {
					char strv[500];
					fgets(strv, 500, f_ini);
					sscanf(strv, "%f %f", &v1, &v2);
					WDcfg->TempSensCoeff[0] = v0;
					WDcfg->TempSensCoeff[1] = v1;
					WDcfg->TempSensCoeff[2] = v2;
				} else {
				 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
				}
			}
		}
		if (streq(str, "OF_OutFileUnit")) {
			fscanf(f_ini, "%s", str1);
			if (streq(str1, "LSB"))					WDcfg->OutFileUnit = 0;
			else if (streq(str1, "ns"))				WDcfg->OutFileUnit = 1;
			else 	Con_printf("LCSw", "WARNING: %s: invalid setting %s\n", str, str1);
		}
		if (streq(str, "SupprZeroCntListFile")) {
			fscanf(f_ini, "%s", str1);
			if (streq(str1, "DISABLED"))			WDcfg->SupprZeroCntListFile = 0;
			else if (streq(str1, "ENABLED"))		WDcfg->SupprZeroCntListFile = 1;
		}

		if (streq(str, "DataFilePath"))				GetDatapath(f_ini, WDcfg);
		if (streq(str, "EHistoNbin"))				WDcfg->EHistoNbin			= GetNbin(f_ini);
		if (streq(str, "ToAHistoNbin"))				WDcfg->ToAHistoNbin			= GetNbin(f_ini);
		if (streq(str, "ToARebin"))					WDcfg->ToARebin				= GetInt(f_ini);
		if (streq(str, "ToAHistoMin"))				WDcfg->ToAHistoMin			= GetTime(f_ini, "ns");
		if (streq(str, "ToTHistoNbin"))				WDcfg->ToTHistoNbin			= GetNbin(f_ini);
		if (streq(str, "MCSHistoNbin"))				WDcfg->MCSHistoNbin			= GetNbin(f_ini);
		if (streq(str, "JobFirstRun"))				WDcfg->JobFirstRun			= GetInt(f_ini);
		if (streq(str, "JobLastRun"))				WDcfg->JobLastRun			= GetInt(f_ini);
		if (streq(str, "RunSleep"))					WDcfg->RunSleep				= GetTime(f_ini, "s");
		if (streq(str, "EnableJobs"))				WDcfg->EnableJobs			= GetInt(f_ini);
		if (streq(str, "DebugLogMask"))				WDcfg->DebugLogMask			= GetHex(f_ini);
		if (streq(str, "EnLiveParamChange"))		WDcfg->EnLiveParamChange	= GetInt(f_ini);
		if (streq(str, "OutFileEnableMask"))		WDcfg->OutFileEnableMask	= GetHex(f_ini);
		if (streq(str, "EnableToT"))				WDcfg->EnableToT			= GetInt(f_ini);
		if (streq(str, "OF_RawBin"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_RAW_DATA_BIN, GetInt(f_ini));
		if (streq(str, "OF_RawAscii"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_RAW_DATA_ASCII, GetInt(f_ini));
		if (streq(str, "OF_ListBin"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_LIST_BIN, GetInt(f_ini));
		if (streq(str, "OF_ListAscii"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_LIST_ASCII, GetInt(f_ini));
		if (streq(str, "OF_Sync"))					WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_SYNC, GetInt(f_ini));
		if (streq(str, "OF_SpectHisto"))			WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_SPECT_HISTO, GetInt(f_ini));
		if (streq(str, "OF_ToAHisto"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_ToA_HISTO, GetInt(f_ini));
		if (streq(str, "OF_ToTHisto"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_TOT_HISTO, GetInt(f_ini));
		if (streq(str, "OF_Staircase"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_STAIRCASE, GetInt(f_ini));
		if (streq(str, "OF_RunInfo"))				WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_RUN_INFO, GetInt(f_ini));
		if (streq(str, "OF_MCS"))					WDcfg->OutFileEnableMask	= SETBIT(WDcfg->OutFileEnableMask, OUTFILE_MCS_HISTO, GetInt(f_ini));
		if (streq(str, "TstampCoincWindow"))		WDcfg->TstampCoincWindow	= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "PresetTime"))				WDcfg->PresetTime			= GetTime(f_ini, "s");
		if (streq(str, "PresetCounts"))				WDcfg->PresetCounts			= GetInt(f_ini);
		if (streq(str, "TrefWindow"))				WDcfg->TrefWindow			= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "TrefDelay"))				WDcfg->TrefDelay			= GetTime(f_ini, "ns");
		if (streq(str, "PtrgPeriod"))				WDcfg->PtrgPeriod			= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "Trg_HoldOff"))				WDcfg->Trg_HoldOff			= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "PairedCnt_CoincWin"))		WDcfg->PairedCnt_CoincWin	= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "TestPulseAmplitude"))		WDcfg->TestPulseAmplitude	= GetInt(f_ini);
		if (streq(str, "WaveformLength"))			WDcfg->WaveformLength		= GetInt(f_ini);
		if (streq(str, "Range_14bit"))				WDcfg->Range_14bit			= GetInt(f_ini);
		if (streq(str, "QD_CoarseThreshold"))		WDcfg->QD_CoarseThreshold	= GetInt(f_ini);
		if (streq(str, "Enable_HV_Adjust"))			WDcfg->Enable_HV_Adjust		= GetInt(f_ini);
		if (streq(str, "HoldDelay"))				WDcfg->HoldDelay			= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "EnableQdiscrLatch"))		WDcfg->EnableQdiscrLatch	= GetInt(f_ini);
		if (streq(str, "EnableChannelTrgout"))		WDcfg->EnableChannelTrgout	= GetInt(f_ini);
		if (streq(str, "MuxClkPeriod"))				WDcfg->MuxClkPeriod			= (uint32_t)GetTime(f_ini, "ns");
		if (streq(str, "Pedestal"))					WDcfg->Pedestal				= GetInt(f_ini);
		if (streq(str, "ProbeChannel0"))			WDcfg->ProbeChannel[0] = GetInt(f_ini);
		if (streq(str, "ProbeChannel1"))			WDcfg->ProbeChannel[1] = GetInt(f_ini);
		if (streq(str, "ProbeChannel")) {
			WDcfg->ProbeChannel[0] = GetInt(f_ini);
			WDcfg->ProbeChannel[1] = GetInt(f_ini);
		}
		if (streq(str, "MajorityLevel"))			WDcfg->MajorityLevel		= GetInt(f_ini);
		if (streq(str, "RunNumber_AutoIncr"))		WDcfg->RunNumber_AutoIncr	= GetInt(f_ini);
		if (streq(str, "EnableTempFeedback"))		WDcfg->EnableTempFeedback	= GetInt(f_ini);
		if (streq(str, "TempFeedbackCoeff"))		WDcfg->TempFeedbackCoeff	= GetFloat(f_ini);
		if (streq(str, "EnableServiceEvents"))      WDcfg->EnableServiceEvents	= GetInt(f_ini);
		if (streq(str, "EnableCntZeroSuppr"))		WDcfg->EnableCntZeroSuppr	= GetInt(f_ini);
		if (streq(str, "AskHVShutDownOnExit"))		WDcfg->AskHVShutDownOnExit  = GetInt(f_ini);

		if (streq(str, "ZS_Threshold_LG"))			SetChannelParam(WDcfg->ZS_Threshold_LG,				GetInt(f_ini));
		if (streq(str, "ZS_Threshold_HG"))			SetChannelParam(WDcfg->ZS_Threshold_HG,				GetInt(f_ini));
		if (streq(str, "QD_FineThreshold"))			SetChannelParam(WDcfg->QD_FineThreshold,			GetInt(f_ini));
		if (streq(str, "TD_FineThreshold"))			SetChannelParam(WDcfg->TD_FineThreshold,			GetInt(f_ini));
		if (streq(str, "HG_Gain"))					SetChannelParam(WDcfg->HG_Gain,						GetInt(f_ini));
		if (streq(str, "LG_Gain"))					SetChannelParam(WDcfg->LG_Gain,						GetInt(f_ini));
		if (streq(str, "HV_IndivAdj"))				SetChannelParam(WDcfg->HV_IndivAdj,					GetInt(f_ini));		
		if (streq(str, "TD_CoarseThreshold"))		SetBoardParam((int *)WDcfg->TD_CoarseThreshold,		GetInt(f_ini));	// for Romualdo
		if (streq(str, "ChEnableMask0"))			SetBoardParam((int *)WDcfg->ChEnableMask0,			GetHex(f_ini));
		if (streq(str, "ChEnableMask1"))			SetBoardParam((int *)WDcfg->ChEnableMask1,			GetHex(f_ini));
		if (streq(str, "Q_DiscrMask0"))				SetBoardParam((int *)WDcfg->Q_DiscrMask0,			GetHex(f_ini));
		if (streq(str, "Q_DiscrMask1"))				SetBoardParam((int *)WDcfg->Q_DiscrMask1,			GetHex(f_ini));
		if (streq(str, "Tlogic_Mask0"))				SetBoardParam((int *)WDcfg->Tlogic_Mask0,			GetHex(f_ini));
		if (streq(str, "Tlogic_Mask1"))				SetBoardParam((int *)WDcfg->Tlogic_Mask1,			GetHex(f_ini));
		if (streq(str, "HV_Vbias"))					SetBoardParamFloat(WDcfg->HV_Vbias,					GetVoltage(f_ini));
		if (streq(str, "HV_Imax"))					SetBoardParamFloat(WDcfg->HV_Imax,					GetCurrent(f_ini));  // Imax is expressed in mA
			
		if (streq(str, "Load"))					LoadExtCfgFile(f_ini, WDcfg);

		if (!ValidParameterName || !ValidParameterValue || !ValidUnits) {
			if (!ValidUnits && ValidParameterValue)
				Con_printf("LCSw", "WARNING: %s: unkwown units. Janus use as default V, mA, ns\n", str);
			else
				Con_printf("LCSw", "WARNING: %s: unkwown parameter\n", str);
			fgets(str, (int)strlen(str)-1, f_ini);
		}
	}

	if (WDcfg->EHistoNbin > (1 << ENERGY_NBIT))	WDcfg->EHistoNbin = (1 << ENERGY_NBIT);
	if (WDcfg->ToAHistoNbin > (1 << TOA_NBIT))	WDcfg->ToAHistoNbin = (1 << TOA_NBIT);	// DNIN: misleading. This is just for plot visualization
	if (WDcfg->ToTHistoNbin > (1 << TOT_NBIT))	WDcfg->ToTHistoNbin = (1 << TOT_NBIT);
	int ediv = 1;

	//if (WDcfg->EHistoNbin < 256)	WDcfg->EHistoNbin = 256;
	//if (WDcfg->ToAHistoNbin < 256)	WDcfg->ToAHistoNbin = 256;
	//if (WDcfg->ToTHistoNbin < 256)	WDcfg->ToTHistoNbin = 256;

	// Scale pedestal value to 8K
	if (WDcfg->EHistoNbin > 0) ediv = WDcfg->Range_14bit ? ((1 << 14) / WDcfg->EHistoNbin) : ((1 << 13) / WDcfg->EHistoNbin);
	WDcfg->Pedestal = WDcfg->Pedestal * ediv;

#ifdef linux
	if (WDcfg->DataFilePath[strlen(WDcfg->DataFilePath)-1] != '/')	sprintf(WDcfg->DataFilePath, "%s/", WDcfg->DataFilePath);
#else
	if (WDcfg->DataFilePath[strlen(WDcfg->DataFilePath)-1] != '\\')	sprintf(WDcfg->DataFilePath, "%s\\", WDcfg->DataFilePath);
#endif

	// if exists, load the extra settings contained in PostConfig.txt
	static int PostConfigDone = 0;
	if (!PostConfigDone) {
		FILE *pcfg = fopen("PostConfig.txt", "r");
		PostConfigDone = 1;
		if (pcfg != NULL) {
			Con_printf("LCSm", "Reading additional configuration file PostConfig.txt\n");
			ParseConfigFile(pcfg, WDcfg, 0);
			fclose(pcfg);
			PostConfigDone = 0;
		}
	}

	return 0;
}

