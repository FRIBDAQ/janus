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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib") // Winsock Library
#endif

#include "FERS_LL.h"
#include "FERSlib.h"
#include "MultiPlatform.h"
#include "console.h"


#define COMMAND_PORT		"9760"  // Slow Control Port (access to Registers)
#define STREAMING_PORT		"9000"	// Data RX Port (streaming)

#define TDL_BLK_SIZE	(1024)						// Max size of one packet in the recv 
#define RX_BUFF_SIZE	(16*1024*1024)					// Size of the local Rx buffer

#define LLBUFF_SIZE		(32*1024)
#define TDLINK_HEADER_SIZE	32 // cluster header size in bytes

// *********************************************************************************************************
// Global variables
// *********************************************************************************************************
uint32_t TDL_NumNodes[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL] = { 0 };	// num of nodes in the chain. Inizialized to <0 or >MAX_NBRD_IN_NODE, to avoid multiple enumaration
float FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES] = { 0 };	// Fiber length (in meters) for individual tuning of the propagation delay along the TDL daisy chains
int InitDelayAdjust[FERSLIB_MAX_NCNC] = { 0 };

static f_socket_t FERS_CtrlSocket[FERSLIB_MAX_NCNC] = {f_socket_invalid};	// slow control (R/W reg)
static f_socket_t FERS_DataSocket[FERSLIB_MAX_NCNC] = {f_socket_invalid};	// data streaming
static char *RxBuff[FERSLIB_MAX_NCNC][2] = { NULL };	// Rx data buffers (two "ping-pong" buffers, one write, one read)
//static uint32_t RxBuff_rp[FERSLIB_MAX_NCNC] = { 0 };	// read pointer in Rx data buffer
//static uint32_t RxBuff_wp[FERSLIB_MAX_NCNC] = { 0 };	// write pointer in Rx data buffer
static int RxB_w[FERSLIB_MAX_NCNC] = { 0 };				// 0 or 1 (which is the buffer being written)
static int RxB_r[FERSLIB_MAX_NCNC] = { 0 };				// 0 or 1 (which is the buffer being read)
static int RxB_Nbytes[FERSLIB_MAX_NCNC][2] = { 0 };		// Number of bytes written in the buffer
static int WaitingForData[FERSLIB_MAX_NCNC] = { 0 };	// data consumer is waiting fro data (data receiver should flush the current buffer)
static int RxStatus[FERSLIB_MAX_NCNC] = { 0 };			// 0=not started, 1=running, -1=error
static int QuitThread[FERSLIB_MAX_NCNC] = { 0 };		// Quit Thread
static f_thread_t ThreadID[FERSLIB_MAX_NCNC];			// RX Thread ID
static mutex_t RxMutex[FERSLIB_MAX_NCNC];				// Mutex for the access to the Rx data buffer and pointers
static FILE *Dump[FERSLIB_MAX_NCNC] = { NULL };			// low level data dump files (for debug)
static FILE* DumpRead[FERSLIB_MAX_NCNC] = { NULL };		// low level data read dump file (for debug)
static uint8_t ReadData_Init[FERSLIB_MAX_NBRD] = { 0 }; // Re-init read pointers after run stop


// *********************************************************************************************************
// Connect to Concentrator through socket
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Connect the socket for the communication with the concentrator (either slow control and readout)
// Inputs:		board_ip_addr = IP address of the concentrator 
//				port = socket port (different for slow control and data readout)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
f_socket_t LLtdl_ConnectToSocket(char *board_ip_addr, char *port)
{
	f_socket_t ConnectSocket = f_socket_invalid;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	unsigned long ul = 1;
	int iResult, connect_fail = 0;

#ifdef _WIN32
	// Initialize Winsock
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(board_ip_addr, port, &hints, &result);
	if (iResult != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("LCSm", "getaddrinfo failed with error: %d\n", iResult);
		f_socket_cleanup();
		return f_socket_invalid;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == f_socket_invalid) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("socket failed with error: %ld\n", f_socket_errno);
			f_socket_cleanup();
			return f_socket_invalid;
		}

		// Connect to server.
		/*
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}*/
#ifdef _WIN32
		ioctlsocket(ConnectSocket, FIONBIO, &ul); //set as non-blocking
