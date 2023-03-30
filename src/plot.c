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
#include <inttypes.h>

#include "plot.h"	
#include "FERSlib.h"
#include "FERSutils.h"
#include "JanusC.h"
#include "Statistics.h"

FILE *plotpipe = NULL;
int LastPlotType = -1; 
int LastPlotName = -1;
const int debug = 0;

#ifdef _WIN32
#define GNUPLOT_COMMAND  "..\\gnuplot\\gnuplot.exe"
#define NULL_PATH		 "nul"
#else
#define GNUPLOT_COMMAND  "/usr/bin/gnuplot"	// anyway .exe does not work for linux
#define NULL_PATH		 "/dev/null"
#endif

#define PLOT_TYPE_SPECTRUM		0
#define PLOT_TYPE_WAVE			1
#define PLOT_TYPE_HISTO			2
#define PLOT_TYPE_2D_MAP		3
#define PLOT_TYPE_SCAN_THR		4
#define PLOT_TYPE_SCAN_DELAY	5

// --------------------------------------------------------------------------------
// Open Gnuplot
// --------------------------------------------------------------------------------
int OpenPlotter()
{
	char str[200];
	//strcpy(str, ConfigVar->GnuPlotPath);
	strcpy(str, "");
	strcat(str, GNUPLOT_COMMAND);
	strcat(str, " 2> ");  
	strcat(str, NULL_PATH); // redirect stderr to nul (to hide messages showing up in the console output)
	if ((plotpipe = popen(str, "w")) == NULL) return -1;

	fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
	fprintf(plotpipe, "set grid\n");
	fprintf(plotpipe, "set mouse\n");
	fflush(plotpipe);
	return 0;
}

// --------------------------------------------------------------------------------
// Close Gnuplot
// --------------------------------------------------------------------------------
int ClosePlotter()
{
	if (plotpipe != NULL)
#ifdef _WIN32
		_pclose(plotpipe);
#else
		pclose(plotpipe);
#endif
	return 0;
}

