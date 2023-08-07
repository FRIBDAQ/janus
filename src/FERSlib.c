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

#ifdef _WIN32
#include "MultiPlatform.h"
#include <Windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>

#include "FERS_LL.h"
#include "FERSlib.h"

#ifndef _WIN32
#include "MultiPlatform.h"
#endif

#define EVBUFF_SIZE			(16*1024)

// *********************************************************************************************************
// Global Variables
// *********************************************************************************************************
int BoardConnected[FERSLIB_MAX_NBRD] = { 0 };			// Board connection status
int CncConnected[FERSLIB_MAX_NCNC] = { 0 };				// Concentrator connection status
char BoardPath[FERSLIB_MAX_NBRD][20];					// Path of the FE boards
char CncPath[FERSLIB_MAX_NCNC][20];						// Path of the concentrator
int CncOpenHandles[FERSLIB_MAX_NCNC] = { 0 };			// Number of handles currently open for the concentrator (slave boards or concentrator itself)
FERS_BoardInfo_t *FERS_BoardInfo[FERSLIB_MAX_NBRD];		// pointers to the board info structs 
int HVinit[FERSLIB_MAX_NBRD] = { 0 };					// HV init flags
uint16_t PedestalLG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH];	// LG Pedestals (calibrate PHA Mux Readout)
uint16_t PedestalHG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH];	// LG Pedestals (calibrate PHA Mux Readout)
uint16_t CommonPedestal = 100;							// Common pedestal added to all channels
int PedestalBackupPage[FERSLIB_MAX_NBRD] = { 0 };		// Enable pedestal backup page
int EnablePedCal= 1;									// 0 = disable calibration, 1 = enable calibration
int FERS_TotalAllocatedMem = 0;							// Total allocated memory 
int FERS_ReadoutStatus = 0;								// Status of the readout processes (idle, running, flushing, etc...)
int FERS_RunningCnt = 0;								// Num of running boards 
#ifdef _WIN32
mutex_t FERS_RoMutex = NULL;							// Mutex for the access to FERS_ReadoutStatus
#else
mutex_t FERS_RoMutex;									// Mutex for the access to FERS_ReadoutStatus
#endif
int DebugLogs = 0;										// Debug Logs


