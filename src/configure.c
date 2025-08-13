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

#include "MultiPlatform.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "FERSlib.h"	
#include "FERSutils.h"	
#include "JanusC.h"	
#include "configure.h"
#include "console.h"

#define NW_SCBS (1144/32+1)  // Num of 32 bit word to contain the SC bit stream

// ---------------------------------------------------------------------------------
// Description: Program the registers of the FERS unit with the relevant parameters
// Inputs:		handle = board handle
//				brd = board number
//				mode: 0=hard cfg (includes reset), 1=soft cfg (no acq restart)
// Outputs:		-
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int ConfigureFERS(int handle, int mode)
{
	int i, ret = 0;  

	uint32_t d32, CitirocCfg, Tpulse_ctrl = 0;
	char CfgStep[100];
	int brd = FERS_INDEX(handle);
	uint32_t SCbs[2][NW_SCBS];  // Citiroc SC bit stream

	int rundelay = 0;
	int deskew = -5;

	// ########################################################################################################
	sprintf(CfgStep, "General System Configuration and Acquisition Mode");
	// ########################################################################################################
	// Reset the unit 
	if (mode == CFG_HARD) {
		ret = FERS_SendCommand(handle, CMD_RESET);  // Reset
		if (ret != 0) goto abortcfg;
	}
	// Channel Enable Mask
	ret |= FERS_WriteRegister(handle, a_channel_mask_0, WDcfg.ChEnableMask0[brd]);  
	ret |= FERS_WriteRegister(handle, a_channel_mask_1, WDcfg.ChEnableMask1[brd]);  

	// Set USB or Eth communication mode
	if ((FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_ETH) || (FERS_CONNECTIONTYPE(handle) == FERS_CONNECTIONTYPE_USB))
		ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 20, 20, 1); // Force disable of TDlink
	else if (FERS_FPGA_FW_MajorRev(handle) >= 4)  
		ret |= FERS_WriteRegister(handle, a_uC_shutdown, 0xDEAD); // Shut down uC (PIC) if not used (readout via TDL)
	
	// Acq Mode
	if (mode == CFG_HARD) ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 0, 3, WDcfg.AcquisitionMode);

	if (FERS_FPGA_FW_MajorRev(handle) >= 4) {
		// Enable Zero suppression in counting mode (only with FW >= 4.00)
		ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 30, 30, WDcfg.EnableCntZeroSuppr);
		// Service event Mode
		if (WDcfg.AcquisitionMode == ACQMODE_COUNT)
			ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 18, 19, WDcfg.EnableServiceEvents & 0x1); 
		else
			ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 18, 19, WDcfg.EnableServiceEvents); 
	}

	//ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 4, 7, WDcfg.TimingMode); // timing mode: 0=common start, 1=common stop, 2=streaming  
	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 8, 9, WDcfg.EnableToT); // timing info: 0=leading edge only, 1=leading edge + ToT

	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 24, 25, WDcfg.Validation_Mode); // 0=disabled, 1=accept, 2=reject

	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 27, 29, WDcfg.Counting_Mode); // 0=singles, 1=paired_AND
	ret |= FERS_WriteRegister(handle, a_hit_width, WDcfg.ChTrg_Width/CLK_PERIOD); // Monostable on Citiroc Self triggers => Coinc Window for Trigger logic and counting in paired-AND mode
	ret |= FERS_WriteRegister(handle, a_tlogic_width, WDcfg.Tlogic_Width/CLK_PERIOD); // Monostable on Trigger Logic Output (0=linear)

	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 26, 26, WDcfg.TrgIdMode); // Trigger ID: 0=trigger counter, 1=validation counter

	//ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 17, 17, 1); // enable blank conversion (citiroc conversion runs also when the board is busy, but data are not saved)
	//ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 16, 16, 1); // set ADC test mode (ADC chips generate fixed patterns = 0x2AAA - 0x1555)
	//ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 23, 23, 1); // set FPGA test mode (generate event with fixed data pattern)
	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 12, 13, WDcfg.GainSelect); // 0=auto, 1=high gain, 2=low gain, 3=both

	ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl, 21, 21, WDcfg.Range_14bit); // use 14 bit for the A/D conversion of the peak (energy)

	// Set fixed pedestal (applied to PHA after subtraction of the calibrated pedestals)
	ret |= FERS_SetCommonPedestal(handle, WDcfg.Pedestal);
	ret |= FERS_EnablePedestalCalibration(handle, 1);

	// Set LEMO I/O mode
	ret |= FERS_WriteRegister(handle, a_t0_out_mask, WDcfg.T0_outMask[FERS_INDEX(handle)]);
	ret |= FERS_WriteRegister(handle, a_t1_out_mask, WDcfg.T1_outMask[FERS_INDEX(handle)]);

	// Waveform length
	if (WDcfg.WaveformLength > 0) {
		ret |= FERS_WriteRegister(handle, a_wave_length, WDcfg.WaveformLength);  
	}
	// Set Periodic Trigger
	ret |= FERS_WriteRegister(handle, a_dwell_time, (uint32_t)(WDcfg.PtrgPeriod/CLK_PERIOD));
	// Set Trigger Source
	ret |= FERS_WriteRegister(handle, a_trg_mask, WDcfg.TriggerMask);  

	// enable 2nd time stamp (relative to Tref)
	if (WDcfg.Enable_2nd_tstamp)
		ret |= FERS_WriteRegisterSlice(handle, a_acq_ctrl2, 0, 0, 1);

	// Set Run Mask
	rundelay = (WDcfg.NumBrd - FERS_INDEX(handle) - 1) * 4;
	if (WDcfg.StartRunMode == STARTRUN_ASYNC) {
		ret |= FERS_WriteRegister(handle, a_run_mask, 0x01); 
	} else if (WDcfg.StartRunMode == STARTRUN_CHAIN_T0) {
		if (FERS_INDEX(handle) == 0) {
			ret |= FERS_WriteRegister(handle, a_run_mask, (deskew << 16) | (rundelay << 8) | 0x01); 
			ret |= FERS_WriteRegister(handle, a_t0_out_mask, 1 << 10);  // set T0-OUT = RUN_SYNC
		} else {
			ret |= FERS_WriteRegister(handle, a_run_mask, (deskew << 16) | (rundelay << 8) | 0x82); 
			//ret |= FERS_WriteRegister(handle, a_t0_out_mask, 1);  // set T0-OUT = T0-IN
			ret |= FERS_WriteRegister(handle, a_t0_out_mask, 1 << 10);  // set T0-OUT = RUN_SYNC
		}
		//if (WDcfg.T0_outMask != (1 << 4)) Con_printf("LCSw", "WARNING: T0-OUT setting has been overwritten for Start daisy chaining\n");
	} else if (WDcfg.StartRunMode == STARTRUN_CHAIN_T1) {
		if (FERS_INDEX(handle) == 0) {
			ret |= FERS_WriteRegister(handle, a_run_mask, (deskew << 16) | (rundelay << 8) | 0x01); 
			ret |= FERS_WriteRegister(handle, a_t1_out_mask, 1 << 10);  // set T1-OUT = RUN_SYNC
		} else {
			ret |= FERS_WriteRegister(handle, a_run_mask, (deskew << 16) | (rundelay << 8) | 0x84); 
			ret |= FERS_WriteRegister(handle, a_t1_out_mask, 1);  // set T1-OUT = T1-IN
		}
		//if (WDcfg.T0_outMask != (1 << 4)) Con_printf("LCSw", "WARNING: T1-OUT setting has been overwritten for Start daisy chaining\n");
	} else if (WDcfg.StartRunMode == STARTRUN_TDL) {
		ret |= FERS_WriteRegister(handle, a_run_mask, 0x01); 
	}

	// Set Tref mask
	FERS_WriteRegister(handle, a_tref_mask, WDcfg.Tref_Mask);
	// Set Tref window
	FERS_WriteRegister(handle, a_tref_window, (uint32_t)(WDcfg.TrefWindow/((float)CLK_PERIOD/16)));
	//uint32_t trf_w = (uint32_t)(WDcfg.TrefWindow / ((float)CLK_PERIOD / 16));
	// Set Tref delay
	if ((WDcfg.AcquisitionMode == ACQMODE_TIMING_CSTART) || (WDcfg.AcquisitionMode == ACQMODE_TSPECT))
		FERS_WriteRegister(handle, a_tref_delay, (uint32_t)(WDcfg.TrefDelay/(float)CLK_PERIOD));
	else if (WDcfg.AcquisitionMode == ACQMODE_TIMING_CSTOP) {
		uint32_t td = ((uint32_t)(-WDcfg.TrefWindow / ((float)CLK_PERIOD)) + (uint32_t)(WDcfg.TrefDelay / ((float)CLK_PERIOD)));
		FERS_WriteRegister(handle, a_tref_delay, td);
	}


	// Set Trigger Logic
	FERS_WriteRegister(handle, a_tlogic_def, (WDcfg.MajorityLevel << 8) | (WDcfg.TriggerLogic & 0xFF));
	// Set Veto mask
	FERS_WriteRegister(handle, a_veto_mask, WDcfg.Veto_Mask);
	// Set Validation mask
	FERS_WriteRegister(handle, a_validation_mask, WDcfg.Validation_Mask);
	
	// Set Test pulse Source and Amplitude
	if (WDcfg.TestPulseSource == -1) {  // OFF
		ret |= FERS_WriteRegister(handle, a_tpulse_ctrl, 0);  // Set Tpulse = external 
		ret |= FERS_WriteRegister(handle, a_tpulse_dac, 0);
		//else if (WDcfg.TestPulseDestination == TEST_PULSE_DEST_NONE) ??? this is not available;
	} else {
		Tpulse_ctrl = WDcfg.TestPulseSource;
		if      (WDcfg.TestPulseDestination == TEST_PULSE_DEST_ALL)  Tpulse_ctrl |= 0x00;
		else if (WDcfg.TestPulseDestination == TEST_PULSE_DEST_EVEN) Tpulse_ctrl |= 0x10;
		else if (WDcfg.TestPulseDestination == TEST_PULSE_DEST_ODD)  Tpulse_ctrl |= 0x20;
		//else if (WDcfg.TestPulseDestination == TEST_PULSE_DEST_NONE) ??? this is not available;
		else Tpulse_ctrl |= 0x30 | (WDcfg.TestPulseDestination << 6);
		Tpulse_ctrl |= ((WDcfg.TestPulsePreamp & 0x3) << 12);
		ret |= FERS_WriteRegister(handle, a_tpulse_ctrl, Tpulse_ctrl);  
		ret |= FERS_WriteRegister(handle, a_tpulse_dac, WDcfg.TestPulseAmplitude); 
	}
	ConfigureProbe(handle);

	// ---------------------------------------------------------------------------------
	// Set Citiroc Parameters
	// ---------------------------------------------------------------------------------
	ret |= FERS_WriteRegister(handle, a_qd_coarse_thr, WDcfg.QD_CoarseThreshold);	// Threshold for Q-discr
	ret |= FERS_WriteRegister(handle, a_td_coarse_thr, WDcfg.TD_CoarseThreshold[brd]);	// Threshold for T-discr
	//ret |= FERS_WriteRegister(handle, a_td_coarse_thr, WDcfg.TD_CoarseThreshold);	// Threshold for T-discr

	ret |= FERS_WriteRegister(handle, a_lg_sh_time, WDcfg.LG_ShapingTime);		// Shaping Time LG
	ret |= FERS_WriteRegister(handle, a_hg_sh_time, WDcfg.HG_ShapingTime);		// Shaping Time HG

	// Set charge discriminator mask
	ret |= FERS_WriteRegister(handle, a_qdiscr_mask_0, WDcfg.Q_DiscrMask0[brd]);  
	ret |= FERS_WriteRegister(handle, a_qdiscr_mask_1, WDcfg.Q_DiscrMask1[brd]);  

	// Set time discriminator mask (applied in FPGA, not in Citiroc)
	ret |= FERS_WriteRegister(handle, a_tdiscr_mask_0, WDcfg.Tlogic_Mask0[brd]);  
	ret |= FERS_WriteRegister(handle, a_tdiscr_mask_1, WDcfg.Tlogic_Mask1[brd]);  

	// Citiroc Modes
	CitirocCfg = 0;
	CitirocCfg |= WDcfg.EnableQdiscrLatch   << crcfg_qdiscr_latch;	// Qdiscr output: 1=latched, 0=direct                        (bit 258 of SR) 
	CitirocCfg |= WDcfg.PeakDetectorMode    << crcfg_pdet_mode_hg;	// Peak_det mode HighGain: 0=peak detector, 1=T&H            (bit 306 of SR)
	CitirocCfg |= WDcfg.PeakDetectorMode    << crcfg_pdet_mode_lg;	// Peak_det mode LowGain:  0=peak detector, 1=T&H            (bit 307 of SR)
	CitirocCfg |= WDcfg.FastShaperInput     << crcfg_pa_fast_sh;	// Fast shaper connection: 0=high gain pa, 1=low gain pa     (bit 328 of SR)
	CitirocCfg |= WDcfg.HV_Adjust_Range     << crcfg_8bit_dac_ref;	// HV adjust DAC reference: 0=2.5V, 1=4.5V                   (bit 330 of SR)
	CitirocCfg |= WDcfg.EnableChannelTrgout << crcfg_enable_chtrg;	// 0 = Channel Trgout Disabled, 1 = Enabled
	// Hard coded settings
	CitirocCfg |= 0 << crcfg_sca_bias;			// SCA bias: 0=high (5MHz readout speed), 1=weak             (bit 301 of SR)
	CitirocCfg |= 0 << crcfg_ps_ctrl_logic;		// Peak Sens Ctrl Logic: 0=internal, 1=use ext. PS_modeb_ext (bit 308 of SR)
	CitirocCfg |= 1 << crcfg_ps_trg_source;		// Peak Sens Trg source: 0=internal, 1=external              (bit 309 of SR)
	CitirocCfg |= 0 << crcfg_lg_pa_bias;		// LG Preamp bias: 0=normal, 1=weak                          (bit 323 of SR)
	CitirocCfg |= 1 << crcfg_ota_bias;			// Output OTA buffer bias auto off: 0=auto, 1=force on       (bit 1133 of SR)
	CitirocCfg |= 1 << crcfg_trg_polarity;		// Trigger polarity: 0=pos, 1=neg                            (bit 1141 of SR)
	CitirocCfg |= 1 << crcfg_enable_gtrg;		// Enable propagation of gtrg to the Citiroc pin global_trig
	CitirocCfg |= 1 << crcfg_enable_veto;		// Enable propagation of gate (= not veto) to the Citiroc pin val_evt
	CitirocCfg |= 1 << crcfg_repeat_raz;		// Enable REZ iterations when Q-latch is not reset (bug in Citiroc chip ?)
	ret |= FERS_WriteRegister(handle, a_citiroc_cfg, CitirocCfg);	

	//ret |= FERS_WriteRegisterBit(handle, a_citiroc_en, 26, 0); // disable trgouts

	ret |= FERS_WriteRegister(handle, a_hold_delay, WDcfg.HoldDelay / CLK_PERIOD);  // Time between trigger and peak sensing Hold

	if (WDcfg.MuxClkPeriod > 0) 
		ret |= FERS_WriteRegisterSlice(handle, a_amux_seq_ctrl, 0, 6, WDcfg.MuxClkPeriod / CLK_PERIOD);		// Period of the mux clock
	ret |= FERS_WriteRegisterSlice(handle, a_amux_seq_ctrl, 8, 9, WDcfg.MuxNSmean);

	// Set Trigger Hold-off (for channel triggers)
	FERS_WriteRegister(handle, a_trgho, (uint32_t)(WDcfg.Trg_HoldOff/CLK_PERIOD));

	if (ret) goto abortcfg;

	// ########################################################################################################
	sprintf(CfgStep, "Channel settings");
	// ########################################################################################################	
	for(i=0; i<MAX_NCH; i++) {
		uint16_t PedHG, PedLG, zs_thr_lg, zs_thr_hg, ediv;
		ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_lg_gain, i), WDcfg.LG_Gain[brd][i]);	// Gain (low gain)
		ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_hg_gain, i), WDcfg.HG_Gain[brd][i]);	// Gain (high gain)
		ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_qd_fine_thr, i), WDcfg.QD_FineThreshold[brd][i]);		
		ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_td_fine_thr, i), WDcfg.TD_FineThreshold[brd][i]);

		FERS_GetChannelPedestalBeforeCalib(handle, i, &PedLG, &PedHG);
		if (WDcfg.EHistoNbin > 0)
			ediv = WDcfg.Range_14bit ? ((1 << 14) / WDcfg.EHistoNbin) : ((1 << 13) / WDcfg.EHistoNbin);
		else
			ediv = 1;
		if (WDcfg.AcquisitionMode == ACQMODE_SPECT) {  // CTIN: ZS doesn't work in TSPEC mode (indeed ZS applies to energy only, not timing data. It would be a partial suppression)
			zs_thr_lg = (WDcfg.ZS_Threshold_LG[brd][i] > 0) ? WDcfg.ZS_Threshold_LG[brd][i] * ediv - WDcfg.Pedestal + PedLG : 0;
			ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_zs_lgthr, i), zs_thr_lg);
			zs_thr_hg = ((WDcfg.ZS_Threshold_HG[brd][i] > 0)) ? WDcfg.ZS_Threshold_HG[brd][i] * ediv - WDcfg.Pedestal + PedHG : 0;
			ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_zs_hgthr, i), zs_thr_hg);
		}

		if (WDcfg.HV_Adjust_Range >= 0)	ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_hv_adj, i), 0x100 | WDcfg.HV_IndivAdj[brd][i]);
		else ret |= FERS_WriteRegister(handle, INDIV_ADDR(a_hv_adj, i), 0);
		if (ret) break;
	}
	if (ret) goto abortcfg;

	// ########################################################################################################
	sprintf(CfgStep, "Configure Citiroc chips");
	// ########################################################################################################

	if (WDcfg.CitirocCfgMode == CITIROC_CFG_FROM_FILE) {
		FERS_WriteRegister(handle, a_scbs_ctrl, 0x100);  // enable manual loading of SCbs (chip 0)
		ReadSCbsFromFile("weerocGUI.txt", SCbs[0]);
		WriteCStoFileFormatted("weerocGUI_formatted.txt", SCbs[0]);
		WriteSCbsToChip(handle, 0, SCbs[0]);
		ReadSCbsFromChip(handle, 0, SCbs[0]);
		//WriteCStoFile("CitirocCfg_bitstream_0.txt", SCbs[0]);
		WriteCStoFileFormatted("CitirocCfg_0.txt", SCbs[0]);

		FERS_WriteRegister(handle, a_scbs_ctrl, 0x300);  // enable manual loading of SCbs (chip 1)
		ReadSCbsFromFile("weerocGUI.txt", SCbs[1]);
		WriteSCbsToChip(handle, 1, SCbs[1]);
		ReadSCbsFromChip(handle, 1, SCbs[1]);
		//WriteCStoFile("CitirocCfg_bitstream_1.txt", SCbs[1]);
		WriteCStoFileFormatted("CitirocCfg_1.txt", SCbs[1]);
	} else {
		FERS_WriteRegister(handle, a_scbs_ctrl, 0x000);  // set citiroc index = 0
		FERS_SendCommand(handle, CMD_CFG_ASIC);
		ReadSCbsFromChip(handle, 0, SCbs[0]);
		//WriteCStoFile("CitirocCfg_bitstream_0.txt", SCbs[0]);
		WriteCStoFileFormatted("CitirocCfg_0.txt", SCbs[0]);

		FERS_WriteRegister(handle, a_scbs_ctrl, 0x200);  // set citiroc index = 1
		FERS_SendCommand(handle, CMD_CFG_ASIC);  
		ReadSCbsFromChip(handle, 1, SCbs[1]);
		//WriteCStoFile("CitirocCfg_bitstream_1.txt", SCbs[1]);
		WriteCStoFileFormatted("CitirocCfg_1.txt", SCbs[1]);
	}
	if (ret) goto abortcfg;


	// ########################################################################################################
	sprintf(CfgStep, "Configure HV module");
	// ########################################################################################################
	ret |= HV_Set_Vbias(handle, WDcfg.HV_Vbias[brd]);  // CTIN: the 1st access to Vset doesn't work (to understand why...). Make it twice
	ret |= HV_Set_Vbias(handle, WDcfg.HV_Vbias[brd]);  
	if (WDcfg.HV_Imax[brd] >= 10) 
		WDcfg.HV_Imax[brd] = (float)9.999;  // Warning 10mA is not accepted!!!
	ret |= HV_Set_Imax(handle, WDcfg.HV_Imax[brd]); // same for Imax...
	ret |= HV_Set_Imax(handle, WDcfg.HV_Imax[brd]); 

	HV_Set_Tsens_Coeff(handle, WDcfg.TempSensCoeff);
	HV_Set_TempFeedback(handle, WDcfg.EnableTempFeedback, WDcfg.TempFeedbackCoeff);
	if (ret) goto abortcfg;


	// ########################################################################################################
	sprintf(CfgStep, "Generic Write accesses with mask");
	// ########################################################################################################
	for(i=0; i<WDcfg.GWn; i++) {
		if (((int)WDcfg.GWbrd[i] < 0) || (WDcfg.GWbrd[i] == brd)) {
			ret |= FERS_ReadRegister(handle, WDcfg.GWaddr[i], &d32);
			d32 = (d32 & ~WDcfg.GWmask[i]) | (WDcfg.GWdata[i] & WDcfg.GWmask[i]);
			ret |= FERS_WriteRegister(handle, WDcfg.GWaddr[i], d32);
		}
	}
	if (ret) goto abortcfg;

	Sleep(10);
	return 0;

	abortcfg:
	sprintf(ErrorMsg, "Error at: %s. Exit Code = %d\n", CfgStep, ret);
	return ret;
}