#else
		ioctl(ConnectSocket, FIONBIO, &ul);
#endif
		if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == -1) {
			fd_set set;
			int error = -1;
			socklen_t len = sizeof(socklen_t);
			struct timeval tm;

			tm.tv_sec = FERS_CONNECT_TIMEOUT;	// set timeout = 3s
			tm.tv_usec = 0;
			FD_ZERO(&set);
			FD_SET(ConnectSocket, &set);
			if (select((int)(ConnectSocket + 1), NULL, &set, NULL, &tm) > 0) {
				getsockopt(ConnectSocket, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
				if (error == 0)	connect_fail = 0;
				else connect_fail = 1;
			} else {
				connect_fail = 1;
			}
		}
		ul = 0;
#ifdef _WIN32
		ioctlsocket(ConnectSocket, FIONBIO, &ul); //set as non-blocking
#else
		ioctl(ConnectSocket, FIONBIO, &ul);
#endif
		if (connect_fail) {
			f_socket_close(ConnectSocket);
			ConnectSocket = f_socket_invalid;
			continue;
		}
		break;
	}
	freeaddrinfo(result);

	if (ConnectSocket == f_socket_invalid) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Unable to connect to server!\n");
		f_socket_cleanup();
		return f_socket_invalid;
	} else return ConnectSocket;
}


// *********************************************************************************************************
// TDlink Chain operations
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Enumerate chain (optical link)
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
// Outputs:		node_count = num of nodes in the chain
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_EnumChain(int cindex, uint16_t chain, uint32_t *node_count)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'U'; p++;
	sendbuf[p] = 'M'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p += 2;
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Enum chain, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 12, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Enum chain, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)&sendbuf[0]);
	*node_count = *((uint32_t *)&sendbuf[4]);
	if (res != 0) return FERSLIB_ERR_TDL_ERROR;
	return 0;
}


// *********************************************************************************************************
// TDlink Chain operations
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Execute Multichain syncronization (optical link)
// Inputs:		cindex = concentrator index
// Outputs:		node_count = num of nodes in the chain
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_SyncChains(int cindex)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'S'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'T'; p++;
	sendbuf[p] = '0'; p++;
	
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Sync chains, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Sync chains, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)&sendbuf[0]);

	if (res != 0) return FERSLIB_ERR_TDL_ERROR;
	return 0;
}




// *********************************************************************************************************
// TDlink Chain operations
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Execute Reset chain to restart enumeration if a link is down(optical link)
// Inputs:		cindex = concentrator index
// Outputs:		node_count = num of nodes in the chain
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_ResetChains(int cindex)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'L'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'K'; p++;


	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Reset chains, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Reset chains, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t*)&sendbuf[0]);

	if (res != 0) return FERSLIB_ERR_TDL_ERROR;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get Chain info
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
// Outputs:		tdl_info = structure with chain info
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_GetChainInfo(int cindex, uint16_t chain, FERS_TDL_ChainInfo_t *tdl_info)
{
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[64];
	int iResult;
	int p = 0;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'I'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'F'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p += 2;
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Get Chain Info, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	iResult = recv(sck, sendbuf, 40, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Get Chain Info, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	p=0;
	tdl_info->Status = *((uint16_t*) &sendbuf[p]); p+=2;
	tdl_info->BoardCount = *((uint16_t*) &sendbuf[p]); p+=2;
	tdl_info->rrt = *((float*) &sendbuf[p]);p+=4;
	tdl_info->EventCount = *((uint64_t*) &sendbuf[p]); p+=8;
	tdl_info->ByteCount = *((uint64_t*) &sendbuf[p]); p+=8;
	tdl_info->EventRate = *((float*) &sendbuf[p]);p+=4;
	tdl_info->Mbps = *((float*) &sendbuf[p]);  p+=4;
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Enable/Disable one chain and set the token interval for data readout. 
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
//				enable = false: chain disabled, true: chain enabled
//				token_interval = Interval between tokens. Minimum interval = 1 (=> max throughput). 
//				Can be increased to limit the bandwidth of one chain.
// Outputs:		tdl_info = structure with chain info
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
static int LLtdl_ControlChain(int cindex, uint16_t chain, bool enable, uint32_t token_interval)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'T'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = enable ? 1:0; p += 2;
	*((uint32_t*)&sendbuf[p]) = token_interval; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Ctrl chain, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Ctrl chain, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)&sendbuf[0]);
	if (res != 0) return -2;
	else return 0;
}