// --------------------------------------------------------------------------------
// Plot 1D Histogram (spectrum)
// --------------------------------------------------------------------------------
int PlotSpectrum()
{
	FILE* debbuff;
	if (debug) debbuff = fopen("gnuplot_debug_PHA.txt", "w");

	int i, t, nt, xcalib, Nbin, max_yval=1;
	int en[8], brd[8], ch[8]; // enable and Board/channel assigned to each trace
	char SorBF[8];
	int rn[8]; // run number for off line spectrum
	char cmd[1000], pixel[3];
	double mean[8], rms[8], A0[8], A1[8] = {}, la0=0, la1=0;
	char xunit[50];
	char ffxunit[50], fftitle[50];
	char description[8][50];
	static int LastNbin = 0, LastXcalib = 0;
	static double LastA0 = 0, LastA1 = 0;
	Histogram1D_t *Histo[8];

	if (plotpipe == NULL) return -1;

	xcalib = RunVars.Xcalib;
	Nbin = 0;
	if (RunVars.PlotType == PLOT_E_SPEC_LG) {	// DNIN: can be improved in writing
		xcalib = 0;
		fprintf(plotpipe, "set title 'PHA LG'\n");
		strcpy(ffxunit, Stats.H1_PHA_LG[0][0].x_unit);
		strcpy(fftitle, "PHA_LG"); // Stats.H1_PHA_LG[0][0].title);
	} else if (RunVars.PlotType == PLOT_E_SPEC_HG) {
		xcalib = 0;
		fprintf(plotpipe, "set title 'PHA HG'\n");
		strcpy(ffxunit, Stats.H1_PHA_HG[0][0].x_unit);
		strcpy(fftitle, "PHA_HG"); // Stats.H1_PHA_HG[0][0].title);
	} else if (RunVars.PlotType == PLOT_TOA_SPEC) {
		fprintf(plotpipe, "set title 'ToA'\n");
		strcpy(ffxunit, Stats.H1_ToA[0][0].x_unit);
		strcpy(fftitle, "ToA"); // Stats.H1_ToA[0][0].title);
	} else if (RunVars.PlotType == PLOT_TOT_SPEC) {
		fprintf(plotpipe, "set title 'ToT'\n");
		strcpy(ffxunit, Stats.H1_ToT[0][0].x_unit);
		strcpy(fftitle, "ToT"); // Stats.H1_ToT[0][0].title);
	}
	else if (RunVars.PlotType == PLOT_MCS_TIME) {
		fprintf(plotpipe, "set title 'MCS'\n");
		strcpy(ffxunit, Stats.H1_MCS[0][0].x_unit);
		strcpy(fftitle, "MCS"); // Stats.H1_MCS[0][0].title);
	}

	nt = 0;
	//char tmp_rn[8][100];
	for(t=0; t<8; t++) {
		if (strlen(RunVars.PlotTraces[t]) == 0) {
			en[t] = 0;
		} else {
			en[t] = 1;
			nt++;
			char tmp_rn[100];
			sscanf(RunVars.PlotTraces[t], "%d %d %s", &brd[t], &ch[t], tmp_rn);
			SorBF[t] = tmp_rn[0];
			if (tmp_rn[0] == 'F'){	//offline
				Nbin = Stats.offline_bin;
				sscanf(tmp_rn + 1, "%d", &rn[t]);
				// H1_File, histogram reserved for reading from file
				Histo[t] = &Stats.H1_File[t];
				//strcpy(Histo[t]->x_unit, ffxunit);
				//strcpy(Histo[t]->title, fftitle);
				sprintf(Histo[t]->spectrum_name, "From_File_%s[%d][%d]", fftitle, brd[t], ch[t]);

				A0[t] = Histo[t]->A[0];
				A1[t] = Histo[t]->A[1];
				mean[t] = 0;
				rms[t] = 0;
				if (Histo[t]->H_cnt > 0) {
					mean[t] = Histo[t]->mean / Histo[t]->H_cnt;
					rms[t] = sqrt((Histo[t]->rms / Histo[t]->H_cnt) - mean[t] * mean[t]);
				}
				//set description
				sprintf(description[t], "T%d Run%d - Offline", t, rn[t]);
			}
			else if (tmp_rn[0] == 'S'){
				Nbin = Stats.offline_bin;
				char ext_file[100] = "";
				sscanf(tmp_rn + 1, "%s", ext_file);

				Histo[t] = &Stats.H1_File[t];
				strcpy(Histo[t]->x_unit, ffxunit);	//why here are needed?
				strcpy(Histo[t]->title, fftitle);
				sprintf(Histo[t]->spectrum_name, "ExtFile_%s", fftitle);

				A0[t] = Histo[t]->A[0];
				A1[t] = Histo[t]->A[1];
				mean[t] = 0;
				rms[t] = 0;
				if (Histo[t]->H_cnt > 0) {
					mean[t] = Histo[t]->mean / Histo[t]->H_cnt;
					rms[t] = sqrt((Histo[t]->rms / Histo[t]->H_cnt) - mean[t] * mean[t]);
				}
				//set description
				sprintf(description[t], "T%d File:%s - Offline", t, ext_file);

			} else {	// online
				if (brd[t] >= WDcfg.NumBrd) brd[t] = 0;
				if (ch[t] >= MAX_NCH) ch[t] = 0;
				if (RunVars.PlotType == PLOT_E_SPEC_LG) {
					Histo[t] = &Stats.H1_PHA_LG[brd[t]][ch[t]];
					Nbin = Stats.H1_PHA_LG[0][0].Nbin;
				} else if (RunVars.PlotType == PLOT_E_SPEC_HG) {
					Histo[t] = &Stats.H1_PHA_HG[brd[t]][ch[t]];
					Nbin = Stats.H1_PHA_HG[0][0].Nbin;
				} else if (RunVars.PlotType == PLOT_TOA_SPEC) {
					Histo[t] = &Stats.H1_ToA[brd[t]][ch[t]];
					Nbin = Stats.H1_ToA[0][0].Nbin;
				} else if (RunVars.PlotType == PLOT_TOT_SPEC) {
					Histo[t] = &Stats.H1_ToT[brd[t]][ch[t]];
					Nbin = Stats.H1_ToT[0][0].Nbin;
				} else if (RunVars.PlotType == PLOT_MCS_TIME) {
					Histo[t] = &Stats.H1_MCS[brd[t]][ch[t]];
					Nbin = Stats.H1_MCS[0][0].Nbin;
					if (WDcfg.TriggerMask == 0x21)
						fprintf(plotpipe, "set title 'MCS - dwell time %f ms'\n", WDcfg.PtrgPeriod / 1e6);
					else
						fprintf(plotpipe, "set title 'MCS - dwell time ?'\n");
					// Set mean cnt and rms
					//uint32_t t_data[16000] = {};
					for (int j = 0; j < (int)Histo[t]->Nbin; ++j) {
						if (Histo[t]->H_data[j] <= 0) continue;
						++Histo[t]->H_cnt;
						Histo[t]->mean += double(Histo[t]->H_data[j]);
						Histo[t]->rms += double(Histo[t]->H_data[j]) * double(Histo[t]->H_data[j]);
						if (Histo[t]->H_data[j] > (uint32_t)max_yval)
							max_yval = Histo[t]->H_data[j];
					}
				}
				strcpy(xunit, Histo[t]->x_unit);  // is the same for all the traces
				A0[t] = Histo[t]->A[0];
				A1[t] = Histo[t]->A[1];
				mean[t] = 0;
				rms[t] = 0;
				if (Histo[t]->H_cnt > 0) {
					mean[t] = Histo[t]->mean / Histo[t]->H_cnt;
					rms[t] = sqrt((Histo[t]->rms / Histo[t]->H_cnt) - mean[t] * mean[t]); // DNIN: Statistically speaking, shouldn't be rms/(cnt-1)
				}

				sprintf(description[t], "T%d Run%d Online", t, RunVars.RunNumber);
			}
		}
	}
	if (nt == 0) return 0;
	// Take A0 and A1 of the first active channel.
	for (int t = 0; t < 8; ++t) {
		if (!en[t])
			continue;
		la0 = A0[t];
		la1 = A1[t];
		break;
	}
	if ((LastPlotType != PLOT_TYPE_SPECTRUM) || (LastNbin != Nbin) || (LastXcalib != xcalib) || (LastPlotName != RunVars.PlotType)
		|| LastA0 != la0 || LastA1 != la1) {
		fprintf(plotpipe, "clear\n");
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
		fprintf(plotpipe, "set ylabel 'Counts'\n");
		fprintf(plotpipe, "set autoscale y\n");
//		fprintf(plotpipe, "set yrange [0:100]");
//		fprintf(plotpipe, "set yrange [0:]\n");
		fprintf(plotpipe, "bind y 'set yrange [0:]'\n");
		fprintf(plotpipe, "set style fill solid\n");
		fprintf(plotpipe, "bind y 'set autoscale y'\n");
		fprintf(plotpipe, "set xtics auto\n");
		fprintf(plotpipe, "set ytics auto\n");
		fprintf(plotpipe, "unset logscale\n");
		fprintf(plotpipe, "unset label\n");
		fprintf(plotpipe, "set grid\n");
		if (xcalib && WDcfg.AcquisitionMode != ACQMODE_COUNT) {
			fprintf(plotpipe, "set xlabel '%s'\n", xunit);
			fprintf(plotpipe, "set xrange [%f:%f]\n", la0, la0 + Nbin * la1);
			fprintf(plotpipe, "bind x 'set xrange [%f:%f]'\n", la0, la0 + Nbin * la1);
		} else {
			fprintf(plotpipe, "set xlabel 'Channels'\n");
			fprintf(plotpipe, "set xrange [0:%d]\n", Nbin);
			fprintf(plotpipe, "bind x 'set xrange [0:%d]'\n", Nbin);
		}
		LastNbin = Nbin;
		LastXcalib = xcalib;
		LastPlotType = PLOT_TYPE_SPECTRUM;
		LastPlotName = RunVars.PlotType;
		LastA0 = la0;
		LastA1 = la1;
	}
	if ((RunVars.PlotType == PLOT_MCS_TIME) && (WDcfg.AcquisitionMode == ACQMODE_COUNT)) {
		fprintf(plotpipe, "set yrange [0:%f]'\n", max_yval * 1.25);
		fprintf(plotpipe, "bind y 'set yrange [0:]'\n");
	}
	
	if (debug) 	fprintf(debbuff, "$PlotData << EOD\n");
	fprintf(plotpipe, "$PlotData << EOD\n");
	for(i=0; i < Nbin; i++) {
		for(t=0; t<8; t++)
			if (en[t]) {
				int ind = i;
				if (RunVars.PlotType == PLOT_MCS_TIME) {	// DNIN: For MCS it is needed to reorder the binning, since the plot is saved in circular buffer but it is visualized as linear one
					if (Histo[t]->H_data[Nbin-2]>0)
						ind = (i + Histo[t]->Bin_set) % (Histo[t]->Nbin - 1);
				}
				fprintf(plotpipe, "%" PRIu32 " ", Histo[t]->H_data[ind]);
				if (debug) fprintf(debbuff, "%" PRIu32 " ", Histo[t]->H_data[ind]);
			}
		fprintf(plotpipe, "\n");
		if (debug) fprintf(debbuff, "\n");
	}
	fprintf(plotpipe, "EOD\n");
	if (debug) fprintf(debbuff, "EOD\n");

	sprintf(cmd, "plot ");
	int nt_written = 0;
	for(t=0; t<8; t++){
		if (!en[t]) continue;
		char title[200], tmpc[500];
		double a0 = xcalib ? A0[t] : 0;
		double a1 = xcalib ? A1[t] : 1;
		double m = a0 + a1 * mean[t];
		double r = a1 * rms[t];
		sprintf(pixel, "%c%c", 'A' + ch2x(ch[t]), '1' + ch2y(ch[t]));
		if (SorBF[t] == 'S') {
			sprintf(title, "%s: Mean=%6.2f, Rms=% 6.2f", description[t], m, r);
			sprintf(tmpc, " $PlotData u ($0*%lf+%lf):($%d) title '%s' noenhanced w step", a1, a0, nt_written + 1, title);
		}
		else {
			sprintf(title, "Brd[%d] Ch[%d] (%s): Mean=%6.2f, Rms=% 6.2f - %s", brd[t], ch[t], pixel, m, r, description[t]);
			//sprintf(title, "CH[%d][%d] (%s): Mean=%6.2f, Rms=% 6.2f - %s", brd[t], ch[t], pixel, m, r, description[t]);
			sprintf(tmpc, " $PlotData u ($0*%lf+%lf):($%d) title '%s' w step", a1, a0, nt_written + 1, title);   // DNIN here check for ToA spectra
		}
		strcat(cmd, tmpc);
		++nt_written;
		
		if (nt_written < nt) strcat(cmd, ", ");
		else strcat(cmd, "\n");
	}
	if (debug) fprintf(debbuff, "%s", cmd);
	fprintf(plotpipe, "%s", cmd);
	fflush(plotpipe);
	if (debug) fflush(debbuff);
	if (debug) fclose(debbuff);
	return 0;
}


