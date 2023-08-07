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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#include "FERSlib.h"
#include "FERSutils.h"
#include "console.h"

#include "JanusC.h"
#include "Statistics.h"

// *********************************************************
// Pixel Map
// *********************************************************
int map_ch2x[FERSLIB_MAX_NCH], map_ch2y[FERSLIB_MAX_NCH], map_xy2ch[8][8];
int MapInit = 0;

int Read_ch2xy_Map (char *filename)
{
	int i, ch, x, y;
	char spix[100];
	FILE *map;

	// Set default mapping (totally arbitrary)
	for(i=0; i<FERSLIB_MAX_NCH; i++) {
		map_ch2x[i] = i%8;
		map_ch2y[i] = i/8;
	}
	if (filename == NULL) return 0;

	map = fopen(filename, "r");
	if (map == NULL) return -1;
	for(i=0; i<FERSLIB_MAX_NCH; i++) {
		if (feof(map)) break;
		fscanf(map, "%d", &ch);
		fscanf(map, "%s", spix);
		if ((ch >= 0) && (ch < FERSLIB_MAX_NCH)) {
			x = toupper(spix[0]) - 'A';
			y = spix[1] - '1';
			map_ch2x[ch] = x;
			map_ch2y[ch] = y;
			map_xy2ch[x][y] = ch;
		}
	}
	fclose(map);
	MapInit = 1;
	return 0;
}

int ch2x(int ch)
{
	if (!MapInit) return ch%8;
	else return map_ch2x[ch];
}

int ch2y(int ch)
{
	if (!MapInit) return ch/8;
	else return map_ch2y[ch];
}

int xy2ch(int x, int y)
{
	if (!MapInit) return y*8+x;
	else return map_xy2ch[x][y];
}

void PrintMap()
{
	int i, j, x, y;
	ClearScreen();
	printf("\n");
	for (y = 0; y < 8; y++) {
		printf("   +----+----+----+----+----+----+----+----+\n");
		printf(" %c |", '1' + 7 - y);
		for (x = 0; x < 8; x++) {
			printf(" %02d |", xy2ch(x, 7-y));
		}
		printf("\n");
	}
	printf("   +----+----+----+----+----+----+----+----+\n");
	printf ("     A    B    C    D    E    F    G    H\n\n\n");

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 4; j++) {
			int ch = i+j*16;
			printf("  Ch%02d=%c%c  ", ch, 'A'+ch2x(ch), '1'+ch2y(ch));
		}
		printf("\n");
	}
	getch();
	ClearScreen();
}

#ifdef FERS_5202
// ****************************************************
// HV Control Panel
// ****************************************************
void HVControlPanel(int handle) 
{
	int c=0;
	int HV_onoff, Ramp, OvC, OvV;
	uint64_t ct=0, pt=0;
	uint32_t RegAddr=0, DataType=1, Rdata=0, Wdata=0;
	float vbias = 0, imax = 10, vmon = 0, imon = 0, temp = 0;
	char onoff_string[2][4] = {"OFF", "ON "};

	ClearScreen();
	while(1) {
		ct = get_time();
		if ((pt == 0) || ((ct-pt) > 1000)) {
			HV_Get_Vmon(handle, &vmon);
			HV_Get_Imon(handle, &imon);
			HV_Get_Vbias(handle, &vbias);
			HV_Get_Imax(handle, &imax);
			HV_Get_Status(handle, &HV_onoff, &Ramp, &OvC, &OvV);
			HV_Get_DetectorTemp(handle, &temp);
			gotoxy(1, 2);
			printf("[v] Vset         %.3f V          \n", vbias);
			printf("[i] Imax         %.3f mA         \n", imax);
			printf("[H] ON/OFF       %s              \n", onoff_string[HV_onoff]);
			printf("[a] Reg Addr     %d              \n", RegAddr);
			printf("[t] Data Type    %d              \n", DataType);
			printf("[r] Read HV Reg  %d (0x%08X)     \n", Rdata, Rdata);
			printf("[w] Write HV Reg %d (0x%08X)     \n", Wdata, Wdata);
			printf("[q] Return\n\n");
			printf("Vmon = %.3f V              \n", vmon);
			printf("Imon = %.3f mA             \n", imon);
			printf("OvC = %d, OvV = %d         \n", OvC, OvV);
			printf("Ramping = %d               \n", Ramp);
			printf("Detector Temp = %.1f degC     \n", temp);
			pt = ct;
		}

		if (kbhit()) { // DNIN: Con_kbhit ??
			c = Con_getch();
			if (c == 'v') {
				float newvset;
				printf("Set HV (V) = ");
				scanf("%f", &newvset);
				HV_Set_Vbias(handle, newvset);
				HV_Get_Vbias(handle, &vbias);
			}
			if (c == 'i') {
				float newimax;
				printf("Set Imax (mA) = ");
				scanf("%f", &newimax);
				HV_Set_Imax(handle, newimax);
				HV_Get_Imax(handle, &imax);
			}
			if (c == 'a') {
				printf("Reg Addr = ");
				scanf("%d", (int*)&RegAddr);
			}
			if (c == 't') {
				printf("Data Type = ");
				scanf("%d", (int*)&DataType);
			}
			if (c == 'r') {
				HV_ReadReg(handle, RegAddr, DataType, &Rdata);
			}
			if (c == 'w') {
				char str[100];
				printf("Reg Data (0x for hex) = ");
				scanf("%s", str);
				if (str[1] == 'x') sscanf(str + 2, "%x", &Wdata);
				else sscanf(str, "%d", &Wdata);
				HV_WriteReg(handle, RegAddr, DataType, Wdata);
			}
			if (c == 'H') {
				HV_onoff ^= 1;
				HV_Set_OnOff(handle, HV_onoff);
			}
			if (c == 'q') break;
			ClearScreen();
		}
	}
	ClearScreen();
}

