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

#define COMMAND_PORT		"9760"  // Slow Control Port (access to Registers)
#define STREAMING_PORT		"9000"	// Data RX Port (streaming)

#define TDL_BLK_SIZE	(1024)						// Max size of one packet in the recv 
#define RX_BUFF_SIZE	(16*1024*1024)					// Size of the local Rx buffer

#define LLBUFF_SIZE		(32*1024)
#define TDLINK_HEADER_SIZE	32 // cluster header size in bytes

#define TDL_RW_MAX_ATTEMPTS 10 // Max num of attempts when the reg R/W or send command returns a timeout error
// *********************************************************************************************************
// Global variables
// *********************************************************************************************************
uint32_t TDL_NumNodes[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL] = { 0 };	// num of nodes in the chain. Inizialized to <0 or >MAX_NBRD_IN_NODE, to avoid multiple enumaration
uint16_t TDL_ChainStatus[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL] = { 0 };	// Status if TDL chains during enumeration
uint8_t TDL_ChainStatus_Code[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL] = { 0 }; // Error codes during enumeration
float TDL_FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES] = { 0 };	// Fiber length (in meters) for individual tuning of the propagation delay along the TDL daisy chains
float max_delay[FERSLIB_MAX_NCNC] = { 0 }; // Maximum delay (in meters) among all fibers connected to the concentrators
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
static f_sem_t RxSemaphore[FERSLIB_MAX_NCNC];			// Semaphore for sync the data receiver thread 
static f_thread_t ThreadID[FERSLIB_MAX_NCNC];			// RX Thread ID
static mutex_t RxMutex[FERSLIB_MAX_NCNC];				// Mutex for the access to the Rx data buffer and pointers
static mutex_t rdf_mutex[FERSLIB_MAX_NCNC];				// Mutex to access RawData file
static FILE *Dump[FERSLIB_MAX_NCNC] = { NULL };			// low level data dump files (for debug)
static FILE* Dump2[FERSLIB_MAX_NCNC] = { NULL };		// low level data dump files (for debug)
static FILE* RawData[FERSLIB_MAX_NBRD] = { NULL };		// low level data saving for a further reprocessing
static uint8_t ReadData_Init[FERSLIB_MAX_NBRD] = { 0 }; // Re-init read pointers after run stop
static int subrun[FERSLIB_MAX_NBRD] = { 0 };			// Sub Run index
static int64_t size_file[FERSLIB_MAX_NBRD] = { 0 };		// Size of Raw Data Binary File

// ********************************************************************************************************
// Write messages or data to debug dump file
// ********************************************************************************************************
static int FERS_DebugDump(int cindex, char *fmt, ...) {
	char msg[1000], filename[200];
	static int fileopened[FERSLIB_MAX_NCNC] = { 0 }; // Negative Logic. Arrays can be initialized at 0
	va_list args;
	if (cindex >= FERSLIB_MAX_NCNC) return -1;
	if (!fileopened[cindex]) {
		sprintf(filename, "ll_log_%d.txt", cindex);
		Dump[cindex] = fopen(filename, "w");
		fileopened[cindex] = 1;
	}

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);
	if (Dump[cindex] != NULL)
		fprintf(Dump[cindex], "%s", msg);
	return 0;
}


static int FERS_DebugDump2(int cindex, const char* fmt, ...) {
	char msg[1000], filename[200];
	static int fileopened[FERSLIB_MAX_NCNC] = { 0 }; // Negative Logic. Arrays can be initialized at 0
	va_list args;
	if (cindex >= FERSLIB_MAX_NCNC) return -1;
	if (!fileopened[cindex]) {
		sprintf(filename, "ll_Readlog_%d.txt", cindex);
		Dump2[cindex] = fopen(filename, "w");
		fileopened[cindex] = 1;
	}

	va_start(args, fmt);
	vsprintf(msg, fmt, args);
	va_end(args);
	if (Dump2[cindex] != NULL)
		fprintf(Dump2[cindex], "%s", msg);
	return 0;
}


// ********************************************************************************************************
// Utility for increase subrun number for RawData output file
// ********************************************************************************************************
// --------------------------------------------------------------------------------------------------------
// Description: Close and Open a new RawData output file increasing the subrun number
// Inputs:		None
// Return:		0
// --------------------------------------------------------------------------------------------------------
int LLtdl_IncreaseRawDataSubrun(int bidx) {
	fclose(RawData[bidx]);
	++subrun[bidx];
	size_file[bidx] = 0;
	char filename[200];
	sprintf(filename, "%s.%d.frd", RawDataFilenameTdl[bidx], subrun[bidx]);
	RawData[bidx] = fopen(filename, "wb");
	return 0;
}

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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] WSAStartup failed with error: %d\n", iResult);
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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] getaddrinfo failed with error: %d\n", iResult);
		f_socket_cleanup();
		return f_socket_invalid;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == f_socket_invalid) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] socket failed with error: %ld\n", f_socket_errno);
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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Unable to connect to server (IP=%s, Port=%s)\n", board_ip_addr, port);
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
static int LLtdl_EnumChain(int cindex, int chain, uint32_t *node_count)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32], recvbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'U'; p++;
	sendbuf[p] = 'M'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Enum chain, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, recvbuf, 12, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Enum chain, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)&recvbuf[0]);
	*node_count = *((uint32_t *)&recvbuf[4]);

	if (res == 0) {
		return 0;
	} else {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Enum chain: Status = %08X\n", cindex, res);
		if ((res == CNC_STATUS_SFP_LOS) || (res == CNC_STATUS_SFP_FAULT))
			return FERSLIB_ERR_TDL_CHAIN_BROKEN;
		else if (res == CNC_STATUS_CHAIN_DOWN)
			return FERSLIB_ERR_TDL_CHAIN_DOWN;
		else
			return FERSLIB_ERR_TDL_ERROR;
	}
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
static int LLtdl_SyncChains(int cindex)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32], recvbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'S'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'T'; p++;
	sendbuf[p] = '0'; p++;
	
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Sync chains, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Sync chains, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)&recvbuf[0]);

	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Sync chain: Status = %08X\n", cindex, res);
		return FERSLIB_ERR_TDL_ERROR;
	} 
	else return 0;
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
static int LLtdl_ResetChains(int cindex)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32], recvbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'L'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'K'; p++;


	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Reset chains, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Reset chains, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t*)&recvbuf[0]);

	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Reset chains: Status = %08X\n", cindex, res);
		return FERSLIB_ERR_TDL_ERROR;
	}
	else return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Get Chain info