// ---------------------------------------------------------------------------------
// Description: Configure analog and digital probes
// Inputs:		handle = board handle
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int ConfigureProbe(int handle)
{
	int i, ret = 0;
	for (i = 0; i < 2; i++) {
		// Set Analog Probe
		if ((WDcfg.AnalogProbe[i] >= 0) && (WDcfg.AnalogProbe[i] <= 5)) {
			uint32_t ctp = (WDcfg.AnalogProbe[i] == 0) ? 0x00 : 0x80 | (WDcfg.ProbeChannel[i] & 0x3F) | ((WDcfg.AnalogProbe[i] - 1) << 9);
			ret |= FERS_WriteRegister(handle, a_scbs_ctrl, i << 9);
			ret |= FERS_WriteRegister(handle, a_citiroc_probe, ctp);
			Sleep(10);
		}
		// Set Digital Probe
		if (FERS_FPGA_FW_MajorRev(handle) >= 5) {
			ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0);
			if (WDcfg.DigitalProbe[i] == DPROBE_OFF)				ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0xFF);
			else if (WDcfg.DigitalProbe[i] == DPROBE_PEAK_LG)		ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x10);
			else if (WDcfg.DigitalProbe[i] == DPROBE_PEAK_HG)		ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x11);
			else if (WDcfg.DigitalProbe[i] == DPROBE_HOLD)			ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x16);
			else if (WDcfg.DigitalProbe[i] == DPROBE_START_CONV)	ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x12);
			else if (WDcfg.DigitalProbe[i] == DPROBE_DATA_COMMIT)	ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x21);
			else if (WDcfg.DigitalProbe[i] == DPROBE_DATA_VALID)	ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x20);
			else if (WDcfg.DigitalProbe[i] == DPROBE_CLK_1024)		ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x00);
			else if (WDcfg.DigitalProbe[i] == DPROBE_VAL_WINDOW)	ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x1A);
			else if (WDcfg.DigitalProbe[i] == DPROBE_T_OR)			ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x04);
			else if (WDcfg.DigitalProbe[i] == DPROBE_Q_OR)			ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, 0x05);
			else if (WDcfg.DigitalProbe[i] & 0x80000000) {  // generic setting
				uint32_t blk = (WDcfg.DigitalProbe[i] >> 16) & 0xF;
				uint32_t sig = WDcfg.DigitalProbe[i] & 0xF;
				ret |= FERS_WriteRegisterSlice(handle, a_dprobe, i*8, i*8+7, (blk << 4) | sig);
			}
		} else {
			if (WDcfg.DigitalProbe[i] == DPROBE_OFF)				ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0xFF);
			else if (WDcfg.DigitalProbe[i] == DPROBE_PEAK_LG)		ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x10);
			else if (WDcfg.DigitalProbe[i] == DPROBE_PEAK_HG)		ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x11);
			else if (WDcfg.DigitalProbe[i] == DPROBE_HOLD)			ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x16);
			else if (WDcfg.DigitalProbe[i] == DPROBE_START_CONV)	ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x12);
			else if (WDcfg.DigitalProbe[i] == DPROBE_DATA_COMMIT)	ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x21);
			else if (WDcfg.DigitalProbe[i] == DPROBE_DATA_VALID)	ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x20);
			else if (WDcfg.DigitalProbe[i] == DPROBE_CLK_1024)		ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x00);
			else if (WDcfg.DigitalProbe[i] == DPROBE_VAL_WINDOW)	ret |= FERS_WriteRegisterSlice(handle, a_citiroc_probe, 12, 18, 0x1A);
			else if (WDcfg.DigitalProbe[i] & 0x80000000)			ret |= FERS_WriteRegister(handle, a_citiroc_probe, WDcfg.DigitalProbe[i] & 0x7FFFFFFF);  // generic setting
		}
	}
	Sleep(10);
	return ret;
}