// ****************************************************
// Citiroc Control Panel
// ****************************************************
void CitirocControlPanel(int handle) 
{
	int c=0, print_menu=1, reload_cfg=0;
	uint32_t tthr, qthr, lgg, hgg, lgst, hgst;

	FERS_ReadRegister(handle, a_qd_coarse_thr, &qthr);	// Threshold for Q-discr
	FERS_ReadRegister(handle, a_td_coarse_thr, &tthr);	// Threshold for T-discr
	FERS_ReadRegister(handle, a_lg_sh_time, &lgst);		// Shaping Time LG
	FERS_ReadRegister(handle, a_hg_sh_time, &hgst);		// Shaping Time HG
	FERS_ReadRegister(handle, INDIV_ADDR(a_lg_gain, 0), &lgg);	// Gain (low gain) of Ch0
	FERS_ReadRegister(handle, INDIV_ADDR(a_hg_gain, 0), &hgg);	// Gain (high gain) of Ch0

	while(c != 'q') {
		if (Con_kbhit()) {	
			c = Con_getch();
			if (c=='s') {
				printf("Shaping Time LG = ");
				scanf("%d", &lgst);
				FERS_WriteRegister(handle, a_lg_sh_time, lgst);	// Shaping Time for LG
				reload_cfg = '1';
			}
			if (c=='S') {
				printf("Shaping Time HG = ");
				scanf("%d", &hgst);
				FERS_WriteRegister(handle, a_hg_sh_time, hgst);	// Shaping Time for HG
				reload_cfg = '1';
			}
			if (c=='g') {
				printf("Gain LG = ");
				scanf("%d", &lgg);
				FERS_WriteRegister(handle, BCAST_ADDR(a_lg_gain), lgg);	// Gain for LG
				reload_cfg = '1';
			}
			if (c=='G') {
				printf("Gain HG = ");
				scanf("%d", &hgg);
				FERS_WriteRegister(handle, BCAST_ADDR(a_hg_gain), hgg);	// Gain for HG
				reload_cfg = '1';
			}
			if (c=='t') {
				printf("Time Threshold = ");
				scanf("%d", &tthr);
				FERS_WriteRegister(handle, a_td_coarse_thr, tthr);	// Threshold for T-discr
				reload_cfg = '1';
			}
			if (c=='T') {
				printf("Charge Threshold = ");
				scanf("%d", &qthr);
				FERS_WriteRegister(handle, a_qd_coarse_thr, qthr);	// Threshold for Q-discr
				reload_cfg = '1';
			}
			if (reload_cfg) {
				FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);  // set citiroc index = 0
				FERS_SendCommand(handle, CMD_CFG_ASIC);
				Sleep(10);
				FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);  // set citiroc index = 1
				FERS_SendCommand(handle, CMD_CFG_ASIC);  
				reload_cfg = 0;
			}
			print_menu = 1;
		}
		if (print_menu) {
			printf("\n");
			printf("[s] set LG sh. time   %6d\n", lgst);
			printf("[S] set HG sh. time   %6d\n", hgst);
			printf("[g] set LG gain       %6d\n", lgg);
			printf("[G] set HG gain       %6d\n", hgg);
			printf("[t] set time thr      %6d\n", tthr);
			printf("[T] set charge thr    %6d\n", qthr);
			printf("[q] quit\n");
			print_menu = 0;
		}
	}
	ClearScreen();
}
#endif