// Inputs:		cindex = concentrator index
//				chain = chain number (optical link)
// Outputs:		tdl_info = structure with chain info
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_GetChainInfo(int cindex, int chain, FERS_TDL_ChainInfo_t *tdl_info)
{
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[64], recvbuf[64];
	int iResult;
	int p = 0;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'I'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'F'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d] Get Chain Info, send failed with error: %d\n", cindex, chain, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	iResult = recv(sck, recvbuf, 40, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d] Get Chain Info, recv failed with error: %d\n", cindex, chain, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}

	p=0;
	tdl_info->Status = *((uint16_t*) &recvbuf[p]); p+=2;
	tdl_info->BoardCount = *((uint16_t*) &recvbuf[p]); p+=2;
	tdl_info->rrt = *((float*) &recvbuf[p]);p+=4;
	tdl_info->EventCount = *((uint64_t*) &recvbuf[p]); p+=8;
	tdl_info->ByteCount = *((uint64_t*) &recvbuf[p]); p+=8;
	tdl_info->EventRate = *((float*) &recvbuf[p]);p+=4;
	tdl_info->Mbps = *((float*) &recvbuf[p]);  p+=4;
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
int LLtdl_ControlChain(int cindex, int chain, bool enable, uint32_t token_interval)
{
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32], recvbuf[32];
	int iResult;
	int p = 0;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'N'; p++;
	sendbuf[p] = 'T'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)enable ? 1:0; p += 2;
	*((uint32_t*)&sendbuf[p]) = token_interval; p += 4;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d] Ctrl chain, send failed with error: %d\n", cindex, chain, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_COMMUNICATION;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d] Ctrl chain, recv failed with error: %d\n", cindex, chain, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_COMMUNICATION;
	}
	res = *((uint32_t *)&recvbuf[0]);
	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d] Control chain: Status = %08X\n", cindex, chain, res);
		return FERSLIB_ERR_COMMUNICATION;
	}
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
	char sendbuf[32], recvbuf[32];
	int iResult;
	int i, p = 0;

	sendbuf[p] = 'W'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p+=2;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)node; p += 2;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;
	*((uint32_t*)&sendbuf[p]) = data; p += 4;

	for (i = 0; i < TDL_RW_MAX_ATTEMPTS; ++i) {
		iResult = send(sck, sendbuf, p, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Write Reg @ %08X, send failed with error: %d\n", cindex, chain, node, address, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		iResult = recv(sck, recvbuf, 4, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Write Reg @ %08X, recv failed with error: %d\n", cindex, chain, node, address, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		res = *((uint32_t*)recvbuf);
		if (res == 0) break;
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][NODE %d:%d:%d] Write Reg @ %08X returned with code 0x%08X: attempt n. %d\n", cindex, chain, node, address, res, i + 1);
	}
	if (i == TDL_RW_MAX_ATTEMPTS) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Write Reg @ %08X failed; Status = 0x%08X\n", cindex, chain, node, address, res);
		return FERSLIB_ERR_COMMUNICATION;
	}
	else return 0;
}

int LLtdl_MultiWriteRegister(int cindex, int chain, int node, uint32_t* address, uint32_t* data, int ncycles) {
	uint32_t res;
	//SOCKET *ConnectSocket = (SOCKET *)handle;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	int iResult, sumResult;
	int timeout = 0;
	int i, j, p = 0;
	//int err = 0;

	char* sendbuf;
	char* recvbuf;
	int msize;
	int cycles = ncycles;

	msize = (cycles * 16);
	sendbuf = (char*)malloc(msize);
	recvbuf = (char*)malloc(msize);

	for (j = 0; j < cycles; j++) {
		sendbuf[p] = 'W'; p++;
		sendbuf[p] = 'R'; p++;
		sendbuf[p] = 'E'; p++;
		sendbuf[p] = 'G'; p++;
		*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
		*((uint16_t*)&sendbuf[p]) = (uint16_t)node; p += 2;
		*((uint32_t*)&sendbuf[p]) = address[j]; p += 4;
		*((uint32_t*)&sendbuf[p]) = data[j]; p += 4;
	}
	for (i = 0; i < TDL_RW_MAX_ATTEMPTS; ++i) {
		iResult = send(sck, sendbuf, p, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] MultiWrite, send failed with error: %d\n", cindex, chain, node, f_socket_errno);
			f_socket_cleanup();
			free(sendbuf);
			free(recvbuf);
			return FERSLIB_ERR_COMMUNICATION;
		}
		sumResult = 0;
		while (sumResult < 4 * cycles) {
			iResult = recv(sck, recvbuf, 4 * cycles, 0);
			sumResult += iResult;  // AMA timeout 
			if (iResult == 0) ++timeout;
			if (iResult == f_socket_error) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] MultiWrite, recv failed with error: %d\n", cindex, chain, node, f_socket_errno);
				f_socket_cleanup();
				free(sendbuf);
				free(recvbuf);
				return FERSLIB_ERR_COMMUNICATION;
			}
			if (timeout == 1000) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] MultiWrite Timeout\n", cindex, chain, node);
				f_socket_cleanup();
				free(sendbuf);
				free(recvbuf);
				return FERSLIB_ERR_COMMUNICATION;
			}
		}

		res = *((uint32_t*)recvbuf); // AMA va gestito meglio res (cos si guarda solo il primo status)
		if (res == 0) break;
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][NODE %d:%d:%d] MultiWrite returned with code %08X: attempt n. %d\n", cindex, chain, node, res, i + 1);
	}

	free(sendbuf);
	free(recvbuf);
	if (i == TDL_RW_MAX_ATTEMPTS) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] MultiWrite failed; Status = %08X\n", cindex, chain, node, res);
		return FERSLIB_ERR_COMMUNICATION;
	} else return 0;
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

	char sendbuf[32], recvbuf[32];
	int iResult, i, p=0;
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];

	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'G'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)node; p += 2;
	*((uint32_t*)&sendbuf[p]) = address; p += 4;

	for (i = 0; i < TDL_RW_MAX_ATTEMPTS; ++i) {
		iResult = send(sck, sendbuf, p, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Read Reg @ %08X, send failed with error: %d\n", cindex, chain, node, address, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		iResult = recv(sck, recvbuf, 8, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Read Reg @ %08X, recv failed with error: %d\n", cindex, chain, node, address, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		res = *((uint32_t*)&recvbuf[0]);
		*data = *((uint32_t*)&recvbuf[4]);
		if (res == 0) break;
		if (ENABLE_FERSLIB_LOGMSG) 
			FERS_LibMsg("[WARNING][NODE %d:%d:%d] Read Reg @ %08X returned with code %08X: attempt n. %d\n", cindex, chain, node, address, res, i + 1);
	}
	if (i == TDL_RW_MAX_ATTEMPTS) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Read Reg @ %08X failed; Status = %08X\n", cindex, chain, node, address, res);
		return FERSLIB_ERR_COMMUNICATION;
	}
	else return 0;
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
	char sendbuf[32], recvbuf[32];
	int iResult;
	int i, p = 0;

	sendbuf[p] = 'F'; p++;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'M'; p++;
	sendbuf[p] = 'D'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)node; p += 2;
	*((uint32_t*)&sendbuf[p]) = cmd; p += 4;
	*((uint32_t*)&sendbuf[p]) = 0; p += 4;
	*((uint32_t*)&sendbuf[p]) = delay; p += 4;

	for (i = 0; i < TDL_RW_MAX_ATTEMPTS; ++i) {
		iResult = send(sck, sendbuf, p, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Send Cmd, send failed with error: %d\n", cindex, chain, node, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		if (delay >= 100000)
			Sleep(delay / 100000);

		iResult = recv(sck, recvbuf, 4, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Send Cmd, recv failed with error: %d\n", cindex, chain, node, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		res = *((uint32_t*)recvbuf);
		if (res == 0) break;
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][NODE %d:%d:%d] Send Cmd 0x%08X returned with code %08X: attempt n. %d d\n", cindex, chain, node, cmd, res, i + 1);
	}
	if (i == TDL_RW_MAX_ATTEMPTS) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Send Cmd 0x%02X failed; Status = %08X\n", cindex, chain, node, cmd, res);
		return FERSLIB_ERR_COMMUNICATION;
	}
	else return 0;
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