// ---------------------------------------------------------------------------------
// Description: Read the SC bit stream (1144 bits) from the board (referred to last programmed chip)
// Inputs:		handle = board handle
// Outputs:		SCbs: bit stream (36 words of 32 bits)
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int ReadSCbsFromChip(int handle, int chip, uint32_t *SCbs)
{
	int ret = 0, i;
	uint32_t d32; 
	uint32_t src = 0; // 0=from chip, 1=from FPGA
	uint32_t man;

	ret |= FERS_ReadRegister(handle, a_scbs_ctrl, &d32);
	man = (WDcfg.CitirocCfgMode == CITIROC_CFG_FROM_FILE) ?	0x100 : 0;
	ret |= FERS_WriteRegister(handle, a_scbs_ctrl, (src << 10) | (chip << 9) | man);
	FERS_SendCommand(handle, CMD_CFG_ASIC);  // rewrite bit stream to read it out from chip
	for(i=0; i<NW_SCBS; i++) {
		ret |= FERS_WriteRegister(handle, a_scbs_ctrl, (src << 10) | (chip << 9) | man | i);
		ret |= FERS_ReadRegister(handle, a_scbs_data, &SCbs[i]);
	}
	ret |= FERS_WriteRegister(handle, a_scbs_ctrl, d32);
	return ret;
}