// *********************************************************************************************************
// R/W Reg and Send commands
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the FERS board
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
//				node = chain node (board index in the optical daisy chain)
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_WriteRegister(int cindex, int chain, int node, uint32_t address, uint32_t data) {
	uint32_t res;
	//SOCKET *ConnectSocket = (SOCKET *)handle;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;

	sendbuf[p] = 'W'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p+=2;
	*((uint16_t*)&sendbuf[p]) = node; p += 2;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;
	*((uint32_t*)&sendbuf[p]) = data; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write Reg, send failed with error: %d\n",f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write Reg, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res != 0) return -2;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the FERS board
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
//				node = chain node (board index in the optical daisy chain)
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_ReadRegister(int cindex, int chain, int node, uint32_t address, uint32_t *data) {

	char sendbuf[32];
	int iResult, p=0;
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];

	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = node; p += 2;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read Reg, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 8, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read Reg, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *) &sendbuf[0]);
	*data = *((uint32_t *) &sendbuf[4]);
	if (res != 0) return -2;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Send a command to a specific FERS board
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
//				node = chain node (board index in the optical daisy chain)
//				cmd = command opcode
//				delay = execution delay (must take into account the time it takes for the 
//				command to travel along the chain)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_SendCommand(int cindex, int chain, int node, uint32_t cmd, uint32_t delay) {
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;

	sendbuf[p] = 'F'; p++;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'M'; p++;
	sendbuf[p] = 'D'; p++;
	*((uint16_t*)&sendbuf[p]) = chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = node; p += 2;
	*((uint32_t*)&sendbuf[p]) = cmd; p += 4;
	*((uint32_t*)&sendbuf[p]) = 0; p += 4;
	*((uint32_t*)&sendbuf[p]) = delay; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Send Cmd, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	if (delay >= 100000)
		Sleep(delay / 100000);

	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Send Cmd, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res != 0) return -2;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Send a command to all the boards connected to the concentrator