int LLtdl_SetDCommandBroadcast(int cindex, uint32_t cmd, uint32_t delay) {
	uint32_t res;
	f_socket_t sck = FERS_CtrlSocket[cindex];
	char sendbuf[32], recvbuf[32];
	int iResult;
	int i, p = 0;
	delay = 100;  // Just for test

	int chain = 0xFF; // broadcast to all chains
	int node = 0xFF; // broadcast to all nodes

	sendbuf[p] = 'D'; p++;
	sendbuf[p] = 'C'; p++;
	sendbuf[p] = 'M'; p++;
	sendbuf[p] = 'D'; p++;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)chain; p += 2;
	*((uint16_t*)&sendbuf[p]) = (uint16_t)node; p += 2;
	*((uint32_t*)&sendbuf[p]) = cmd; p += 4;
	*((uint32_t*)&sendbuf[p]) = 0; p += 4;
	*((uint32_t*)&sendbuf[p]) = delay; p += 4;

	for (i = 0; i < TDL_RW_MAX_ATTEMPTS; ++i) {
		iResult = send(sck, sendbuf, p, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Set DCmd, send failed with error: %d\n", cindex, chain, node, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		if (delay >= 100000)
			Sleep(delay / 100000);

		iResult = recv(sck, recvbuf, 4, 0);
		if (iResult == f_socket_error) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Set DCmd, recv failed with error: %d\n", cindex, chain, node, f_socket_errno);
			f_socket_cleanup();
			return FERSLIB_ERR_COMMUNICATION;
		}
		res = *((uint32_t*)recvbuf);
		if (res == 0) break;
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][NODE %d:%d:%d] Set DCmd returned with code %08X: attempt n. %d d\n", cindex, chain, node, res, i + 1);
	}
	if (i == TDL_RW_MAX_ATTEMPTS) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][NODE %d:%d:%d] Set DCmd %02X failed; Status = %08X\n", cindex, chain, node, cmd, res);
		return FERSLIB_ERR_COMMUNICATION;
	} else return 0;
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
	char sendbuf[32], recvbuf[32];
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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Write CNC Reg, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_COMMUNICATION;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Write CNC Reg, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_COMMUNICATION;
	}
	res = *((uint32_t *)recvbuf);
	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Write CNC Reg @ %08X; Status = %08X\n", cindex, address, res);
		return FERSLIB_ERR_COMMUNICATION;
	} else return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the concentrator
// Inputs:		cindex = concentrator index
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_CncReadRegister(int cindex, uint32_t address, uint32_t *data) {

	char sendbuf[32], recvbuf[32];
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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Read CNC Reg, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}
	iResult = recv(sck, recvbuf, 8, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Read CNC Reg, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}
	res = *((uint32_t *) &recvbuf[0]);
	*data = *((uint32_t *) &recvbuf[4]);
	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Read CNC Reg @ %08X; Status = %08X\n", cindex, address, res);
		return FERSLIB_ERR_COMMUNICATION;
	}
	else return 0;
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
	char sendbuf[32], recvbuf[2048];
	int iResult, size, p = 0, i, ret;

	// There are two different commands that return concentrator info; this function calls both commands and combine the information into a single struct

	// PART1: "VERS"
	sendbuf[p] = 'V'; p++;
	sendbuf[p] = 'E'; p++;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'S'; p++;

	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc version, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc version, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}

	size = *((uint32_t *)recvbuf);
	if (size > 1024) return FERSLIB_ERR_GENERIC;
	iResult = recv(sck, recvbuf, size, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc version, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}

	strcpy(CncInfo->SW_rev, recvbuf);
	strcpy(CncInfo->FPGA_FWrev, recvbuf + 16);
	sscanf(recvbuf + 48, "%d", &CncInfo->pid);

	// PART2: "RBIC"
	p = 0;
	sendbuf[p] = 'R'; p++;
	sendbuf[p] = 'B'; p++;
	sendbuf[p] = 'I'; p++;
	sendbuf[p] = 'C'; p++;
	iResult = send(sck, sendbuf, p, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc info, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}
	iResult = recv(sck, recvbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc info, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}

	size = *((uint32_t*)recvbuf);
	if (size > 2048) return FERSLIB_ERR_COMMUNICATION;
	iResult = recv(sck, recvbuf, size, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Get cnc info, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return FERSLIB_ERR_GENERIC;
	}

	// Split BIC string (field separated by ';')
	char bic[10][100]; // BIC fields
	char sep[2] = ";";
	char* token = strtok(recvbuf, sep);
	float pcbrev;
	int nlink;
	i = 0;
	while (token != NULL) {
		strcpy(bic[i++], token);
		token = strtok(NULL, sep);
		if (i == 10) break;
	}
	strcpy(CncInfo->ModelName, bic[1]);
	strcpy(CncInfo->ModelCode, bic[2]);
	sscanf(bic[4], "%f", &pcbrev);
	sprintf(CncInfo->PCBrevision, "%.2f", pcbrev);
	strcpy(CncInfo->MACaddr_10GbE, bic[8]);
	sscanf(bic[5], "%d", &nlink);
	CncInfo->NumLink = (uint16_t)nlink;

	// Get Master/Slave configuration
	uint32_t cnc_controller = 0;
	ret = LLtdl_CncReadRegister(cindex, VR_IO_MASTER_SALVE, &cnc_controller);
	if (cnc_controller == 0) {
		CncInfo->MasterSlave = 1;
		FERS_LibMsg("[INOF][CNC %02d] Concentrator set as Master\n", FERS_INDEX(cindex));
	} else {
		CncInfo->MasterSlave = 0;
		FERS_LibMsg("[INOF][CNC %02d] Concentrator set as Salve\n", FERS_INDEX(cindex));
	}
	return 0;
}



