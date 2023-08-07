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

#ifndef _FERS_LLTDL_H
#define _FERS_LLTDL_H				// Protect against multiple inclusion

#include "FERSlib.h"
#ifndef _WIN32
#include <sys/ioctl.h>
#endif

extern uint16_t PedestalLG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH];	// LG Pedestals (calibrate PHA Mux Readout)
extern uint16_t PedestalHG[FERSLIB_MAX_NBRD][FERSLIB_MAX_NCH];	// LG Pedestals (calibrate PHA Mux Readout)
extern uint16_t CommonPedestal;									// Common pedestal added to all channels
extern int EnablePedCal;										// 0 = disable calibration, 1 = enable calibration

extern uint32_t TDL_NumNodes[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL];	// num of nodes in the chain

// TDL fiber delay setting
#define FIBER_DELAY(length_m) ((float)(22 + 0.781 * length_m))  // Delay ~= 22 + 0.781 * length (in m)
#define DEFAULT_FIBER_LENGTH  ((float)0.3)  // default fiber length = 0.3 m

// -----------------------------------------------------------------------------------
// Connect 
// -----------------------------------------------------------------------------------
int LLtdl_OpenDevice(char *board_ip_addr, int cindex);
int LLtdl_CloseDevice(int cindex);
int LLtdl_InitTDLchains(int cindex, float DelayAdjust[FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES]);
bool LLtdl_TDLchainsInitialized(int cindex);
int LLtdl_EnumChain(int cindex, uint16_t chain, uint32_t *node_count);
int LLtdl_GetChainInfo(int cindex, uint16_t chain, FERS_TDL_ChainInfo_t *tdl_info);

int LLeth_OpenDevice(char *board_ip_addr, int bindex);
int LLeth_CloseDevice(int bindex);

int LLusb_OpenDevice(int PID, int bindex);
int LLusb_CloseDevice(int bindex);
int LLusb_StreamEnable(int bindex, bool Enable);
int LLusb_Reset_IPaddress(int bindex);

// -----------------------------------------------------------------------------------
// R/W data to REGS memory 
// -----------------------------------------------------------------------------------
//int LLtdl_WriteMem(int cindex, int chain, int node, uint32_t address, char *data, uint16_t size);
//int LLtdl_ReadMem(int cindex, int chain, int node, uint32_t address, char *data, uint16_t size);
int LLtdl_WriteRegister(int cindex, int chain, int node, uint32_t address, uint32_t data);
int LLtdl_ReadRegister(int cindex, int chain, int node, uint32_t address, uint32_t *data);
int LLtdl_SendCommand(int cindex, int chain, int node, uint32_t cmd, uint32_t delay);
int LLtdl_SendCommandBroadcast(int cindex, uint32_t cmd, uint32_t delay);
int LLtdl_CncWriteRegister(int cindex, uint32_t address, uint32_t data);
int LLtdl_CncReadRegister(int cindex, uint32_t address, uint32_t *data);
int LLtdl_GetCncInfo(int cindex, FERS_CncInfo_t *CncInfo);

int LLeth_WriteMem(int bindex, uint32_t address, char *data, uint16_t size);
int LLeth_ReadMem(int bindex, uint32_t address, char *data, uint16_t size);
int LLeth_WriteRegister(int bindex, uint32_t address, uint32_t data);
int LLeth_ReadRegister(int bindex, uint32_t address, uint32_t *data);

int LLusb_WriteMem(int bindex, uint32_t address, char *data, uint16_t size);
int LLusb_ReadMem(int bindex, uint32_t address, char *data, uint16_t size);
int LLusb_WriteRegister(int bindex, uint32_t address, uint32_t data);
int LLusb_ReadRegister(int bindex, uint32_t address, uint32_t *data);

// -----------------------------------------------------------------------------------
// Read raw data
// -----------------------------------------------------------------------------------
int LLtdl_ReadData(int cindex, char *buff, int size, int *nb);
int LLeth_ReadData(int bindex, char *buff, int size, int *nb);
int LLusb_ReadData(int bindex, char *buff, int size, int *nb);
int LLtdl_Flush(int cindex);

#endif