// Inputs:		cmd = command opcode
//				delay = execution delay (must take into account the time it takes for the 
//				command to travel along the chain)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_SendCommandBroadcast(int cindex, uint32_t cmd, uint32_t delay) {
	return LLtdl_SendCommand(cindex, 0xFF, 0xFF, cmd, delay);
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the concentrator
// Inputs:		cindex = concentrator index
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_CncWriteRegister(int cindex, uint32_t address, uint32_t data) {
	uint32_t res;
	//SOCKET *ConnectSocket = (SOCKET *)handle;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;

	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'W'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;
	*((uint32_t*)&sendbuf[p]) = data; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write Reg, send failed with error: %d\n",f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write Reg, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res != 0) return -2;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the concentrator
// Inputs:		cindex = concentrator index
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_CncReadRegister(int cindex, uint32_t address, uint32_t *data) {

	char sendbuf[32];
	int iResult, p=0;
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];

	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read Reg, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 8, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read Reg, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *) &sendbuf[0]);
	*data = *((uint32_t *) &sendbuf[4]);
	if (res != 0) return -2;
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get concentrator info (PID, FW version, etc...)
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
//				enable = false: chain disabled, true: chain enabled
//				token_interval = Interval between tokens. Minimum interval = 1 (=> max throughput). 
//				Can be increased to limit the bandwidth of one chain.
// Outputs:		tdl_info = structure with chain info
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_GetCncInfo(int cindex, FERS_CncInfo_t *CncInfo)
{
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[1024];
	int iResult, size, p = 0;
	sendbuf[p] = 'V'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'S'; p++;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Get cnc info, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Get cnc info, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	size = *((uint32_t *)sendbuf);
	if (size > 1024) return FERSLIB_ERR_COMMUNICATION;
	iResult = recv(sck, sendbuf, size, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Get cnc info, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	strcpy(CncInfo->SW_rev, sendbuf);
	strcpy(CncInfo->FPGA_FWrev, sendbuf + 16);
	sscanf(sendbuf + 48, "%d", &CncInfo->pid);
	return 0;
}



// *********************************************************************************************************
// Raw data readout
// *********************************************************************************************************
// Thread that keeps reading data from the data socket (at least until the Rx buffer gets full)
//#define DEBUG_THREAD_MSG
static void* tdl_data_receiver(void *params) {
	int bindex = *(int *)params;
	int nbrx, nbreq, res, empty=0;
	int FlushBuffer = 0;
	int WrReady = 1;
	//static char* bpnt;
	static uint32_t RxBuff_wp[FERSLIB_MAX_NCNC] = { 0 };	// write pointer in Rx data buffer
	char *wpnt;
	uint64_t ct, pt, tstart, tstop, tdata = 0;
	fd_set socks;
	struct timeval timeout;

	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Data receiver thread is started\n", bindex);
	if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "RX thread started\n");
	lock(RxMutex[bindex]);
	RxStatus[bindex] = RXSTATUS_IDLE;
	unlock(RxMutex[bindex]);

	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	ct = get_time();
	pt = ct;
	while(1) {
		ct = get_time();
		if (QuitThread[bindex]) break;
		if (RxStatus[bindex] == RXSTATUS_IDLE) {
			lock(RxMutex[bindex]);
			if ((FERS_ReadoutStatus == ROSTATUS_RUNNING) && empty) {  // start of run
				// Clear Buffers
				ReadData_Init[bindex] = 1;
				RxBuff_wp[bindex] = 0;
				RxB_r[bindex] = 0;
				RxB_w[bindex] = 0;
				WaitingForData[bindex] = 0;
				WrReady = 1;
				FlushBuffer = 0;
				for(int i=0; i<2; i++)
					RxB_Nbytes[bindex][i] = 0;
				tstart = ct;
				tdata = ct;
				if (DebugLogs & DBLOG_LL_MSGDUMP) {
					char st[100];
					time_t ss;
					time(&ss);
					strcpy(st, asctime(gmtime(&ss)));
					st[strlen(st) - 1] = 0;
					fprintf(Dump[bindex], "\nRUN_STARTED at %s\n", st);
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Run Started\n", bindex);
					fflush(Dump[bindex]);
				}
				RxStatus[bindex] = RXSTATUS_RUNNING;
				lock(FERS_RoMutex);
				FERS_RunningCnt++;
				unlock(FERS_RoMutex);
			} else {
				// make "dummy" reads while not running to flush old data. Stop when there is no data
				if (!empty) {
					FD_ZERO(&socks); 
					FD_SET((int)FERS_DataSocket[bindex], &socks);
					res = select((int)FERS_DataSocket[bindex] + 1, &socks, NULL, NULL, &timeout);
					if (res < 0) {  // socket error, quit thread
						f_socket_cleanup();
						RxStatus[bindex] = -1;
						break; 
					}
					if (res == 0) empty = 1;
					else nbrx = recv(FERS_DataSocket[bindex], RxBuff[bindex][0], RX_BUFF_SIZE, 0);
					if (!empty && (DebugLogs & DBLOG_LL_MSGDUMP)) fprintf(Dump[bindex], "Reading old data...\n");
				}
				unlock(RxMutex[bindex]);
				Sleep(10);
				continue;
			}
			unlock(RxMutex[bindex]);
		}

		if ((RxStatus[bindex] == RXSTATUS_RUNNING) && (FERS_ReadoutStatus != ROSTATUS_RUNNING)) {  // stop of run 
			lock(RxMutex[bindex]);
			tstop = ct;
			RxStatus[bindex] = RXSTATUS_EMPTYING;
			if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Board Stopped. Emptying data (T=%.3f)\n", 0.001 * (tstop - tstart));
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Stop Command issued (T=%.3f)\n", bindex, 0.001 * (tstop - tstart));
			unlock(RxMutex[bindex]);
		}

		if (RxStatus[bindex] == RXSTATUS_EMPTYING) {
			// stop RX for one of these conditions:
			//  - flush command 
			//  - there is no data for more than NODATA_TIMEOUT
			//  - STOP_TIMEOUT after the stop to the boards
			lock(RxMutex[bindex]);
			if ((FERS_ReadoutStatus == ROSTATUS_FLUSHING) || ((ct - tdata) > NODATA_TIMEOUT) || ((ct - tstop) > STOP_TIMEOUT)) {  
				RxStatus[bindex] = RXSTATUS_IDLE;
				lock(FERS_RoMutex);
				if (FERS_RunningCnt > 0) FERS_RunningCnt--;
				unlock(FERS_RoMutex);
				if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Run stopped (T=%.3f)\n", 0.001 * (ct - tstart));
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] RX Thread is now IDLE. (T=%.3f)\n", bindex, 0.001 * (ct - tstart));
				empty = 0;
				unlock(RxMutex[bindex]);
				continue;
			}
			unlock(RxMutex[bindex]);
		}

		if (!WrReady) {  // end of current buff reached => switch to the other buffer (if available for writing)
			lock(RxMutex[bindex]);
			if (RxB_Nbytes[bindex][RxB_w[bindex]] > 0) {  // the buffer is not empty (being used for reading) => retry later
				unlock(RxMutex[bindex]);
				Sleep(1);
				continue;
			}
			unlock(RxMutex[bindex]);
			WrReady = 1;
		} 

		wpnt = RxBuff[bindex][RxB_w[bindex]] + RxBuff_wp[bindex];
		nbreq = RX_BUFF_SIZE - RxBuff_wp[bindex];  // try to read enough bytes to fill the buffer
		FD_ZERO(&socks); 
		FD_SET((int)FERS_DataSocket[bindex], &socks);
		res = select((int)FERS_DataSocket[bindex] + 1, &socks, NULL, NULL, &timeout);
		if (res == 0) { // Timeout
			FlushBuffer = 1;
			continue; 
		}
		if (res < 0) {  // socket error, quit thread
			f_socket_cleanup();
			lock(RxMutex[bindex]);
			RxStatus[bindex] = -1;
			break; 
		}
		nbrx = recv(FERS_DataSocket[bindex], wpnt, nbreq, 0);
		if (nbrx == f_socket_error) {
			f_socket_cleanup();
			lock(RxMutex[bindex]);
			RxStatus[bindex] = -1;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] socket recv failed in data receiver thread (board %d)\n", bindex);
			break;
		}
		tdata = ct;