// *********************************************************************************************************
// Raw data readout
// *********************************************************************************************************
// Thread that keeps reading data from the data socket (at least until the Rx buffer gets full)
//#define DEBUG_THREAD_MSG
static void* tdl_data_receiver(void *params) {
	int cindex = *(int *)params;
	int nbrx, nbreq, res, empty=0;
	int FlushBuffer = 0;
	int WrReady = 1;
	//static char* bpnt;
	static uint32_t RxBuff_wp[FERSLIB_MAX_NCNC] = { 0 };	// write pointer in Rx data buffer
	char *wpnt;
	uint64_t ct, pt, tstart=0, tstop=0, tdata = 0;
	fd_set socks;
	struct timeval timeout;

	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Data receiver thread is started\n", cindex);
	if (DebugLogs & DBLOG_LL_MSGDUMP) FERS_DebugDump(cindex, "RX thread started\n");
	f_sem_post(&RxSemaphore[cindex]);
	lock(RxMutex[cindex]);
	RxStatus[cindex] = RXSTATUS_IDLE;
	unlock(RxMutex[cindex]);

	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	ct = get_time();
	pt = ct;
	while (1) {
		ct = get_time();
		if (QuitThread[cindex]) {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Data receiver thread is quitting\n", cindex);
			break;
		}
		if (RxStatus[cindex] == RXSTATUS_IDLE) {
			lock(RxMutex[cindex]);
			if ((FERS_ReadoutStatus == ROSTATUS_RUNNING) && empty) {  // start of run
				// Clear Buffers
				ReadData_Init[cindex] = 1;
				RxBuff_wp[cindex] = 0;
				RxB_r[cindex] = 0;
				RxB_w[cindex] = 0;
				WaitingForData[cindex] = 0;
				WrReady = 1;
				FlushBuffer = 0;
				for (int i = 0; i < 2; i++)
					RxB_Nbytes[cindex][i] = 0;
				tstart = ct;
				tdata = ct;
				if (DebugLogs & DBLOG_LL_MSGDUMP) {
					char st[100];
					time_t ss;
					time(&ss);
					strcpy(st, asctime(gmtime(&ss)));
					st[strlen(st) - 1] = 0;
					FERS_DebugDump(cindex, "\nRUN_STARTED at %s\n", st);
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Run Started at %s\n", cindex, st);
				}
				RxStatus[cindex] = RXSTATUS_RUNNING;
				lock(FERS_RoMutex);
				FERS_RunningCnt++;
				FERS_LibMsg("[INFO][CNC %02d] Run Started, FERS_RunningCnt=%d\n", cindex, FERS_RunningCnt);
				unlock(FERS_RoMutex);
			} else {
				// make "dummy" reads while not running to flush old data. Stop when there is no data
				if (!empty) {
					FD_ZERO(&socks);
					FD_SET((int)FERS_DataSocket[cindex], &socks);
					res = select((int)FERS_DataSocket[cindex] + 1, &socks, NULL, NULL, &timeout);
					if (res < 0) {  // socket error, quit thread
						f_socket_cleanup();
						RxStatus[cindex] = -1;
						break;
					}
					if (res == 0) empty = 1;
					else nbrx = recv(FERS_DataSocket[cindex], RxBuff[cindex][0], RX_BUFF_SIZE, 0);
					if (!empty && (DebugLogs & DBLOG_LL_MSGDUMP)) FERS_DebugDump(cindex, "Reading old data...\n");
				}
				unlock(RxMutex[cindex]);
				Sleep(10);
				continue;
			}
			unlock(RxMutex[cindex]);
		}

		if ((RxStatus[cindex] == RXSTATUS_RUNNING) && (FERS_ReadoutStatus != ROSTATUS_RUNNING)) {  // stop of run 
			lock(RxMutex[cindex]);
			tstop = ct;
			RxStatus[cindex] = RXSTATUS_EMPTYING;
			if (DebugLogs & DBLOG_LL_MSGDUMP) FERS_DebugDump(cindex, "Board Stopped. Emptying data (RunTime=%.3f s)\n", 0.001 * (tstop - tstart));
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Stop Command issued (RunTIme=%.3f s)\n", cindex, 0.001 * (tstop - tstart));
			unlock(RxMutex[cindex]);
		}

		if (RxStatus[cindex] == RXSTATUS_EMPTYING) {
			// stop RX for one of these conditions:
			//  - flush command 
			//  - there is no data for more than NODATA_TIMEOUT
			//  - STOP_TIMEOUT after the stop to the boards
			lock(RxMutex[cindex]);
			if ((FERS_ReadoutStatus == ROSTATUS_FLUSHING) || ((ct - tdata) > NODATA_TIMEOUT) || ((ct - tstop) > STOP_TIMEOUT)) {
				RxStatus[cindex] = RXSTATUS_IDLE;
				lock(FERS_RoMutex);
				if (FERS_RunningCnt > 0) --FERS_RunningCnt;
				//unlock(FERS_RoMutex);
				if (DebugLogs & DBLOG_LL_MSGDUMP) FERS_DebugDump(cindex, "Run stopped (RunTime=%.3f s)\n", 0.001 * (ct - tstart));
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] RX Thread is now IDLE. (RunTime=%.3f s). FERS_RunningCnt=%d\n", cindex, 0.001 * (ct - tstart), FERS_RunningCnt);
				unlock(FERS_RoMutex);  // DNIN: here for debug on FERS_RunningCnt
				empty = 0;
				unlock(RxMutex[cindex]);
				continue;
			}
			unlock(RxMutex[cindex]);
		}

		if (!WrReady) {  // end of current buff reached => switch to the other buffer (if available for writing)
			lock(RxMutex[cindex]);
			if (RxB_Nbytes[cindex][RxB_w[cindex]] > 0) {  // the buffer is not empty (being used for reading) => retry later
				unlock(RxMutex[cindex]);
				Sleep(1);
				continue;
			}
			unlock(RxMutex[cindex]);
			WrReady = 1;
		}

		wpnt = RxBuff[cindex][RxB_w[cindex]] + RxBuff_wp[cindex];
		nbreq = RX_BUFF_SIZE - RxBuff_wp[cindex];  // try to read enough bytes to fill the buffer
		FD_ZERO(&socks);
		FD_SET((int)FERS_DataSocket[cindex], &socks);
		res = select((int)FERS_DataSocket[cindex] + 1, &socks, NULL, NULL, &timeout);
		if (res == 0) { // Timeout
			FlushBuffer = 1;
			continue;
		}
		if (res < 0) {  // socket error, quit thread
			f_socket_cleanup();
			lock(RxMutex[cindex]);
			RxStatus[cindex] = -1;
			break;
		}
		nbrx = recv(FERS_DataSocket[cindex], wpnt, nbreq, 0);
		if (nbrx == f_socket_error) {
			f_socket_cleanup();
			lock(RxMutex[cindex]);
			RxStatus[cindex] = -1;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] socket recv failed in data receiver thread\n", cindex);
			break;
		}
		tdata = ct;

#if (THROUGHPUT_METER == 1)
		// Rate Meter: print raw data throughput (RxBuff_wp is not updated, thus the data consumer doesn't see any data to process)
		static uint64_t totnb = 0, lt = 0, l0 = 0;
		if (l0 == 0) {
			l0 = ct;
			lt = ct;
		}
		totnb += nbrx;
		if ((ct - lt) > 1000) {
			printf("%6.1f s: %10.6f MB/s\n", (float)(ct - l0) / 1000, (float)(totnb) / (ct - lt) / 1000);
			totnb = 0;
			lt = ct;
		}
		unlock(RxMutex[cindex]);
		continue;
#endif

		RxBuff_wp[cindex] += nbrx;
		if ((ct - pt) > 10) {  // every 10 ms, check if the data consumer is waiting for data or if the thread has to quit
			if (QuitThread[cindex]) break;
			if (WaitingForData[cindex] && (RxBuff_wp[cindex] > 0)) FlushBuffer = 1;
			pt = ct;
		}

		if ((RxBuff_wp[cindex] == RX_BUFF_SIZE) || FlushBuffer) {  // the current buffer is full or must be flushed
			lock(RxMutex[cindex]);
			RxB_Nbytes[cindex][RxB_w[cindex]] = RxBuff_wp[cindex];
			RxB_w[cindex] ^= 1;  // switch to the other buffer
			RxBuff_wp[cindex] = 0;
			unlock(RxMutex[cindex]);
			WrReady = 0;
			FlushBuffer = 0;
		}
		// Dump data to log file (for debugging)
		if ((DebugLogs & DBLOG_LL_DATADUMP) && (nbrx > 0)) {
			for (int i = 0; i < nbrx; i += 4) {
				uint32_t* d32 = (uint32_t*)(wpnt + i);
				FERS_DebugDump(cindex, "%08X\n", *d32);
			}
			if (Dump[cindex] != NULL) fflush(Dump[cindex]);
		}
		// Saving Raw data output file
		if (FERScfg[cindex] != NULL) {
			if (FERScfg[cindex]->OF_RawData && !FERS_Offline) {
				size_file[cindex] += nbrx;
				if (FERScfg[cindex]->OF_LimitedSize && size_file[cindex] > FERScfg[cindex]->MaxSizeDataOutputFile) {
					lock(rdf_mutex[cindex]);
					LLtdl_IncreaseRawDataSubrun(cindex);
					size_file[cindex] = nbrx;
					unlock(rdf_mutex[cindex]);
				}
				lock(rdf_mutex[cindex]);
				if (RawData[cindex] != NULL) {
					fwrite(wpnt, sizeof(char), nbrx, RawData[cindex]);
					fflush(RawData[cindex]);
				}
				unlock(rdf_mutex[cindex]);
			}
		}
	}

	RxStatus[cindex] = RXSTATUS_OFF;
	unlock(RxMutex[cindex]);
	lock(FERS_RoMutex);
	if (FERS_RunningCnt > 0)
		FERS_RunningCnt = 0;
	FERS_LibMsg("[INFO][CNC %02d] Quitting readout thread. FERS_RunningCnt=%d\n", cindex, FERS_RunningCnt);
	unlock(FERS_RoMutex);
	return NULL;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Copy a data block from RxBuff of the concentrator to the user buffer 