// --------------------------------------------------------------------------------
// Plot Count Rate vs Channel
// --------------------------------------------------------------------------------
int PlotCntHisto()
{
	int i, ch, brd;
	FILE *pd;

	brd = 0;
	ch = 0;
	// Take the first trace active or brd/ch 0 0
	for (int m = 0; m < 8; m++) {
		if (strlen(RunVars.PlotTraces[m]) != 0) {
			sscanf(RunVars.PlotTraces[m], "%d %d", &brd, &ch);
			break;
		}
	}

	if (plotpipe == NULL) return -1;
	if (LastPlotType != PLOT_TYPE_HISTO) {
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
		fprintf(plotpipe, "set xlabel 'Channel'\n");
		fprintf(plotpipe, "set ylabel 'cps'\n");
		fprintf(plotpipe, "set xrange [0:64]\n");
		fprintf(plotpipe, "bind x 'set xrange [0:68]'\n");
		fprintf(plotpipe, "unset yrange\n");
		fprintf(plotpipe, "set yrange [0:]\n");
		fprintf(plotpipe, "bind y 'set yrange [0:]'\n");
		fprintf(plotpipe, "set style fill solid\n");
		fprintf(plotpipe, "set ytics auto\n");
		fprintf(plotpipe, "set xtics 2\n");
		fprintf(plotpipe, "unset logscale\n");
		fprintf(plotpipe, "unset label\n");
		fprintf(plotpipe, "set grid\n");
		LastPlotType = PLOT_TYPE_HISTO;
	}
	pd = fopen("PlotData.txt", "w");
	if (pd == NULL) return -1;
	fprintf(plotpipe, "set title 'Cnt vs Channel (Board %d)'\n", brd);
	for(i=0; i<MAX_NCH; i++) 
		fprintf(pd, "%lf\n", Stats.ChTrgCnt[brd][i].rate);
	fclose(pd);
	fprintf(plotpipe, "plot 'PlotData.txt' title '' w histo linecolor 'orange'\n");
	fflush(plotpipe);
	return 0;
}


