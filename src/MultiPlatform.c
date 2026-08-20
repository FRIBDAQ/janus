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

#include "MultiPlatform.h"
#include "time.h"

//#include <CAENThread.h>
//#include <CAENSocket.h>


#ifdef _WIN32

// --------------------------------------------------------------------------------------------------------- 
// Description: get time from the computer
// Return:		time in ms
// --------------------------------------------------------------------------------------------------------- 
uint64_t j_get_time()
{
	uint64_t time_ms;   // long
	struct _timeb timebuffer;
	_ftime( &timebuffer );
	time_ms = (uint64_t)timebuffer.time * 1000 + (uint64_t)timebuffer.millitm;
	return time_ms;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: get write time of a file 
// Return:		0: OK, -1: error
// --------------------------------------------------------------------------------------------------------- 
int GetFileUpdateTime(char *fname, uint64_t *ftime)
{
    FILETIME ftCreate, ftAccess, ftWrite;
    SYSTEMTIME stUTC, stLocal;
    HANDLE hFile;

	wchar_t wtext[200];
	mbstowcs(wtext, fname, strlen(fname)+1);//Plus null
	LPWSTR ptr = wtext;

	*ftime = 0;
    hFile = CreateFile(ptr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);   // DNIN THIS CREATE A CRASH - "A Geap has been corrupted
    if(hFile == INVALID_HANDLE_VALUE) 
        return -1;

    // Retrieve the file times for the file.
    if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
        return -1;

    // Convert the last-write time to local time.
    FileTimeToSystemTime(&ftWrite, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
	CloseHandle(hFile);    

	//Con_printf("C", "Y=%d M=%d D=%d - H=%d, min=%d, sec=%d\n", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
	*ftime = 366*31*24*3600*(uint64_t)stLocal.wYear   + 
	 	         31*24*3600*(uint64_t)stLocal.wMonth  + 
	 			    24*3600*(uint64_t)stLocal.wDay    + 
	 			       3600*(uint64_t)stLocal.wHour   + 
	 				     60*(uint64_t)stLocal.wMinute + 
						    (uint64_t)stLocal.wSecond;

    return 0;
}


#else

// --------------------------------------------------------------------------------------------------------- 
// Description: get time from the computer
// Return:		time in ms
// --------------------------------------------------------------------------------------------------------- 
uint64_t j_get_time()
{
    uint64_t time_ms;
    struct timeval t1;
//    struct timezone tz;
    gettimeofday(&t1, NULL);
    time_ms = (uint64_t)(t1.tv_sec) * 1000 + (uint64_t)(t1.tv_usec) / 1000;
    return time_ms;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: get write time of a file 
// Return:		0: OK, -1: error
// --------------------------------------------------------------------------------------------------------- 
int GetFileUpdateTime(char *fname, uint64_t *ftime)
{
	struct stat t_stat;

	*ftime = 0;
    stat(fname, &t_stat);
    //struct tm * timeinfo = localtime(&t_stat.st_mtime); 
    *ftime = (uint64_t)t_stat.st_mtime; // last modification time
    return 0;
}

// creates a timespec representing time NOW+ms from epoch
static struct timespec _getTimeSpecFromNow(uint32_t ms) {
	struct timespec ts;
	uint32_t sec = (ms / 1000);
	uint32_t nsec = (ms - sec * 1000) * 1000000;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += sec;
	ts.tv_nsec += nsec;
	//tv_nsec can be now higher than 1e9
	ts.tv_sec += ts.tv_nsec / 1000000000;
	ts.tv_nsec = ts.tv_nsec % 1000000000;
	return ts;
}

#endif

// ***********************************************
// Semaphore functions
// ***********************************************
// Initialized a semaphore
int j_sem_init(j_sem_t* s) {
	if (s == NULL)
		return -1;
	int32_t ret = 0;
#ifndef _WIN32
	if (sem_init(s, 0, 0) != 0)
		ret = -1;
#else
	const HANDLE sem = CreateSemaphoreA(NULL, 0, 1, NULL);
	if (sem == NULL)
		ret = -1;
	else
		*s = sem;
#endif
	return ret;
}

// Destroy a semaphore
int32_t j_sem_destroy(j_sem_t* s) {
	if (s == NULL)
		return -1;
	int32_t ret = 0;
#ifndef _WIN32
	if (sem_destroy(s) != 0)
		ret = -1;
#else
	if (!CloseHandle(*s))
		ret = -1;
#endif
	return ret;
}


// Wait on a semaphore
int32_t j_sem_wait(j_sem_t* s, int32_t ms) {
	if (s == NULL)
		return -1;
	int32_t ret = 0;
#ifndef _WIN32
	int r;
	switch (ms) {
	case 0: {
		struct timespec ts = { 0 };
		r = (sem_timedwait(s, &ts) == 0) ? 0 : errno;
		break;
	}
	case INFINITE:
		r = (sem_wait(s) == 0) ? 0 : errno;
		break;
	default: {
		const struct timespec ts = _getTimeSpecFromNow(ms);
		r = (sem_timedwait(s, &ts) == 0) ? 0 : errno;
		break;
	}
	}
	switch (r) {
	case 0:
		break;
	case ETIMEDOUT:
		ret = -1;
		break;
	default:
		ret = -1;
		break;
	}
#else
	const DWORD r = WaitForSingleObjectEx(*s, (DWORD)ms, FALSE);
	switch (r) {
	case WAIT_OBJECT_0:
		break;
	case WAIT_TIMEOUT:
		ret = -1;
		break;
	default:
		ret = -1;
		break;
	}
#endif
	return ret;
}


// Post on a semaphore
int32_t j_sem_post(j_sem_t* s) {
	if (s == NULL)
		return -1;
	int32_t ret = 0;
#ifndef _WIN32
	if (sem_post(s) != 0)
		ret = -1;
#else
	if (!ReleaseSemaphore(*s, 1, NULL))
		ret = -1;
#endif
	return ret;
}