// Inputs:		cindex = concentrator index
//				buff = user data buffer to fill
//				maxsize = max num of bytes being transferred
//				nb = num of bytes actually transferred
// Return:		0=No Data, 1=Good Data 2=Not Running, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_ReadData(int cindex, char *buff, int maxsize, int *nb) {
	char *rpnt;
	//static char* rbuff[FERSLIB_MAX_NCNC] = { 0 };
	static int RdReady[FERSLIB_MAX_NCNC] = { 0 };
	static int Nbr[FERSLIB_MAX_NCNC] = { 0 };
	static int Rd_pnt[FERSLIB_MAX_NCNC] = { 0 };	// read pointer in Rx data buffer

	*nb = 0;
	if (ReadData_Init[cindex]) {
		while (trylock(RxMutex[cindex]));
		RdReady[cindex] = 0;
		Nbr[cindex] = 0;
		ReadData_Init[cindex] = 0;
		unlock(RxMutex[cindex]);
		return 2;
	} 

	lock(RxMutex[cindex]);
	if ((RxStatus[cindex] != RXSTATUS_RUNNING) && (RxStatus[cindex] != RXSTATUS_EMPTYING)) {
		unlock(RxMutex[cindex]);
		return 2;
	}
	unlock(RxMutex[cindex]);

	if (!RdReady[cindex]) {
		if (trylock(RxMutex[cindex]) != 0) return 0;
		if (RxB_Nbytes[cindex][RxB_r[cindex]] == 0) {  // The buffer is empty => assert "WaitingForData" and return 0 bytes to the caller
			WaitingForData[cindex] = 1;
			unlock(RxMutex[cindex]);
			return 0;
		}
		RdReady[cindex] = 1;
		Nbr[cindex] = RxB_Nbytes[cindex][RxB_r[cindex]];  // Get the num of bytes available for reading in the buffer
		Rd_pnt[cindex] = 0;
		WaitingForData[cindex] = 0;
		unlock(RxMutex[cindex]);
	}

	rpnt = RxBuff[cindex][RxB_r[cindex]] + Rd_pnt[cindex]; // rbuff[cindex] + Rd_pnt[cindex];
	*nb = Nbr[cindex] - Rd_pnt[cindex];  // num of bytes currently available for reading in RxBuff
	if (*nb > maxsize) 
		*nb = maxsize;
	if (*nb > 0) {
		memcpy(buff, rpnt, *nb);
		Rd_pnt[cindex] += *nb;
		if (DebugLogs & DBLOG_LL_READDUMP) {
			for (int i = 0; i < *nb; i += 4) {
				uint32_t* d32 = (uint32_t*)(buff + i);
				FERS_DebugDump2(cindex, "%08X\n", *d32);
				fflush(Dump2[cindex]);

			}
		}
	}

	if (Rd_pnt[cindex] == Nbr[cindex]) {  // end of current buff reached => switch to other buffer 
		while (trylock(RxMutex[cindex]));
		RxB_Nbytes[cindex][RxB_r[cindex]] = 0;  
		RxB_r[cindex] ^= 1;
		Rd_pnt[cindex] = 0;
		RdReady[cindex] = 0;
		unlock(RxMutex[cindex]);
	}
	return 1;
}

int LLtdl_ReadData_File(int cindex, char* buff, int64_t maxsize, int* nb, int flushing) {
	//uint8_t stop_loop = 1;
	static int tmp_srun[FERSLIB_MAX_NCNC] = { 0 };
	static int64_t fsizeraw[FERSLIB_MAX_NCNC] = { 0 };
	static FILE* ReadRawData[FERSLIB_MAX_NCNC] = { NULL };
	int fret = 0;
	if (flushing) {  // Used for reset, in case of any Readout Error
		if (ReadRawData[cindex] != NULL)
			fclose(ReadRawData[cindex]);
		tmp_srun[cindex] = 0;
		return 0;
	}
	if (ReadRawData[cindex] == NULL) {	// Open Raw data file
		char filename[200];
		if (EnableSubRun)	// The Raw Data is the one FERS-like with subrun
			sprintf(filename, "%s.%d.frd", RawDataFilenameTdl[cindex], tmp_srun[cindex]);
		else	// The Raw Data has a custom name and there are not subrun
			sprintf(filename, "%s", RawDataFilenameTdl[cindex]);

		ReadRawData[cindex] = fopen(filename, "rb");
		if (ReadRawData[cindex] == NULL) {	// If the file is still NULL, no more subruns are available
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] RawData reprocessing completed.\n", cindex);
			tmp_srun[cindex] = 0;
			return 4;
		}
		f_fseek(ReadRawData[cindex], 0, SEEK_END);	// Get the file size
		fsizeraw[cindex] = f_ftell(ReadRawData[cindex]);
		f_fseek(ReadRawData[cindex], 0, SEEK_SET);

		// Here the file has the correct format. For both files with FERSlib name format
		// and custom one, the header have to be search when tmp_srun is 0
		if (tmp_srun[cindex] == 0) {
			// Read Header keyword
			char file_header[50];
			fret = fread(&file_header, 32, 1, ReadRawData[cindex]);
			if ((strcmp(file_header, "$$$$$$$FERSRAWDATAHEADER$$$$$$$") != 0) && 
				(strcmp(file_header, "$$$$$$FERSRAWDATAHEADERv2$$$$$$") != 0)) { // No header mark found
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] No valid header found in Raw Data filename %s\n.", cindex, filename);
				_setLastLocalError("ERROR: No valid keyword header found");
				fclose(ReadRawData[cindex]);
				return FERSLIB_ERR_GENERIC;
			}
			size_t jump_size_header = 0;
			fret = fread(&jump_size_header, sizeof(jump_size_header), 1, ReadRawData[cindex]);
			f_fseek(ReadRawData[cindex], (long)(jump_size_header - sizeof(size_t)), SEEK_CUR);

			fsizeraw[cindex] -= f_ftell(ReadRawData[cindex]);
		}
	}

	fsizeraw[cindex] -= maxsize;
	if (fsizeraw[cindex] < 0)	// Read what is missing from the current file
		maxsize = maxsize + fsizeraw[cindex]; // fsizeraw is < 0

	fret = fread(buff, sizeof(char), maxsize, ReadRawData[cindex]);

	if (fsizeraw[cindex] <= 0) {
		fclose(ReadRawData[cindex]);
		ReadRawData[cindex] = NULL;
		if (EnableSubRun)
			++tmp_srun[cindex];
		else {
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] RawData reprocessing completed.\n", cindex);
			tmp_srun[cindex] = 0;
			return 4;
		}
	}
	*nb = maxsize;

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
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Flush Cmd, send failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Flush Cmd, recv failed with error: %d\n", cindex, f_socket_errno);
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res != 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Flush Cmd failed. Status = %08X\n", cindex, res);
		return -2;
	}
	else return 0;
}