#if (THROUGHPUT_METER == 1)
		// Rate Meter: print raw data throughput (RxBuff_wp is not updated, thus the data consumer doesn't see any data to process)
		static uint64_t totnb = 0, lt = ct, l0 = ct;
		totnb += nbrx;
		if ((ct - lt) > 1000) {
			printf("%6.1f s: %10.6f MB/s\n", float(ct - l0) / 1000, float(totnb) / (ct - lt) / 1000);
			totnb = 0;
			lt = ct;
		}
		unlock(RxMutex[bindex]);
		continue;
#endif

		RxBuff_wp[bindex] += nbrx;
		if ((ct - pt) > 10) {  // every 10 ms, check if the data consumer is waiting for data or if the thread has to quit
			if (QuitThread[bindex]) break;  
			if (WaitingForData[bindex] && (RxBuff_wp[bindex] > 0)) FlushBuffer = 1;
			pt = ct;
		}

		if ((RxBuff_wp[bindex] == RX_BUFF_SIZE) || FlushBuffer) {  // the current buffer is full or must be flushed
			lock(RxMutex[bindex]);
			RxB_Nbytes[bindex][RxB_w[bindex]] = RxBuff_wp[bindex];
			RxB_w[bindex] ^= 1;  // switch to the other buffer
			RxBuff_wp[bindex] = 0;
			unlock(RxMutex[bindex]); 
			WrReady = 0;
			FlushBuffer = 0;
		}
		// Dump data to log file (for debugging)
		if ((DebugLogs & DBLOG_LL_DATADUMP) && (nbrx > 0) && (Dump[bindex] != NULL)) {
			for(int i=0; i<nbrx; i+=4) {
				uint32_t *d32 = (uint32_t *)(wpnt + i);
				fprintf(Dump[bindex], "%08X\n", *d32);
			}
			//fflush(Dump[bindex]);
		}
	}

	RxStatus[bindex] = RXSTATUS_OFF;
	unlock(RxMutex[bindex]);
	return NULL;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Copy a data block from RxBuff of the concentrator to the user buffer 
