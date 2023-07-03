import sys
import subprocess
import time
import os

from threading import Thread, Lock

from tkinter import *
from tkinter import ttk
from tkinter import font
from tkinter import messagebox
from tkinter.filedialog import askopenfilename, asksaveasfilename, askdirectory
from tkinter.ttk import Combobox, Progressbar
#from tkinter.ttk import *

import shared as sh
import cfgfile_rw as cfg
import socket2daq as comm
import leds as leds
import tooltips as tt

params = sh.params
sections = sh.sections

#def resource_path(relative_path):
#        if hasattr(sys, '_MEIPASS'):
#            return os.path.join(sys._MEIPASS, relative_path)
#        return os.path.join(os.path.abspath("."), relative_path)

# *******************************************************************************
# Control Panel
# *******************************************************************************
class CtrlPanel():
	def __init__(self):

		# images and logos
		if sys.platform.find('win') < 0:
			sh.ImgPath = '../img/'
		self.img_CAENlogo   = PhotoImage(file=sh.ImgPath + "CAENlogo.png"   ).subsample(3, 3)
		self.img_FERSlogo   = PhotoImage(file=sh.ImgPath + "FERSlogo.png"   ).subsample(3, 3)
		self.img_plug       = PhotoImage(file=sh.ImgPath + "plug.png"       ).subsample(3, 3)
		self.img_start      = PhotoImage(file=sh.ImgPath + "start.png"      ).subsample(3, 3)
		self.img_startjob   = PhotoImage(file=sh.ImgPath + "startjob.png"   ).subsample(3, 3)
		self.img_stop       = PhotoImage(file=sh.ImgPath + "stop.png"       ).subsample(3, 3)
		self.img_pause      = PhotoImage(file=sh.ImgPath + "pause.png"      ).subsample(3, 3)
		self.img_single     = PhotoImage(file=sh.ImgPath + "single.png"     ).subsample(3, 3)
		self.img_clear      = PhotoImage(file=sh.ImgPath + "clear.png"      ).subsample(3, 3)
		self.img_trg        = PhotoImage(file=sh.ImgPath + "trg.png"        ).subsample(3, 3)
		self.img_probe      = PhotoImage(file=sh.ImgPath + "plotprobe.png"  ).subsample(3, 3)
		self.img_stair      = PhotoImage(file=sh.ImgPath + "staircase.png"  ).subsample(3, 3)
		self.img_holdscan   = PhotoImage(file=sh.ImgPath + "holdscan.png"   ).subsample(3, 3)
		self.img_savecfg    = PhotoImage(file=sh.ImgPath + "savecfg.png"    ).subsample(3, 3)
		# self.img_savecfgc	= PhotoImage(file=sh.ImgPath + "savecopy2.png"  ).subsample(3, 3)
		self.img_savecfgr   = PhotoImage(file=sh.ImgPath + "savecfgrun.png" ).subsample(3, 3)
		self.img_loadcfg    = PhotoImage(file=sh.ImgPath + "loadcfg.png"    ).subsample(3, 3)
		# self.add_file		= PhotoImage(file=sh.ImgPath + "addfile.png"	).subsample(10, 10)
		# self.remove_file	= PhotoImage(file=sh.ImgPath + "removefile2.png").subsample(10, 10)
		self.img_bin2csv	= PhotoImage(file=sh.ImgPath + "export_csv100.png"	).subsample(3, 3)

		self.FreezeStat = IntVar()
		self.FreezeStat.set(0)
		
		self.PlotMaskWinIsOpen = False
		self.InitPlotMaskButtons = True
		self.enable_runvarsave = False
		self.SpecialRunWinIsOpen = False
		self.RunCfgWinIsOpen = False
		self.ConvWinIsOpen = False
		self.IsExtFileOpen = False

		self.HV_ON = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
		self.prev_brd = [0, 0] # enter in Ramping, board

		self.PlotTraceSel = ["0 0 B", "", "", "", "", "", "", ""]
		# self.PlotTraceSel = ["", "", "", "", "", "", "", ""]
		self.StaircaseSettings = ""
		self.HoldScanSettings = ""

		self.active_channel = StringVar()
		self.active_channel.set(0)

		self.active_board = StringVar()
		self.active_board.set(0)
		self.active_board.trace('w', lambda name, index, mode: self.SaveRunVars())

		self.Xcalib = IntVar()
		self.Xcalib.set(0)

		self.CfgReloaded = IntVar()

		self.CfgFileName = StringVar()
		self.CfgFileName.set("Janus_Config.txt")

		self.CfgNameSaved = StringVar()
		self.CfgNameSaved.set("")

		self.RisedWarning = IntVar()
		self.RisedWarning.set(0)

		self.CfgWarning = IntVar()
		self.CfgWarning.set(0)

		self.ConvCsvTrace = IntVar()
		self.ConvCsvTrace.set(0)

		self.len_macro = IntVar()
		self.len_macro.set(0)

		self.time_unit = IntVar()
		self.time_unit.set(0)
		self.list_of_bfile = IntVar()
		self.list_of_bfile.set(0)

	def validate_RunNumber(self, new_value):
		if new_value.isdigit(): return int(new_value) < 10000
		elif not new_value: return True
		else: return False

	def OpenControlPanel(self, parent):
		# ------------------------------------------------------------
		# Header
		# ------------------------------------------------------------
		y0 = 1
		Label(parent, image=self.img_FERSlogo).place(relx=float(5)/sh.Win_Ctrl_W, rely=float(y0-1.1)/sh.Win_Ctrl_H, relwidth=225./sh.Win_Ctrl_W, relheight=8./sh.Win_Ctrl_H)  #  x = 5, y = y0)
		Label(parent, image=self.img_CAENlogo).place(relx=float(520)/sh.Win_Ctrl_W, rely=float(y0-0.9)/sh.Win_Ctrl_H)  #  x = 520, y = y0)
		Label(parent, text="JANUS", font=("Arial Bold", 20), fg = 'steelblue').place(relx=float(320)/sh.Win_Ctrl_W, rely=float(y0-1.6)/sh.Win_Ctrl_H)  #   x = 320, y = y0-5)
		Label(parent, text="Ver. " + sh.Version + " - Rel." + sh.Release, font=("Arial", 8), fg = 'steelblue').place(relx=float(280)/sh.Win_Ctrl_W, rely=float(y0+3.1)/sh.Win_Ctrl_H)  #  x = 280, y = y0+25)

		# ------------------------------------------------------------
		# Run Ctrl Buttons and Run Status
		# ------------------------------------------------------------
		y0 = 57
		Frame(parent, relief=RIDGE, bd=2).place(relx=float(2)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=0.994, relheight=7.5/sh.Win_Ctrl_H)  # , height=48 , width=sh.Win_W-4 x=2, y=y0)
		x0 = 7
		y0 += 7
		xw = 40
		self.plugged = IntVar()
		self.plugged.set(0)
		self.bplug = Checkbutton(parent, image=self.img_plug, indicatoron = 0, variable=self.plugged, relief = 'sunken', offrelief = 'groove') #, height=28, width=28,
		self.bplug.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #  y=y0)  #  x = x0, y = y0)
		tt.Tooltip(self.bplug, text='Connect GUI to JanusC and FERS boards', wraplength=200)
		x0 += xw
		self.bstart = Button(parent, image=self.img_start, command=lambda:comm.SendCmd('s'), relief = 'groove') # , height=30, width=30
		self.bstart.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H)  #  x = x0, y = y0)
		tt.Tooltip(self.bstart, text='Start Run', wraplength=200)
		x0 += xw
		self.bstop = Button(parent, image=self.img_stop, command=lambda:comm.SendCmd('S'), relief = 'groove') # , height=30, width=30
		self.bstop.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H)  #  x = x0, y = y0)
		tt.Tooltip(self.bstop, text='Stop Run', wraplength=200)
		x0 += xw
		self.bpause = Checkbutton(parent, image=self.img_pause, indicatoron = 0, command=self.Freeze, variable=self.FreezeStat, height=28, width=28, relief = 'sunken', offrelief = 'groove')
		self.bpause.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.bpause, text='Freeze Plot Update. Run is not paused', wraplength=200)
		x0 += xw
		self.bsingle = Button(parent, image=self.img_single, command=lambda:comm.SendCmd('o'), height=30, width=30, relief = 'groove')
		self.bsingle.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.bsingle, text='Refresh Plot', wraplength=200)
		#x0 += xw	#DNIN sofware trigger, for future?
		#self.btrg = Button(parent, image=self.img_trg, command=lambda:comm.SendCmd('t'), height=30, width=30, relief = 'groove')
		#self.btrg.place(x = x0, y = y0)
		x0 += xw
		self.bclear = Button(parent, image=self.img_clear, command=lambda:comm.SendCmd('r'), height=30, width=30, relief = 'groove')
		self.bclear.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.bclear, text='Clear histograms, statistics and restart Run', wraplength=200)

		x0 += xw + 10
		self.PlotTraces_button = Button(parent, image=self.img_probe, command=self.OpenPlotMaskWin, width=30, height=30, relief = 'groove')
		self.PlotTraces_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.PlotTraces_button, text='Select Board and Channel for Plot Traces', wraplength=200)
		x0 += xw
		self.staircase_button = Button(parent, image=self.img_stair, command=lambda:self.OpenSpecialRunWin('Staircase', parent), width=30, height=30, relief = 'groove')
		self.staircase_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.staircase_button, text='Run staircase acquisition (threshold scan)', wraplength=200)
		x0 += xw
		self.holdscan_button = Button(parent, image=self.img_holdscan, command=lambda:self.OpenSpecialRunWin('HoldScan', parent), width=30, height=30, relief = 'groove')
		self.holdscan_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.holdscan_button, text='Run hold-delay sweep', wraplength=200)
		x0 += xw
		self.SaveAs_button = Button(parent, image = self.img_savecfg, command=self.SaveCfgFileAs, width=30, height=30, relief = 'groove')
		self.SaveAs_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.SaveAs_button, text='Save configuration with a different name', wraplength=200)
		# x0 += xw
		# self.SaveCopy_button = Button(parent, image = self.img_savecfgc, command=self.SaveCfgCopyFileAs, width=30, height=30, relief = 'groove')
		# self.SaveCopy_button.place(x = x0, y = y0)
		# tt.Tooltip(self.SaveCopy_button, text='Save a configuration copy with a different name', wraplength=200)
		x0 += xw
		self.SaveToRun_button = Button(parent, image = self.img_savecfgr, command=self.SaveCfgFileForRun, width=30, height=30, relief = 'groove')
		self.SaveToRun_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x = x0, y = y0)
		tt.Tooltip(self.SaveToRun_button, text='Save configuration for the selected Run Number (used in jobs)', wraplength=200)
		x0 += xw
		self.LoadCfg_button = Button(parent, image = self.img_loadcfg, command=self.ReadCfgFile, width=30, height=30, relief = 'groove')
		self.LoadCfg_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H) #x=x0, y=y0)
		tt.Tooltip(self.LoadCfg_button, text='Load configuration from file', wraplength=200)
		x0 += xw
		self.bin2csv_button = Button(parent, image = self.img_bin2csv, command=self.ConvertBin2CSV, width=30, height=30, relief = 'groove')
		self.bin2csv_button.place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0)/sh.Win_H, relwidth=float(36)/sh.Win_Ctrl_W, relheight=float(36)/sh.Win_H)  #x=x0, y=y0)
		tt.Tooltip(self.bin2csv_button, text='Convert binary file into CSV format', wraplength=200)


		x0 = 570
		Label(parent, text="Run#", font=("Arial Bold", 12)).place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0+7)/sh.Win_H, relwidth=50./sh.Win_Ctrl_W, relheight=23./sh.Win_H) # , relwidth=float(45)/sh.Win_Ctrl_W, relheight=float(25)/sh.Win_H)  #x=x0, y = y0+7)
		self.RunNumber = StringVar()
		self.RunNumber.trace('w', lambda name, index, mode: self.SaveRunVars())
		vcmd = (parent.register(self.validate_RunNumber), '%P')  #  , width=4
		Spinbox(parent, textvariable=self.RunNumber, from_=0, to=10000, font=("Arial Bold", 12), validate='key', validatecommand=vcmd).place(relx=float(x0+50)/sh.Win_Ctrl_W, rely=float(y0+7)/sh.Win_H, relwidth=53./sh.Win_Ctrl_W, relheight=23./sh.Win_H)#x = x0+50, y = y0+7)

		# ------------------------------------------------------------
		# Cfg, plot and Stats Buttons
		# ------------------------------------------------------------
		y0 = 110
		x0 = 5

		self.combobox_writing = IntVar()	# In case of control in ExpertMode
		self.combobox_writing.set(0)

		Label(parent, text="Plot Type").place(relx=float(x0)/sh.Win_W, rely=float(y0)/sh.Win_H)  #  x=x0, y = y0)
		self.plot_options_v = ['Spect LG','Spect HG','Spect ToA', 'Spect ToT', 'TrgRate-Ch', 'MultiCh Scaler', 'Waveform','2D-TrgRate','2D-Charge LG', '2D-Charge HG', 'Staircase', 'HoldDelay-Scan']
		self.plot_options = ['Spect LG','Spect HG','Spect ToA', 'Spect ToT', 'TrgRate-Ch','Waveform','2D-TrgRate','2D-Charge LG', '2D-Charge HG', 'Staircase', 'HoldDelay-Scan', 'MultiCh Scaler']
		self.plot_type = StringVar()
		self.plot_type.trace('w', lambda name, index, mode: self.SaveRunVars())  #  , width=15
		ttk.Combobox(parent, values=self.plot_options_v, textvariable=self.plot_type, state='readonly').place(relx=float(x0)/sh.Win_Ctrl_W, rely=float(y0+20)/sh.Win_H, relwidth=113./sh.Win_W, relheight=21./sh.Win_H)  #   x=x0, y=y0+20)
		x0 += 120
		Label(parent, text="Statistics Type").place(relx=float(x0)/sh.Win_W, rely=float(y0)/sh.Win_H)
		self.smon_options = ['ChTrg Rate', 'ChTrg Cnt', 'Tstamp Rate', 'Tstamp cnt', 'PHA Rate', 'PHA Cnt']
		self.smon_type = StringVar()
		self.smon_type.set('ChTrg Rate')
		self.smon_type.trace('w', lambda name, index, mode: self.SaveRunVars())  #  , width=12
		ttk.Combobox(parent, values=self.smon_options, textvariable=self.smon_type, state='readonly').place(relx=float(x0)/sh.Win_W, rely=float(y0+20)/sh.Win_H, relwidth=95./sh.Win_W, relheight=21./sh.Win_H)  #  x=x0, y=y0+20)
		x0 += 110
		y0 = y0
		self.show_warning = IntVar()	# to add in the configuration file ?!
		self.show_warning.set(1)	
		# ttk.Checkbutton(parent, text = "Show warning pop-up", variable = self.show_warning).place(relx=float(x0)/sh.Win_W, rely=float(y0+8)/sh.Win_H) #, relwidth=110./sh.Win_W, relheight=22./sh.Win_H) # x=x0, y=y0+20)
		
		self.Macro_msg = Label(parent, font=("Arial Bold", 10), text="")
		self.Macro_msg.place(relx=(510)/sh.Win_W, rely=(y0+3)/sh.Win_H)

		self.len_macro.trace('w', lambda name, index, mode: self.change_macro_msg())
		self.len_macro.set(len(cfg.cfg_file_list))

		# ttk.Checkbutton(parent, text = "Enable combobox change (Expert)", variable = self.combobox_writing).place(x=x0, y=y0+20)
		# self.b_show_warning = 

		self.b_apply = Button(parent, text='Apply', command=self.SaveCfgFile, state=DISABLED, width=14, height=2)
		self.b_apply.place(relx=570./sh.Win_W, rely=110./sh.Win_H, relwidth=108./sh.Win_W, relheight=41./sh.Win_H)  #  x=570, y=110)

		# ------------------------------------------------------------
		# Status Bar
		# ------------------------------------------------------------
		x0 = sh.Win_StatBar_X + 5
		y0 = sh.Win_StatBar_Y
		Label(parent, text="Status").place(relx=float(x0)/sh.Win_W, rely=float(y0)/sh.Win_H)  #  x=x0, y = y0)
		x0 += 38
		self.statled = leds.Led(parent, 18)
		self.statled.rel_place(float(x0)/sh.Win_W, float(y0)/sh.Win_H)  #  place(x0, y0)
		self.statled.set_color("grey")
		x0 += 28
		self.AcqStatus = Text(parent, font=("Arial", 10), height=1, width=65)
		self.AcqStatus.place(relx=float(x0)/sh.Win_W, rely=float(y0)/sh.Win_H, relwidth=459/sh.Win_W, relheight=20./sh.Win_H) # x = x0, y = y0)
		x0 += 500
		Label(parent, text="Run").place(relx=float(x0)/sh.Win_W, rely=float(y0)/sh.Win_H)  #  x=x0, y = y0)
		x0 += 27
		self.runled = leds.Led(parent, 18)
		self.runled.rel_place(float(x0)/sh.Win_W, float(y0)/sh.Win_H)  #  place(x0, y0)
		self.runled.set_color("grey")

		self.SetAcqStatus(0, "offline")
		self.LoadRunVars()
		self.enable_runvarsave = True


	def Freeze(self):
		if self.FreezeStat.get() == 1:
			comm.SendCmd('f1')
			self.bsingle.config(state=NORMAL)
		else:
			comm.SendCmd('f0')
			self.bsingle.config(state=DISABLED)

	def SaveCfgFile(self):
		# DNIN: to decide: is better to implement everything in py or do with socket?
		# if not self.CfgReloaded.get():	# new cfg file not loaded, update the Vbias from socket
		# 	mycmd = "B" + params['HV_Vbias'].default	# not needed, the control is performed on Janus
		# 	comm.SendCmd(mycmd)
		# 	for i in range(sh.MaxBrd):
		# 		if params['HV_Vbias'].value[i] != "": 
		# 			comm.SendCmd("B"+ params['HV_Vbias'].value[i] + " b" + str(i))
		# time.sleep(0.2)
		############	Write and update Janus_config.txt
		cfg.WriteConfigFile(sections, params, sh.CfgFile, self.show_warning.get())
		self.CfgReloaded.set(0)
		# GUI rising Warning 
		if len(cfg.empty_field) > 0 or len(cfg.jobs_check) > 0 or len(cfg.gain_check) > 0 or len(cfg.cfg_file_list) > 0: 
			self.CfgWarning.set(1)
		self.RisedWarning.set(0)
		self.b_apply.configure(bg = sh.BgCol, state=DISABLED)

	def SaveCfgFileAs(self):
		files = [('All Files', '*.*'), 
                 ('Text Document', '*.txt')]
		name = asksaveasfilename(filetypes=files, defaultextension=files)
		if name: 
			cfg.WriteConfigFile(sections, params, name, self.show_warning.get())
			# self.SaveCfgFile()
			self.CfgNameSaved.set(name)

	def SaveCfgFileForRun(self):
		filename, file_extension = os.path.splitext(sh.CfgFile)
		filename = os.path.join(params['DataFilePath'].default, filename + '_Run' + str(self.RunNumber.get()))
		name = filename + file_extension
		cfg.WriteConfigFile(sections, params, name, self.show_warning.get())

	def ReadCfgFile(self):
		try:
			name = os.path.relpath(askopenfilename(initialdir=".", filetypes=(("Text File", "*.txt"), ("All Files", "*.*")), title="Choose a file."))
			cfg.ReadConfigFile(params, name, any(self.HV_ON))	# check if Vbias changed, HV connected
			if self.CfgFileName.get() != "Janus_Config.txt":
				self.CfgFileName.set(name)
			self.CfgReloaded.set(1)
		except:
			name = ''

	def SaveRunVars(self):
		if self.enable_runvarsave:
			rf = open("RunVars.txt", "w")
			rf.write("ActiveBrd      " + self.active_board.get() + "\n")  #str(0) + "\n")  # Where to get the Active brd from Ctrl?
			rf.write("ActiveCh       " + str(self.active_channel.get()) + "\n")
			rf.write("PlotType       " + str(self.plot_options.index(self.plot_type.get())) + "\n")
			rf.write("SMonType       " + str(self.smon_options.index(self.smon_type.get())) + "\n")
			rf.write("RunNumber      " + self.RunNumber.get() + "\n")
			rf.write("Xcalib         " + str(self.Xcalib.get()) + "\n")
			default_PltTrSel = 1
			for i in range(8):
				if self.PlotTraceSel[i] != "": 
					default_PltTrSel = 0
					rf.write("PlotTraces     " + str(i) + " " + self.PlotTraceSel[i] + "\n") #+ "  ")	# save Plot Trace on different lines
					
			if default_PltTrSel == 1:
				rf.write    ("PlotTraces     " + "0 0 0 B\n")

			if self.StaircaseSettings != "":
				rf.write("Staircase      " + self.StaircaseSettings + "\n")
			if self.HoldScanSettings != "":
				rf.write("HoldDelayScan  " + self.HoldScanSettings) # + "\n")
			rf.close()

	def LoadRunVars(self):
		if os.path.isfile("RunVars.txt"):
			self.enable_runvarsave = False
			rf = open("RunVars.txt", "r")
			for line in rf:
				# line = line.split('#')[0]  # remove comments if line[0].count("#"): continue
				if line[0].count("#"): continue
				p = line.split()
				if len(p) >= 2:
					#if p[0] == "ActiveBrd": self.active_channel.set(int(p[1]))
					# if p[0] == "ActiveBrd" and int(p[1]) < sh.MaxBrd: self.Tbrd.set(int(p[1]))
					if p[0] == "ActiveCh" and int(p[1]) < 64: self.active_channel.set(int(p[1]))
					if p[0] == "PlotType" and int(p[1]) < len(self.plot_options): self.plot_type.set(self.plot_options[int(p[1])])
					if p[0] == "SMonType" and int(p[1]) < len(self.smon_options): self.smon_type.set(self.smon_options[int(p[1])])
					if p[0] == "RunNumber": self.RunNumber.set(p[1])
					if p[0] == "Xcalib": self.Xcalib.set(int(p[1]))
					if p[0] == "PlotTraces":
						self.PlotTraceSel[int(p[1])] = p[2]
						for i in range(len(p)-3):
							self.PlotTraceSel[int(p[1])] = self.PlotTraceSel[int(p[1])] + ' ' + p[i+3]
						# for i in range((len(p)-1) // 3):
						# 	self.PlotTraceSel[int(p[i*3+1])] = p[i*3+2] + ' ' + p[i*3+3]
					if p[0] == "Staircase" and len(p) == 5:
						self.StaircaseSettings = p[1] + ' ' + p[2] + ' ' + p[3] + ' ' + p[4]
					if p[0] == "HoldDelayScan" and len(p) == 5:
						self.HoldScanSettings = p[1] + ' ' + p[2] + ' ' + p[3] + ' ' + p[4]
			rf.close()
			self.enable_runvarsave = True

	def SetAcqStatus(self, status, msg):
		self.AcqStatus.delete(1.0, END)
		self.AcqStatus.insert(INSERT, msg)
		self.bin2csv_button["state"] = NORMAL
		if status <= 0: # disconnected or connection fail
			# self.HV_ON
			if status < 0: 
				self.statled.set_color("red") 
				self.AcqStatus.config(fg='red')
			else: 
				self.statled.set_color("grey") 
				self.AcqStatus.config(fg='black')
			self.prev_brd[0] = 0
			self.runled.set_color("grey") 
			self.staircase_button.config(state=DISABLED)
			self.holdscan_button.config(state=DISABLED)
			self.bstart.config(state=DISABLED)
			self.bstop.config(state=DISABLED)
			self.bpause.config(state=DISABLED)
			self.bsingle.config(state=DISABLED)
			self.bclear.config(state=DISABLED)
			self.LoadCfg_button.config(state=NORMAL)
			self.SaveAs_button.config(state=NORMAL)
			self.SaveToRun_button.config(state=NORMAL)
			self.RisedWarning.set(0)
		elif status == sh.ACQSTATUS_SOCK_CONNECTED: 
			self.statled.set_color("yellow") 
			self.AcqStatus.config(fg='black')
		elif status == sh.ACQSTATUS_HW_CONNECTED: 
			self.statled.set_color("yellow")
			self.AcqStatus.config(fg='black')
		elif status == sh.ACQSTATUS_READY: # ready for run
			self.LoadCfg_button.config(state=NORMAL)
			self.SaveAs_button.config(state=NORMAL)
			self.SaveToRun_button.config(state=NORMAL)
			self.bin2csv_button["state"] = NORMAL
			self.AcqStatus.config(fg='black')
			s1 = msg.split('#')
			if params['EnableJobs'].default == '1': self.bstart.config(image=self.img_startjob)
			else: 
				self.bstart.config(image=self.img_start)
				self.enable_runvarsave = False	# The RunNumber is updated when Jobs are not enabled 
				self.RunNumber.set(s1[1].split()[0])
				self.enable_runvarsave = True
			if not self.RisedWarning.get(): self.statled.set_color("green")
			else: self.statled.set_color("yellow")
			self.runled.set_color("grey") 
			self.bstart.config(state=NORMAL)
			self.holdscan_button.config(state=NORMAL)
			self.staircase_button.config(state=NORMAL)
			self.bstop.config(state=DISABLED)
			self.bpause.config(state=DISABLED)
			self.bsingle.config(state=DISABLED)
			self.bclear.config(state=DISABLED)
			self.prev_brd[0] = 0
		elif status == sh.ACQSTATUS_RUNNING : # running 
			self.AcqStatus.config(bg='white')
			self.runled.set_color("green") 
			self.bstart.config(state=DISABLED)
			self.staircase_button.config(state=DISABLED)
			self.holdscan_button.config(state=DISABLED)
			self.bin2csv_button["state"] = DISABLED # DNIN: it is meant to avoid to convert a file that is going to be written during DAQ..is that useful?
			self.bstop.config(state=NORMAL)
			self.bpause.config(state=NORMAL)
			self.LoadCfg_button.config(state=DISABLED)	# Is it correct to have this option?
			self.SaveAs_button.config(state=DISABLED)
			self.SaveToRun_button.config(state=DISABLED)
			if self.FreezeStat.get() == 1: self.bsingle.config(state=NORMAL)
			else: self.bsingle.config(state=DISABLED)
			self.bclear.config(state=NORMAL)
		elif status == sh.ACQSTATUS_RAMPING_HV: 
			self.AcqStatus.config(bg='white')
			self.bstart.config(state=DISABLED)
			self.bstop.config(state=DISABLED)
			self.bpause.config(state=DISABLED)
			self.bsingle.config(state=DISABLED)
			self.bclear.config(state=DISABLED)
			self.LoadCfg_button.config(state=DISABLED)
			self.SaveAs_button.config(state=DISABLED)
			self.SaveToRun_button.config(state=DISABLED)
			rmp_board = int(msg.split(" ")[2])	#  setting HV	HVstatus ^ 1 to be used in ReadConfig		
			if self.prev_brd[0] != 1 or self.prev_brd[1] != rmp_board:
				self.HV_ON[rmp_board] = self.HV_ON[rmp_board]^1
				self.prev_brd[0] = 1
				self.prev_brd[1] = rmp_board
		elif status == sh.ACQSTATUS_STAIRCASE or status == sh.ACQSTATUS_HOLDSCAN: # running staircase or holdscan
			self.AcqStatus.config(bg='white')
			if status == sh.ACQSTATUS_STAIRCASE: self.runled.set_color("green") 
			self.bstart.config(state=DISABLED)
			self.bstop.config(state=DISABLED)
			self.bpause.config(state=DISABLED)
			self.bsingle.config(state=DISABLED)
			self.bclear.config(state=DISABLED)
			if msg.find('Running') >= 0:
				p1 = msg.split('(')
				if len(p1) > 1 and self.SpecialRunWinIsOpen:
					p2 = p1[1].split()
					prog = float(p2[0])
					self.sc_progress['value'] = prog
					if prog >= 100:
						self.sc_run.config(state=NORMAL)
						self.CloseSpecialRunWin()


	#####################################################################################
	# Open list of loaded external file appended as Cfg File (free writes)
	# ###################################################################################
	def change_macro_msg(self): # Set Macro message
		if self.len_macro.get():
			self.Macro_msg.configure(text = "Active\nMacros!")
		else:
			self.Macro_msg.configure(text = "")

	def OpenExternalCfg(self):	#
		if self.IsExtFileOpen: self.CloseExternalCfg() 
		self.no_update2 = True
		xw = 280
		yw = 420
		self.displ_y = 25
		self.displ_x = 35
		self.start_y = 30
		self.ExtCfgLoad = Toplevel()
		self.ExtCfgLoad.geometry("{}x{}+{}+{}".format(xw, yw, 550, 200))
		self.ExtCfgLoad.protocol("WM_DELETE_WINDOW", self.CloseExternalCfg)
		self.mf = Frame(self.ExtCfgLoad, width=xw, height=yw, relief=RIDGE).place(x=0, y=0) # , bd=2	
		self.ExtCfgFileName = [] # Cfg File path
		self.IsExtFileOpen = True
		self.VarList = StringVar()	# indexes of the cfg path
		self.VarList.set("0")	
		self.CfgFilesList = []	# Entry where Cfg File are displaied (maybe the stringvar can be avoid??)
		self.NumCfgFile = [] # Radiobutton of the indexes
		self.previous_idx = "0"

		self.CBCfg = []	# Check button placed at the right
		self.myint = [IntVar() for i in range(12)] # Variables of CBcfg - take trace of the path enabled

		Label(self.ExtCfgLoad, text="Macro File Path", font=("Arial Bold", 10), anchor="center").place(relx=0.125, rely=0.0095) # x=self.displ_x, y=4)
		for i in range(12):
			self.ExtCfgFileName.append(StringVar())
			try:
				self.ExtCfgFileName[i].set(cfg.cfg_file_list[i])
			except:
				self.ExtCfgFileName[i].set("")
			if len(self.ExtCfgFileName[i].get()) > 0: self.myint[i].set(1)	
			self.NumCfgFile.append(Radiobutton(self.ExtCfgLoad, fg='black', text=str(i+1), variable=self.VarList, value=str(i), font=("Arial Bold", 10), indicatoron=0, width=2))
			self.NumCfgFile[i].place(relx=0.014, rely=0.067+0.06*i, relwidth=0.09, relheight=0.06) #  x=4, y=self.start_y+self.displ_y*i-2)
			self.CfgFilesList.append(Entry(self.ExtCfgLoad, textvariable = self.ExtCfgFileName[i], state='readonly'))
			self.CfgFilesList[i].place(relx=0.125, rely= 0.071+0.06*i, relwidth=0.76, relheight=0.05) # , width=35 x = self.displ_x, y = self.start_y+self.displ_y*i)
			self.ExtCfgFileName[i].trace('w', lambda name, index, mode:self.set_cfg_path(1))
			self.CBCfg.append(Checkbutton(self.ExtCfgLoad, variable=self.myint[i]))
			self.CBCfg[i].place(relx=0.9, rely=0.067+0.06*i, relwidth=0.075, relheight=0.05) #  x=250, y=self.start_y+self.displ_y*i-2)
			self.myint[i].trace('w', lambda name, index, mode: self.set_cfg_path(2))

		self.VarList.trace('w', lambda name, index, mode:self.set_cfg_path(0))
		self.CfgFilesList[0].config(state='normal')
		Button(self.ExtCfgLoad, text="Add Macro", command=self.AddCfgFile, fg="#00CC00", height=2).place(relx=0.089, rely=0.798, relwidth=0.24, relheight=0.1) # , width=8 x=25, y=334)
		Button(self.ExtCfgLoad, text="Remove\nMacro", command=self.RmCfgFile, fg="#FF0000", height=2).place(relx=0.375, rely=0.798, relwidth=0.24, relheight=0.1) # width=8, x=105, y=334)
		Button(self.ExtCfgLoad, text="Remove\nAll Macros", command=self.RmAllFile, fg="#FF0000", height=2).place(relx=0.66, rely=0.795, relwidth=0.24, relheight=0.1) # width=8, x=185, y=334)
		Button(self.ExtCfgLoad, text="DONE", font=("Arial Bold",9), command=self.AppendCfgFile, bg="#00CC00").place(relx=0.0893, rely=0.914, relwidth=0.81, relheight=0.065) #  width=31, x=25, y=384)

		self.no_update2 = False
	
	def AppendCfgFile(self): # Active when you save, 
		cfg_not_found = []
		idx = [i for i in range(len(self.myint)) if self.myint[i].get()==1]
		cfg.cfg_file_list.clear()
		if len(idx) == 0: self.CloseExternalCfg
		for vv in idx:
			if len(self.ExtCfgFileName[vv].get()) <= 0: continue
			try:
				with open(self.ExtCfgFileName[vv].get(), "r") as f:
					cfg.cfg_file_list.append(self.ExtCfgFileName[vv].get())
			except: cfg_not_found.append(self.ExtCfgFileName[vv].get())
	
		if len(cfg_not_found):
			msg = 'Selected macros ('
			for mfile in cfg_not_found:
				msg += '"' + mfile + '" '
			msg = msg[:-1] 
			msg += ") not found. Skipped in Janus_Config."
			messagebox.showinfo("Not found", msg)
		
		self.len_macro.set(len(cfg.cfg_file_list))
		self.SaveCfgFile()
		self.CloseExternalCfg()

	def CloseExternalCfg(self):
		# self.AppendCfgFile()
		self.ExtCfgLoad.destroy()

	def set_cfg_path(self, turn_on):
		if self.no_update2: return
		self.no_update2 = True

		midx = int(self.VarList.get(),10)
		if turn_on==1:
			if len(self.ExtCfgFileName[midx].get())>0:
				self.NumCfgFile[midx].config(fg='red')
				self.myint[midx].set(1)
			else:
				self.NumCfgFile[midx].config(fg='black')
				self.myint[midx].set(0)
		elif turn_on == 0:
			for i in range(12):
				if str(i) == self.VarList.get():
					self.CfgFilesList[i].config(state='normal')
				else:
					self.CfgFilesList[i].config(state='readonly')
			if len(self.ExtCfgFileName[midx].get())>0 and self.previous_idx == midx:
				if self.NumCfgFile[midx].cget('foreground') == 'red':
					self.NumCfgFile[midx].config(fg='black')
					self.myint[midx].set(0)
				elif  self.NumCfgFile[midx].cget('foreground') == 'black':
					self.NumCfgFile[midx].config(fg='red')
					self.myint[midx].set(1)
		elif turn_on == 2:
			for i in range(12):
				if self.myint[i].get() == 0: self.NumCfgFile[i].config(fg='black')
				elif self.myint[i].get() == 1 and len(self.ExtCfgFileName[i].get())>0: self.NumCfgFile[i].config(fg='red')				
				else: self.myint[i].set(0) 
		self.no_update2 = False
		self.previous_idx = midx

	def RmAllFile(self): 
		self.no_update2 = True
		self.VarList.set("0")
		for i in range(10):
			self.ExtCfgFileName[i].set("")
			self.NumCfgFile[i].config(fg='black')
			self.myint[i].set(0)
		self.no_update2 = False

	def RmCfgFile(self):
		j=int(self.VarList.get(),10)
		self.ExtCfgFileName[j].set("") 
		self.NumCfgFile[j].config(fg='black')
		self.myint[j].set(0)

	def AddCfgFile(self):
		j=int(self.VarList.get(),10)
		try: name = os.path.relpath(askopenfilename(initialdir=".", filetypes=(("Text File", "*.txt"), ("All Files", "*.*")), title="Choose a file."))
		except: name=""
		self.ExtCfgFileName[j].set(name)

		
	# ***************************************************************************************
	# Popup window for Plot enable mask
	# ***************************************************************************************
	def OpenPlotMaskWin(self):
		if self.PlotMaskWinIsOpen: self.ClosePlotMaskWin()
		self.Tbrd = IntVar()
		self.Tbrd.set(0)
		self.Tch = 0
		self.FromBrdFile = StringVar()
		self.FromBrdFile.set("B")
		self.RunFromFile = StringVar()
		self.RunFromFile.set("1")
		self.BrdFile = "B"
		self.SelRunFile = []
		self.ExtRunFile = []
		self.EntryRunFile = []
		self.PlotTraceSelVar = IntVar()
		self.PlotMaskVar = StringVar()
		self.maskbutton = []
		self.selbutton = []
		self.octetbutton = []
		self.MapYlabel = []
		self.MapXlabel = []

		if not os.path.isfile("pixel_map.txt"): return
		pm = open("pixel_map.txt", "r")
		self.pixmap = ["" for i in sh.Channels]
		for line in pm:
			p = line.split()
			ch = int(p[0])
			if ch >= 0 and ch < sh.MaxCh: self.pixmap[ch] = p[1]
		pm.close	

		if sys.platform.find('win') < 0: xw = 310  # Linux
		else: xw = 270 		# Windows
		yw = 420
		#yw = 370
		self.PlotMaskWin = Toplevel()
		self.PlotMaskWin.geometry("{}x{}+{}+{}".format(xw, yw, 550, 200))
		#self.PlotMask.overrideredirect(1)  # no window bar
		self.PlotMaskWin.wm_title("")
		self.PlotMaskWin.protocol("WM_DELETE_WINDOW", self.ClosePlotMaskWin)
		self.PlotMaskWinIsOpen = True
		Frame(self.PlotMaskWin, width=xw, height=yw, relief=RIDGE).place(x=0, y=0) # , bd=2

		self.no_update = True  # prevent updating while setting the mask with initial values
		self.PlotTraceSelVar.set(0)

		x0, y0 = 17, 5
		self.sp = 26
		Label(self.PlotMaskWin, text = "Plot Traces").place(relx=float(x0)/xw, rely=float(y0)/yw) #  x = x0, y = y0)

		y0 += 20
		for i in range(8):
			self.selbutton.append(Radiobutton(self.PlotMaskWin, text='T'+str(i), value = i, variable=self.PlotTraceSelVar, command=self.ActiveTrace, indicatoron=0)) # , height = 1, width=2
			self.selbutton[i].place(relx=0.063+i*0.096, rely=float(y0)/yw, relwidth=0.093, relheight=0.06) #  x = x0 + self.sp*i, y = y0)
			self.selbutton[i].config(font=("Arial Bold", 10))
			if self.PlotTraceSel[i] != "": self.selbutton[i].config(fg='red')

		y0 += 40
		Label(self.PlotMaskWin, text = "Brd", font=("Arial", 12)).place(relx=float(x0)/xw, rely=float(y0)/yw) #   x = x0, y = y0)
		Spinbox(self.PlotMaskWin, textvariable=self.Tbrd, from_=0, to=sh.MaxBrd-1, command=self.UpdateMask, state='readonly', font=("Arial", 14), bg="white").place(relx=0.193, rely=float(y0+1)/yw, relwidth=0.241, relheight=26.04/420)
		#	x = x0 + 35, y = y0) , width=4
		Button(self.PlotMaskWin, text='Trace-OFF', command=self.DisableTrace).place(relx=0.444, rely=float(y0)/yw, relwidth=0.241, relheight=26.04/420) # , width=8 x = x0 + 100, y = y0)
		Button(self.PlotMaskWin, text='All-OFF', command=self.DisableAllTraces).place(relx=0.685, rely=float(y0)/yw, relwidth=0.241, relheight=26.04/420) #  , width=8 x = x0 + 172, y = y0)

		#### From board or file option, two Radiobutton
		y0 += 40
		vcmd2 = (self.PlotMaskWin.register(self.validate_RunNumber), '%P')	# font=("Arial", 10),
		Radiobutton(self.PlotMaskWin, text="Online", variable=self.FromBrdFile, value="B", indicatoron=0, command=self.UpdateMask).place(relx=0.063, rely=float(y0)/yw, relwidth=0.19, relheight=25.2/420) # ,, height=1 width=6  x=x0, y=y0)
		Radiobutton(self.PlotMaskWin, text="Offline", variable=self.FromBrdFile, value="F", indicatoron=0, command=self.UpdateMask).place(relx=0.25, rely=float(y0)/yw, relwidth=0.19, relheight=25.2/420) #    x=x0+50, y=y0) # 8 - 70   , height=1, width=6
		Radiobutton(self.PlotMaskWin, text="Browse", variable=self.FromBrdFile, value="S", indicatoron=0, # width = 6 to include browse button, no font too!
		 	command=self.UpdateMask).place(relx=0.433, rely=float(y0)/yw, relwidth=0.19, relheight=25.2/420) #   x=x0+100, y=y0) # state='disabled' , height=1, width=6
		#### Set the Run# of the plot you want to visualize
		self.SelRunFile.append(Label(self.PlotMaskWin, text="Run#", font=("Arial Bold", 11)))
		# self.SelRunFile[0].place(x=x0+155, y=y0+1)
		self.RunFromFile.trace("w", lambda name, index, mode: self.UpdateMask())
		self.SelRunFile.append(Spinbox(self.PlotMaskWin, textvariable=self.RunFromFile, from_=0, to=1000, command=self.UpdateMask, 
			validate="key", validatecommand=vcmd2, font=("Arial Bold", 12), width=2))
		# self.SelRunFile[1].place(x=x0+200, y=y0+1)
		for i in range(8):
			self.ExtRunFile.append(StringVar())
			self.ExtRunFile[i].set("")
			self.EntryRunFile.append(Entry(self.PlotMaskWin, textvariable=self.ExtRunFile[i], width=40))
		# self.SelRunFile.append(Entry(self.PlotMaskWin, textvariable=self.ExtRunFile, width=40, state='readonly'))
		# self.SelRunFile[2].place(x=x0, y=y0+10)
		self.SelRunFile.append(Button(self.PlotMaskWin, text="...", command=self.OpenExtRunFile))  #  , height=1, width=2
		# self.SelRunFile[3].place(x=x0+8*self.sp+5, y=y0+10)
		self.FromBrdFile.trace("w", lambda name, index, mode: self.EnableRunFile())	# Enable the legend and the SpinBox
		#### I would add a button "Trace-ON" to activate the trace, as the opposite of Trace-OFF
		
		y0 += 20
		self.xm = x0
		self.ym = y0

		for y in range(8):
			self.octetbutton.append(Button(self.PlotMaskWin, text='', command=lambda octet=y: self.AssignOctet(octet)))  #  , height = 1, width=2
			self.octetbutton[y].place(relx=0.063+8*0.096, rely=float(y0+(y+1)*self.sp)/yw, relwidth=0.093, relheight=0.06)  #   x = x0+8*self.sp + 5, y=y0+(y+1)*self.sp)
			self.MapYlabel.append(Label(self.PlotMaskWin, text = ""))
			self.MapYlabel[y].place(relx=0.011, rely=float(y0+(y+1)*self.sp)/yw)  #  x = 3, y = y0+(y+1)*self.sp)
			for x in range(8): 
				if y == 0: 
					self.MapXlabel.append(Label(self.PlotMaskWin, text = ""))
					self.MapXlabel[x].place(relx=0.081 + x*0.096, rely=float(y0+9*self.sp-1)/yw)  #  x = x0+5+x*self.sp, y = y0+9*self.sp-1)
				i = 8*y+x
				self.maskbutton.append(Radiobutton(self.PlotMaskWin, text=str(i), value = str(i), variable=self.PlotMaskVar, command=self.UpdateMask, indicatoron=0))  #  , height = 1, width=2
				self.maskbutton[i].place(relx=0.063 + x*0.096, rely=float(y0+(y+1)*self.sp)/yw, relwidth=0.093, relheight=0.06)  #  x = x0+x*self.sp, y=y0+(y+1)*self.sp
		if self.GetBrdCh():
			self.PlotMaskVar.set(str(self.Tch))

		y0 = y0 + self.sp*9 + 20
		Checkbutton(self.PlotMaskWin, variable = self.Xcalib, command = self.SaveRunVars, height = 1, width=1).place(relx=float(x0)/xw, rely=float(y0)/yw)  #   x = x0, y = y0)
		Label(self.PlotMaskWin, text = "Calib X-axis").place(relx=float(x0+20)/xw, rely=float(y0+2)/yw)  #  x = x0 + 20, y = y0+2)
		self.EnablePixelMap = IntVar()
		Checkbutton(self.PlotMaskWin, text='Pixel Map', variable = self.EnablePixelMap, indicatoron=0).place(relx=float(x0+140)/xw, rely=float(y0)/yw, relwidth=0.26, relheight=26.04/420)  # , width=8 x = x0 + 140, y = y0)
		self.EnablePixelMap.trace('w', lambda name, index, mode: self.PixelMap())

		self.no_update = False
		self.InitPlotMaskButtons = False

	############################################################
	## Methods for OpenPlotMaskWin
	############################################################
	def UpdateMask(self):
		if self.no_update: return
		sel = self.PlotTraceSelVar.get()
		if self.FromBrdFile.get() == "S":
			[self.EntryRunFile[i].place_forget() for i in range(8)]
			[self.EntryRunFile[sel].place(relx=(17./270), rely=(130./420), relwidth=0.88, relheight=19/420)]   #  x=17, y=125+5)]
		if self.PlotMaskVar.get() == "": 
			self.PlotTraceSel[sel]=""
			self.selbutton[sel].config(fg='black')
			if self.FromBrdFile.get() == "S" and len(self.ExtRunFile[sel].get().split())>0: 
				self.BrdFile = self.FromBrdFile.get() + self.ExtRunFile[sel].get()
				# self.PlotMaskVar is empty, so better we put 0! in such a case maybe the
				# RunVars reading would create a new trace switching to On/Offline..is that correct?
				self.PlotTraceSel[sel] = str(self.Tbrd.get()) + " 0 " + self.BrdFile
				self.selbutton[sel].config(fg='red')
		else:
			if self.FromBrdFile.get() == "F": self.BrdFile = self.FromBrdFile.get() + self.RunFromFile.get() # str(self.RunFromFile.get())
			elif self.FromBrdFile.get() == "B": self.BrdFile = self.FromBrdFile.get()
			elif self.FromBrdFile.get() == "S": self.BrdFile = self.FromBrdFile.get() + self.ExtRunFile[sel].get()
			self.PlotTraceSel[sel] = str(self.Tbrd.get()) + " " + self.PlotMaskVar.get() + " " + self.BrdFile	#B for Online FRun_Number for Offine
			self.selbutton[sel].config(fg='red')		
		self.SaveRunVars()

	def AssignOctet(self, octet):
		if self.no_update: return
		# Set Variable
		if self.FromBrdFile.get() == "F": self.BrdFile = self.FromBrdFile.get() + self.RunFromFile.get() # str(self.RunFromFile.get())
		elif self.FromBrdFile.get() == "B": self.BrdFile = self.FromBrdFile.get()
		for t in range(8):
			if self.EnablePixelMap.get() == 0:
				ch = octet*8+t
			else:
				pixs = chr(ord('A') + t) + chr(ord('0') + 8 - octet)
				ch = self.pixmap.index(pixs)
				if ch < 0 or ch > 63: continue
			if t==self.PlotTraceSelVar.get(): cc = ch	
			self.PlotTraceSel[t] =  str(self.Tbrd.get()) + " " + str(ch) + " " + self.BrdFile
			self.PlotMaskVar.set(str(ch))
			self.selbutton[t].config(fg='red')
		self.PlotMaskVar.set(str(cc))
		self.SaveRunVars()

	def ActiveTrace(self):
		if self.no_update: return
		if self.GetBrdCh():	self.PlotMaskVar.set(str(self.Tch))
		else: self.PlotMaskVar.set("")

	def DisableTrace(self):
		if self.no_update: return
		sel = self.PlotTraceSelVar.get()
		self.PlotTraceSel[sel] = ""
		self.PlotMaskVar.set("")
		self.ExtRunFile[sel].set("")
		self.selbutton[sel].config(fg='black')
		self.SaveRunVars()
		
	def DisableAllTraces(self):
		if self.no_update: return
		self.no_update = True # Prevent Updating Mask while setting RunVars
		for sel in range(8):
			self.PlotTraceSel[sel] = ""
			self.PlotMaskVar.set("")
			self.FromBrdFile.set("B")
			self.RunFromFile.set("1")
			self.selbutton[sel].config(fg='black')
		self.SaveRunVars()
		self.no_update = False

	def GetBrdCh(self):
		ts = self.PlotTraceSel[self.PlotTraceSelVar.get()].split()
		# [self.EntryRunFile[i].place_forget() for i in range(8)]
		# self.EntryRunFile[self.PlotTraceSelVar.get()].place(x=17, y=125+5)
		if len(ts) != 3: 
			[self.EntryRunFile[i].place_forget() for i in range(8)]
			if self.FromBrdFile.get() == "S":
				self.EntryRunFile[self.PlotTraceSelVar.get()].place(relx=17./270, rely=130./420, relwidth=0.88, relheight=19/420)   #   x=17, y=125+5)
			return False	
		if len(ts[0]) > 0: 
			try: self.Tbrd.set(int(ts[0]))
			except: self.Tbrd.set(0)
		else: self.Tbrd.set(0)
		if len(ts[1]) > 0: 
			try: self.Tch = int(ts[1])
			except: self.Tch = 0
		else: self.Tch = 0
		# if len(ts[2]) > 0:
		# 	self.FromBrdFile.set(ts[2][0])
		# 	self.EnableRunFile()
		# if len(ts[2]) > 1: self.RunFromFile.set(ts[2][1:])
		# else: self.RunFromFile.set(1)
		if len(ts[2]) > 0: 
			self.FromBrdFile.set(ts[2][0])
			if self.FromBrdFile.get() == "B":
				self.RunFromFile.set("1")
			elif self.FromBrdFile.get() == "F":
				self.RunFromFile.set(str(ts[2][1:]))
				self.SelRunFile[0].config(state="normal")
				self.SelRunFile[1].config(state="normal")
				# self.SelRunFile[2].place_forget()
			elif self.FromBrdFile.get() == "S":
				self.ExtRunFile[self.PlotTraceSelVar.get()].set(ts[2][1:])
				[self.EntryRunFile[i].place_forget() for i in range(len(self.EntryRunFile))]
				self.EntryRunFile[self.PlotTraceSelVar.get()].place(relx=17./270, rely=130./420, relwidth=0.88, relheight=19/420)  #  x=17, y=125+5)
				# self.RunFromFile.set(1)
		else: 
			self.FromBrdFile.set("B")
			[self.SelRunFile[i].place_forget() for i in range(len(self.SelRunFile))]
			[self.EntryRunFile[i].place_forget() for i in range(len(self.EntryRunFile))]
		return True

	def PixelMap(self):
		x0, y0 = self.xm, self.ym
		for y in range(8):
			if self.EnablePixelMap.get() == 1: self.MapYlabel[y].config(text = str(8-y))
			else: self.MapYlabel[y].config(text = "")
			for x in range(8): 
				if y == 0: 
					if self.EnablePixelMap.get() == 1: self.MapXlabel[x].config(text = chr(65+x))
					else: self.MapXlabel[x].config(text = "")
				i = 8*y+x
				if self.EnablePixelMap.get() == 1:
					xp = ord(self.pixmap[i][0]) - ord('A')
					yp = 7 - (ord(self.pixmap[i][1]) - ord('1'))
				else:	
					xp, yp = x, y
				self.maskbutton[i].place(relx=float(x0+xp*self.sp)/270, rely=float(y0+(yp+1)*self.sp)/420)  #  x = x0+xp*self.sp, y=y0+(yp+1)*self.sp)

	def EnableRunFile(self):
		if self.FromBrdFile.get() == 'B':
			[self.SelRunFile[i].place_forget() for i in range(len(self.SelRunFile))]
			[self.EntryRunFile[i].place_forget() for i in range(8)]
			[self.octetbutton[i].config(state = "normal") for i in range(8)]
			[self.maskbutton[i].config(state = "normal") for i in range(64)]
		elif self.FromBrdFile.get() == 'F':
			self.SelRunFile[0].place(relx=170./270, rely=106./420)  #  x=17+155, y=105+1)
			self.SelRunFile[1].place(relx=0.78, rely=106./420, relwidth=0.14, relheight=23.52/420)  #  x=17+200, y=105+1)
			[self.SelRunFile[i+2].place_forget() for i in range(1)]
			[self.EntryRunFile[i].place_forget() for i in range(8)]
			[self.octetbutton[i].config(state = "normal") for i in range(8)]
			[self.maskbutton[i].config(state = "normal") for i in range(64)]
		elif self.FromBrdFile.get() == 'S':	# not yey implemented
			[self.SelRunFile[i].place_forget() for i in range(2)]
			# self.SelRunFile[2].place(x=17, y=125+5)
			self.SelRunFile[2].place(relx=0.64, rely=104./420, relwidth=25.2/270, relheight=25.2/420)  # DNIN: scale to 420 and 270 x=17+155, y=104)
			# self.SelRunFile[2].config(state = 'readonly')
			self.EntryRunFile[self.PlotTraceSelVar.get()].place(relx=17./270, rely=130./420, relwidth=0.88, relheight=19/420)  #  x=17, y=125+5)
			[self.octetbutton[i].config(state = "disabled") for i in range(8)]
			[self.maskbutton[i].config(state = "disabled") for i in range(64)]

		# if self.FromBrdFile.get() == "F": 
		# 	[self.SelRunFile[i].config(state="normal") for i in range(2)]
		# 	self.BrdFile = self.FromBrdFile.get() + str(self.RunFromFile.get())
		# else: 
		# 	[self.SelRunFile[i].config(state="disabled") for i in range(2)]
		# 	self.BrdFile = self.FromBrdFile.get()

	def OpenExtRunFile(self):	# for loading histograms from specific file
		[self.EntryRunFile[i].place_forget() for i in range(8)]
		self.EntryRunFile[int(self.PlotTraceSelVar.get())].place(relx=17./270, rely=130./420, relwidth=0.88, relheight=19/420)  #  x=17, y=125+5)
		try:
			self.ExtRunFile[int(self.PlotTraceSelVar.get())].set(os.path.relpath(askopenfilename(initialdir=".", filetypes=(("Text File", "*.txt"), ("All Files", "*.*")), title="Choose a file.")))
		except:
			self.ExtRunFile[int(self.PlotTraceSelVar.get())].set("")
		self.UpdateMask()
		
	def ClosePlotMaskWin(self):
		self.UpdateMask()
		self.PlotMaskWin.destroy()
		self.PlotMaskWinIsOpen = False


	# ***************************************************************************************
	# Popup window for Staircase
	# ***************************************************************************************
	def OpenSpecialRunWin(self, RunType, parent):
		self.MinScan = StringVar()
		self.MaxScan = StringVar()
		self.StepScan = StringVar()
		self.DwellNpts = StringVar()
		self.StepValue = ["8", "16", "24", "32", "40"]
		self.RunType = RunType
		vcmd = (parent.register(self.validate_RunNumber), '%P')
		if self.RunType == 'Staircase': ss = self.StaircaseSettings.split()
		else: ss = self.HoldScanSettings.split()
		if len(ss) == 4:
			self.MinScan.set(ss[0])
			self.MaxScan.set(ss[1])
			self.StepScan.set(ss[2])
			self.DwellNpts.set(ss[3])
		elif self.RunType == 'Staircase':	
			self.MinScan.set("150")
			self.MaxScan.set("300")
			self.StepScan.set("1")
			self.DwellNpts.set("500")
		else:	
			self.MinScan.set("0")
			self.MaxScan.set("256")
			self.StepScan.set("8")
			self.DwellNpts.set("500")
		xw = 195
		yw = 230 # 250
		if self.SpecialRunWinIsOpen: self.CloseSpecialRunWin()
		self.SpecialRunWin = Toplevel()
		self.SpecialRunWin.geometry("{}x{}+{}+{}".format(xw, yw, 550, 250))
		self.SpecialRunWin.wm_title("")
		self.SpecialRunWin.protocol("WM_DELETE_WINDOW", self.CloseSpecialRunWin)
		self.SpecialRunWinIsOpen = True
		Frame(self.SpecialRunWin, width=xw, height=yw, relief=RIDGE).place(x=0, y=0)  #  , bd=2

		x0, y0 = 5, 5
		sp = 26
		if self.RunType == 'Staircase': 
			Label(self.SpecialRunWin, text = "Staircase Settings", font=("Arial Bold", 10)).place(relx=float(x0)/xw, rely=float(y0)/yw)  #  x = x0, y = y0)
			Label(self.SpecialRunWin, text = "Min Threshold").place(relx=float(x0)/xw, rely=float(y0+sp)/yw)  #  x = x0, y = y0+sp)
			Label(self.SpecialRunWin, text = "Max Threshold").place(relx=float(x0)/xw, rely=float(y0+sp*2)/yw)  #  x = x0, y = y0+sp*2)
			Label(self.SpecialRunWin, text = "Step").place(relx=float(x0)/xw, rely=float(y0+sp*3)/yw)  #  x = x0, y = y0+sp*3)
			Label(self.SpecialRunWin, text = "Dwell Time (ms)").place(relx=float(x0)/xw, rely=float(y0+sp*4)/yw)  #  x = x0, y = y0+sp*4)
			Entry(self.SpecialRunWin, textvariable=self.StepScan, validate='key', validatecommand=vcmd, width=8).place(relx=float(x0+130)/xw, rely=float(y0+sp*3)/yw, relwidth=52./195, relheight=20./250)  #  x = x0 + 130, y = y0+sp*3)
		else:	
			Label(self.SpecialRunWin, text = "Hold Scan Settings", font=("Arial Bold", 10)).place(relx=float(x0)/xw, rely=float(y0)/yw) # x = x0, y = y0)
			Label(self.SpecialRunWin, text = "Min Delay (ns)").place(relx=float(x0)/xw, rely=float(y0+sp)/yw) # x = x0, y = y0+sp)
			Label(self.SpecialRunWin, text = "Max Delay (ns)").place(relx=float(x0)/xw, rely=float(y0+sp*2)/yw) # x = x0, y = y0+sp*2)
			Label(self.SpecialRunWin, text = "Step (mult. of 8 ns)").place(relx=float(x0)/xw, rely=float(y0+sp*3)/yw) # x = x0, y = y0+sp*3)
			Label(self.SpecialRunWin, text = "Num averaged points").place(relx=float(x0)/xw, rely=float(y0+sp*4)/yw) # x = x0, y = y0+sp*4)
			ttk.Combobox(self.SpecialRunWin, values=self.StepValue, textvariable=self.StepScan, state="readonly", width=5).place(relx=float(x0+130)/xw, rely=float(y0+sp*3)/yw)  #  x = x0 + 130, y = y0+sp*3)
		Entry(self.SpecialRunWin, textvariable=self.MinScan, validate='key', validatecommand=vcmd, width=8).place(relx=float(x0+130)/xw, rely=float(y0+sp)/yw, relwidth=52./195, relheight=20./250) # x = x0 + 130, y = y0+sp)
		Entry(self.SpecialRunWin, textvariable=self.MaxScan, validate='key', validatecommand=vcmd, width=8).place(relx=float(x0+130)/xw, rely=float(y0+sp*2)/yw, relwidth=52./195, relheight=20./250) # x = x0 + 130, y = y0+sp*2)
		Entry(self.SpecialRunWin, textvariable=self.DwellNpts, validate='key', validatecommand=vcmd, width=8).place(relx=float(x0+130)/xw, rely=float(y0+sp*4)/yw, relwidth=52./195, relheight=20./250) # x = x0 + 130, y = y0+sp*4)

		y0 += sp*4 + 50
		self.sc_run = Button(self.SpecialRunWin, text='Start Scan', command=self.StartScan)  #  , height = 1, width=25
		self.sc_run.place(relx=float(x0)/xw, rely=float(y0)/yw, relwidth=0.95, relheight=0.11) # x = x0, y= y0)
		self.sc_run.config(state=NORMAL)

		y0 += 30
		self.sc_progress = Progressbar(self.SpecialRunWin, orient = HORIZONTAL, length = 185, mode = 'determinate') 
		self.sc_progress.place(relx=float(x0)/xw, rely=float(y0)/yw, relwidth=0.95, relheight=0.11) # x = x0, y = y0)

	def StartScan(self):
		self.sc_run.config(state=DISABLED)
		if self.RunType == 'Staircase': self.StaircaseSettings = self.MinScan.get() + ' ' + self.MaxScan.get() + ' ' + self.StepScan.get() + ' ' + self.DwellNpts.get()
		else: self.HoldScanSettings = self.MinScan.get() + ' ' + self.MaxScan.get() + ' ' + self.StepScan.get() + ' ' + self.DwellNpts.get()
		self.SaveRunVars()
		if self.RunType == 'Staircase': 
			comm.SendCmd('y')
		else: 
			comm.SendCmd('Y')

	def CloseSpecialRunWin(self):
		self.SpecialRunWin.destroy()
		self.SpecialRunWinIsOpen = False