// ****************************************************
// Manual Controller for register R/W access
// ****************************************************
void ManualController(int handle) 
{
	int c=0, i, print_menu=1, pagenum;
	static uint32_t base=0x0100, ch=0, offs=0, addr, rdata=0, wdata=0, cmd=0;
	static uint32_t i2c_reg_addr=0, i2c_reg_rdata=0, i2c_reg_wdata=0, i2c_dev_index=0;
	uint8_t fpage[FLASH_PAGE_SIZE];
	uint32_t *buff = NULL;
	char fname[100];
	char i2c_dev_name[4][20] = {"TDC0", "TDC1", "PLL0", "PLL1"};
	uint32_t i2c_dev_addr[4] = {I2C_ADDR_TDC(0), I2C_ADDR_TDC(1), I2C_ADDR_PLL0, I2C_ADDR_PLL1};
	FILE *fp;

	while(c != 'q') {
		if (Con_kbhit()) {
			c = Con_getch();
			if (c=='b') {
				if      (base == 0x0100) base = 0x0200;
				else if (base == 0x0200) base = 0x0300;
				else if (base == 0x0300) base = 0x0100;
			}
			if (c=='c') {
				printf("Channel = ");
				scanf("%d", &ch);
				ch &= 0x3F;
			}
			if (c=='+') {
				if (ch < 64) ch++;
			}
			if (c=='-') {
				if (ch > 0) ch--;
			}
			if ((c=='a') || (c=='1')) {
				printf("Address (offset only) = ");
				scanf("%x", &offs);
				offs &= 0xFFFF;
			}
			if (c=='w') {
				printf("Data = ");
				scanf("%x", &wdata);
				FERS_WriteRegister(handle, addr, wdata);
			}
			if (c=='s') {
				printf("Command = ");
				scanf("%x", &cmd);
				FERS_SendCommand(handle, cmd);
			}
			if (c=='r') {
				FERS_ReadRegister(handle, addr, &rdata);
			}
			if (c=='i') {
				while(1) {
					ClearScreen();
					printf("[d] I2C Dev Addr  : %s\n", i2c_dev_name[i2c_dev_index]);
					printf("[a] I2C Reg Addr  : %04X\n", i2c_reg_addr);
					printf("[r] Read Cycle    : %08X\n", i2c_reg_rdata);
					printf("[w] Write Cycle   : %08X\n", i2c_reg_wdata);
					/*printf("[0] Configure PLL0\n");
					printf("[1] Configure PLL1\n");*/
					printf("[q] Quit I2C\n");
					c = getch();
					if (c == 'd') i2c_dev_index = (i2c_dev_index + 1) % 4;
					if (c == 'q') {
						c = 0;
						break;
					}
					if ((c == 'a') || (c == '1')) {
						printf("Enter Reg Addr (Hex): ");
						scanf("%x", &i2c_reg_addr);
					}
					if (c == 'w') {
						printf("Enter Reg Data (Hex): ");
						scanf("%x", &i2c_reg_wdata);
						FERS_I2C_WriteRegister(handle, i2c_dev_addr[i2c_dev_index], i2c_reg_addr, i2c_reg_wdata);
					}
					if (c == 'r') {

						FERS_I2C_ReadRegister(handle, i2c_dev_addr[i2c_dev_index], i2c_reg_addr, &i2c_reg_rdata);
					}
					if (c == 'l') {
						char line[500];
						int PLLindex = 0, ret = 0;
						char fname[200] = "D:\\work\\a5203\\PLL\\Si5394_Reg.txt";
						uint32_t addr, data_r, devaddr;
						FILE* reg = fopen(fname, "r");
						FILE* PLL_reg = fopen("D:\\work\\a5203\\PLL\\PLL_read_reg.txt", "w");
						if (reg == NULL) {
							printf("ERR: can't open %s", fname);
						} else {
							devaddr = (PLLindex == 0) ? I2C_ADDR_PLL0 : I2C_ADDR_PLL1;
							while (!feof(reg)) {
								fgets(line, 100, reg);
								sscanf(line + 2, "%x", &addr);
								if (line[0] != '0') continue;
								ret |= FERS_I2C_ReadRegister(handle, devaddr, addr, &data_r);
								if (ret < 0) {
									printf("Error in read register 0x%04X\n", addr);
									break;
								}
								fprintf(PLL_reg, "0x%04X,0x%02X\n", addr, data_r);
								printf("%04X - %02X\n", addr, data_r);
							}						
						}
						fclose(PLL_reg);
						fclose(reg);
						
						getch();
					}

					if (c == 'p') {
						char line[500];
						int s = 0;
						int PLLindex = 1, ret = 0;
						int cnt_r = 0;
						char fname[200] = "D:\\work\\a5203\\PLL\\Si5394_Reg.txt";
						uint32_t addr, data, devaddr;
						FILE* reg = fopen(fname, "r");
						if (reg == NULL) {
							printf("ERR: can't open %s", fname);
						} else {
							devaddr = (PLLindex == 0) ? I2C_ADDR_PLL0 : I2C_ADDR_PLL1;
							while (!feof(reg)) {
								fgets(line, 100, reg);
								if (line[0] != '0') continue;
								sscanf(line + 2, "%x", &addr);
								sscanf(line + 9, "%x", &data);
								printf("%04X - %08X\n", addr, data);
								FERS_I2C_WriteRegister(handle, devaddr, addr, data);  // CTIN: patch for bug in FW (write 4 bytes intead of 1 in PLL)
								if ((addr == 0x0540) && (cnt_r == 0)) {
									Sleep(500);
									cnt_r++;
								} else
									cnt_r = 0;
							}
						}
						printf("Operation ended successfully\n");
						fclose(reg);
						getch();
					}
				}
			}


			if (c=='R') {
				printf("Enter page number : ");
				scanf("%d", &pagenum);
				FERS_ReadFlashPage(handle, pagenum, FLASH_PAGE_SIZE, fpage);
				ClearScreen();
				fp = fopen("flashpage.txt", "w");
				for(i=0; i<FLASH_PAGE_SIZE; i++) {
					if ((i%16) == 0) printf("[%03d:%03d] ", i, i+15);
					printf("%02X ", fpage[i]);
					if ((i%16) == 15) printf("\n");
					if (fp != NULL) fprintf(fp, "%02X\n", fpage[i]);
				}
				if (fp != NULL) {
					printf("Flash data saved to file flashpage.txt\n");
					fclose(fp);
				}
			}
			if (c=='W') {
				printf("Enter page number : ");
				scanf("%d", &pagenum);
				printf("Enter file name : ");
				scanf("%s", fname);
				fp = fopen("flashpage.txt", "r");
				if (fp != NULL) {
					for(i=0; i<FLASH_PAGE_SIZE; i++) {
						int v;
						fscanf(fp, "%x", &v);
						fpage[i] = (uint8_t)v;
						if (feof(fp)) break;
					}
					fclose(fp);
				}
				FERS_WriteFlashPage(handle, pagenum, FLASH_PAGE_SIZE, fpage);
			}
#ifdef FERS_5202
			if (c=='p') {
				char date[20];
				uint16_t pedLG[FERSLIB_MAX_NCH], pedHG[FERSLIB_MAX_NCH];
				uint16_t DCoffset[4];
				FERS_ReadPedestalsFromFlash(handle, date, pedLG, pedHG, DCoffset);
				ClearScreen();
				printf("CH   PedLG   PedHG        CH   PedLG   PedHG\n");
				for(ch=0; ch<FERSLIB_MAX_NCH/2; ch++) {
					printf("%02d:   %4d    %4d        %02d:   %4d    %4d\n", ch, pedLG[ch], pedHG[ch], 32+ch, pedLG[32+ch], pedHG[32+ch]);
				}
				printf("\nDCoffset: LG0=%4d  HG0=%4d  LG1=%4d   HG1=%4d\n", DCoffset[0], DCoffset[1], DCoffset[2], DCoffset[3]);
				getch();
			}

			
#endif
			print_menu = 1;
		}
		if (base == 0x0200)
			addr = (base << 16) | (ch << 16) | offs;
		else
			addr = (base << 16) | offs;
		if (print_menu) {
			ClearScreen();
			printf("Board n. %d\n", FERS_INDEX(handle));
			printf("[b] Change base      (%04X)\n", base);
			printf("[c] Change channel   (%02d)\n", ch);
			printf("[+] Next channel\n");
			printf("[-] Prev channel\n");
			printf("[a] Set address      (%08X)\n", addr);
			printf("[r] Read reg         (%08X)\n", rdata);
			printf("[w] Write reg        (%08X)\n", wdata);
			printf("[s] Send command     (%02X)\n", cmd);
			printf("[i] I2C R/W reg\n");
			printf("[R] Read flash page\n");
			printf("[W] Write flash page\n");
#ifdef FERS_5202
			printf("[p] read pedestal calibration\n");
#endif
			printf("[q] Return\n");
			print_menu = 0;
		}
	}
	if (buff != NULL) free(buff);
	ClearScreen();
}