// Inputs:		bindex = concentrator index
//				buff = user data buffer to fill
//				maxsize = max num of bytes being transferred
//				nb = num of bytes actually transferred
// Return:		0=No Data, 1=Good Data 2=Not Running, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_ReadData(int bindex, char *buff, int maxsize, int *nb) {
	char *rpnt;
	static char* bpnt;
	static int RdReady[FERSLIB_MAX_NBRD] = { 0 };
	static int Nbr[FERSLIB_MAX_NBRD] = { 0 };
	static int Rd_pnt[FERSLIB_MAX_NBRD] = { 0 };	// read pointer in Rx data buffer

	*nb = 0;
	//if (trylock(RxMutex[bindex]) != 0) return 0;
	if (ReadData_Init[bindex]) {
		while (trylock(RxMutex[bindex]));
		RdReady[bindex] = 0;
		Nbr[bindex] = 0;
		ReadData_Init[bindex] = 0;
		unlock(RxMutex[bindex]);
		return 2;
	} 

	if ((RxStatus[bindex] != RXSTATUS_RUNNING) && (RxStatus[bindex] != RXSTATUS_EMPTYING)) {
		return 2;
	}

	if (!RdReady[bindex]) {
		if (trylock(RxMutex[bindex]) != 0) return 0;
		if (RxB_Nbytes[bindex][RxB_r[bindex]] == 0) {  // The buffer is empty => assert "WaitingForData" and return 0 bytes to the caller
			WaitingForData[bindex] = 1;
			unlock(RxMutex[bindex]);
			return 0;
		}
		RdReady[bindex] = 1;
		Nbr[bindex] = RxB_Nbytes[bindex][RxB_r[bindex]];  // Get the num of bytes available for reading in the buffer
		bpnt = RxBuff[bindex][RxB_r[bindex]];
		Rd_pnt[bindex] = 0;
		WaitingForData[bindex] = 0;
		unlock(RxMutex[bindex]);
	}

	rpnt = bpnt + Rd_pnt[bindex];
	*nb = Nbr[bindex] - Rd_pnt[bindex];  // num of bytes currently available for reading in RxBuff
	if (*nb > maxsize) 
		*nb = maxsize;
	if (*nb > 0) {
		memcpy(buff, rpnt, *nb);
		Rd_pnt[bindex] += *nb;

		if (DebugLogs & DBLOG_LL_READDUMP) {
			for (int i = 0; i < *nb; i += 4) {
				uint32_t* d32 = (uint32_t*)(rpnt + i);
				fprintf(DumpRead[bindex], "%08X\n", *d32);
			}
			fprintf(DumpRead[bindex], "\n");
			// fflush(DumpRead[bindex]);
		}
	}

	if (Rd_pnt[bindex] == Nbr[bindex]) {  // end of current buff reached => switch to other buffer 
		while (trylock(RxMutex[bindex]));
		RxB_Nbytes[bindex][RxB_r[bindex]] = 0;  
		RxB_r[bindex] ^= 1;
		//RxBuff_rp[bindex] = 0;
		RdReady[bindex] = 0;
		unlock(RxMutex[bindex]);
	}
	return 1;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Flush all data in the concentrator
// Inputs:		cindex = concentrator index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_Flush(int cindex) {
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'L'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'S'; p++;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Flush Cmd, send failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Flush Cmd, recv failed with error: %d\n", f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res != 0) return -2;
	return 0;
}



// *********************************************************************************************************
// Open/Close
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Open the connection to the concentrator through the ethernet or USB interface. 
//				After the connection, the function allocate the memory buffers and starts 
//				the thread that receives the data
// Inputs:		board_ip_addr = IP address of the concentrator
// Outputs:		cindex = concentrator index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_OpenDevice(char *board_ip_addr, int cindex) {
	int ret = 0, started;

	if (cindex >= FERSLIB_MAX_NBRD) return -1;

	FERS_CtrlSocket[cindex] = LLtdl_ConnectToSocket(board_ip_addr, COMMAND_PORT);
	FERS_DataSocket[cindex] = LLtdl_ConnectToSocket(board_ip_addr, STREAMING_PORT);
	if ((FERS_CtrlSocket[cindex] == f_socket_invalid) || (FERS_DataSocket[cindex] == f_socket_invalid)) 
		return FERSLIB_ERR_COMMUNICATION;

	for(int i=0; i<2; i++) {
		RxBuff[cindex][i] = (char *)malloc(RX_BUFF_SIZE);
		FERS_TotalAllocatedMem += RX_BUFF_SIZE;
	}
	//LLBuff[cindex] = (char *)malloc(LLBUFF_SIZE);
	initmutex(RxMutex[cindex]);
	QuitThread[cindex] = 0;
	if ((DebugLogs & DBLOG_LL_DATADUMP) || (DebugLogs & DBLOG_LL_MSGDUMP)) {
		char filename[200];
		sprintf(filename, "ll_log_%d.txt", cindex);
		Dump[cindex] = fopen(filename, "w");
	}
	if (DebugLogs & DBLOG_LL_READDUMP) {
		char filename[200];
		sprintf(filename, "ll_readData_log_%d.txt", cindex);
		DumpRead[cindex] = fopen(filename, "w");
	}

	// Set timeout on receive