// --------------------------------------------------------------------------------
// Plot waveforms
// --------------------------------------------------------------------------------
int PlotWave(WaveEvent_t *wev, char *title)
{
	int i;
	FILE *pd;
	int xcalib = 0; 
	static int LastXcalib = 0;
	if ((wev->wave_hg == NULL) || (wev->wave_lg == NULL) || (wev->dig_probes == NULL)) return -1;
	if (plotpipe == NULL) return -1;
	if (LastPlotType != PLOT_TYPE_WAVE || LastXcalib != xcalib) {
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
		
		fprintf(plotpipe, "set ylabel 'ADC'\n");

		fprintf(plotpipe, "set autoscale y\n");
		fprintf(plotpipe, "bind y 'set autoscale y'\n");
		fprintf(plotpipe, "set xtics auto\n");
		fprintf(plotpipe, "set ytics auto\n");
		fprintf(plotpipe, "unset logscale\n");
		fprintf(plotpipe, "unset label\n");
		fprintf(plotpipe, "set grid\n");
		if (xcalib) {
			fprintf(plotpipe, "set xlabel 'ns'\n");
			fprintf(plotpipe, "set xrange [0:%d]\n", WDcfg.WaveformLength * 25); // 12 sample is a MuxClkPeriod, 1000 for us
			fprintf(plotpipe, "bind x 'set xrange [0:%d]'\n", WDcfg.WaveformLength * WDcfg.MuxClkPeriod / (12000));
		}
		else {
			fprintf(plotpipe, "set xlabel 'Samples'\n");
			fprintf(plotpipe, "set xrange [0:%d]\n", WDcfg.WaveformLength);
			fprintf(plotpipe, "bind x 'set xrange [0:%d]'\n", WDcfg.WaveformLength);
		}
		LastXcalib = xcalib;
		LastPlotType = PLOT_TYPE_WAVE;
	}
	pd = fopen("PlotData.txt", "w");
	for (i = 0; i < wev->ns; i++) {
		int j = 0;
		while (j < 25) {
			float whg = (float)(wev->wave_hg[i] & 0x3FFF);
			float wlg = (float)(wev->wave_lg[i] & 0x3FFF);
			int d1 = 100 + 70 * ((wev->dig_probes[i] >> 0) & 1);
			int d2 = 200 + 70 * ((wev->dig_probes[i] >> 1) & 1);
			int d3 = 300 + 70 * ((wev->dig_probes[i] >> 2) & 1);
			int d4 = 400 + 70 * ((wev->dig_probes[i] >> 3) & 1);
			fprintf(pd, "%lf %lf %d %d %d %d\n", whg, wlg, d1, d2, d3, d4);
			if (!xcalib) j = 25;
			else ++j;
		}
	}
	
	fclose(pd);
	fprintf(plotpipe, "set title '%s'\n", title);
	fprintf(plotpipe, "plot 'PlotData.txt' u 1 title 'HG' with step, 'PlotData.txt' u 2 title 'LG' with step, 'PlotData.txt' u 3 title 'hold' with step, 'PlotData.txt' u 4 title 'adc-mean' with step, 'PlotData.txt' u 5 title 'hit' with step, 'PlotData.txt' u 6 title 'clk-mux' with step\n");
	fflush(plotpipe);
	return 0;
}