// ---------------------------------------------------------------------------------
// Description: Write the SC bit stream (1144 bits) into one Citiroc chip
// Inputs:		handle = board handle
//              chip = Citiroc index (0 or 1)
// Outputs:		SCbs: bit stream (36 words of 32 bits)
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int WriteSCbsToChip(int handle, int chip, uint32_t *SCbs)
{
	int ret = 0, i;
	uint32_t d32; 
	ret |= FERS_ReadRegister(handle, a_scbs_ctrl, &d32);
	for(i=0; i<NW_SCBS; i++) {
		ret |= FERS_WriteRegister(handle, a_scbs_ctrl, chip << 9 | 0x100 | i);
		ret |= FERS_WriteRegister(handle, a_scbs_data, SCbs[i]);
	}
	ret |= FERS_SendCommand(handle, CMD_CFG_ASIC);  // Configure Citiroc (0 or 1 depending on bit 9 of a_scbs_ctrl
	ret |= FERS_WriteRegister(handle, a_scbs_ctrl, d32);
	Sleep(10);
	return ret;
}

// ---------------------------------------------------------------------------------
// Description: Read the SC bit stream (1144 bits) from text file (sequence of 0 and 1)
// Inputs:		filename = input file
// Outputs:		SCbs: bit stream (36 words of 32 bits)
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int ReadSCbsFromFile(char *filename, uint32_t *SCbs)
{
	FILE *scf;
	char c;
	int i;
	if ((scf = fopen(filename, "r")) == NULL) return -1;
	memset(SCbs, 0, NW_SCBS * sizeof(uint32_t));
	for(i=0; i<1144; i++) {
		fscanf(scf, "%c", &c);
		if ((c != '0') && (c != '1')) {
			if (!SockConsole) printf("Invalid CS file (%s)\n", filename);
			return -1;
		}
		if (c == '1')
			SCbs[i/32] |= (1 << (i%32));
	}
	fclose(scf);
	return 0;
}


