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

// *********************************************************************************************************
// Global variables	
// *********************************************************************************************************
static f_socket_t FERS_CtrlSocket[FERSLIB_MAX_NBRD] = {f_socket_invalid};	// slow control (R/W reg)
static f_socket_t FERS_DataSocket[FERSLIB_MAX_NBRD] = {f_socket_invalid};	// data streaming
static char *RxBuff[FERSLIB_MAX_NBRD][2] = { NULL };	// Rx data buffers (two "ping-pong" buffers, one write, one read)
static uint32_t RxBuff_rp[FERSLIB_MAX_NBRD] = { 0 };	// read pointer in Rx data buffer
static uint32_t RxBuff_wp[FERSLIB_MAX_NBRD] = { 0 };	// write pointer in Rx data buffer
static int RxB_w[FERSLIB_MAX_NBRD] = { 0 };				// 0 or 1 (which is the buffer being written)
static int RxB_r[FERSLIB_MAX_NBRD] = { 0 };				// 0 or 1 (which is the buffer being read)
static int RxB_Nbytes[FERSLIB_MAX_NBRD][2] = { 0 };		// Number of bytes written in the buffer
static int WaitingForData[FERSLIB_MAX_NBRD] = { 0 };	// data consumer is waiting fro data (data receiver should flush the current buffer)
static int RxStatus[FERSLIB_MAX_NBRD] = { 0 };			// 0=not started, 1=idle (wait for run), 2=running (taking data), 3=closing run (reading data in the pipes)
static int QuitThread[FERSLIB_MAX_NBRD] = { 0 };		// Quit Thread
static f_thread_t ThreadID[FERSLIB_MAX_NBRD];			// RX Thread ID
static mutex_t RxMutex[FERSLIB_MAX_NBRD];				// Mutex for the access to the Rx data buffer and pointers
static FILE *Dump[FERSLIB_MAX_NBRD] = { NULL };			// low level data dump files (for debug)
static uint8_t ReadData_Init[FERSLIB_MAX_NBRD] = { 0 }; // Re-init read pointers after run stop

#define ETH_BLK_SIZE  (1024)						// Max size of one packet in the recv 
#define RX_BUFF_SIZE  (1024*1024)					// Size of the local Rx buffer