// *********************************************************************************************************
// Open/Close
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Open the connection to the concentrator through the ethernet or USB interface. 
//				After the connection, the function allocate the memory buffers and starts 
//				the thread that receives the data
// Inputs:		board_ip_addr = IP address of the concentrator
//				cindex = concentrator index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_OpenDevice(char *board_ip_addr, int cindex) {
	int ret = 0, started;

	if (cindex >= FERSLIB_MAX_NBRD) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Max number of CNC (%d) reached!\n", FERSLIB_MAX_NBRD);
		return -1;
	}

	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Opening CNC with IP-ADDR = %s\n", cindex, board_ip_addr);
	FERS_CtrlSocket[cindex] = LLtdl_ConnectToSocket(board_ip_addr, COMMAND_PORT);
	if (FERS_CtrlSocket[cindex] == f_socket_invalid) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Failed to open Control Socket\n", cindex);
		return FERSLIB_ERR_COMMUNICATION;
	}
	Sleep(500);  // Sleep for a while to let the concentrator set up the data socket after opening the control socket
	FERS_DataSocket[cindex] = LLtdl_ConnectToSocket(board_ip_addr, STREAMING_PORT);
	if (FERS_DataSocket[cindex] == f_socket_invalid) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Failed to open Data Socket\n", cindex);
		return FERSLIB_ERR_COMMUNICATION;
	}

	for(int i=0; i<2; i++) {
		RxBuff[cindex][i] = (char *)malloc(RX_BUFF_SIZE);
		FERS_TotalAllocatedMem += RX_BUFF_SIZE;
	}
	//LLBuff[cindex] = (char *)malloc(LLBUFF_SIZE);
	initmutex(RxMutex[cindex]);
	initmutex(rdf_mutex[cindex]);
	QuitThread[cindex] = 0;
	f_sem_init(&RxSemaphore[cindex]);

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
		f_sem_wait(&RxSemaphore[cindex], INFINITE);
		started = 1;
		//lock(RxMutex[cindex]);
		//if (RxStatus[cindex] != 0) 
		//	started = 1;
		//unlock(RxMutex[cindex]);
	}
	for(int chain=0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret |= LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		if (ret != 0) {
			f_sem_destroy(&RxSemaphore[cindex]);
			return ret;
		}
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
	f_sem_destroy(&RxSemaphore[cindex]);

	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Enumerate the TDlink chains. If the enum failed for one chain, the function will retry
//              up to N-times. If the enum fails, the process starts from the first chain again.
// Inputs:		cindex = concentrator index//				DelayAdjust = individual fiber delay adjust
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_EnumerateTDLChains(int cindex, float DelayAdjust[FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES]) {
	int ret = 0, chain;
	int do_sync = 0;
	float max_d = 0, node_delay, flength, del_sum;
	uint32_t i;
	bool enumerated_before;

	// Set Delay on chains
	if (DelayAdjust != NULL) {
		memcpy(TDL_FiberDelayAdjust[cindex], DelayAdjust, sizeof(float) * FERSLIB_MAX_NTDL * FERSLIB_MAX_NNODES);
		InitDelayAdjust[cindex] = 1;
	}

	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		if (TDL_NumNodes[cindex][chain] == 0) continue;
		del_sum = 0;
		for (int i = 0; i < TDL_NumNodes[cindex][chain]; i++) {
			flength = (!InitDelayAdjust[cindex] || (TDL_FiberDelayAdjust[cindex][chain][i] == 0)) ? DEFAULT_FIBER_LENGTH : TDL_FiberDelayAdjust[cindex][chain][i];
			del_sum += FIBER_DELAY(flength);
		}
		max_d = max(del_sum, max_d);
	}
	max_delay[cindex] = (float)ceil(max_d);

	SyncCnc_Completed = 0;

	// get chain info (if already enumerated)
	enumerated_before = true;
	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret |= LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		TDL_ChainStatus[cindex][chain] = tdl_info.Status;
		if (tdl_info.Status == 0) { // disabled
			TDL_NumNodes[cindex][chain] = 0;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Disabled (Status=%d)\n", cindex, chain, TDL_ChainStatus[cindex][chain]);
		} else if ((tdl_info.Status == 1) || (tdl_info.Status == 2)) {  // down or up
			enumerated_before = false;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Not Initialized (Status=%d)\n", cindex, chain, TDL_ChainStatus[cindex][chain]);
			TDL_NumNodes[cindex][chain] = 0;
		} else { // ready or running
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Ready. Num Nodes=%d, RRT=%.1f (Status=%d)\n", cindex, chain, tdl_info.BoardCount, tdl_info.rrt, TDL_ChainStatus[cindex][chain]);
			TDL_NumNodes[cindex][chain] = tdl_info.BoardCount;
		}
	}

	// try enumeration at least N times
	int MaxCyc = 5;
	for (int cyc = 1; cyc <= MaxCyc; cyc++) {
		if (enumerated_before == true)
			break;

		// reset all chains if not enumerated before
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Resetting chains\n", cindex);
		LLtdl_ResetChains(cindex);
		do_sync = 1;

		// enumerate chains 
		enumerated_before = true;
		for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
			if (TDL_ChainStatus[cindex][chain] == 0) continue;  // chain is disabled
			ret = LLtdl_EnumChain(cindex, chain, &TDL_NumNodes[cindex][chain]);
			if (ret != 0) {
				TDL_NumNodes[cindex][chain] = 0;
				if (cyc < MaxCyc) {
					enumerated_before = false; // retry enumeration
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][CNC %02d][CHAIN %02d]: Enumeration failed (cyc %d) => Retry from chain 0\n", cindex, chain, cyc);
					break;
				} else {
					FERS_LibMsg("[ERROR][CNC %02d][CHAIN %02d] Enumeration failed after %d attempts (ret=%d)\n", cindex, chain, cyc);
					_setLastLocalError("Enumeration failed for cnc %d, chain %d after %d attempts (ret=%d)", cindex, chain, cyc, ret);
					return ret;
				}
			} else {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d]: Chain enumerated succesfully. %d nodes found\n", 
					cindex, chain, TDL_NumNodes[cindex][chain]);
			}
		}
	}

	// if all attempts failed => return error
	if (enumerated_before == false) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d]: Enumeration Error after %d attempts!\n", cindex, MaxCyc);
		_setLastLocalError("Enumeration for cnc %d failed after %d attempts", cindex, MaxCyc);
		return FERSLIB_ERR_COMMUNICATION;
	}

	return 0;
}



// --------------------------------------------------------------------------------------------------------- 
// Description: Enumerate the TDlink chains
// Inputs:		cindex = concentrator index
//				DelayAdjust = individual fiber delay adjust
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_EnumerateTDLChains_deprecated(int cindex, float DelayAdjust[FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES]) {
	int ret = 0, chain;
	int do_sync = 0;
	float max_d = 0, node_delay, flength, del_sum;
	uint32_t i;
	bool enumerated_before;

	// Set Delay on chains
	if (DelayAdjust != NULL) {
		memcpy(TDL_FiberDelayAdjust[cindex], DelayAdjust, sizeof(float) * FERSLIB_MAX_NTDL * FERSLIB_MAX_NNODES);
		InitDelayAdjust[cindex] = 1;
	}

	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		if (TDL_NumNodes[cindex][chain] == 0) continue;
		del_sum = 0;
		for (int i = 0; i < TDL_NumNodes[cindex][chain]; i++) {
			flength = (!InitDelayAdjust[cindex] || (TDL_FiberDelayAdjust[cindex][chain][i] == 0)) ? DEFAULT_FIBER_LENGTH : TDL_FiberDelayAdjust[cindex][chain][i];
			del_sum += FIBER_DELAY(flength);
		}
		max_d = max(del_sum, max_d);
	}
	max_delay[cindex] = (float)ceil(max_d);

	SyncCnc_Completed = 0;

	// get chain info (if already enumerated)
	enumerated_before = true;
	for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
		FERS_TDL_ChainInfo_t tdl_info;
		ret |= LLtdl_GetChainInfo(cindex, chain, &tdl_info);
		TDL_ChainStatus[cindex][chain] = tdl_info.Status;
		if (tdl_info.Status == 0) { // disabled
			TDL_NumNodes[cindex][chain] = 0;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Disabled (Status=%d)\n", cindex, chain, TDL_ChainStatus[cindex][chain]);
		} else if ((tdl_info.Status == 1) || (tdl_info.Status == 2)) {  // down or up
			enumerated_before = false;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Not Initialized (Status=%d)\n", cindex, chain, TDL_ChainStatus[cindex][chain]);
			TDL_NumNodes[cindex][chain] = 0;
		} else { // ready or running
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d] Chain Ready. Num Nodes=%d, RRT=%.1f (Status=%d)\n", cindex, chain, tdl_info.BoardCount, tdl_info.rrt, TDL_ChainStatus[cindex][chain]);
			TDL_NumNodes[cindex][chain] = tdl_info.BoardCount;
		}
	}

	// try enumeration at least N times
	int MaxCyc = 3;
	for (int cyc = 1; cyc <= MaxCyc; cyc++) {
		if (enumerated_before == true)
			break;

		// reset all chains if not enumerated before
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Resetting chains\n", cindex);
		LLtdl_ResetChains(cindex);
		do_sync = 1;

		// enumerate chains 
		enumerated_before = true;
		for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
			if (TDL_ChainStatus[cindex][chain] == 0) continue;  // chain is disabled
			ret = LLtdl_EnumChain(cindex, chain, &TDL_NumNodes[cindex][chain]);
			if (ret == FERSLIB_ERR_TDL_CHAIN_DOWN) {
				TDL_NumNodes[cindex][chain] = 0;
				if (cyc == MaxCyc) {
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][CHAIN %d:%d]: Enumeration failed (cyc%d) => This chain will be ignored\n", cindex, chain, cyc);
				} else {
					enumerated_before = false; // retry enumeration
					if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[WARNING][CHAIN %d:%d]: Enumeration failed (cyc%d) => retry\n", cindex, chain, cyc);
					break;
				}
			} else if (ret < 0) {
				TDL_NumNodes[cindex][chain] = 0;
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CHAIN %d:%d]: Enumeration Error %d. This chain will be ignored\n", cindex, chain, ret);  // HACK CTIN: disable this chain?
				return ret;
			} else {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CHAIN %d:%d]: Chain enumerated succesfully. %d nodes found\n", cindex, chain, TDL_NumNodes[cindex][chain]);
			}
		}
	}

	// if all attempts failed => return error
	if (enumerated_before == false) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d]: Enumeration Error after %d attempts!\n", cindex, MaxCyc);
		_setLastLocalError("ERROR: Enumeration Error after %d attempts", MaxCyc);
		return FERSLIB_ERR_COMMUNICATION;
	}

	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Send Sync commands to all chains of the concentrators