// ---------------------------------------------------------------------------------
// Description: Read the SC bit stream (1144 bits) from text file (sequence of 0 and 1)
// Inputs:		filename = input file
//				SCbs: bit stream (36 words of 32 bits)
// Outputs:		-
// Return:		Error code (0=success) 
// ---------------------------------------------------------------------------------
int WriteCStoFile(char *filename, uint32_t *SCbs)
{
	FILE *scf;
	char c;
	int i;
	if ((scf = fopen(filename, "w")) == NULL) return -1;
	for(i=0; i<1144; i++) {
		c = '0' + ((SCbs[i/32] >> (i%32)) & 1);
		fprintf(scf, "%c", c);
	}
	fclose(scf);
	return 0;
}


uint16_t CSslice(uint32_t *SCbs, int start, int stop)
{
	int a = start/32;
	uint64_t d64;
	uint64_t one = 1;

	if ((start < 0) || (start > 1143) || (stop < 0) || (stop > 1143)) return 0; 
	if (a < (NW_SCBS-1))
		d64 = ((uint64_t)SCbs[a+1] << 32) | (uint64_t)SCbs[a];
	else
		d64 = (uint64_t)SCbs[a];
	d64 = d64 >> (start % 32);
	d64 = d64 & ((one << (stop-start+1)) - 1);
	return (uint16_t)d64;
}