// --------------------------------------------------------------------------------
// Plot 2D (matrix)
// --------------------------------------------------------------------------------
int Plot2Dmap(int StatIntegral)
{
    int brd, ch, i, x, y;
	double zmax = 0;
	double zval[MAX_NCH];  // Z-axes in 2D plots
	char title[100];
	FILE *pd;

	brd = 0;
	ch = 0;
	// take the first trace active or brd/ch 0 0
	for (int m = 0; m < 8; m++) {
		if (strlen(RunVars.PlotTraces[m]) != 0) {
			sscanf(RunVars.PlotTraces[m], "%d %d", &brd, &ch);
			break;
		}
	}

	if (RunVars.PlotType == PLOT_2D_CNT_RATE) {
		for(i=0; i<MAX_NCH;i++)	zval[i] = Stats.ChTrgCnt[brd][i].rate;
		sprintf(title, "Channel Trg Rate (Board %d)", brd);
	} else if (RunVars.PlotType == PLOT_2D_CHARGE_LG) {
		for(i=0; i<MAX_NCH;i++)	{
			double dmean = (StatIntegral == 0) ? Stats.H1_PHA_LG[brd][i].mean - Stats.H1_PHA_LG[brd][i].p_mean : Stats.H1_PHA_LG[brd][i].mean;
			int dcnt = (StatIntegral == 0) ? Stats.H1_PHA_LG[brd][i].H_cnt - Stats.H1_PHA_LG[brd][i].H_p_cnt : Stats.H1_PHA_LG[brd][i].H_cnt;
			zval[i] = (dcnt > 0) ? dmean / dcnt : 0;
			Stats.H1_PHA_LG[brd][i].H_p_cnt = Stats.H1_PHA_LG[brd][i].H_cnt;
			Stats.H1_PHA_LG[brd][i].p_mean = Stats.H1_PHA_LG[brd][i].mean;
		}
		sprintf(title, "Charge LG (Board %d)", brd);
	} else if (RunVars.PlotType == PLOT_2D_CHARGE_HG) {
		for(i=0; i<MAX_NCH;i++)	{
			double dmean = (StatIntegral == 0) ? Stats.H1_PHA_HG[brd][i].mean - Stats.H1_PHA_HG[brd][i].p_mean : Stats.H1_PHA_HG[brd][i].mean;
			int dcnt = (StatIntegral == 0) ? Stats.H1_PHA_HG[brd][i].H_cnt - Stats.H1_PHA_HG[brd][i].H_p_cnt : Stats.H1_PHA_HG[brd][i].H_cnt;
			zval[i] = (dcnt > 0) ? dmean / dcnt : 0;
			Stats.H1_PHA_HG[brd][i].H_p_cnt = Stats.H1_PHA_HG[brd][i].H_cnt;
			Stats.H1_PHA_HG[brd][i].p_mean = Stats.H1_PHA_HG[brd][i].mean;
		}
		sprintf(title, "Charge HG (Board %d)", brd);
	}

	if (plotpipe == NULL) return -1;
	pd = fopen("PlotData.txt", "w");
	for(y=0; y<8; y++) {
		for(x=0; x<8; x++) {
			double pixz = zval[xy2ch(x, y)];
			fprintf(pd, "%d %d %lf\n", x, y, pixz);
			if (zmax < pixz) zmax = pixz;
		}
		fprintf(pd, "\n");
	}
	fclose(pd);
	if (zmax == 0) zmax = 1;

	if (LastPlotType != PLOT_TYPE_2D_MAP) {
		fprintf(plotpipe, "clear\n");
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 400,400 position 700,10\n");
		fprintf(plotpipe, "set xlabel 'X'\n");
		fprintf(plotpipe, "set ylabel 'Y'\n");
		fprintf(plotpipe, "set xrange [-0.5:7.5]\n");
		fprintf(plotpipe, "set yrange [-0.5:7.5]\n");
		fprintf(plotpipe, "set xtics 1\n");
		fprintf(plotpipe, "set ytics 1\n");
		// Pixel Mapping: gnuplot has pixel (0,0) = A1 in the lower left corner 
		fprintf(plotpipe, "set xtics ('A' 0, 'B' 1, 'C' 2, 'D' 3, 'E' 4, 'F' 5, 'G' 6, 'H' 7)\n");
		fprintf(plotpipe, "set ytics ('1' 0, '2' 1, '3' 2, '4' 3, '5' 4, '6' 5, '7' 6, '8' 7)\n");
		fprintf(plotpipe, "unset logscale\n");
		fprintf(plotpipe, "unset label\n");
		for(y=0; y<8; y++) {
			for(x=0; x<8; x++) {
				fprintf(plotpipe, "set label front center '%d' at graph %f, %f\n", xy2ch(x, y), 0.0625 + x/8., 0.0625 + y/8.);	// may create a function to float the value ...
			}
		}
		LastPlotType = PLOT_TYPE_2D_MAP;
	}
	fprintf(plotpipe, "bind z 'set cbrange [0:%f]'\n", zmax);
	if ((RunVars.PlotType == PLOT_2D_CHARGE_LG) || (RunVars.PlotType == PLOT_2D_CHARGE_HG))	fprintf(plotpipe, "set cbrange [0:%d]\n", WDcfg.EHistoNbin);
	else fprintf(plotpipe, "set cbrange [0:%f]\n", zmax);
	fprintf(plotpipe, "set title '%s'\n", title);
	fprintf(plotpipe, "unset grid; set palette model CMY rgbformulae 15,7,3\n");
	fprintf(plotpipe, "plot 'PlotData.txt' with image\n");
	fflush(plotpipe);		
    return 0;
}