#ifdef _WIN32	
	DWORD timeout = FERS_CONNECT_TIMEOUT * 1000;
	setsockopt(FERS_CtrlSocket[cindex], SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
#else
	struct timeval tv;
	tv.tv_sec = FERS_CONNECT_TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(FERS_CtrlSocket[cindex], SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#endif

	thread_create(tdl_data_receiver, &cindex, &ThreadID[cindex]);
	started = 0;
	while(!started) {
		lock(RxMutex[cindex]);
		if (RxStatus[cindex] != 0) 
			started = 1;
		unlock(RxMutex[cindex]);
	}
	for(int chain=0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret |= LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		if (ret != 0) return ret;
		if (tdl_info.Status <= 2) // Not enumerated before
			TDL_NumNodes[cindex][chain] = 0;
		else
			TDL_NumNodes[cindex][chain] = tdl_info.BoardCount;
	}
	// Remove timeout on receive
#ifdef WIN32
	timeout = 0;
	setsockopt(FERS_CtrlSocket[cindex], SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
#else
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	setsockopt(FERS_CtrlSocket[cindex], SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#endif
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Initialize the TDlink chains (enumerate chains, the send sync commands)
// Inputs:		board_ip_addr = IP address of the concentrator
//				DelayAdjust = individual fiber delay adjust
// Outputs:		cindex = concentrator index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_InitTDLchains(int cindex, float DelayAdjust[FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES]) {
	int ret = 0, chain;
	float max_delay = 0, node_delay, flength, del_sum;
	uint32_t i;
	bool not_enumerated_before = false;

	if (DelayAdjust != NULL) {
		memcpy(FiberDelayAdjust[cindex], DelayAdjust, sizeof(float) * FERSLIB_MAX_NTDL * FERSLIB_MAX_NNODES);
		InitDelayAdjust[cindex] = 1;
	}

	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret |= LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		if (tdl_info.Status > 0) {
			if (tdl_info.Status <= 2) {
				not_enumerated_before = true;
			} else {
				TDL_NumNodes[cindex][chain] = tdl_info.BoardCount;
			}
		} else {
			TDL_NumNodes[cindex][chain] = 0;
		}
	}
	//reset all chains if not enumerated before
	if (not_enumerated_before == true)
		LLtdl_ResetChains(cindex);

	// Find maximum delay 
	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		del_sum = 0;
		for (i = 0; i < TDL_NumNodes[cindex][chain]; i++) {
			flength = (InitDelayAdjust[cindex] || (FiberDelayAdjust[cindex][chain][i] == 0)) ? DEFAULT_FIBER_LENGTH : FiberDelayAdjust[cindex][chain][i];// (DelayAdjust[chain][i] == 0) DelayAdjust[chain][i];  
			del_sum += FIBER_DELAY(flength);
		}
		max_delay = max(del_sum, max_delay);
	}
	max_delay = (float)ceil(max_delay);

	for(chain=0; chain < FERSLIB_MAX_NTDL; chain++) {
		ret = 0;
		if (not_enumerated_before == true) { // Not enumerated before
			ret |= LLtdl_EnumChain(cindex, chain, &TDL_NumNodes[cindex][chain]);
		}
		if (ret == -15) {
			TDL_NumNodes[cindex][chain] = 0;
		} else { 
			// Set the propagation delay between the nodes on the optical chain.
			//for (uint32_t i = 0; i < TDL_NumNodes[cindex][chain]; i++) {
			//	// Delay ~= 22 + 0.781 * length (in m)
			//	const int DELAY_COMP = 22;  // this is the correct delay compensation for a fiber of 30 cm
			//	//const int DELAY_COMP = 53;  // this is the correct delay compensation for a fiber of 40 m
			//	uint32_t prop_delay = DELAY_COMP * (TDL_NumNodes[cindex][chain] - i - 1);
			//	uint32_t data = (chain << 24) | (i << 16) | prop_delay;
			node_delay = max_delay;
			for (i=0; i< TDL_NumNodes[cindex][chain]; ++i) {
				flength = (InitDelayAdjust[cindex] || (FiberDelayAdjust[cindex][chain][i] == 0)) ? DEFAULT_FIBER_LENGTH : FiberDelayAdjust[cindex][chain][i];
				node_delay -= FIBER_DELAY(flength);
				if (node_delay < 0) node_delay = 0;
				uint32_t data = (chain << 24) | (i < 16) | (uint32_t)node_delay;
				ret = LLtdl_CncWriteRegister(cindex, VR_SYNC_DELAY, data);
				if (ret < 0) return FERSLIB_ERR_COMMUNICATION;
			}

			ret |= LLtdl_ControlChain(cindex, chain, 1, 0x100);
			if (ret) return FERSLIB_ERR_COMMUNICATION;
		}
	}

	//uint32_t cmd_delay = TDL_COMMAND_DELAY;  // CTIN: set delay as a function of the number of boards in the chain (?)
	ret = 0;
	ret |= LLtdl_SendCommandBroadcast(cindex, CMD_TDL_SYNC, TDL_COMMAND_DELAY);
	//ret |= LLtdl_SendCommandBroadcast(cindex, CMD_TDL_SYNC, cmd_delay); // CTIN: may need 2 sync commands (not clear why...)
	//Sleep(300);  // CTIN: do you need this ???
	ret |= LLtdl_SendCommandBroadcast(cindex, CMD_TIME_RESET, TDL_COMMAND_DELAY);
	//ret |= LLtdl_SendCommandBroadcast(cindex, CMD_TIME_RESET, cmd_delay);
	ret |= LLtdl_SendCommandBroadcast(cindex, CMD_RES_PTRG, TDL_COMMAND_DELAY);
	//ret |= LLtdl_SendCommandBroadcast(cindex, CMD_RES_PTRG, cmd_delay);
	if (ret) return FERSLIB_ERR_COMMUNICATION;
	if (not_enumerated_before) {
		ret = LLtdl_SyncChains(cindex);
		if (ret) return FERSLIB_ERR_COMMUNICATION;
	}
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Check if the TDlink chains are initialized 
// Inputs:		board_ip_addr = IP address of the concentrator
// Return:		true = init done; false = not done
// ----------------------------------------------------------
bool LLtdl_TDLchainsInitialized(int cindex) {
	int ret, chain;
	for(chain=0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret = LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		if (tdl_info.Status > 0) {
			if (ret || (tdl_info.Status <= 2)) return false;
		}
	}
	return true;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Close the connection to the concentrator and free buffers
// Inputs:		cindex: concentrator index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_CloseDevice(int cindex)
{
	lock(RxMutex[cindex]);
	QuitThread[cindex] = 1;
	unlock(RxMutex[cindex]);

	shutdown(FERS_CtrlSocket[cindex], SHUT_SEND);
	if (FERS_CtrlSocket[cindex] != f_socket_invalid) f_socket_close(FERS_CtrlSocket[cindex]);

	shutdown(FERS_DataSocket[cindex], SHUT_SEND);
	if (FERS_DataSocket[cindex] != f_socket_invalid) f_socket_close(FERS_DataSocket[cindex]);

	f_socket_cleanup();
	//thread_join(ethThreadID[cindex]);
	if (Dump[cindex] != NULL) {
		fclose(Dump[cindex]);
		Dump[cindex] = NULL;
	}
	if (DumpRead[cindex] != NULL) {
		fclose(DumpRead[cindex]);
		DumpRead[cindex] = NULL;
	}
	for(int i=0; i<2; i++)
		if (RxBuff[cindex][i] != NULL) free(RxBuff[cindex][i]);
	//if (LLBuff[cindex] == NULL) free(LLBuff[cindex]);
	return 0;
}

