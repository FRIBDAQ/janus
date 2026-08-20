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

#ifndef _PARAMPARSER_H
#define _PARAMPARSER_H                    // Protect against multiple inclusion

#include "JanusC.h"
#include "FERSutils.h"

#define PARSEMODE_FIRST_CALL		0x01
#define PARSEMODE_PARSE_CONNECTION	0x02
#define PARSEMODE_PARSE_ALL			0x04
#define PARSEMODE_RESET				0x08
//****************************************************************************
// Function prototypes
//****************************************************************************
int ParseConfigFile(FILE *f_ini, Janus_Config_t *J_cfg, int ParseMode);


#endif