// *********************************************************
// Calculate Pedestals
// *********************************************************
#ifdef FERS_5202
#define CALIB_NCYC  200
int AcquirePedestals(int handle, uint16_t *pedestalLG, uint16_t *pedestalHG)
{
	int nb, dtq; 
	uint32_t i, nn, rmask;
	uint32_t ml[64], mh[64];
	void *Event;

	for(i=0; i<64; i++) {
		ml[i] = 0;
		mh[i] = 0;
	}

	/*
	FERS_SendCommand(handle, CMD_RESET);  // Reset

	FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);  // set citiroc index = 0
	FERS_SendCommand(handle, CMD_CFG_ASIC);
	FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);  // set citiroc index = 1
	FERS_SendCommand(handle, CMD_CFG_ASIC);  
	*/

	FERS_ReadRegister(handle, a_run_mask, &rmask);  
	FERS_WriteRegister(handle, a_run_mask, 1);  // swrun
	FERS_WriteRegister(handle, a_acq_ctrl, ACQMODE_SPECT);
	FERS_WriteRegister(handle, a_trg_mask, 0x1);  // SW Trigger
	FERS_WriteRegisterSlice(handle, a_acq_ctrl, 12, 13, GAIN_SEL_BOTH);  // Set Gain Selection = Both
	FERS_EnablePedestalCalibration(handle, 0);

	Sleep(100);
	FERS_SendCommand(handle, CMD_ACQ_STOP);  // Stop Command (in case the board is still running)
	FERS_FlushData(handle);
	FERS_SendCommand(handle, CMD_ACQ_START);  // Start Command
	nn = 0;
	while (nn < CALIB_NCYC) {
		double ts;
		FERS_SendCommand(handle, CMD_TRG);  // SW trigger
		FERS_GetEventFromBoard(handle, &dtq, &ts, &Event, &nb);
		if (nb>0) {
			SpectEvent_t *Ev = (SpectEvent_t  *)Event;
			for(i=0; i<64; i++) {
				ml[i] += Ev->energyLG[i];
				mh[i] += Ev->energyHG[i];
			}
			nn++;
		}
	}

	for(i=0; i<64; i++) {
		pedestalLG[i] = (uint16_t)(ml[i] / CALIB_NCYC);
		pedestalHG[i] = (uint16_t)(mh[i] / CALIB_NCYC);
	}

	FERS_EnablePedestalCalibration(handle, 1);
	FERS_WriteRegister(handle, a_run_mask, rmask);  // swrun
    return 0;
}