// *********************************************************************************************************
// Messaging and errors
// *********************************************************************************************************
int FERS_LibMsg(char *fmt, ...) 
{
	char msg[1000];
	static FILE *LibLog;
	static int openfile = 1;
	va_list args;

	if (openfile) {
		LibLog = fopen("FERSlib_Log.txt", "w");
		openfile = 0;
	}

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);

	//printf("%s", msg); // Write to console
	if (LibLog != NULL) {
		fprintf(LibLog, "%s", msg); // Write to Log File
		fflush(LibLog);
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Enable log messages and output files for debugging
// Inputs:		DebugEnableMask: enable mask:
//					DBLOG_FERSLIB_MSG			0x0001		// Enable FERSlib to print log messages to console
//					DBLOG_RAW_DATA_OUTFILE		0x0002		// Enable read data function to dump event data (32 bit words) into a text file
//					DBLOG_LL_DATADUMP			0x0004		// Enable low level lib to dump raw data (from usb, eth and tdl) into a text file
//					DBLOG_LL_MSGDUMP			0x0008		// Enable low level lib to log messages (from usb, eth and tdl) into a text file
//					DBLOG_QUEUES				0x0010		// Enable messages from queues (push and pop) used in event sorting
//					DBLOG_RAW_DECODE			0x0020		// Enable messages from raw data decoding
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_SetDebugLogs(int DebugEnableMask) {
	DebugLogs = DebugEnableMask;
	return 0;
}

// *********************************************************************************************************
// Open/Close
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Open a device (either FERS board or concentrator). Further operation might be performed
//              after the boatf has been opened, such as calibration, data flush, etc...
//              NOTE: when opening a FE board connected to a concentrator that has not been opened before,
//              the function automatically opens the concentrator first (without any notice to the caller)
// Inputs:		path = device path (e.g. eth:192.168.50.125:tdl:0:0)
// Outputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_OpenDevice(char *path, int *handle) 
{
	int BoardIndex, CncIndex, i, ret, ns;
	int cnc_handle=-1;
	char *s, *sep, ss[10][20], cpath[50];

	// split path into strings separated by ':'
	ns = 0;
	s = path;
	while(s < path+strlen(path)) {
		sep = strchr(s, ':');
		if (sep == NULL) break;
		strncpy(ss[ns], s, sep-s);
		ss[ns][sep-s] = 0;
		s += sep-s+1;
		ns++;
		if (ns == 9) break;
	}
	strcpy(ss[ns++], s);
	if (ns < 2) return FERSLIB_ERR_INVALID_PATH;

	if ((strstr(path, "cnc") != NULL) || (strstr(path, "tdl") != NULL)) {  // Connection through concentrator
		// Find cnc already opened with this path or find a free cnc index and open it
		sprintf(cpath, "%s:%s:cnc", ss[0], ss[1]);
		for(i=0; i < FERSLIB_MAX_NCNC; i++) {
			if (CncConnected[i]) {
				if (strcmp(cpath, CncPath[i]) == 0) {
					cnc_handle = FERS_CONNECTIONTYPE_CNC | i;
					break;
				}
			} else break;
		}
		if (cnc_handle == -1) {
			if (i == FERSLIB_MAX_NCNC) return FERSLIB_ERR_MAX_NBOARD_REACHED;
			CncIndex = i;
			if (strstr(path, "eth") != NULL) {  // Ethernet
				ret = LLtdl_OpenDevice(ss[1], CncIndex);
			} else if (strstr(path, "usb") != NULL) {  // Usb
				ret = LLtdl_OpenDevice(ss[1], CncIndex);
			}
			if (ret < 0) return FERSLIB_ERR_COMMUNICATION;
			CncConnected[CncIndex] = 1;
			strcpy(CncPath[CncIndex], cpath);
			cnc_handle = FERS_CONNECTIONTYPE_CNC | CncIndex;
		}
	}

	if ((strstr(path, "cnc") != NULL) && (ns == 3)) {  // Open Concentrator only
		if (cnc_handle == -1) return FERSLIB_ERR_INVALID_PATH;
		*handle = cnc_handle;
		CncOpenHandles[FERS_INDEX(cnc_handle)]++;
		return 0;

	} else {  // Open FE board

	 // Find free board index 
		for (i = 0; i < FERSLIB_MAX_NBRD; i++)
			if (!BoardConnected[i]) break;
		if (i == FERSLIB_MAX_NBRD) return FERSLIB_ERR_MAX_NBOARD_REACHED;
		BoardIndex = i;
		if ((strstr(path, "tdl") != NULL) && (ns == 5)) {  // Concentrator => TDlink => FE-board
			int chain, node, cindex = FERS_INDEX(cnc_handle);
			sscanf(ss[3], "%d", &chain);
			sscanf(ss[4], "%d", &node);
			if ((chain < 0) || (chain >= FERSLIB_MAX_NTDL) || (node < 0) || (node >= FERSLIB_MAX_NNODES)) return FERSLIB_ERR_INVALID_PATH;
			ret = LLtdl_InitTDLchains(cindex, NULL);
			if (ret < 0) return ret;
			if (node >= (int)TDL_NumNodes[cindex][chain]) return FERSLIB_ERR_INVALID_PATH;
			CncOpenHandles[cindex]++;
			*handle = (cindex << 30) | (chain << 24) | (node << 20) | FERS_CONNECTIONTYPE_TDL | BoardIndex;

		} else if (ns == 2) {  // Direct to FE-board
			if (strstr(path, "eth") != NULL) {  // Ethernet
				ret = LLeth_OpenDevice(ss[1], BoardIndex);
				if (ret < 0) return FERSLIB_ERR_COMMUNICATION;
				*handle = FERS_CONNECTIONTYPE_ETH | BoardIndex;
			}
			else if (strstr(path, "usb") != NULL) { // USB
				int pid;
				sscanf(ss[1], "%d", &pid);
				ret = LLusb_OpenDevice(pid, BoardIndex);
				if (ret < 0) return FERSLIB_ERR_COMMUNICATION;
				*handle = FERS_CONNECTIONTYPE_USB | BoardIndex;
			} else {
				return FERSLIB_ERR_INVALID_PATH;
			}
		} else {
			return FERSLIB_ERR_INVALID_PATH;
		}

		BoardConnected[BoardIndex] = 1;
		strcpy(BoardPath[BoardIndex], path);
		FERS_BoardInfo[BoardIndex] = (FERS_BoardInfo_t*)malloc(sizeof(FERS_BoardInfo_t));

		ret = FERS_ReadBoardInfo(*handle, FERS_BoardInfo[BoardIndex]);
		if ((ret != 0) && (ret != FERSLIB_ERR_INVALID_BIC)) return ret;

#ifdef FERS_5202
		// Read pedestal calibration
		uint16_t DCoffset[4];
		ret = FERS_ReadPedestalsFromFlash(*handle, NULL, PedestalLG[BoardIndex], PedestalHG[BoardIndex], DCoffset);
		for(i=0; i<4; i++) {  // 0=LG0, 1=HG0, 2=LG1, 3=HG1
			if ((DCoffset[i] > 0) && (DCoffset[i] < 4095))
				FERS_WriteRegister(*handle, a_dc_offset, (i << 14) | DCoffset[i]); 
			else 
				FERS_WriteRegister(*handle, a_dc_offset, (i << 14) | 2750);
			Sleep(1);
		}
#endif

		// Send a Stop command (in case the board is still running from a previous connection)
		FERS_SendCommand(*handle, CMD_ACQ_STOP);

	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Check if a device is already open
// Inputs:		path = device path
// Return:		0=not open, 1=open
// --------------------------------------------------------------------------------------------------------- 
int FERS_IsOpen(char *path) 
{
	int i;
	for(i=0; i<FERSLIB_MAX_NBRD; i++) 
		if (BoardConnected[i] && (strcmp(BoardPath[i], path) == 0)) return 1;
	for(i=0; i<FERSLIB_MAX_NCNC; i++) 
		if (CncConnected[i] && (strcmp(CncPath[i], path) == 0)) return 1;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Close device (either FERS board or concentrator)
// Inputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_CloseDevice(int handle) 
{
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_CNC) {
		CncOpenHandles[FERS_CNCINDEX(handle)]--;
		if (CncOpenHandles[FERS_CNCINDEX(handle)] == 0) {
			LLtdl_CloseDevice(FERS_CNCINDEX(handle));
			CncConnected[FERS_CNCINDEX(handle)] = 0;
		}
	} else {
		if ((handle < 0) || (FERS_INDEX(handle) >= FERSLIB_MAX_NBRD)) return FERSLIB_ERR_INVALID_HANDLE;
		if (BoardConnected[FERS_INDEX(handle)]) {
			if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH) {
				LLeth_CloseDevice(FERS_INDEX(handle));
			} else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB) {
				LLusb_CloseDevice(FERS_INDEX(handle));
			} else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
				CncOpenHandles[FERS_CNCINDEX(handle)]--;
				if (CncOpenHandles[FERS_CNCINDEX(handle)] == 0)
					LLtdl_CloseDevice(FERS_CNCINDEX(handle));
			}
		} else if ((FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) || (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_CNC)) {
			CncOpenHandles[FERS_INDEX(handle)]--;
			if (CncOpenHandles[FERS_INDEX(handle)] == 0)
				LLtdl_CloseDevice(FERS_INDEX(handle));
		}
		free(FERS_BoardInfo[FERS_INDEX(handle)]);  // free the board info struct
		BoardConnected[FERS_INDEX(handle)] = 0;
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Return the total size of the allocated buffers for the readout of all boards
// Return:		alloctaed memory (bytes)
// --------------------------------------------------------------------------------------------------------- 
int FERS_TotalAllocatedMemory() {
	return FERS_TotalAllocatedMem;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Restore the factory IP address (192.168.50.3) of the device. The board must be connected 
//              through the USB port
// Inputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Reset_IPaddress(int handle) 
{
	if ((handle < 0) || (FERS_INDEX(handle) >= FERSLIB_MAX_NBRD) || (FERS_CONNECTIONTYPE(handle) != FERS_CONNECTIONTYPE_USB)) return FERSLIB_ERR_INVALID_HANDLE;
	return (LLusb_Reset_IPaddress(FERS_INDEX(handle)));
}


// --------------------------------------------------------------------------------------------------------- 
// Description: find the path of the concentrator to which a device is connected
// Inputs:		dev_path = device path
// Outputs:		cnc_path = concentrator path
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Get_CncPath(char *dev_path, char *cnc_path) 
{
	char *cc = strstr(dev_path, "tdl");
	strcpy(cnc_path, "");
	if (cc == NULL) return -1;
	strncpy(cnc_path, dev_path, cc - dev_path);
	cnc_path[cc - dev_path] = 0;
	strcat(cnc_path, "cnc");
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Initialize the TDL chains 
// Inputs:		handle = concentrator handle
//				DelayAdjust = individual fiber delay adjust
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_InitTDLchains(int handle, float DelayAdjust[FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES]) 
{
	return LLtdl_InitTDLchains(FERS_CNCINDEX(handle), DelayAdjust);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Check if TDL chains are initialized
// Inputs:		handle = concentrator handle
// Return:		false = not init, true = init done
// --------------------------------------------------------------------------------------------------------- 
bool FERS_TDLchainsInitialized(int handle)
{
	return LLtdl_TDLchainsInitialized(FERS_CNCINDEX(handle));
}

// *********************************************************************************************************
// Register Read/Write, Send Commands
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the board
// Inputs:		handle = device handle
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_ReadRegister(int handle, uint32_t address, uint32_t *data) {
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH)
		return LLeth_ReadRegister(FERS_INDEX(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
		return LLusb_ReadRegister(FERS_INDEX(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL)
		return LLtdl_ReadRegister(FERS_CNCINDEX(handle), FERS_CHAIN(handle), FERS_NODE(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_CNC)
		return LLtdl_CncReadRegister(FERS_CNCINDEX(handle), address, data);
	else 
		return FERSLIB_ERR_INVALID_HANDLE;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the board
// Inputs:		handle = device handle
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WriteRegister(int handle, uint32_t address, uint32_t data) {
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH)
		return LLeth_WriteRegister(FERS_INDEX(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
		return LLusb_WriteRegister(FERS_INDEX(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL)
		return LLtdl_WriteRegister(FERS_CNCINDEX(handle), FERS_CHAIN(handle), FERS_NODE(handle), address, data);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_CNC)
		return LLtdl_CncWriteRegister(FERS_CNCINDEX(handle), address, data);
	else 
		return FERSLIB_ERR_INVALID_HANDLE;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Send a command to the board
// Inputs:		handle = device handle
//				cmd = command opcode
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_SendCommand(int handle, uint32_t cmd) {
	if (cmd == CMD_ACQ_START) FERS_ReadoutStatus = ROSTATUS_RUNNING;
	if (cmd == CMD_ACQ_STOP) FERS_ReadoutStatus = ROSTATUS_IDLE;
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL)
		return (LLtdl_SendCommand(FERS_CNCINDEX(handle), FERS_CHAIN(handle), FERS_NODE(handle), cmd, TDL_COMMAND_DELAY));
	else
		return (FERS_WriteRegister(handle, a_commands, cmd));
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Send a broadcast command to multiple boards connected to a concentrator
// Inputs:		handle = device handles of all boards that should receive the command
//				cmd = command opcode
//				delay = execution delay (0 for automatic). 
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_SendCommandBroadcast(int *handle, uint32_t cmd, uint32_t delay) {
	if (cmd == CMD_ACQ_START) FERS_ReadoutStatus = ROSTATUS_RUNNING;
	if (cmd == CMD_ACQ_STOP) FERS_ReadoutStatus = ROSTATUS_IDLE;
	if (FERS_CONNECTIONTYPE(*handle) == FERS_CONNECTIONTYPE_TDL) {
		if (delay == 0) delay = TDL_COMMAND_DELAY;  // CTIN: manage auto delay mode (the minimum depends on the num of boards in the TDL chain)
		return (LLtdl_SendCommandBroadcast(FERS_CNCINDEX(handle[0]), cmd, delay));  // CTIN: manage multiple concentrators
	}
	else return FERSLIB_ERR_NOT_APPLICABLE;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a slice of a register 
// Inputs:		handle = device handle
//				address = reg address 
//				start_bit = fisrt bit of the slice (included)
//				stop_bit = last bit of the slice (included)
//				data = slice data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WriteRegisterSlice(int handle, uint32_t address, uint32_t start_bit, uint32_t stop_bit, uint32_t data) {
	int ret=0;
	uint32_t i, reg, mask=0;

	ret |= FERS_ReadRegister(handle, address, &reg);
	for(i=start_bit; i<=stop_bit; i++)
		mask |= 1<<i;
	reg = (reg & ~mask) | ((data << start_bit) & mask);
	ret |= FERS_WriteRegister(handle, address, reg);   
	return ret;
}

// *********************************************************************************************************
// I2C Register R/W 
// *********************************************************************************************************
static void Wait_i2c_busy(int handle) 
{
	uint32_t stat;
	for (int i=0; i<50; i++) {
		FERS_ReadRegister(handle, a_acq_status, &stat); 
		if ((stat & (1 << 17)) == 0) break;
		Sleep(1);
	}
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of an I2C device (picoTDC, PLL, etc...)
// Inputs:		handle = device handle
//				i2c_dev_addr = I2V devive address (7 bit)
//				reg_addr = register address (in the device)
//				reg_data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_I2C_WriteRegister(int handle, uint32_t i2c_dev_addr, uint32_t reg_addr, uint32_t reg_data) {
	int ret=0;
	if ((i2c_dev_addr == I2C_ADDR_PLL0) || (i2c_dev_addr == I2C_ADDR_PLL1) || (i2c_dev_addr == I2C_ADDR_PLL2)) {
		ret |= FERS_WriteRegister(handle, a_i2c_data, (reg_addr >> 8) & 0xFF);
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 16 | 0x01);
		Wait_i2c_busy(handle);
		ret |= FERS_WriteRegister(handle, a_i2c_data, reg_data);
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 16 | (reg_addr & 0xFF));
	} else {
		ret |= FERS_WriteRegister(handle, a_i2c_data, reg_data);
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 17 | reg_addr);
	}
	Wait_i2c_busy(handle);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of an I2C device (picoTDC, PLL, etc...)
// Inputs:		handle = device handle
//				i2c_dev_addr = I2V devive address (7 bit)
//				reg_addr = register address (in the device)
// Outputs:		reg_data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_I2C_ReadRegister(int handle, uint32_t i2c_dev_addr, uint32_t reg_addr, uint32_t *reg_data) {
	int ret=0;
	if ((i2c_dev_addr == I2C_ADDR_PLL0) || (i2c_dev_addr == I2C_ADDR_PLL1) || (i2c_dev_addr == I2C_ADDR_PLL2)) {
		ret |= FERS_WriteRegister(handle, a_i2c_data, (reg_addr >> 8) & 0xFF);
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 17 | 0x01);
		Wait_i2c_busy(handle);
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 17 | 0x10000 | (reg_addr & 0xFF));
	} else {
		ret |= FERS_WriteRegister(handle, a_i2c_addr, i2c_dev_addr << 17 | 0x10000 | reg_addr);
	}
	Wait_i2c_busy(handle);
	ret |= FERS_ReadRegister(handle, a_i2c_data, reg_data);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a slice of a register of an I2C device
// Inputs:		handle = device handle
//				i2c_dev_addr = I2V devive address (7 bit) 
//				address = reg address 
//				start_bit = fisrt bit of the slice (included)
//				stop_bit = last bit of the slice (included)
//				data = slice data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_I2C_ReadRegisterSlice(int handle, uint32_t i2c_dev_addr, uint32_t address, uint32_t start_bit, uint32_t stop_bit, uint32_t data) {
	int ret=0;
	uint32_t i, reg, mask=0;

	ret |= FERS_I2C_ReadRegister(handle, i2c_dev_addr, address, &reg);
	for(i=start_bit; i<=stop_bit; i++)
		mask |= 1<<i;
	reg = (reg & ~mask) | ((data << start_bit) & mask);
	ret |= FERS_I2C_WriteRegister(handle, i2c_dev_addr, address, reg);   
	return ret;
}


// *********************************************************************************************************
// Flash Mem Read/Write (AT54DB321)
// *********************************************************************************************************
// Parameters for the access to the Flash Memory
#define MAIN_MEM_PAGE_READ_CMD          0xD2
#define MAIN_MEM_PAGE_PROG_TH_BUF1_CMD  0x82
#define STATUS_READ_CMD                 0x57
#define FLASH_KEEP_ENABLED				0x100 // Keep Chip Select active during SPI operation

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a page of the flash memory 
// Inputs:		handle = board handle 
//				page = page number
// Outputs		data = page data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_ReadFlashPage(int handle, int pagenum, int size, uint8_t *data)
{
	int i, ret = 0, nb;
	uint32_t rdata, page_addr;

	if ((size == 0) || (size > FLASH_PAGE_SIZE)) nb = FLASH_PAGE_SIZE;
	else nb = size;
	page_addr = (pagenum & 0x1FFF) << 10;

	// Enable flash and write Opcode
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | MAIN_MEM_PAGE_READ_CMD);
	// Write Page address
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | ((page_addr >> 16) & 0xFF));
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | ((page_addr >> 8)  & 0xFF));
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | (page_addr         & 0xFF));
	// additional don't care bytes
	for (i = 0; i < 4; i++) {
		ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED);
	}
	if (ret) return ret;
	Sleep(10);
	// Read Data (full page with the relevant size for that flash)
	for (i = 0; i < nb; i++) {
		if (i == (nb-1))
			ret |= FERS_WriteRegister(handle, a_spi_data, 0);
		else
			ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED);
		ret |= FERS_ReadRegister(handle, a_spi_data, &rdata);
		data[i] = (uint8_t)rdata;
		if (ret) return ret;
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Write a page of the flash memory. 
//              WARNING: the flash memory contains vital parameters for the board. Overwriting certain pages
//                       can damage the hardware!!! Do not use this function without contacting CAEN first
// Inputs:		handle = board handle 
//				page: page number
//				data = data to write
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WriteFlashPage(int handle, int pagenum, int size, uint8_t *data)
{
	int i, ret = 0, nb;
	uint32_t stat, page_addr;

	if ((size == 0) || (size > FLASH_PAGE_SIZE)) nb = FLASH_PAGE_SIZE;
	else nb = size;
	page_addr = (pagenum & 0x1FFF) << 10;
	// Enable flash and write Opcode
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | MAIN_MEM_PAGE_PROG_TH_BUF1_CMD);
	// Page address
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | ((page_addr >> 16) & 0xFF));
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | ((page_addr >> 8)  & 0xFF));
	ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | ( page_addr        & 0xFF));
	if (ret)
		return ret;
	// Write Data
	for (i = 0; i < nb; i++) {
		if (i == (nb-1))
			ret |= FERS_WriteRegister(handle, a_spi_data, (uint32_t)data[i]);
		else
			ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | (uint32_t)data[i]);
		if (ret)
			return ret;
	}

	// wait for Tep (Time of erase and programming)
	do {
		ret |= FERS_WriteRegister(handle, a_spi_data, FLASH_KEEP_ENABLED | STATUS_READ_CMD); // Status read Command
		ret |= FERS_WriteRegister(handle, a_spi_data, 0);
		ret |= FERS_ReadRegister(handle, a_spi_data, &stat); // Status read
		if (ret)
			return ret;
	} while (!(stat & 0x80));
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write Board info into the relevant flash memory page
//              WARNING: the flash memory contains vital parameters for the board. Overwriting certain pages
//                       can damage the hardware!!! Do not use this function without contacting CAEN first
// Inputs:		handle = board handle 
//				binfo = board info struct
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WriteBoardInfo(int handle, FERS_BoardInfo_t binfo)
{
	int ret;
	const char BICversion = 0;
	const int PedestalCalibPage = FLASH_PEDCALIB_PAGE;
	uint8_t bic[FLASH_PAGE_SIZE];

	memset(bic, 0, FLASH_PAGE_SIZE);
	bic[0] = 'B';								// identifier			(1)
	bic[1] = BICversion;						// BIC version			(1) 
	memcpy(bic + 2,  &binfo.pid, 4);			// PID					(4)
	memcpy(bic + 6,  &binfo.FERSCode, 2);		// e.g. 5202			(2)
	memcpy(bic + 8,  &binfo.PCBrevision, 1);	// PCB_Rev				(1)
	memcpy(bic + 9,   binfo.ModelCode, 16);		// e.g. WA5202XAAAAA	(16)
	memcpy(bic + 25,  binfo.ModelName, 16);		// e.g. A5202			(16)
	memcpy(bic + 41, &binfo.FormFactor, 1);		// 0=naked, 1=boxed		(1)
	memcpy(bic + 42, &binfo.NumCh, 2);			// Num Channels			(2)
	memcpy(bic + 44, &PedestalCalibPage, 2);	// Pedestal Calib page	(2)
	ret = FERS_WriteFlashPage(handle, FLASH_BIC_PAGE, 46, bic);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read Board info from the relevant flash memory page of the FERS unit
// Inputs:		handle = board handle 
// Outputs:		binfo = board info struct
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_ReadBoardInfo(int handle, FERS_BoardInfo_t *binfo)
{
	int ret;
	uint8_t bic[FLASH_PAGE_SIZE];

	memset(binfo, 0, sizeof(FERS_BoardInfo_t));
	for (int i = 0; i < 10; i++) {
		ret = FERS_ReadFlashPage(handle, FLASH_BIC_PAGE, 46, bic);
		if (ret < 0) return ret;
		if ((bic[0] != 'B') || (bic[1] != 0)) {
			if (i == 3)	return FERSLIB_ERR_INVALID_BIC;
		}
		else break;
	}
	memcpy(&binfo->pid,			bic + 2, 4);
	memcpy(&binfo->FERSCode,	bic + 6, 2);
	memcpy(&binfo->PCBrevision,	bic + 8, 1);
	memcpy( binfo->ModelCode,	bic + 9, 16);
	memcpy( binfo->ModelName,	bic + 25, 16);
	memcpy(&binfo->FormFactor,	bic + 41, 1);
	memcpy(&binfo->NumCh,		bic + 42, 2);
	//memcpy(&PedestalCalibPage,	bic + 44, 2);
	FERS_ReadRegister(handle, a_fw_rev, &binfo->FPGA_FWrev);
	FERS_ReadRegister(handle, 0xF0000000, &binfo->uC_FWrev);  
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Write Concentrator info 
// Inputs:		handle = cnc handle 
// 				cinfo = concentrator info struct
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WriteConcentratorInfo(int handle, FERS_CncInfo_t cinfo)
{
	// CTIN: to do....
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read Concentrator info 
// Inputs:		handle = cnc handle 
// Outputs:		cinfo = concentrator info struct
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_ReadConcentratorInfo(int handle, FERS_CncInfo_t *cinfo)
{
	int ret, i;
	ret = LLtdl_GetCncInfo(FERS_CNCINDEX(handle), cinfo);
	if (ret) return ret;
	// CTIN: the following info are hard-coded for the moment. Will be read from the concentrator...
	strcpy(cinfo->ModelCode, "WDT5215XAAAA");
	strcpy(cinfo->ModelName, "DT5215");
	cinfo->PCBrevision = 1;
	cinfo->NumLink = 8;
	for(i=0; i<8; i++) {
		ret = LLtdl_GetChainInfo(FERS_CNCINDEX(handle), i, &cinfo->ChainInfo[i]);
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get FPGA Die Temperature
// Inputs:		handle = board handle 
// Outputs:     temp = FPGA temperature (Celsius)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Get_FPGA_Temp(int handle, float *temp) {
	uint32_t data;
	int ret;
	for (int i = 0; i < 5; i++) {
		ret = FERS_ReadRegister(handle, a_fpga_temp, &data);
		*temp = (float)(((data * 503.975) / 4096) - 273.15);
		if ((*temp > 0) && (*temp < 125)) break;
	}
	return ret;
}


#ifdef FERS_5203
// --------------------------------------------------------------------------------------------------------- 
// Description: Get board Temperature (between PIC and FPGA)
// Inputs:		handle = board handle 
// Outputs:     temp = PIC board temperature (Celsius)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Get_Board_Temp(int handle, float* temp) {
	uint32_t data;
	int ret;
	for (int i = 0; i < 5; i++) {
		ret = FERS_ReadRegister(handle, a_board_temp, &data);
		*temp = (float)(data / 4.);
		if ((*temp > 0) && (*temp < 125)) break;
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get TDC0 Temperature
// Inputs:		handle = board handle 
// Outputs:     temp = TDC temperature (Celsius)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Get_TDC0_Temp(int handle, float* temp) {
	uint32_t data;
	int ret;
	for (int i = 0; i < 5; i++) {
		ret = FERS_ReadRegister(handle, a_tdc0_temp, &data);
		*temp = (float)(data / 4.);
		if ((*temp > 0) && (*temp < 125)) break;
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get TDC1 Temperature
// Inputs:		handle = board handle 
// Outputs:     temp = TDC temperature (Celsius)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_Get_TDC1_Temp(int handle, float* temp) {
	uint32_t data;
	int ret;
	for (int i = 0; i < 5; i++) {
		ret = FERS_ReadRegister(handle, a_tdc1_temp, &data);
		*temp = (float)(data / 4.);
		if ((*temp > 0) && (*temp < 125)) break;
	}
	return ret;
}
#endif

#ifdef FERS_5202
// --------------------------------------------------------------------------------------------------------- 
// Description: Write Pedestal calibration
//              WARNING: the flash memory contains vital parameters for the board. Overwriting certain pages
//                       can damage the hardware!!! Do not use this function without contacting CAEN first
// Inputs:		handle = board handle 
//				PedLG, PedHG = pedestals (64+64 values)
//				dco = DCossfet (use NULL pointer to keep old values)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_WritePedestals(int handle, uint16_t *PedLG, uint16_t *PedHG, uint16_t *dco)
{
	int ret, sz = 64*sizeof(uint16_t);
	uint8_t ped[FLASH_PAGE_SIZE];
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	int pedpage = PedestalBackupPage[FERS_INDEX(handle)] ? FLASH_PEDCALIB_BCK_PAGE : FLASH_PEDCALIB_PAGE;

	ret = FERS_ReadFlashPage(handle, pedpage, 16 + sz*2, ped);
	if (ret < 0) return ret;
	ped[0] = 'P';	// Tag
	ped[1] = 0;		// Format
	*(uint16_t *)(ped+2) = (uint16_t)(tm.tm_year + 1900);
	ped[4] = tm.tm_mon + 1;
	ped[5] = tm.tm_mday;
	if (dco != NULL) memcpy(ped+6, dco, 4 * sizeof(uint16_t));  // 0=LG0, 1=HG0, 2=LG1, 3=HG1
	// Note: 10 to 15 are unused bytes (spares). Calib data start from 16
	memcpy(ped+16, PedLG, sz);
	memcpy(ped+16+sz, PedHG, sz);
	ret = FERS_WriteFlashPage(handle, pedpage, 16 + sz*2, ped);

	// Update local pedestals and DC offset (used in the library)
	memcpy(PedestalLG, PedLG, sz);
	memcpy(PedestalHG, PedHG, sz);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read Pedestal calibration and DC offset
//              WARNING: the flash memory contains vital parameters for the board. Overwriting certain pages
//                       can damage the hardware!!! Do not use this function without contacting CAEN first
// Inputs:		handle = board handle 
// Outputs:		date = calibration date	(DD/MM/YYYY)
//				PedLG, PedHG = pedestals (64+64 values)
//				dco = DCoffset (DAC). 4 values. Use NULL pointer if not requested
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_ReadPedestalsFromFlash(int handle, char *date, uint16_t *PedLG, uint16_t *PedHG, uint16_t *dco)
{
	int ret, sz = 64*sizeof(uint16_t);
	int year, month, day;
	uint8_t ped[FLASH_PAGE_SIZE];
	int pedpage = PedestalBackupPage[FERS_INDEX(handle)] ? FLASH_PEDCALIB_BCK_PAGE : FLASH_PEDCALIB_PAGE;

	if (date != NULL) strcpy(date, "");
	ret = FERS_ReadFlashPage(handle, pedpage, 16 + sz*2, ped);
	if ((ped[0] != 'P') || (ped[1] != 0)) {
		EnablePedCal= 0;
		return FERSLIB_ERR_PEDCALIB_NOT_FOUND;
	}
	year = *(uint16_t *)(ped+2);
	month = ped[4];
	day = ped[5];
	if (date != NULL) sprintf(date, "%02d/%02d/%04d", day, month, year);
	if (dco != NULL) memcpy(dco, ped+6, 4 * sizeof(uint16_t));
	memcpy(PedLG, ped+16, sz);
	memcpy(PedHG, ped+16+sz, sz);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Switch to pedesatl backup page
// Inputs:		handle = board handle 
//				EnBckPage: 0=normal page, 1=packup page
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_PedestalBackupPage(int handle, int EnBckPage)
{
	PedestalBackupPage[FERS_INDEX(handle)] = EnBckPage;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Set a common pedestal (applied to all channels after pedestal calibration)
//				WARNING: this function enables pedestal calibration
// Inputs:		handle = board handle 
//				Ped = fixed pedestals 
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_SetCommonPedestal(int handle, uint16_t Pedestal)
{
	CommonPedestal = Pedestal;
	EnablePedCal = 1;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Enable/Disable pedestal calibration
// Inputs:		handle = board handle 
//				enable = 0: disabled, 1=enabled
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_EnablePedestalCalibration(int handle, int enable)
{
	EnablePedCal = enable;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get Channel pedestal
// Inputs:		handle = board handle 
//				enable = 0: disabled, 1=enabled
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_GetChannelPedestalBeforeCalib(int handle, int ch, uint16_t *PedLG, uint16_t *PedHG)
{
	if ((ch < 0) || (ch > 63)) return FERSLIB_ERR_INVLID_PARAM;
	*PedLG = PedestalLG[FERS_INDEX(handle)][ch];
	*PedHG = PedestalHG[FERS_INDEX(handle)][ch];
	return 0;
}

// *********************************************************************************************************
// High Voltage Control
// *********************************************************************************************************

// --------------------------------------------------------------------------------------------------------- 
// Description: Initialize the HV communication bus (I2C)
// Inputs:		handle = device handle
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Init(int handle) 
{
	int ret=0;
	// Set Digital Control
	ret |= FERS_WriteRegister(handle, a_hv_regaddr, 0x02001);  
	Wait_i2c_busy(handle);
	ret |= FERS_WriteRegister(handle, a_hv_regdata, 0);  
	Wait_i2c_busy(handle);
	HVinit[FERS_INDEX(handle)] = 1;

	// Set PID = 1 (for more precision)
	HV_WriteReg(handle, 30, 2, 1);

	// Set Ramp Speed = 10 V/s
	//HV_WriteReg(handle, 3, 1, 50000);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the HV 
// Inputs:		handle = device handle
//				reg_addr = register address
//				dtype = data type (0=signed int, 1=fixed point (Val*10000), 2=unsigned int, 3=float)
//				reg_data = register data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_WriteReg(int handle, uint32_t reg_addr, uint32_t dtype, uint32_t reg_data) 
{
	int ret=0;
	if (!HVinit[FERS_INDEX(handle)]) HV_Init(handle);
	ret |= FERS_WriteRegister(handle, a_hv_regaddr, 0x00000 | (dtype << 8) | reg_addr);  
	Wait_i2c_busy(handle);
	ret |= FERS_WriteRegister(handle, a_hv_regdata, reg_data);  
	Wait_i2c_busy(handle);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the HV 
// Inputs:		handle = device handle
//				reg_addr = register address
//				dtype = data type (0=signed int, 1=fixed point (Val*10000), 2=unsigned int, 3=float)
// Outputs:		reg_data = register data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_ReadReg(int handle, uint32_t reg_addr, uint32_t dtype, uint32_t *reg_data) 
{
	int ret=0;
	if (!HVinit[FERS_INDEX(handle)]) HV_Init(handle);
	for(int i=0; i<5; i++) {
		ret |= FERS_WriteRegister(handle, a_hv_regaddr, 0x10000 | (dtype << 8) | reg_addr);  
		Wait_i2c_busy(handle);
		ret |= FERS_ReadRegister(handle, a_hv_regdata, reg_data);
		Wait_i2c_busy(handle);
		if (*reg_data != 0xFFFFFFFF) break;  // sometimes, the I2C access fails and returns 0xFFFFFFFF. In this case, make another read
	}
	return ret;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Switch HV output on or off
// Inputs:		handle = device handle
//				OnOff = 0:OFF, 1:ON
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Set_OnOff(int handle, int OnOff) 
{
	return HV_WriteReg(handle, 0, 2, OnOff);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get HV Status
// Inputs:		handle = device handle
// Outputs:		OnOff = 0:OFF, 1:ON
//				Ramping = HV is rumping up
//				OvC = over current
//				OvV = over voltage
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_Status(int handle, int *OnOff, int *Ramping, int *OvC, int *OvV) 
{
	int i, ret = 0;
	uint32_t d32;
	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		ret = FERS_ReadRegister(handle, a_hv_status, &d32);
		*OnOff =   (d32 >> 26) & 0x1;
		*Ramping = (d32 >> 27) & 0x1;
		*OvC     = (d32 >> 28) & 0x1;
		*OvV     = (d32 >> 29) & 0x1;
	} else {
		for (i=0; i<5; i++) {
			ret |= HV_ReadReg(handle, 0, 2, (uint32_t *)OnOff);
			if ((*OnOff == 0) || (*OnOff == 1)) break;
		}
		ret |= HV_ReadReg(handle, 32, 2, (uint32_t *)Ramping);
		ret |= HV_ReadReg(handle, 250, 2, (uint32_t *)OvC);
		ret |= HV_ReadReg(handle, 249, 2, (uint32_t *)OvV);
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get HV Serial Number
// Inputs:		handle = device handle
// Outputs:		sernum = serial number
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_SerNum(int handle, int *sernum)
{
	int ret = 0;
	ret |= HV_ReadReg(handle, 254, 2, (uint32_t*)sernum);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Set HV output voltage (bias)
// Inputs:		handle = device handle
//				vbias = output voltage in Volts
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Set_Vbias(int handle, float vbias) 
{
	return HV_WriteReg(handle, 2, 1, (uint32_t)(vbias * 10000));
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get HV output voltage setting
// Inputs:		handle = device handle
// Outputs:		vbias = output voltage setting in Volts (read from register)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_Vbias(int handle, float *vbias) 
{
	uint32_t d32;
	int ret;
	ret = HV_ReadReg(handle, 2, 1, &d32);
	*vbias = (float)d32 / 10000;
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get HV output voltage (HV read back with internal ADC)
// Inputs:		handle = device handle
// Outputs:		vmon = output voltage in Volts (read-back)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_Vmon(int handle, float *vmon) 
{
	uint32_t d32;
	int ret = 0;
	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		ret = FERS_ReadRegister(handle, a_hv_Vmon, &d32);
	} else {
		ret = HV_ReadReg(handle, 231, 1, &d32);  
	}
	*vmon = (float)d32 / 10000;
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Set maximum output current 
// Inputs:		handle = device handle
//				imax = max output current in mA
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Set_Imax(int handle, float imax) 
{
	return HV_WriteReg(handle, 5, 1, (uint32_t)(imax * 10000));
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get maximum output current setting
// Inputs:		handle = device handle
// Outputs:		imax = max output current in mA (read from register)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_Imax(int handle, float *imax) 
{
	uint32_t d32;
	int ret = 0;
	ret = HV_ReadReg(handle, 5, 1, &d32);
	*imax = (float)d32 / 10000;
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get output current flowing into the detector
// Inputs:		handle = device handle
// Outputs:		imon = detector current
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_Imon(int handle, float *imon) 
{
	uint32_t d32;
	int ret = 0;
	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		ret = FERS_ReadRegister(handle, a_hv_Imon, &d32);
	} else {
		ret = HV_ReadReg(handle, 232, 1, &d32);
	}
	*imon = (d32>>31) ? 0 : (float)d32 / 10000;
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get internal temperature of the HV module
// Inputs:		handle = device handle
// Outputs:		temp = temperature (degC)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_IntTemp(int handle, float *temp) 
{
	uint32_t d32;
	int ret = 0;
	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		ret = FERS_ReadRegister(handle, a_hv_status, &d32);
		*temp = (float)((d32 >> 13) & 0x1FFF) * 256 / 10000;
	} else {
		ret = HV_ReadReg(handle, 228, 1, &d32);
		*temp = (float)(d32 & 0x1FFFFF)/10000;
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get external temperature (of the detector). Temp sensor must be connected to dedicated lines
// Inputs:		handle = device handle
// Outputs:		temp = detector temperature (degC)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Get_DetectorTemp(int handle, float *temp) 
{
	uint32_t d32;
	int ret = 0;
	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		ret = FERS_ReadRegister(handle, a_hv_status, &d32);
		*temp = (float)(d32 & 0x1FFF) * 256 / 10000;
	} else {
		ret = HV_ReadReg(handle, 234, 1, &d32);
		*temp = (float)(d32 & 0x1FFFFF)/10000;
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Set coefficients for external temperature sensor. T = V*V*c2 + V*c1 + c0
// Inputs:		Tsens_coeff = coefficients (0=offset, 1=linear, 2=quadratic)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Set_Tsens_Coeff(int handle, float Tsens_coeff[3]) 
{
	int ret = 0;
	for(int i=0; i<2; i++) {
		ret |= HV_WriteReg(handle, 7, 1, (uint32_t)(Tsens_coeff[2] * 10000));
		ret |= HV_WriteReg(handle, 8, 1, (uint32_t)(Tsens_coeff[1] * 10000));
		ret |= HV_WriteReg(handle, 9, 1, (uint32_t)(Tsens_coeff[0] * 10000));
	}
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Set coefficients for Vbias temperature feedback 
// Inputs:		Tsens_coeff = coefficient (mV/C)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int HV_Set_TempFeedback(int handle, int enable, float Tsens_coeff)
{
	int ret = 0;
	for(int i=0; i<2; i++) {
		ret |= HV_WriteReg(handle, 28, 1, (uint32_t)(-Tsens_coeff * 10000));
		if (enable) ret |= HV_WriteReg(handle, 1, 0, 2);
		else ret |= HV_WriteReg(handle, 1, 0, 0);
	}
	return ret;
}


// *********************************************************************************************************
// TDC Config and readout (for test)
// *********************************************************************************************************
uint32_t calib_periods = 10; 
// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the TDC
// Inputs:		handle = device handle
//				addr = register address
//				data = register data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int TDC_WriteReg(int handle, int tdc_id, uint32_t addr, uint32_t data) 
{
	if (addr <= 0x09)
		return FERS_WriteRegister(handle, a_tdc_data, ((tdc_id & 1) << 24) | 0x4000 | ((addr & 0xFF) << 8) | (data & 0xFF));
	else 
		return FERSLIB_ERR_INVLID_PARAM;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the TDC
// Inputs:		handle = device handle
//				addr = register address
// Outputs:		data = register data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int TDC_ReadReg(int handle, int tdc_id, uint32_t addr, uint32_t *data) 
{
	int ret = 0;
	int size_bit = (addr <= 0x09) ? 0 : 1;  // 0 = 8 bits, 1 = 24 bits
	FERS_WriteRegister(handle, a_tdc_data, (size_bit << 25) | ((tdc_id & 1) << 24) | ((addr & 0xFF) << 8));
	ret |= FERS_ReadRegister(handle, a_tdc_data, data);
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Configure TDC
// Inputs:		handle = device handle
//				tdc_id = TDC index (0 or 1)
//				StartSrc = start source (0=bunch_trg, 1=t0_in, 2=t1_in, 3=T-OR, 4=ptrg)
//				StopSrc = stop source (0=bunch_trg, 1=t0_in, 2=t1_in, 3=T-OR, 4=delayed ptrg)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int TDC_Config(int handle, int tdc_id, int StartSrc, int StopSrc) 
{
	int ret = 0;
	int meas_mode = 0;		// 0=Mode1, 1=Mode2
	int startEdge = 0;		// 0=rising, 1=falling
	int stopEdge = 0;		// 0=rising, 1=falling
	int num_stops = 1;		// num of stops (1 to 5)
	int cal_mode = 1;		// 0=2 periods, 1=10 periods, 2=20 periods, 3=40 periods

	FERS_WriteRegister(handle, a_tdc_mode, ((StopSrc << 4) | StartSrc) << tdc_id * 8);
	calib_periods = (cal_mode == 0) ? 2 : cal_mode * 10;
	ret |= TDC_WriteReg(handle, tdc_id, 0x00, (stopEdge << 4) | (startEdge << 3) | (meas_mode << 1)); // Config1
	ret |= TDC_WriteReg(handle, tdc_id, 0x01, (cal_mode << 6) | num_stops); // Config2
	return ret;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Make a start-stop measurements
// Inputs:		handle = device handle
//				tdc_id = TDC index (0 or 1)
// Outputs:		tof_ns = measured start-stop time (ns)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int TDC_DoStartStopMeasurement(int handle, int tdc_id, double *tof_ns) 
{
	int ret = 0, i;
	double calCount, ClockPeriod = 125;  // 8 MHz, T = 125 ns
	uint32_t calib1, calib2, tmeas, int_status;

	ret |= TDC_WriteReg(handle, tdc_id, 0x00, 0x01); // start measurement (mode = 0)
	for (i=0; i<10; i++) {
		ret |= TDC_ReadReg(handle, tdc_id, 0x02, &int_status); 
		if (int_status & 0x1) break;
	} 
	if (ret) return ret;
	if (!(int_status & 0x1)) {
		*tof_ns = 0;
		return 0;
	}
	ret |= TDC_WriteReg(handle, tdc_id, 0x02, 0x01); // clear interrupt
	ret |= TDC_ReadReg(handle, tdc_id, 0x1B, &calib1);  
	ret |= TDC_ReadReg(handle, tdc_id, 0x1C, &calib2);  
	ret |= TDC_ReadReg(handle, tdc_id, 0x10, &tmeas);  
	calCount = (double)(calib2 - calib1) / (calib_periods - 1);
	*tof_ns = (double)tmeas * ClockPeriod / calCount;
	return ret;
}
#endif

// *********************************************************
// Firmware Upgrade
// *********************************************************

static int waitFlashfree(int handle)
{
	uint32_t reg_add;
	uint32_t status;
	if (FERS_CONNECTIONTYPE(handle) != FERS_CONNECTIONTYPE_TDL)
		reg_add = 8189;
	else
		reg_add = FUP_BA + FUP_RESULT_REG;
	do {
		//Inserire qui un timeout a 5 secondi
		FERS_ReadRegister(handle, reg_add, &status);
	} while (status != 1);
	return 0;
}

uint32_t crc32c(uint32_t crc, const unsigned char* buf, size_t len)
{
	int k;

	crc = ~crc;
	while (len--) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
	}
	return ~crc;
}

static int WriteFPGAFirmwareOnFlash(int handle, char *pageText, int textLen, void(*ptr)(char *msg, int progress))
{
	uint32_t temp;
	uint32_t* datarow;  // [8192] ;
	size_t msize;
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL)
		msize = 1024;
	else
		msize = 8192;
	datarow = (uint32_t*)calloc(msize, sizeof(uint32_t));

	uint32_t *i_pageText;
	const uint32_t chunk_size_byte = 60 * 512 ;
	const uint32_t n_chunks = (uint32_t)ceil((double)textLen / chunk_size_byte);
	uint32_t crc_R, crc_T;

	for (int cnk = 0; cnk < (int)n_chunks; cnk++) {
		i_pageText = (uint32_t*)&(pageText[cnk*chunk_size_byte]);
		for (int i = 0; i < 8192; i++)
			datarow[i] = 0;

		for (int i = 0; i < (int)(chunk_size_byte/4); i++)
			datarow[i] = i_pageText[i];
	
		if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
			for (int i = 0; i < 512; i++) {
				FERS_WriteRegister(handle, FUP_BA + i, datarow[i]);
			}

			FERS_WriteRegister(handle, FUP_BA + FUP_CONTROL_REG, 0x1F);
			crc_T = crc32c(0, (unsigned char*)i_pageText, 512 * 4);
			do {
				FERS_ReadRegister(handle, FUP_BA + FUP_CONTROL_REG, &temp);
			} while (temp == 0x1F);

			FERS_ReadRegister(handle, FUP_BA + FUP_RESULT_REG, &crc_R);
			if (crc_R != crc_T) {
				printf("CRC TX error: %08X %08X\n", crc_R, crc_T);
			}


			FERS_WriteRegister(handle, FUP_BA + FUP_PARAM_REG, cnk * chunk_size_byte);
			FERS_WriteRegister(handle, FUP_BA + FUP_CONTROL_REG, 0xA);
		} else {
			uint32_t cmd = 0xA;
			datarow[8190] = cnk * chunk_size_byte;
			datarow[8191] = cmd;
			const int ChunkSize = 64;
			for (int i = 0; i < 8192; i += ChunkSize) {
				if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH)
					LLeth_WriteMem(FERS_INDEX(handle), i, (char*)(&datarow[i]), ChunkSize * 4);
				else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
					LLusb_WriteMem(FERS_INDEX(handle), i, (char*)(&datarow[i]), ChunkSize * 4);
			}
		}

		waitFlashfree(handle);

		if (ptr != NULL) {
			(*ptr)("", cnk * 100 / n_chunks);
		}
	}
	(*ptr)("", 100);
	return 0;
}

static int EraseFPGAFirmwareFlash(int handle, uint32_t size_in_byte)
{
	uint32_t reg_add0, reg_add1;
	if (FERS_CONNECTIONTYPE(handle) != FERS_CONNECTIONTYPE_TDL) {
		reg_add0 = 8190;
		reg_add1 = 8191;
	} else {
		reg_add0 = FUP_BA + FUP_PARAM_REG;
		reg_add1 = FUP_BA + FUP_CONTROL_REG;
	}
	FERS_WriteRegister(handle, reg_add0, size_in_byte);
	Sleep(10);
	FERS_WriteRegister(handle, reg_add1, 0x1);
	Sleep(10);
	waitFlashfree(handle);
	return 0;
}

static int FirmwareBootApplication(int handle)
{
	uint32_t reg_add0;
	if (FERS_CONNECTIONTYPE(handle) != FERS_CONNECTIONTYPE_TDL) {
		reg_add0 = 8191;
	} else {
		reg_add0 = FUP_BA + FUP_CONTROL_REG;
	}
	FERS_WriteRegister(handle, reg_add0, 0xFE);
	Sleep(10);
	return 0;
}

static int RebootFromFWuploader(int handle)
{
	FERS_WriteRegister(handle, 0x0100FFF0, 1);
	Sleep(10);
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Check if the FPGA is in "bootloader mode" and read the bootloader version
// Inputs:		handle = board handle 
// Outputs:		isInBootloader = if 1, the FPGA is in bootloader mode 
//				version = BL version
//				release = BL release
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_CheckBootloaderVersion(int handle, int *isInBootloader, uint32_t *version, uint32_t *release) 
{
	char buffer[32];
	uint32_t *p_intdata;
	int res;

	*isInBootloader = 0;
	*version = 0;
	*release = 0;

	for (int i = 0; i < 3; i++) {
		res = FERS_WriteRegister(handle, 8191, 0xFF);
		if (res) return res;
		Sleep(5);
	}
	
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH)
		res = LLeth_ReadMem(FERS_INDEX(handle), 0, buffer, 16);
	else if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
		res = LLusb_ReadMem(FERS_INDEX(handle), 0, buffer, 16);
	if (res) return res;
	p_intdata = (uint32_t *)buffer;

	if (p_intdata[0] == 0xB1FFAA1B) {
		*isInBootloader = 1;
		*version = p_intdata[1];
		*release = p_intdata[2];
	}
	return 0;
}

int FUP_CheckControllerVersion(int handle, int* isValid, uint32_t* version, uint32_t* release)
{
	*isValid = 0;
	*version = 0;
	*release = 0;
	FERS_WriteRegister(handle, 0xFF000000 + FUP_CONTROL_REG, FUP_CMD_READ_VERSION);

	uint32_t key;
	FERS_ReadRegister(handle, 0xFF000000 + 0, &key);
	FERS_ReadRegister(handle, 0xFF000000 + 1, version);
	FERS_ReadRegister(handle, 0xFF000000 + 2, release);

	printf("key: %x\n", key);
	printf("version: %x\n", version);
	printf("timestamp: %x\n", release);

	if (key == 0xA1FFAA1B) {
		printf("unlock key ok\n");
		*isValid = 1;
	} else {
		printf("key not ok\n");
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Upgrade the FPGA firmware 
// Inputs:		handle = board handle 
//				fp = pointer to the configuration file being loaded
//				ptr = call back function (used for messaging during the upgrade)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int FERS_FirmwareUpgrade(int handle, FILE *fp, void(*ptr)(char *msg, int progress))
{
	int isInBootloader, isValid, msize, retfsf;
	uint32_t BLversion;
	uint32_t BLrelease;
	uint32_t FUPversion, FUPrelease;
	char msg[100];
	char header[5][100]; // header reading
	uint16_t board_family[20] = {};
	int board_compatibility = 0;
	char *firmware;
	char b0;
	int firmware_size_byte =0;

	if (fp == NULL) return FERSLIB_ERR_INVALID_FWFILE;
	//if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) return FERSLIB_ERR_OPER_NOT_ALLOWED;
	// check for header
	retfsf = fscanf(fp, "%s", header[0]);
	if (strcmp(header[0], "$$$$CAEN-Spa") == 0) { // header is found in FW_File
		for (int i = 0; i < 15; ++i) {	// 15 is > 10, # of lines used in the header. not while since I didn't find a proper exit condition
			retfsf = fscanf(fp, "%s", header[0]);
			if (strcmp(header[0], "Header:") == 0)
				retfsf = fscanf(fp, "%s", header[1]);
			else if (strcmp(header[0], "Rev:") == 0)
				retfsf = fscanf(fp, "%s", header[2]);
			else if (strcmp(header[0], "Build:") == 0)
				retfsf = fscanf(fp, "%s", header[3]);
			else if (strcmp(header[0], "Board:") == 0) {
				for (int j = 0; j < 50; ++j) {
					retfsf = fscanf(fp, "%s", header[4]);
					if (strcmp(header[4], "$$$$") == 0) {
						printf("\n");
						i = 15;
						break;
					} else {
						sscanf(header[4], "%" SCNu16, &board_family[j]);
						++board_compatibility;
					}
				}
			}
		}
		char read_bit;
		while (!feof(fp)) {	// remove carriage and return byte written in the header - last byte '0x0a'
			fread(&read_bit, sizeof(read_bit), 1, fp);
			if (read_bit == 0xa) // or read_bit == -1, with SEEK(fp, -1, SEEK_CUR)
				break;
		}
		// Check for firmware compatibility
		FERS_BoardInfo_t BInfo;
		retfsf = FERS_ReadBoardInfo(handle, &BInfo);
		if (retfsf == 0) {
			// NOTE: if the firmware is corrupted, then the board info cannot be read and it is not possible to check the compatibility.
			//       Nevertheless, the upgrade cannot be skipped, otherwise it won't be possible to recover
			for (int i = 0; i < board_compatibility; ++i) {
				if (BInfo.FERSCode == board_family[i]) {
					board_family[board_compatibility] = 1;
					break;
				}
			}
			if (board_family[board_compatibility] == 0) { // raise an error, the firmware is not compatible with the board
				(*ptr)("ERROR! The firmware version is not compatible with the model of board %d", handle); // set also which board is raising?
				return FERSLIB_ERR_UPGRADE_ERROR;
			}
		}
	}
	else
		fseek(fp, 0, SEEK_SET);	// back to the begin of the file

	int firmware_start = ftell(fp); // in case of header, this offset point to begin of the firmware anyway (0 or byte_of_header)
	// Check 1st byte (must be -1 in Xilinx .bin files)
 	fread(&b0, 1, 1, fp);
	if (b0 != -1) return FERSLIB_ERR_INVALID_FWFILE;

	//Get file length
	fseek(fp, 0, SEEK_END);
	firmware_size_byte = ftell(fp) - firmware_start; // offset in case of header
	fseek(fp, firmware_start, SEEK_SET);
	
	// Read file contents into buffer
	msize = firmware_size_byte + (8192*4);
	firmware = (char*)malloc(msize);
	if (!firmware)	return FERSLIB_ERR_UPGRADE_ERROR;
	fread(firmware, firmware_size_byte, 1, fp);

	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_TDL) {
		FERS_FlushData(handle);

		for (int i = 0; i < 20; i++) {
			FUP_CheckControllerVersion(handle, &isValid, &FUPversion, &FUPrelease);
			if (isValid) break;
			Sleep(100);
		}
		if (!isValid) {
			(*ptr)("ERROR: Fiber uploader not installed!", 0);
			return FERSLIB_ERR_UPGRADE_ERROR;
		}
		sprintf(msg, "Fiber uploader version %X, build: %8X", FUPversion, FUPrelease);
		(*ptr)(msg, 0);
	} else {
		// Reboot from FWloader
		(*ptr)("Reboot from Firmware loader", 0);
		RebootFromFWuploader(handle);
		if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
			LLusb_StreamEnable(FERS_INDEX(handle), false);
		FERS_FlushData(handle);

		for (int i = 0; i < 20; i++) {
			FERS_CheckBootloaderVersion(handle, &isInBootloader, &BLversion, &BLrelease);
			if (isInBootloader) break;
			Sleep(100);
		}
		if (!isInBootloader) {
			(*ptr)("ERROR: FW loader not installed!", 0);
			return FERSLIB_ERR_UPGRADE_ERROR;
		}
		sprintf(msg, "FW loader version %X, build: %8X", BLversion, BLrelease);
		(*ptr)(msg, 0);
	}

	// Erase FPGA
	(*ptr)("Erasing FPGA...", 0);
	EraseFPGAFirmwareFlash(handle, firmware_size_byte);

	// Write FW
	(*ptr)("Writing new firmware", 0);
	WriteFPGAFirmwareOnFlash(handle, firmware, firmware_size_byte, ptr);

	// Reboot from Application
	FirmwareBootApplication(handle);
	if (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB)
		LLusb_StreamEnable(FERS_INDEX(handle), true);
	Sleep(100);
	FERS_FlushData(handle);

	return 0;
}