// --------------------------------------------------------------------------------
// Plot Threshold scan (Staircase)
// --------------------------------------------------------------------------------
int PlotStaircase()
{
	char pixel[3], cmd[1000], tmp_rn[8][100], description[8][50];
	char mytitle[100] = "";
	int i, t, nt=0, nstep;
	int en[8], brd[8], ch[8], rn[8];
	float ymax = 0;

	FILE* deb = NULL;
	if (debug) deb = fopen("staircase_gnuplot_deb.txt", "w");

	if (plotpipe == NULL) return -1;
	if (LastPlotType != PLOT_TYPE_SCAN_THR) {
		fprintf(plotpipe, "clear'\n");
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
		fprintf(plotpipe, "set xlabel 'Threshold'\n");
		fprintf(plotpipe, "set ylabel 'Cps'\n");
		fprintf(plotpipe, "set autoscale x\n");
		fprintf(plotpipe, "bind x 'set autoscale x'\n");
		fprintf(plotpipe, "set autoscale y\n");
		fprintf(plotpipe, "bind y 'set autoscale y'\n");
		fprintf(plotpipe, "set logscale y\n");
		fprintf(plotpipe, "set xtics auto\n");
		fprintf(plotpipe, "set ytics auto\n");
		fprintf(plotpipe, "unset label\n");
		fprintf(plotpipe, "set grid\n");
		LastPlotType = PLOT_TYPE_SCAN_THR;
	}
	if (RunVars.StaircaseCfg[SCPARAM_STEP] == 0) return 0;

	int nstep_off = 0;
	for(t=0; t<8; t++) {
		if (strlen(RunVars.PlotTraces[t]) == 0) {
			en[t] = 0;
		} else {
			en[t] = 1;
			nt++;
			sscanf(RunVars.PlotTraces[t], "%d %d %s", &brd[t], &ch[t], tmp_rn[t]);
			if (tmp_rn[t][0] == 'S') sscanf(tmp_rn[t] + 1, "%s", mytitle);
			else sscanf(tmp_rn[t] + 1, "%d", &rn[t]);
			nstep_off = max(nstep_off, Stats.Staircase_offline[t].nsteps);
		}
	}

	// 	Plot 2 columns x:y for each staircase plot, 
	// 	different scans can be compared
	//
	fprintf(plotpipe, "$PlotData << EOD\n");
	if (debug) fprintf(deb, "$PlotData << EOD\n");
	nstep = (RunVars.StaircaseCfg[SCPARAM_MAX] - RunVars.StaircaseCfg[SCPARAM_MIN])/RunVars.StaircaseCfg[SCPARAM_STEP] + 1;
	int nstepmax = max(nstep_off, nstep);

	for (i = 0; i < nstepmax; i++) {
		// fprintf(plotpipe, "%d ", RunVars.StaircaseCfg[SCPARAM_MIN] + i * RunVars.StaircaseCfg[SCPARAM_STEP]);
		// fprintf(deb, "%d", RunVars.StaircaseCfg[SCPARAM_MIN] + i * RunVars.StaircaseCfg[SCPARAM_STEP]);
		for(t=0; t<8; t++) {
			if (!en[t]) continue;
			if (tmp_rn[t][0] == 'B') {	// From saved file
				if (i >= nstep) {
					int last = nstep;
					float val_tmp = 1;
					if (Stats.Staircase[brd[t]][ch[t]][last] != 0) val_tmp = Stats.Staircase[brd[t]][ch[t]][last];
					fprintf(plotpipe, "%d %8e ", RunVars.StaircaseCfg[SCPARAM_MIN] + last * RunVars.StaircaseCfg[SCPARAM_STEP], val_tmp);
					fprintf(deb, "%d %8e ", RunVars.StaircaseCfg[SCPARAM_MIN] + last * RunVars.StaircaseCfg[SCPARAM_STEP], val_tmp);
					fflush(deb);
					continue;	// more data in the saved file
				}
				float val_tmp = 1;
				if (Stats.Staircase[brd[t]][ch[t]][i] != 0) val_tmp = Stats.Staircase[brd[t]][ch[t]][i];
				fprintf(plotpipe, "%d %8e ", RunVars.StaircaseCfg[SCPARAM_MIN] + i * RunVars.StaircaseCfg[SCPARAM_STEP], val_tmp);
				if (debug) fprintf(deb, "%d %8e ", RunVars.StaircaseCfg[SCPARAM_MIN] + i * RunVars.StaircaseCfg[SCPARAM_STEP], val_tmp);
			ymax = max(ymax, Stats.Staircase[brd[t]][ch[t]][i]);
		}
			else {
				if (i >= Stats.Staircase_offline[t].nsteps) {
					int last = Stats.Staircase_offline[t].nsteps;
					if (last > 0) {
						fprintf(plotpipe, "%d %8e ", Stats.Staircase_offline[t].threshold[last - 1], (double)Stats.Staircase_offline[t].counts[last - 1]);
						if (debug) fprintf(deb, "%d %8e ", Stats.Staircase_offline[t].threshold[last - 1], (double)Stats.Staircase_offline[t].counts[last - 1]);
					}
					else {
						fprintf(plotpipe, "%d %8d ",150, 1);
						if (debug) fprintf(deb, "%d %8d ", 150, 1);
					}
					if (debug) fflush(deb);
					continue;	// more data in the saved file
				}
				//int thresho = Stats.Staircase_offline[t].threshold[i];		// here for debugging
				//double cnts = (double)Stats.Staircase_offline[t].counts[i];
				fprintf(plotpipe, "%d %8e ", Stats.Staircase_offline[t].threshold[i], (double)Stats.Staircase_offline[t].counts[i]);
				if (debug) fprintf(deb, "%d %8e ", Stats.Staircase_offline[t].threshold[i], (double)Stats.Staircase_offline[t].counts[i]);
				ymax = max(ymax, Stats.Staircase_offline[t].counts[i]);
				if (debug) fflush(deb);
			}
		}
		fprintf(plotpipe, "\n");
		if (debug) fprintf(deb, "\n");
	}
	fprintf(plotpipe, "EOD\n");
	if (debug) fprintf(deb, "EOD\n");

	fprintf(plotpipe, "set title 'Staircase (scan threshold)'\n");
	if (debug) fprintf(deb, "set title 'Staircase (scan threshold)'\n");
	/*fprintf(plotpipe, "set y range [0.5:%f]\n", ymax * 1.1);	// not sure what they meant
	fprintf(plotpipe, "bind y 'y range [0.5:%f]'\n", ymax * 1.1);*/
	sprintf(cmd, "plot ");
	int nt_written = 0;
	//for(t=0; t<nt; t++) {
	for (t = 0; t < 8; t++) {
		if (!en[t]) continue;
		sprintf(pixel, "%c%c", 'A' + ch2x(ch[t]), '1' + ch2y(ch[t]));
		char tmpc[500];
		if (tmp_rn[t][0] == 'B') {
			sprintf(description[t], "T%d Run%d Online", t, RunVars.RunNumber);
			sprintf(tmpc, " $PlotData u %d:%d title '%s CH[%d][%d] (%s)' w step", 2 * nt_written + 1, 2 * nt_written + 2, description[t], brd[t], ch[t], pixel);
		}
		else if (tmp_rn[t][0] == 'F') {
			sprintf(description[t], "T%d Run%d Offline", t, rn[t]);
			sprintf(tmpc, " $PlotData u %d:%d title '%s CH[%d][%d] (%s)' w step", 2 * nt_written + 1, 2 * nt_written + 2, description[t], brd[t], ch[t], pixel);
		}
		else if (tmp_rn[t][0] == 'S') {
			sprintf(description[t], "T%d %s Offline", t, mytitle);
			sprintf(tmpc, " $PlotData u %d:%d title '%s' noenhanced w step", 2 * nt_written + 1, 2 * nt_written + 2, description[t]);
		}
		strcat(cmd, tmpc);
		++nt_written;
		if (nt_written < (nt)) strcat(cmd, ", ");
		else strcat(cmd, "\n");
	}
	fprintf(plotpipe, "%s", cmd);
	fflush(plotpipe);
	if (debug) fprintf(deb, "%s", cmd);
	if (debug) fflush(deb);
	if (debug) fclose(deb);
	return 0;
}