// Inputs:		*cnchandle = handle to all concentrator
//				DelayAdjust = individual fiber delay adjust
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_SyncTDLchains(int* cnchandle, uint32_t StartRunMode)
{
	int ret = 0;
	int chain = 0;
	float node_delay[FERSLIB_MAX_NCNC] = { 0 }, flength[FERSLIB_MAX_NCNC] = { 0 }, del_sum[FERSLIB_MAX_NCNC] = { 0 };

	//// Setting Clock - This is now performed by Web Interface
	for (int cindex = 0; cindex < NumCncConnected; ++cindex) {
		if (FERS_isCncMaster(cindex)) { // Concentrator as Master
			// Set CmdTrg Sync for DCmdBroadcast
			uint32_t val = 0x20 | VR_IO_CMD_SYNC;
			ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CMD, val);
			Sleep(10);
			//		// Set Clock source
			//		if ((sync_mode == STARTRUN_TDL) || (sync_mode == STARTRUN_TDL_EXTRUN))
			//			ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CLK_SOURCE, VR_IO_CLKSOURCE_INTERNAL); // VR_IO_CLKSOURCE_INTERNAL);
			//		if (sync_mode == STARTRUN_TDL_EXTRUN_EXTCLK)
			//			ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CLK_SOURCE, ext_clk_src);
			//		if (sync_mode == STARTRUN_TDL_GPS)
			//			ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CLK_SOURCE, VR_IO_CLKSOURCE_LEMO);

			if (ret != 0) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Setting cmd trigger failed\n", cindex);
				_setLastLocalError("ERROR: Setting cmd trigger CNC%d source failed", cindex);
				return FERSLIB_ERR_COMMUNICATION;
			}
		} else {
			// Set CmdTrg
			uint32_t val = 0x20 | VR_IO_CMD_SYNC_B;
			ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CMD, val);
			//		// Set Clock from SyncInA 
			//		ret |= LLtdl_CncWriteRegister(cindex, VR_IO_CLK_SOURCE, VR_IO_CLKSOURCE_SYNC);
			if (ret != 0) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR][CNC %02d] Setting Command Trigger Source failed\n", cindex);
				_setLastLocalError("ERROR: Setting Command Trigger Source for CNC%d source failed", cindex);
				return FERSLIB_ERR_COMMUNICATION;
			}
		}
	}

	// Sending T0 phy
	for (int cindex = 0; cindex < NumCncConnected; ++cindex) {
		if (FERS_isCncMaster(FERS_INDEX(cnchandle[cindex]))) {
			FERS_LibMsg("[INFO][CNC %02d] Send T0 Command Sync\n", cindex);
			ret |= LLtdl_SyncChains(cindex);  // This is the real T0-Command
		}
	}
	if (ret < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Sync T0 Command failed\n");
		_setLastLocalError("ERROR: Sync T0 Command failed");
		return FERSLIB_ERR_COMMUNICATION;
	}

	float delay_max = 0;
	for (int i = 0; i < 4; ++i)
		delay_max = max(delay_max, max_delay[i]);

	// Settings fibers delay for the chains of all CNCs
	for (int cindex = 0; cindex < NumCncConnected; ++cindex) {
		for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
			if (TDL_NumNodes[cindex][chain] == 0) continue;
			node_delay[cindex] = delay_max; // max_delay[cindex];
			for (int i = 0; i < TDL_NumNodes[cindex][chain]; i++) {
				flength[cindex] = (!InitDelayAdjust[cindex] || (TDL_FiberDelayAdjust[cindex][chain][i] == 0)) ? DEFAULT_FIBER_LENGTH : TDL_FiberDelayAdjust[cindex][chain][i];
				node_delay[cindex] -= FIBER_DELAY(flength[cindex]);
				if (node_delay[cindex] < 0) node_delay[cindex] = 0;
				uint32_t data = (chain << 24) | (i << 16) | (uint32_t)node_delay[cindex];
				ret |= LLtdl_CncWriteRegister(cindex, VR_SYNC_DELAY, data);
				if (ret < 0) return FERSLIB_ERR_COMMUNICATION;
			}
		}
	}

	if (ret < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Fiber Delay setting failed\n");
		_setLastLocalError("ERROR: Fiber Delay setting failed");
		return FERSLIB_ERR_COMMUNICATION;
	}

	// Send sync commands to FERS units
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO] Sending Sync commands to Concentrators\n");

	// Disable chains (stop tokens)
	for (int cindex = 0; cindex < NumCncConnected; ++cindex) {
		for (chain = 0; chain < FERSLIB_MAX_NTDL; chain++) {
			if (TDL_NumNodes[cindex][chain] == 0) continue;
			ret |= LLtdl_ControlChain(cindex, chain, 0, 0);
		}
	}

	// Send sync command via DCMD
	ret |= LLtdl_SendDCommandBroadcast(cnchandle, CMD_TDL_SYNC, TDL_COMMAND_DELAY); // TDL_COMMAND_DELAY can be a bit too long for 2 cnc
	if (ret < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Sending TDL Sync Command failed\n");
		_setLastLocalError("ERROR: Sending TDL Sync Command failed");
		return FERSLIB_ERR_COMMUNICATION;
	}

	// Set Synchronization completed
	SyncCnc_Completed = 1;

	return ret;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Set a DCMD to all cnc and execute with a Sw Command (SYNC). CmdTrg Must be set as 'SYNC'