int WriteCStoFileFormatted(char *filename, uint32_t *SCbs)
{
	FILE *of;
	int i, a, b;
	of = fopen(filename, "w");
	for(i=0; i<32; i++) {
		a = i*4;
		b = a+3;
		fprintf(of, "[%4d : %4d] (%2d) TD-FineThr[%02d] = %-6d (0x%X)\n", b, a, b-a+1, i, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	}
	for(i=0; i<32; i++) {
		a = 128+i*4;
		b = a+3;
		fprintf(of, "[%4d : %4d] (%2d) QD-FineThr[%02d] = %-6d (0x%X)\n", b, a, b-a+1, i, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	}
	a =  256; b =  256; fprintf(of, "[%4d : %4d] (%2d) -PWen QD       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  257; b =  257; fprintf(of, "[%4d : %4d] (%2d) -PWpm QD       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  258; b =  258; fprintf(of, "[%4d : %4d] (%2d) QDiscr latch   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  259; b =  259; fprintf(of, "[%4d : %4d] (%2d) -PWen TD       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  260; b =  260; fprintf(of, "[%4d : %4d] (%2d) -PWpm TD       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  261; b =  261; fprintf(of, "[%4d : %4d] (%2d) -PWen fineQthr = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  262; b =  262; fprintf(of, "[%4d : %4d] (%2d) -PWpm fineQthr = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  263; b =  263; fprintf(of, "[%4d : %4d] (%2d) -PWen fineTthr = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  264; b =  264; fprintf(of, "[%4d : %4d] (%2d) -PWpm fineTthr = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  265; b =  296; fprintf(of, "[%4d : %4d] (%2d) Discr Mask     = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  297; b =  297; fprintf(of, "[%4d : %4d] (%2d) -PWpm T&H HG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  298; b =  298; fprintf(of, "[%4d : %4d] (%2d) -PWen T&H HG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  299; b =  299; fprintf(of, "[%4d : %4d] (%2d) -PWpm T&H LG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  300; b =  300; fprintf(of, "[%4d : %4d] (%2d) -PWen T&H LG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  301; b =  301; fprintf(of, "[%4d : %4d] (%2d) SCA bias       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  302; b =  302; fprintf(of, "[%4d : %4d] (%2d) -PWpm PkD HG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  303; b =  303; fprintf(of, "[%4d : %4d] (%2d) -PWen PkD HG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  304; b =  304; fprintf(of, "[%4d : %4d] (%2d) -PWpm PkD LG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  305; b =  305; fprintf(of, "[%4d : %4d] (%2d) -PWen PkD LG   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  306; b =  306; fprintf(of, "[%4d : %4d] (%2d) Pk-det mode HG = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  307; b =  307; fprintf(of, "[%4d : %4d] (%2d) Pk-det mode LG = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  308; b =  308; fprintf(of, "[%4d : %4d] (%2d) PkS Ctrl Logic = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  309; b =  309; fprintf(of, "[%4d : %4d] (%2d) PkS Trg Source = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  310; b =  310; fprintf(of, "[%4d : %4d] (%2d) -PWpm Fast ShF = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  311; b =  311; fprintf(of, "[%4d : %4d] (%2d) -PWen Fast Sh  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  312; b =  312; fprintf(of, "[%4d : %4d] (%2d) -PWpm Fast Sh  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  313; b =  313; fprintf(of, "[%4d : %4d] (%2d) -PWpm LG Sl Sh = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  314; b =  314; fprintf(of, "[%4d : %4d] (%2d) -PWen LG Sl Sh = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  315; b =  317; fprintf(of, "[%4d : %4d] (%2d) LG Shap Time   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  318; b =  318; fprintf(of, "[%4d : %4d] (%2d) -PWpm HG Sl Sh = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  319; b =  319; fprintf(of, "[%4d : %4d] (%2d) -PWen HG Sl Sh = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  320; b =  322; fprintf(of, "[%4d : %4d] (%2d) HG Shap Time   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  323; b =  323; fprintf(of, "[%4d : %4d] (%2d) LG preamp bias = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  324; b =  324; fprintf(of, "[%4d : %4d] (%2d) -PWpm HG PA    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  325; b =  325; fprintf(of, "[%4d : %4d] (%2d) -PWen HG PA    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  326; b =  326; fprintf(of, "[%4d : %4d] (%2d) -PWpm LG PA    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  327; b =  327; fprintf(of, "[%4d : %4d] (%2d) -PWen LG PA    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  328; b =  328; fprintf(of, "[%4d : %4d] (%2d) PA Fast Shaper = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  329; b =  329; fprintf(of, "[%4d : %4d] (%2d) En Input DAC   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a =  330; b =  330; fprintf(of, "[%4d : %4d] (%2d) 8bit Dac Ref   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	for(i=0; i<32; i++) {
		a = 331+i*9;
		b = a+8;
		fprintf(of, "[%4d : %4d] (%2d) HV-adjust[%02d]  = %-6d (0x%X)\n", b, a, b-a+1, i, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	}
	for(i=0; i<32; i++) {
		a = 619+i*15;
		b = a+14;
		fprintf(of, "[%4d : %4d] (%2d) Preamp-Cfg[%02d] = %-6d (0x%X)\n", b, a, b-a+1, i, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	}
	a = 1099; b = 1099; fprintf(of, "[%4d : %4d] (%2d) -PWpm TempSens = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1100; b = 1100; fprintf(of, "[%4d : %4d] (%2d) -PWen TempSens = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1101; b = 1101; fprintf(of, "[%4d : %4d] (%2d) -PWpm Bandgap  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1102; b = 1102; fprintf(of, "[%4d : %4d] (%2d) -PWen Bandgap  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1103; b = 1103; fprintf(of, "[%4d : %4d] (%2d) -PWen Q-DAC    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1104; b = 1104; fprintf(of, "[%4d : %4d] (%2d) -PWpm Q-DAC    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1105; b = 1105; fprintf(of, "[%4d : %4d] (%2d) -PWen T-DAC    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1106; b = 1106; fprintf(of, "[%4d : %4d] (%2d) -PWpm T-DAC    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1107; b = 1116; fprintf(of, "[%4d : %4d] (%2d) QD coarse Thr  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1117; b = 1126; fprintf(of, "[%4d : %4d] (%2d) TD coarse Thr  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1127; b = 1127; fprintf(of, "[%4d : %4d] (%2d) -PWen HG OTA   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1128; b = 1128; fprintf(of, "[%4d : %4d] (%2d) -PWpm HG OTA   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1129; b = 1129; fprintf(of, "[%4d : %4d] (%2d) -PWen LG OTA   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1130; b = 1130; fprintf(of, "[%4d : %4d] (%2d) -PWpm LG OTA   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1131; b = 1131; fprintf(of, "[%4d : %4d] (%2d) -PWen Prb OTA  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1132; b = 1132; fprintf(of, "[%4d : %4d] (%2d) -PWpm Prb OTA  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1133; b = 1133; fprintf(of, "[%4d : %4d] (%2d) OTA bias       = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1134; b = 1134; fprintf(of, "[%4d : %4d] (%2d) -PWen Val Evt  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1135; b = 1135; fprintf(of, "[%4d : %4d] (%2d) -PWpm Val Evt  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1136; b = 1136; fprintf(of, "[%4d : %4d] (%2d) -PWen Raz Chn  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1137; b = 1137; fprintf(of, "[%4d : %4d] (%2d) -PWpm Raz Chn  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1138; b = 1138; fprintf(of, "[%4d : %4d] (%2d) -PWen Out Dig  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1139; b = 1139; fprintf(of, "[%4d : %4d] (%2d) -PWen Out32    = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1140; b = 1140; fprintf(of, "[%4d : %4d] (%2d) -PWen Out32OC  = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1141; b = 1141; fprintf(of, "[%4d : %4d] (%2d) Trg Polarity   = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1142; b = 1142; fprintf(of, "[%4d : %4d] (%2d) -PWen Out32TOC = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	a = 1143; b = 1143; fprintf(of, "[%4d : %4d] (%2d) -PWen ChTrgOut = %-6d (0x%X)\n", b, a, b-a+1, CSslice(SCbs, a, b), CSslice(SCbs, a, b));
	fclose(of);
	return 0;
}