// --------------------------------------------------------------------------------
// Plot Hold Delay scan 
// --------------------------------------------------------------------------------
int PlotScanHoldDelay(int *newrun)	// DNIN: Would be useful to have a view of all the channels together?
{
	int b, ch, nstep;
	char pixel[3];
	FILE *st;
	if (plotpipe == NULL) return -1;
	if (LastPlotType != PLOT_TYPE_SCAN_DELAY || *newrun) {
		fprintf(plotpipe, "clear'\n");
		fprintf(plotpipe, "set terminal wxt noraise title 'FERS Readout' size 1200,800 position 700,10\n");
		fprintf(plotpipe, "set xtics auto\n");
		fprintf(plotpipe, "set ytics auto\n");
		fprintf(plotpipe, "unset logscale\n");
		fprintf(plotpipe, "set xlabel 'Hold Delay (ns)'\n");
		fprintf(plotpipe, "set ylabel 'Peak Height'\n");
		//fprintf(plotpipe, "set autoscale x\n");
		fprintf(plotpipe, "set xrange [%d:%d]\n", RunVars.HoldDelayScanCfg[0] - 5, RunVars.HoldDelayScanCfg[1]);
		fprintf(plotpipe, "bind x 'set xrange [*:*]'\n"); // 
		//fprintf(plotpipe, "bind x 'set autoscale x'\n");
		//fprintf(plotpipe, "set autoscale y\n");
		fprintf(plotpipe, "set yrange [0:*]\n");
		fprintf(plotpipe, "bind y 'set yrange [*:*]'\n");
		fprintf(plotpipe, "set autoscale cb\n");
		fprintf(plotpipe, "unset label\n");
		LastPlotType = PLOT_TYPE_SCAN_DELAY;
		*newrun = 0;
	}

	sscanf(RunVars.PlotTraces[0], "%d %d", &b, &ch);
	//if (Stats.Hold_PHA_2Dmap[b][ch] == NULL) return -1; 
	st = fopen("PlotData.txt", "w");
	nstep = (RunVars.HoldDelayScanCfg[HDSPARAM_MAX] - RunVars.HoldDelayScanCfg[HDSPARAM_MIN]) / RunVars.HoldDelayScanCfg[HDSPARAM_STEP] + 1;
	for(int y=0; y<512; y++) {
		for(int x=0; x<nstep; x++) {
			if (Stats.Hold_PHA_2Dmap[b][ch] != NULL)
				fprintf(st, "%d %d %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MIN] + x*RunVars.HoldDelayScanCfg[HDSPARAM_STEP], y*WDcfg.EHistoNbin/512, Stats.Hold_PHA_2Dmap[b][ch][y*nstep+x]);
			else
				fprintf(st, "%d %d %d\n", RunVars.HoldDelayScanCfg[HDSPARAM_MIN] + x * RunVars.HoldDelayScanCfg[HDSPARAM_STEP], 0, 0);	// DNIN: allows the visualization of HoldDelay with no data
		}
		fprintf(st, "\n");
	}
	fflush(st);
	fclose(st);
	//fprintf(plotpipe, "EOD\n");

	sprintf(pixel, "%c%c", 'A' + ch2x(ch), '1' + ch2y(ch));
	fprintf(plotpipe, "set title 'Scan Hold Delay ch [%d][%d] (%s)'\n", b, ch, pixel);
	fprintf(plotpipe, "unset grid; set palette model CMY rgbformulae 15,7,3\n");
	fprintf(plotpipe, "plot 'PlotData.txt' with image\n");
	fflush(plotpipe);		
	return 0;
}