// Inputs:		cnchandle = pointer to the cncs handles
// 				cmd = command opcode	
//				delay = execution delay (must take into account the time it takes for the 
//				command to travel along the chain)
// Return:		true = init done; false = not done
// ---------------------------------------------------------------------------------------------------------
int LLtdl_SendDCommandBroadcast(int *cnchandle, uint32_t cmd, uint32_t delay) {
	int ret = 0;
	uint8_t idxMaster[FERSLIB_MAX_NCNC] = { 0 };
	uint8_t numMaster = 0;  // Only 1 Master, but ...
	for (int i = 0; i < NumCncConnected; ++i) {  //DNIN: WARNING, si presuppone ci sia un unico CNC master
		if (FERS_isCncMaster(FERS_INDEX(cnchandle[i]))) {
			idxMaster[numMaster] = FERS_INDEX(cnchandle[i]);
			++numMaster;
		}
	}
	// Set DCMD to all concentrators
	for (int i = 0; i < NumCncConnected; ++i) 
		ret |= LLtdl_SetDCommandBroadcast(FERS_INDEX(cnchandle[i]), cmd, delay);
	Sleep(10);
	if (ret < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Setting DCMD %" PRIu32 " Broadcast failed\n", cmd);
		_setLastLocalError("ERROR: Setting DCMD %" PRIu32 " Broadcast failed", cmd);
		return FERSLIB_ERR_COMMUNICATION;
	}
	
	// Send CmdTrg 'SW' as Sync command to Master cnc
	for (int i = 0; i < numMaster; ++i)
		ret |= LLtdl_CncWriteRegister(idxMaster[i], VR_IO_SYNC_SEND, 1); 

	if (ret < 0) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Executing DCMD %" PRIu32 " with Sync failed\n", cmd);
		_setLastLocalError("ERROR: Executing DCMD %" PRIu32 " with Sync failed", cmd);
		return FERSLIB_ERR_COMMUNICATION;
	}

	return ret;
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

	// Wait for the thread to stop (with timeout)
	thread_join(ThreadID[cindex], NULL);

	for (int i = 0; i < 100; i++) {
		if (RxStatus[cindex] == RXSTATUS_OFF) break;
		Sleep(1);
	}

	shutdown(FERS_CtrlSocket[cindex], SHUT_SEND);
	if (FERS_CtrlSocket[cindex] != f_socket_invalid) f_socket_close(FERS_CtrlSocket[cindex]);

	shutdown(FERS_DataSocket[cindex], SHUT_SEND);
	if (FERS_DataSocket[cindex] != f_socket_invalid) f_socket_close(FERS_DataSocket[cindex]);

	f_socket_cleanup();
	//thread_join(ethThreadID[cindex]);
	for(int i=0; i<2; i++) {
		if (RxBuff[cindex][i] != NULL) {
			free(RxBuff[cindex][i]);
			RxBuff[cindex][i] = NULL;
			FERS_TotalAllocatedMem -= RX_BUFF_SIZE;
		}
	}

	destroymutex(RxMutex[cindex]);
	destroymutex(rdf_mutex[cindex]);
	//if (LLBuff[cindex] == NULL) free(LLBuff[cindex]);
	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][CNC %02d] Device closed\n", cindex);
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Open the Raw (LL) data output file
// Inputs:		bindex: board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_OpenRawOutputFile(int *handle, int cindex) {
	if (ProcessRawData) return 0;
	
	int cidx = cindex;	// Considered only 1 concentrator connection

	if (FERScfg[cidx]->OF_RawData && RawData[cidx] == NULL) {
		FERS_BoardInfo_t tmpInfo[FERSLIB_MAX_NBRD];

		FERS_CncInfo_t tmpCncInfo;
		FERS_GetCncInfo2(cidx, &tmpCncInfo);
		uint16_t brdConnected = tmpCncInfo.ChainInfo->BoardCount;  // FERS_GetNumBrdConnected();

		char filename[200];
		sprintf(filename, "%s.%d.frd", RawDataFilenameTdl[cidx], subrun[cidx]);
		RawData[cidx] = fopen(filename, "wb");

		size_t header_size =  // sizeof(size_t) + sizeof(CncHandles[cidx]) + sizeof(FERS_CncInfo_t) + tmpCncInfo.ChainInfo->BoardCount * (sizeof(handle[0]) + sizeof(FERS_BoardInfo_t));
			sizeof(size_t) +
			sizeof(int8_t) * 5 + // Library and RawData version
			sizeof(CncHandles[cidx]) +
			sizeof(FERS_CncInfo_t) +
			brdConnected * (sizeof(handle[0]) + sizeof(FERS_BoardInfo_t));
			
		for (int i = 0; i < brdConnected; ++i) {
			if (FERS_CNCINDEX(handle[i]) != cidx) continue; // Only for the current concentrator

			FERS_GetBoardInfo(handle[i], &tmpInfo[i]);
			if (FERS_IsXROC(handle[i])) header_size += sizeof(PedestalLG[i]) + sizeof(PedestalHG[i]);
		}

		char title[32] = "$$$$$$FERSRAWDATAHEADERv2$$$$$$";
		fwrite(&title, sizeof(title), 1, RawData[cidx]);
		fwrite(&header_size, sizeof(header_size), 1, RawData[cidx]);
		
		// Library version
		int8_t my_tmp_version = FERSLIB_VERSION_MAJOR;
		fwrite(&my_tmp_version, sizeof(my_tmp_version), 1, RawData[cidx]);
		my_tmp_version = FERSLIB_VERSION_MINOR; 
		fwrite(&my_tmp_version, sizeof(my_tmp_version), 1, RawData[cidx]);
		my_tmp_version = FERSLIB_VERSION_PATCH;
		fwrite(&my_tmp_version, sizeof(my_tmp_version), 1, RawData[cidx]);
		// RawData version
		my_tmp_version = FERSLIB_RAWDATA_VERSION_MAJOR;
		fwrite(&my_tmp_version, sizeof(my_tmp_version), 1, RawData[cidx]);
		my_tmp_version = FERSLIB_RAWDATA_VERSION_MINOR;
		fwrite(&my_tmp_version, sizeof(my_tmp_version), 1, RawData[cidx]);
		

		// Write Cnc handle and Info
		//FERS_CncInfo_t tmpCInfo;
		//LLtdl_GetCncInfo(cidx, &tmpCInfo);
		fwrite(&CncHandles[cidx], sizeof(CncHandles[cidx]), 1, RawData[cidx]);
		fwrite(&tmpCncInfo, sizeof(FERS_CncInfo_t), 1, RawData[cidx]);

		// Write Board Info
		for (int i = 0; i < brdConnected; ++i) {
			if ((FERS_CONNECTIONTYPE(handle[i]) != FERS_CONNECTIONTYPE_TDL) || (FERS_CNCINDEX(handle[i]) != cidx))
				continue; // In case not all the boards are connected via tdl
			fwrite(&handle[i], sizeof(int), 1, RawData[cidx]);
			fwrite(&tmpInfo[i], sizeof(tmpInfo[i]), 1, RawData[cidx]);
			if (FERS_IsXROC(handle[i])) {
				fwrite(&PedestalLG[i], sizeof(PedestalLG[i]), 1, RawData[cidx]);
				fwrite(&PedestalHG[i], sizeof(PedestalHG[i]), 1, RawData[cidx]);
			}
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Close the Raw (LL) data output file
// Inputs:		bindex: board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLtdl_CloseRawOutputFile(int handle) {
	if (ProcessRawData) return 0;

	int cidx = FERS_CNCINDEX(handle);
	lock(rdf_mutex[cidx]);
	if (RawData[cidx] != NULL) {
		fclose(RawData[cidx]);
		RawData[cidx] = NULL;
	}
	size_file[cidx] = 0;
	subrun[cidx] = 0;
	unlock(rdf_mutex[cidx]);
	return 0;
}