##############################################################################
# Convert Binary file to CSV format
##############################################################################
	def ConvertBin2CSV(self):
		if self.ConvWinIsOpen: self.CloseConvWin()
		xw = 360
		yw = 520
		y_displ = 0.82/16
		displ_x = 35
		start_y = 28.14/yw # 0.067
		start_y2 = 29.82/yw # 0.071
		self.Bin_fname = []
		self.ConvWin = Toplevel()
		self.ConvWin.geometry("{}x{}+{}+{}".format(xw, yw, 550, 200))
		self.ConvWin.wm_title("Convert binary files into CSV format")
		# self.ConvWin.attributes('-topmost', 'true')
		self.ConvWin.protocol("WM_DELETE_WINDOW", self.CloseConvWin)
		self.ConvFrame = Frame(self.ConvWin, width=xw, height=yw)#, relief=RIDGE, bd=2)
		self.ConvFrame.place(x=0, y=0)
		self.ConvWinIsOpen = True
		self.bFileName = StringVar()
		 
 		# self.ExtCfgFileName = [] # Binary File path
		self.BinVarList = StringVar()	# indexes of the Bin path
		self.BinVarList.set("0")	
		self.BinFilesList = []	# Entry where Bin File are displaied (maybe the stringvar can be avoid??)
		self.NumBinFile = [] # Radiobutton of the indexes
		self.pr_idx = "0"

		Label(self.ConvWin, text="Insert/Browse the binary files to be converted", font=("Arial Bold", 10), anchor="center").place(relx=0.097, rely=4.2/yw) #x=displ_x, y=4)
		for i in range(16):
			self.Bin_fname.append(StringVar())
			# self.ExtCfgFileName[i].set("") # DNIN: Is it useful?
			self.NumBinFile.append(Button(self.ConvWin, fg='black', text=str(i+1), command=lambda i=i: self.SelectToConvertFile(i)))
			self.NumBinFile[i].place(relx=0.011, rely=start_y+y_displ*i, relwidth=0.07, relheight=y_displ) #  x=4, y=start_y+displ_y*i-2)
			self.BinFilesList.append(Entry(self.ConvWin, textvariable = self.Bin_fname[i])) # width=50
			self.BinFilesList[i].place(relx=0.097, rely=start_y2+y_displ*i, relwidth=0.85, relheight=(y_displ-0.01)) #   x = displ_x, y = start_y+displ_y*i)

		self.BinVarList.trace('w', lambda name, index, mode: self.ActivePath())
		self.BinFilesList[0].config(state=NORMAL)
		
		t_unit = []
		t_unit.append('LSB')
		t_unit.append('ns')
		# Label(self.ConvWin, text='ToA/ToT Unit:', font=("Arial Bold", 10)).place(relx=0.097, rely=0.1+0.06*14)
		Checkbutton(self.ConvWin, text='Force ToA/ToT to ns', font=("Arial Bold", 10), variable=self.time_unit).place(relx=0.097, rely=0.045+0.06*14)
		Checkbutton(self.ConvWin, text='List of binary files names', font=("Arial Bold", 10), variable=self.list_of_bfile).place(relx=0.097, rely=0.09+0.06*14)
		Button(self.ConvWin, text='Convert', command=self.ConvertFile, bg='#00FF00').place(relx=0.097+0.55, rely=0.06+0.06*14, relwidth=0.25, relheight=0.075) # , width=12  x=displ_x+170, y=start_y+displ_y*14+5)

	def ActivePath(self):
		self.BinFilesList[int(self.pr_idx)].config(state='readonly')
		self.BinFilesList[int(self.BinVarList.get())].config(state='normal')
		self.pr_idx = self.BinVarList.get()

	def SelectToConvertFile(self, index):
		try:
			mff = askopenfilename(initialdir="DataFiles", filetypes=(("Binary files", "*.dat"), ("Binary files", "*.bin"), ("All Files", "*.*")), title="Choose a file.")
			self.Bin_fname[index].set(os.path.relpath(mff))
		except:
			self.Bin_fname[index].set("")

	def ConvertFile(self):
		ON_POSIX = 'posix' in sys.builtin_module_names
		exe_cmd = [] # list with the command to execute - executable + input arguments
		if sys.platform.find('win') < 0: 
			exe_name = "./BinToCsv"	# name of the executable on linux
		else:
			exe_name = "BinToCsv.exe"

		if not os.path.exists(exe_name):	#   Error: BinToCsv is not the folder and cannot be launched
			Jmsg="Warning, BinToCsV executable is missing!!!\nPlease, check if the antivirus cancel it during the unzip (Windows)"
			Jmsg=Jmsg+"\nor run Janus_Install.sh from the main folder (Linux)"
			messagebox.showwarning(title=None, message=Jmsg)
			self.CloseConvWin()
			return

		exe_cmd.append(exe_name)
		if self.time_unit.get(): exe_cmd.append('--ns') # The time is forced to be converted in ns
		if self.list_of_bfile.get(): exe_cmd.append("--lfile") # The file inserted contains the list of binary file to convert
		else: exe_cmd.append("--bfile")  # Option to append binary files to convert
		for ffb in self.Bin_fname:
			if len(ffb.get()) > 0:
				exe_cmd.append(ffb.get())
		
		self.ConvCsvTrace.set(1)
		# display on log that the conversion started
		process = subprocess.Popen(exe_cmd, shell=False) # Run on a separate process from the GUI
		# process = subprocess.Popen(["BinToCsv.exe", self.Bin_fname], shell=False) # Run on a separate process from the GUI
		self.CloseConvWin()
	
	def CloseConvWin(self):
		self.ConvWin.destroy()
		self.ConvWinIsOpen = False