// *********************************************************
// Scan Thresholds 
// *********************************************************
int ScanThreshold(int handle)
{
	int i, s, brd;
	uint32_t thr;
	uint32_t hitcnt[FERSLIB_MAX_NCH], Tor_cnt, Qor_cnt;
	FILE *st;

	brd = FERS_INDEX(handle);
	uint16_t start = RunVars.StaircaseCfg[SCPARAM_MIN];
	uint16_t step = RunVars.StaircaseCfg[SCPARAM_STEP];
	uint16_t nstep = (RunVars.StaircaseCfg[SCPARAM_MAX] - RunVars.StaircaseCfg[SCPARAM_MIN])/RunVars.StaircaseCfg[SCPARAM_STEP] + 1;
	float dwell_s = (float)RunVars.StaircaseCfg[SCPARAM_DWELL] / 1000;

	Con_printf("CSm", "Scanning thresholds (press 'S' to stop):\n");
	Con_printf("Sa", "%02dRunning Staircase (0 %%)", ACQSTATUS_STAIRCASE);

	st = fopen(SCAN_THR_FILENAME, "w");
	FERS_WriteRegister(handle, a_acq_ctrl, ACQMODE_COUNT);
	FERS_WriteRegisterSlice(handle, a_acq_ctrl, 27, 29, 0);  // Set counting mode = singles
	FERS_WriteRegister(handle, a_dwell_time, (uint32_t)(dwell_s * 1e9 / CLK_PERIOD)); 
	FERS_WriteRegister(handle, a_qdiscr_mask_0, WDcfg.Q_DiscrMask0[brd]); 
	FERS_WriteRegister(handle, a_qdiscr_mask_1, WDcfg.Q_DiscrMask1[brd]);  
	FERS_WriteRegister(handle, a_tdiscr_mask_0, WDcfg.Tlogic_Mask0[brd]);  
	FERS_WriteRegister(handle, a_tdiscr_mask_1, WDcfg.Tlogic_Mask1[brd]);  
	FERS_WriteRegister(handle, a_citiroc_cfg, 0x00070f20); // Q-discr direct (not latched)
	FERS_WriteRegister(handle, a_lg_sh_time, SHAPING_TIME_25NS); // Shaping Time LG
	FERS_WriteRegister(handle, a_hg_sh_time, SHAPING_TIME_25NS); // Shaping Time HG
	FERS_WriteRegister(handle, a_trg_mask, 0x1); // SW trigger only
	FERS_WriteRegister(handle, a_t1_out_mask, 0x10); // PTRG (for debug)
	FERS_WriteRegister(handle, a_t0_out_mask, 0x04); // T-OT (for debug)

	// Start Scan
	Sleep(100);
	Con_printf("CSm", "            --------- Rate (cps) ---------\n");
	Con_printf("CSm", " Adv  Thr     ChMean       T-OR       Q-OR  \n");
	for(s = nstep; s >= 0; s--) {
		thr = start + s * step;
		FERS_WriteRegister(handle, a_qd_coarse_thr, thr);	// Threshold for Q-discr
		FERS_WriteRegister(handle, a_td_coarse_thr, thr);	// Threshold for T-discr
		FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);		// set citiroc index = 1
		FERS_SendCommand(handle, CMD_CFG_ASIC);
		Sleep(20);
		FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);		// set citiroc index = 0
		FERS_SendCommand(handle, CMD_CFG_ASIC);
		Sleep(20);
		FERS_WriteRegister(handle, a_trg_mask, 0x20); // enable periodic trigger
		FERS_SendCommand(handle, CMD_RES_PTRG);  // Reset period trigger counter and count for dwell time
		Sleep((int)(dwell_s/1000 + 200));  // wait for a complete dwell time (+ margin), then read counters
		FERS_ReadRegister(handle, a_t_or_cnt, &Tor_cnt);
		FERS_ReadRegister(handle, a_q_or_cnt, &Qor_cnt);
		if (s < nstep) {  // skip 1st pass 
			uint64_t chmean = 0;
			fprintf(st, "%5d ", thr);
			for(i=0; i<FERSLIB_MAX_NCH; i++) {
				FERS_ReadRegister(handle, a_hitcnt + (i << 16), &hitcnt[i]);
				chmean += (uint64_t)hitcnt[i];
				fprintf(st, "%8.3e ", hitcnt[i]/dwell_s);
				Stats.Staircase[FERS_INDEX(handle)][i][s] = hitcnt[i]/dwell_s; 
			}
			chmean /= FERSLIB_MAX_NCH;
			int perc = (100 * (nstep-s)) / nstep;
			if (perc > 100) perc = 100;
			Con_printf("CSm", "%3d%%%5d  %8.3e  %8.3e  %8.3e\n", perc, thr, chmean/dwell_s, Tor_cnt/dwell_s, Qor_cnt/dwell_s);
			Con_printf("Sa", "%02dRunning Staircase (%d %%)", ACQSTATUS_STAIRCASE, perc);
			fprintf(st, "%10.3e %10.3e ", Tor_cnt/dwell_s, Qor_cnt/dwell_s);
			fprintf(st, "\n");
		}
		if (Con_kbhit()) {
			if (Con_getch() == 'S')
				break;
		}
	}

	Con_printf("Sa", "%02dDone\n", ACQSTATUS_STAIRCASE);
	fclose(st);
    return 0;
}