// *********************************************************************************************************
// Socket connection
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Connect the socket for the communication with the FERS board (either slow control and readout)
// Inputs:		board_ip_addr = IP address of the board
//				port = socket port (different for slow control and data readout)
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
f_socket_t LLeth_ConnectToSocket(char *board_ip_addr, char *port)
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
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("socket failed with error: %ld\n", f_socket_errno); // WSAGetLastError());
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
		ioctlsocket(ConnectSocket, FIONBIO, &ul); //set as blocking
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
// R/W Reg via I2C (old version, slow... don't use it)
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the FERS board (via I2C)
// Inputs:		bindex = FERS index
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_WriteReg_i2c(int bindex, uint32_t address, uint32_t data)
{
	uint32_t res;
	char sendbuf[32];
	int iResult;
	f_socket_t sck = FERS_CtrlSocket[bindex];

	sendbuf[0] = 'A';
	sendbuf[1] = 'B';
	sendbuf[2] = 'B';
	sendbuf[3] = 'A';
	memcpy(&sendbuf[4], &address, 4);
	memcpy(&sendbuf[8], &data, 4);
	iResult = send(sck, sendbuf, 12, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("WriteReg_i2c, send with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("WriteReg_i2c, recv failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}
	res = *((uint32_t *)sendbuf);
	if (res == 0) return -2;
	else return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the FERS board (via I2C)
// Inputs:		bindex = FERS index
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_ReadReg_i2c(int bindex, uint32_t address, uint32_t *data)
{
	char sendbuf[32];
	int iResult;
	f_socket_t sck = FERS_CtrlSocket[bindex];
	
	sendbuf[0] = 'A';
	sendbuf[1] = 'B';
	sendbuf[2] = 'B';
	sendbuf[3] = 'C';
	memcpy(&sendbuf[4], &address, 4);
	iResult = send(sck, sendbuf, 8, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("ReadReg_i2c, send failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}
	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult < 4)
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Incomplete read reg\n");
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("ReadReg_i2c, recv failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}
	*data = *((uint32_t *)sendbuf);
	return 0;
}


// *********************************************************************************************************
// R/W Reg via SPI (new version, faster than I2C)
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Write a memory block to the FERS board (via SPI)
// Inputs:		bindex = FERS index
//				address = mem address (1st location)
//				data = reg data
//				size = num of bytes being written
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_WriteMem(int bindex, uint32_t address, char *data, uint16_t size)
{
	uint32_t res;
	char *sendbuf = (char *) malloc(12+size);// [32];
	int iResult;
	f_socket_t sck = FERS_CtrlSocket[bindex];
	sendbuf[0] = 'A';
	sendbuf[1] = 'B';
	sendbuf[2] = 'B';
	sendbuf[3] = 'D';
	memcpy(&sendbuf[4], &address, 4);
	memcpy(&sendbuf[8], &size, 4);
	memcpy(&sendbuf[12], data, size);

	iResult = send(sck, sendbuf, 12+size, 0);
 	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write mem, send failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		free(sendbuf);
		return -1;
	}

	iResult = recv(sck, sendbuf, 4, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Write mem, recv failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		free(sendbuf);
		return -1;
	}

	res = *((uint32_t *)sendbuf);
	free(sendbuf);

	if (res != 0) return -2;
	else return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a memory block from the FERS board (via SPI)
// Inputs:		bindex = FERS index
//				address = mem address (1st location)
//				size = num of bytes being written
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_ReadMem(int bindex, uint32_t address, char *data, uint16_t size)
{
	char sendbuf[32];
	int iResult;
	f_socket_t sck = FERS_CtrlSocket[bindex];
	sendbuf[0] = 'A';
	sendbuf[1] = 'B';
	sendbuf[2] = 'B';
	sendbuf[3] = 'E';
	memcpy(&sendbuf[4], &address, 4);
	memcpy(&sendbuf[8], &size, 4);

	iResult = send(sck, sendbuf, 12, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read mem, send failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}

	iResult = recv(sck, data, size, 0);
	if (iResult == f_socket_error) {
		if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("Read mem, recv failed with error: %d\n", f_socket_errno); // WSAGetLastError());
		f_socket_cleanup();
		return -1;
	}
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the FERS board (via SPI)
// Inputs:		bindex = FERS index
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_WriteRegister(int bindex, uint32_t address, uint32_t data) {
	return LLeth_WriteMem(bindex, address, (char *)&data, 4);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the FERS board (via SPI)
// Inputs:		bindex = FERS index
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_ReadRegister(int bindex, uint32_t address, uint32_t *data) {
	return LLeth_ReadMem(bindex, address, (char *)data, 4);
}


// *********************************************************************************************************
// Raw data readout
// *********************************************************************************************************
// Thread that keeps reading data from the data socket (at least until the Rx buffer gets full)
static void* eth_data_receiver(void *params) {
	int bindex = *(int *)params;
	int nbrx, nbreq, res, empty=0;
	int FlushBuffer = 0;
	int WrReady = 1;
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
		lock(RxMutex[bindex]);
		if (QuitThread[bindex]) break;
		if (RxStatus[bindex] == RXSTATUS_IDLE) {
			if ((FERS_ReadoutStatus == ROSTATUS_RUNNING) && empty) {  // start of run
				// Clear Buffers
				ReadData_Init[bindex] = 1;
				RxBuff_rp[bindex] = 0;
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
		}

		if ((RxStatus[bindex] == RXSTATUS_RUNNING) && (FERS_ReadoutStatus != ROSTATUS_RUNNING)) {  // stop of run 
			tstop = ct;
			RxStatus[bindex] = RXSTATUS_EMPTYING;
			if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Board Stopped. Emptying data (T=%.3f)\n", 0.001 * (tstop - tstart));
		}

		if (RxStatus[bindex] == RXSTATUS_EMPTYING) {
			// stop RX for one of these conditions:
			//  - flush command 
			//  - there is no data for more than NODATA_TIMEOUT
			//  - STOP_TIMEOUT after the stop to the boards
			if ((FERS_ReadoutStatus == ROSTATUS_FLUSHING) || ((ct - tdata) > NODATA_TIMEOUT) || ((ct - tstop) > STOP_TIMEOUT)) {  
				RxStatus[bindex] = RXSTATUS_IDLE;
				lock(FERS_RoMutex);
				if (FERS_RunningCnt > 0) FERS_RunningCnt--;
				unlock(FERS_RoMutex);
				if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Run stopped (T=%.3f)\n", 0.001 * (ct - tstart));
				empty = 0;
				unlock(RxMutex[bindex]);
				continue;
			}
		}

		if (!WrReady) {  // end of current buff reached => switch to the other buffer (if available for writing)
			if (RxB_Nbytes[bindex][RxB_w[bindex]] > 0) {  // the buffer is not empty (being used for reading) => retry later
				unlock(RxMutex[bindex]);
				Sleep(1);
				continue;
			}
			WrReady = 1;
		} 

		wpnt = RxBuff[bindex][RxB_w[bindex]] + RxBuff_wp[bindex];
		nbreq = RX_BUFF_SIZE - RxBuff_wp[bindex];  // try to read enough bytes to fill the buffer
		unlock(RxMutex[bindex]);

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

		lock(RxMutex[bindex]);
		RxBuff_wp[bindex] += nbrx;
		if ((ct - pt) > 10) {  // every 10 ms, check if the data consumer is waiting for data or if the thread has to quit
			if (QuitThread[bindex]) break;  
			if (WaitingForData[bindex] && (RxBuff_wp[bindex] > 0)) FlushBuffer = 1;
			pt = ct;
		}

		if ((RxBuff_wp[bindex] == RX_BUFF_SIZE) || FlushBuffer) {  // the current buffer is full or must be flushed
			RxB_Nbytes[bindex][RxB_w[bindex]] = RxBuff_wp[bindex];
			RxB_w[bindex] ^= 1;  // switch to the other buffer
			RxBuff_wp[bindex] = 0;
			WrReady = 0;
			FlushBuffer = 0;
		}
		// Dump data to log file (for debugging)
		if ((DebugLogs & DBLOG_LL_DATADUMP) && (nbrx > 0) && (Dump[bindex] != NULL)) {
			for(int i=0; i<nbrx; i+=4) {
				uint32_t *d32 = (uint32_t *)(wpnt + i);
				fprintf(Dump[bindex], "%08X\n", *d32);
			}
			fflush(Dump[bindex]);
		}
		unlock(RxMutex[bindex]);
	}

	RxStatus[bindex] = RXSTATUS_OFF;
	unlock(RxMutex[bindex]);
	return NULL;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Copy a data block from RxBuff of the FERS board to the user buffer 
// Inputs:		bindex = board index
//				buff = user data buffer to fill
//				maxsize = max num of bytes being transferred
//				nb = num of bytes actually transferred
// Return:		0=No Data, 1=Good Data 2=Not Running, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_ReadData(int bindex, char *buff, int maxsize, int *nb) {
	char *rpnt;
	static int RdReady[FERSLIB_MAX_NBRD] = { 0 };
	static int Nbr[FERSLIB_MAX_NBRD] = { 0 };

	*nb = 0;
	if (trylock(RxMutex[bindex]) != 0) return 0;
	if (ReadData_Init[bindex]) {
		RdReady[bindex] = 0;
		Nbr[bindex] = 0;
		ReadData_Init[bindex] = 0;
		unlock(RxMutex[bindex]);
		return 2;
	} 

	if ((RxStatus[bindex] != RXSTATUS_RUNNING) && (RxStatus[bindex] != RXSTATUS_EMPTYING)) {
		unlock(RxMutex[bindex]);
		return 2;
	}

	if (!RdReady[bindex]) {
		if (RxB_Nbytes[bindex][RxB_r[bindex]] == 0) {  // The buffer is empty => assert "WaitingForData" and return 0 bytes to the caller
			WaitingForData[bindex] = 1;
			unlock(RxMutex[bindex]);
			return 0;
		}
		RdReady[bindex] = 1;
		Nbr[bindex] = RxB_Nbytes[bindex][RxB_r[bindex]];  // Get the num of bytes available for reading in the buffer
		WaitingForData[bindex] = 0;
	}

	rpnt = RxBuff[bindex][RxB_r[bindex]] + RxBuff_rp[bindex];
	*nb = Nbr[bindex] - RxBuff_rp[bindex];  // num of bytes currently available for reading in RxBuff
	if (*nb > maxsize) *nb = maxsize;
	if (*nb > 0) {
		memcpy(buff, rpnt, *nb);
		RxBuff_rp[bindex] += *nb;
	}

	if (RxBuff_rp[bindex] == Nbr[bindex]) {  // end of current buff reached => switch to other buffer 
		RxB_Nbytes[bindex][RxB_r[bindex]] = 0;  
		RxB_r[bindex] ^= 1;
		RxBuff_rp[bindex] = 0;
		RdReady[bindex] = 0;
	}
	unlock(RxMutex[bindex]);
	return 1;
}



// *********************************************************************************************************
// Open/Close
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Open the direct connection to the FERS board through the ethernet interface. 
//				After the connection the function allocates the memory buffers starts the thread  
//				that receives the data
// Inputs:		board_ip_addr = IP address of the concentrator
//				bindex = board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_OpenDevice(char *board_ip_addr, int bindex) {
	int started, ret;

	if (bindex >= FERSLIB_MAX_NBRD) return -1;

	FERS_CtrlSocket[bindex] = LLeth_ConnectToSocket(board_ip_addr, COMMAND_PORT);
	FERS_DataSocket[bindex] = LLeth_ConnectToSocket(board_ip_addr, STREAMING_PORT);
	if ((FERS_CtrlSocket[bindex] == f_socket_invalid) || (FERS_DataSocket[bindex] == f_socket_invalid)) 
		return FERSLIB_ERR_COMMUNICATION;

	for(int i=0; i<2; i++) {
		RxBuff[bindex][i] = (char *)malloc(RX_BUFF_SIZE);
		FERS_TotalAllocatedMem += RX_BUFF_SIZE;
	}
	initmutex(RxMutex[bindex]);
	QuitThread[bindex] = 0;
	if ((DebugLogs & DBLOG_LL_DATADUMP) || (DebugLogs & DBLOG_LL_MSGDUMP)) {
		char filename[200];
		sprintf(filename, "ll_log_%d.txt", bindex);
		Dump[bindex] = fopen(filename, "w");
	}
	thread_create(eth_data_receiver, &bindex, &ThreadID[bindex]);
	started = 0;
	while(!started) {
		lock(RxMutex[bindex]);
		if (RxStatus[bindex] != 0) started = 1;
		unlock(RxMutex[bindex]);
	}

	ret = LLeth_WriteRegister(bindex, a_commands, CMD_ACQ_STOP);	// stop acquisition (in case it was still running)
	ret |= LLeth_WriteRegister(bindex, a_commands, CMD_CLEAR);		// clear data in the FPGA FIFOs
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Close the connection to the FERS board and free buffers
// Inputs:		bindex: board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLeth_CloseDevice(int bindex)
{
	lock(RxMutex[bindex]);
	QuitThread[bindex] = 1;
	unlock(RxMutex[bindex]);

	shutdown(FERS_CtrlSocket[bindex], SHUT_SEND);	
	if (FERS_CtrlSocket[bindex] != f_socket_invalid) {
		f_socket_close(FERS_CtrlSocket[bindex]);
	}

	shutdown(FERS_DataSocket[bindex], SHUT_SEND);
	if (FERS_DataSocket[bindex] != f_socket_invalid) {
		f_socket_close(FERS_DataSocket[bindex]);
	}
	f_socket_cleanup();
	//thread_join(ethThreadID[bindex]);
	for(int i=0; i<2; i++)
		if (RxBuff[bindex][i] != NULL) free(RxBuff[bindex][i]);
	if (Dump[bindex] != NULL) fclose(Dump[bindex]);
	return 0;
}