// *********************************************************
// Scan Hold Delay
// *********************************************************
#define MAX_SH_NSTEP	64
int ScanHoldDelay(int handle)
{
	uint32_t delay, i, ns, si;
	int ch, nb, dtq, x, y;
	int hh = handle;
	void *Event;
	int b = FERS_INDEX(handle);

	uint16_t start = RunVars.HoldDelayScanCfg[HDSPARAM_MIN];
	uint16_t stop = RunVars.HoldDelayScanCfg[HDSPARAM_MAX];
	uint16_t step = RunVars.HoldDelayScanCfg[HDSPARAM_STEP] & 0xFFF8;
	uint16_t nmean = RunVars.HoldDelayScanCfg[HDSPARAM_NMEAN];
	uint16_t nstep = (stop - start) / step + 1;
	if (nstep > MAX_SH_NSTEP) nstep = MAX_SH_NSTEP;

	if (!(WDcfg.AcquisitionMode & ACQMODE_SPECT)) {
		Con_printf("CSw", "WARNING: Scan: Need Spectroscopy mode for Hold Scan\n");
		return 1;
	}

	for(ch=0; ch<MAX_NCH; ch++) {
		if (Stats.Hold_PHA_2Dmap[b][ch] != NULL) free(Stats.Hold_PHA_2Dmap[b][ch]);
		Stats.Hold_PHA_2Dmap[b][ch] = (uint32_t *)calloc(512 * nstep * 2, sizeof(uint32_t)); // DNIN: this works but the problem need to be further investigated(double the memory allocation)
	}

	Con_printf("CSm", "Scanning Hold Delay (press 'S' to stop)\n");
	Con_printf("Sa", "%02dRunning Hold-Scan (0 %%)", ACQSTATUS_HOLD_SCAN);

	FERS_StopAcquisition(&hh, 1, STARTRUN_ASYNC);
	FERS_WriteRegisterSlice(handle, a_acq_ctrl, 0, 3, ACQMODE_SPECT);
	FERS_WriteRegisterSlice(handle, a_acq_ctrl, 12, 13, 3); // 0=auto, 1=high gain, 2=low gain, 3=both
	FERS_WriteRegister(handle, a_trg_mask, WDcfg.TriggerMask);  
	FERS_WriteRegister(handle, a_run_mask, 0x01); 
	FERS_SetCommonPedestal(handle, WDcfg.Pedestal);

	for(si = 0; si < nstep; si++) {
		delay = start + step * si;
		FERS_WriteRegister(handle, a_hold_delay, delay / CLK_PERIOD);	// Hold delay (step = 8 ns)
		FERS_StartAcquisition(&hh, 1, STARTRUN_ASYNC);
		Sleep(10);
		ns = 0;

		for(i=0; i<nmean; i++) {
			int niter = 0;
			nb = 0;
			while ((nb == 0) && (niter < 500)) {
				double ts;
				FERS_GetEventFromBoard(handle, &dtq, &ts, &Event, &nb);
				if (nb == 0) Sleep(10);
				niter++;
			}
			if (nb > 0) {
				SpectEvent_t *Ev = (SpectEvent_t *)Event;
				x = si;
				for(ch=0; ch<MAX_NCH; ch++) {
					if (WDcfg.Range_14bit) y = Ev->energyHG[ch] >> 5;
					else y = Ev->energyHG[ch] >> 4;
					if (y > 511) y = 511;
					Stats.Hold_PHA_2Dmap[b][ch][y*nstep + x]++;
				}
				ns++;
			} else {
				break;
			}
		}
		if (i < nmean) {
			break;
		}
		Con_printf("CSm", "Hold-Delay = %3d ns (%2d %%)\n", delay, 100*(delay-start)/(stop-start));
		Con_printf("Sa", "%02dRunning Hold-Scan (%d %%)", ACQSTATUS_HOLD_SCAN, 100*(delay-start)/(stop-start));
		FERS_StopAcquisition(&hh, 1, STARTRUN_ASYNC);
		Sleep(100);
		FERS_FlushData(handle);
		if (Con_kbhit()) {
			if (Con_getch() == 'S')
				break;
		}

	}
	Con_printf("Sa", "%02dRunning Hold-Scan (100 %%)", ACQSTATUS_HOLD_SCAN);
	Con_printf("Sa", "%02dDone\n", ACQSTATUS_HOLD_SCAN);
    return 0;
}
#endif
