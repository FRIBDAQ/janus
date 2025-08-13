# ------------------------------------------------------------------
# python GUI for PyCROS (FERS readout software by Tintori)
# ------------------------------------------------------------------

import sys
import subprocess
import time
import os
import re
import json

from threading import Thread, Lock

from tkinter import *
from tkinter import messagebox
from tkinter import ttk
from tkinter import font
from tkinter.filedialog import askdirectory

import shared as sh
import leds as leds
import cfgfile_rw as cfg
import socket2daq as comm
import tooltips as tt

params = sh.params
sections = sh.sections

# *******************************************************************************
# Main Tabs
# *******************************************************************************

class TabsPanel():
	def __init__(self):

		# self.defaultFont = font.nametofont("TkDefaultFont")
		# print(self.defaultFont)
		if sys.platform.find('win') < 0:
			sh.ImgPath = '../img/'
		self.img_hvon = PhotoImage(file=sh.ImgPath + "pwon.png").subsample(3, 3)

		self.MaskWinIsOpen = False
		self.Mask = [IntVar() for i in range(64)]
		self.ActiveBrd = IntVar()
		self.ActiveBrd.set(0)
		self.ActiveBrd.trace('w', lambda name, index, mode: self.SendActiveBrd())    # comm.SendCmd('b ' + str(self.ActiveBrd.get()))) # 

		# Default settings
		self.par_def_svar = {}			# String Var 
		self.par_def_entry = {}			# Entry
		self.par_def_checkbox = {}		# check box
		self.par_def_combo = {}			# combo
		self.par_def_spinbox = {}		# spinbox
		self.par_def_label = {}			# label - to change GUImode
		# Board settings
		self.par_brd_svar = {}			# String Var 
		self.par_brd_entry = {}			# Entry
		self.par_brd_checkbox = {}		# check box
		self.par_brd_combo = {}			# combo
		# Channel settings
		self.par_ch_svar = {}			# String Var 
		self.par_ch_entry = {}			# Entry
		self.par_ch_checkbox = {}		# check box
		self.par_ch_combo = {}			# combo
		self.par_ch_label = {}			# labels (monitror)

		# Widget positions
		self.ypos_def = {}				# y pos of the widget
		self.ypos_brd = {}				# y pos of the widget
		self.ypos_ch = {}				# y pos of the widget

		self.button_names = {}			# additional widget on tabs

		self.StopUpdate = False

		self.param_rename = {}
		if os.path.isfile("param_rename.txt"):
			cf = open("param_rename.txt", "r")
			for line in cf:
				line = line.split('=')
				if len(line) == 2:
					self.param_rename[line[0].strip()] = line[1].strip()
			cf.close		

		self.status_now = sh.ACQSTATUS_DISCONNECTED
		self.CfgChanged = IntVar()
		self.CfgChanged.set(0)
		self.change_statistics = IntVar()
		self.change_statistics.set(0)
		self.change_statistics.trace('w', lambda name, index, mode:self.ChgStatMode()) #new function that changes the visualization and sends a command to JanusC for change the statistics
		self.change_stat_integral = IntVar()
		self.change_stat_integral.set(0)
		self.change_stat_integral.trace('w', lambda name, index, mode:self.ChgStatIntegr())

		# self.change_statistics.trace('w', lambda name, index, mode: self.all_brd_statistics)

		self.update_stats = True
		self.gui_update = True

		self.AllBrdLabel = [] # for statistic visualization change
		self.AllBrdCounts = {}

		self.AcqMode_Dict = {} # {"AcqMode0": 0, "AcqMode1": 1 ...}, for GUI visualization, making it more general

		self.new_options = {}	# GUI parameter new options 
		self.offline = False

	def validate_ch(self, new_value):
		if new_value.isdigit(): return int(new_value) < sh.MaxCh
		elif not new_value: return True
		else: return False

	def SendActiveBrd(self):
		try: comm.SendCmd(f"b{str(self.ActiveBrd.get())}")
		except: pass


	def OpenTabs(self, parent):
		# ***************************************************************************************
		# create Notebook for Main Tabs (tabs are defined in param_defs file)
		# ***************************************************************************************
		self.Mtabs_nb = ttk.Notebook(parent)
		self.Mtabs = {}
		self.Mtabs_shown = {}
		i = 0
		for s in sections:
			self.Mtabs[s] = ttk.Frame(self.Mtabs_nb)  #, width=sh.Win_Tabs_W, height=sh.Win_Tabs_H)
			self.Mtabs_nb.add(self.Mtabs[s], text=' ' + s + ' ')
			i += 1	

		self.Mtabs_nb.place(relx=float(sh.Win_Tabs_X)/sh.Win_W, rely=float(sh.Win_Tabs_Y)/sh.Win_H, relwidth=float(sh.Win_Tabs_W+3)/sh.Win_W, relheight=float(sh.Win_Tabs_H+26)/sh.Win_H)  #    x=sh.Win_Tabs_X, y=sh.Win_Tabs_Y)
	
		# ***************************************************************************************
		# Create controls for the params of the config file (distributed over multiple tabs)
		# ***************************************************************************************
		# fill user tabs with parameters defined in param_defs
		x_def = 140	# x-pos of default entry/combo
		x_brd = 3	# x-pos of board entry/combo
		x_ch = 300	# x-pos of channel entry/combo
		yrow = {s: 20 for s in sections} # initial Y-position for default and channel rows (one variable per section)
		ypos = {}	# save y pos of each parameter
		#create_brdtabs = {s: TRUE for s in sections} # flag indacating that board tabs are not created yet
		#create_chtabs = {s: TRUE for s in sections} # flag indacating that channel tabs are not created yet
		ngr = 8  # num of groups
		nch_gr = 8  # num of channels per group

		# find sections that require board and channel tabs (0=no tabs, 1=board tab only, 2=board and channel tabs)
		self.tabmode = {s : 0 for s in sections}
		for param in params.values():
			if param.distr == 'c': 
				self.tabmode[param.section] = 2
				yrow[param.section] = 40 #68
			if param.distr == 'b' and self.tabmode[param.section] == 0: 
				self.tabmode[param.section] = 1
				yrow[param.section] = 10 #16

		# create TABs for boards and channels
		self.BrdTabs_nb = {}
		self.BrdTabs = {}
		self.ChTabs_nb = {}
		self.ChTabs = {}
		self.GlbTab = {}
		self.GlbFrame_nb = {s:ttk.Notebook(self.Mtabs[s]) for s in sections}
		self.GlbFrame = {s:ttk.Frame(self.GlbFrame_nb[s]) for s in sections}
		self.Level2nd_nb = {s:ttk.Notebook(self.GlbFrame[s]) for s in sections}
		self.Level2nd = {s:ttk.Frame(self.Level2nd_nb[s]) for s in sections} 
		for s in sections:
			if self.tabmode[s] > 0 and s != 'Connect':
				self.GlbFrame_nb[s].place(relx=0, rely=0, relwidth=1, relheight=1)   # to allign with boards 
				self.GlbFrame_nb[s].add(self.GlbFrame[s], text="Global Settings")
				self.GlbTab[s] = self.GlbFrame[s]

				self.BrdTabs_nb[s] = ttk.Notebook(self.Mtabs[s])
				self.aaa = ttk.Notebook(self.Mtabs[s])
				self.aaa.winfo_geometry()
				self.BrdTabs_nb[s].place(relx=256./sh.Win_W, rely=0, relwidth=float(sh.Win_Tabs_W-x_ch+49)/sh.Win_W, relheight=float(sh.Win_Tabs_H)/sh.Win_Tabs_H)  #  x=256, y=0)
				self.BrdTabs[s] = [ttk.Frame(self.BrdTabs_nb[s]) for i in sh.Boards] # , width=sh.Win_Tabs_W - x_ch + 40, height=sh.Win_Tabs_H - 40
				for brd in sh.Boards: 
					self.BrdTabs_nb[s].add(self.BrdTabs[s][brd], text=str("B" + str(brd)))
				if self.tabmode[s] == 2:
					self.Level2nd_nb[s].place(relx=0, rely=0, relwidth=1, relheight=1)
					self.Level2nd_nb[s].add(self.Level2nd[s], text="")
					Frame(self.GlbFrame[s]).place(relx=0, rely=0, relwidth=1, relheight=0.1)
					self.GlbTab[s] = self.Level2nd[s]

					self.ChTabs[s] = []
					self.ChTabs_nb[s] = []
					for brd in sh.Boards:
						self.ChTabs_nb[s].append(ttk.Notebook(self.BrdTabs[s][brd]))
						self.ChTabs_nb[s][brd].place(relx=0, rely=0, relwidth=float(sh.Win_Tabs_W)/sh.Win_Tabs_W, relheight=float(sh.Win_Tabs_H+2)/sh.Win_Tabs_H)  #   x=0, y=0)	
						self.ChTabs[s].append([])
						for gr in range(ngr):
							self.ChTabs[s][brd].append(ttk.Frame(self.ChTabs_nb[s][brd]))  #  , width=sh.Win_Tabs_W - x_ch + 10, height=sh.Win_Tabs_H - 40
							self.ChTabs_nb[s][brd].add(self.ChTabs[s][brd][gr], text='   ' + str(gr*nch_gr) + ':' + str((gr+1)*nch_gr-1)+ '   ')
			else:
				self.GlbTab[s] = self.Mtabs[s]

		# for s in sections: 
		# 	yrow[s] = 5
		for param in params.values():
			if param.section == 'Connect': # this has a separate management!
				self.conn_path = [StringVar() for i in sh.Boards]
				for brd in sh.Boards:
					self.conn_path[brd].set(param.value[brd])
				continue
			
			tab = self.GlbTab[param.section]  # self.Mtabs[param.section]
			yd = yrow[param.section]  	# y position for parameter label and control (default setting)
			yb = yd #*14.1/13.1 - 24.756  # y position for relative placement (board setting)  yb = yd - 23 # y position for parameter label and control (board setting)
			yc = yd #*14.1/12.45 - 53.23  # y position for relative placement (channel setting) yd - 45 # y position for parameter label and control (channel setting)
			yrow[param.section] += 25

			m_xdef = x_def 
			lx = 0
			if param.name == "OutputFiles":
				yrow[param.section] = 20
				yd = yrow[param.section]
				yrow[param.section] += 25
				lx = 310
			if param.name == "DataFilePath":
				m_xdef = 420
				lx = 310
			if "OF_" in param.name:
				m_xdef = 480
				lx = 310

			if param.type == '-':  # separator or labels
				if param.name.find('_BLANK') < 0:	
					self.par_def_label[param.name] = ttk.Label(tab, text=param.name.ljust(31, ' '), font=("Courier", 10, "bold underline"))
					self.par_def_label[param.name].place(relx=lx, rely=float(yd)/sh.Win_Tabs_H)  # x=0, y=yd) #  
			else:
				ypos[param.name] = yd
				if param.name in self.param_rename: parname = self.param_rename[param.name]
				else: parname = param.name
				self.par_def_label[param.name] = ttk.Label(tab, text=parname)
				self.par_def_label[param.name].place(relx=lx, rely=float(yd)/sh.Win_Tabs_H)  #   x=0, y=yd) #
				# ------------------------------------------------------------------------------
				# create variables and controls for Default
				# ------------------------------------------------------------------------------
				if  param.type != 'm':
					self.par_def_svar[param.name] = StringVar()
					self.par_def_svar[param.name].set(param.default)
					self.par_def_svar[param.name].trace('w', lambda name, index, mode, param=param: self.update_def_param(param))
					#if param.distr != 'b':  # board params don't have controls for default value 
					if TRUE:  # board params don't have controls for default value 
						if param.type == 'c':  # Combobox
							self.par_def_combo[param.name] = ttk.Combobox(tab, textvariable=self.par_def_svar[param.name], state='readonly') #, width=15)
							# self.par_def_combo[param.name].append(ttk.Combobox(tab, textvariable=self.par_def_svar[param.name], state='readonly', width=15))
							self.par_def_combo[param.name]['values'] = param.options
							self.par_def_combo[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H, relwidth=0.163, relheight=0.037)  #   x=x_def, y=yd)
								# x_def)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H, relwidth=0.163, relheight=0.039)  #   x=x_def, y=yd)
							tt.Tooltip(self.par_def_combo[param.name], text=param.descr, wraplength=200)
						elif param.type == 'b':	# boolean
							self.par_def_checkbox[param.name] = Checkbutton(tab, variable=self.par_def_svar[param.name])
							self.par_def_checkbox[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H)  #   x=x_def, y=yd)
							tt.Tooltip(self.par_def_checkbox[param.name], text=param.descr, wraplength=200)
						else:  # entry (string)
							self.par_def_entry[param.name] = Entry(tab, textvariable=self.par_def_svar[param.name]) #, width=18)  
							self.par_def_entry[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H, relwidth=0.163, relheight=0.036)  #   x=x_def, y=yd)
							tt.Tooltip(self.par_def_entry[param.name], text=param.descr, wraplength=200)

				# ------------------------------------------------------------------------------
				# create controls for board params
				# ------------------------------------------------------------------------------
				if (param.distr == 'b'):
					chn = 1
					self.par_brd_svar[param.name] = []
					self.par_brd_combo[param.name] = []
					self.par_brd_checkbox[param.name] = []
					self.par_brd_entry[param.name] = []
					self.ypos_brd[param.name] = yb		# y pos of the widget
					# yb = (yb)*(sh.Win_Tabs_H+60)/(sh.Win_Tabs_W-12)
					# height = sh.Win_Tabs_H
					for brd in sh.Boards:
						btab = self.BrdTabs[param.section][brd]
						if self.tabmode[param.section] == 2:
							chn = 8
						
						self.par_brd_svar[param.name].append(StringVar())
						self.par_brd_svar[param.name][brd].set(param.value[brd])
						self.par_brd_svar[param.name][brd].trace('w', lambda name, index, mode, param=param, brd=brd: self.update_brd_param(param, brd))
						for tch in range(chn):
							if chn == 8:
								btab = self.ChTabs[param.section][brd][tch] 

							if param.type == 'c':  # Combobox    # before was par_def_....
								if tch == 0: self.par_brd_combo[param.name].append([])
								self.par_brd_combo[param.name][brd].append(ttk.Combobox(btab, textvariable=self.par_brd_svar[param.name][brd], state='readonly')) # , width=15
								self.par_brd_combo[param.name][brd][tch]['values'] = param.options
								self.par_brd_combo[param.name][brd][tch].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Tabs_H, relwidth=0.2, relheight=0.039) #x=x_brd, y=yb)#)  #   			
							elif param.type == 'b':	# Boolean
								if tch == 0: self.par_brd_checkbox[param.name].append([])
								self.par_brd_checkbox[param.name][brd].append(Checkbutton(btab, variable=self.par_brd_svar[param.name][brd]))
								self.par_brd_checkbox[param.name][brd][tch].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Tabs_H)  #x=x_brd, y=yb)#
							else:  # entry (string)
								if tch == 0: self.par_brd_entry[param.name].append([])
								self.par_brd_entry[param.name][brd].append(Entry(btab, textvariable=self.par_brd_svar[param.name][brd])) # , width=18
								self.par_brd_entry[param.name][brd][tch].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Tabs_H, relwidth=0.27, relheight=0.041) #x=x_brd, y=yb)# relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/height)#, relwidth=0.27, relheight=0.037)  #  


				# ------------------------------------------------------------------------------
				# create variables and controls for channel settings
				# ------------------------------------------------------------------------------
				if (param.distr == 'c'): 
					self.par_ch_svar[param.name] = []
					self.par_ch_entry[param.name] = []
					self.par_ch_checkbox[param.name] = []
					self.par_ch_combo[param.name] = []
					self.par_ch_label[param.name] = []
					self.ypos_ch[param.name] = yc			# y pos of the widget
					for brd in sh.Boards: 
						self.par_ch_svar[param.name].append([])
						for gr in range(ngr):
							for i in range(nch_gr):
								ch = gr * nch_gr + i
								# x = 2 + i * 81.5  #50
								x = x_brd + i * 81.5
								ctab = self.ChTabs[param.section][brd][gr]
								if param.type == 'm':
									if ch == 0: self.par_ch_label[param.name].append([])
									self.par_ch_label[param.name][brd].append(Label(ctab, text='', anchor = 'w', relief = 'groove'))#,  width=6
									self.par_ch_label[param.name][brd][ch].place(relx=float(x)/sh.Win_Tabs_W, rely=float(yc)/sh.Win_Tabs_H, relwidth=0.11, relheight=0.041) # x=x, y=yc)#
								else:
									self.par_ch_svar[param.name][brd].append(StringVar())
									self.par_ch_svar[param.name][brd][ch].set(param.value[brd][ch])
									self.par_ch_svar[param.name][brd][ch].trace('w', lambda name, index, mode, param=param, brd=brd, ch=ch: self.update_ch_param(param, brd, ch))
									if param.type == 'c':
										if ch == 0: self.par_ch_combo[param.name].append([])
										self.par_ch_combo[param.name].append(ttk.Combobox(ctab, textvariable=self.par_ch_svar[param.name][brd][ch], state = 'readonly')) #, width=6
										opt = param.options + ['']
										self.par_ch_combo[param.name][brd][ch]['values'] = opt
										self.par_ch_combo[param.name][brd][ch].place(relx=float(x)/sh.Win_Tabs_W, rely=float(yc)/sh.Win_Tabs_H, relwidth=0.11, relheight=0.041)# x=x, y=yc)# 
									elif param.type == 'b':	
										if ch == 0: self.par_ch_checkbox[param.name].append([])
										self.par_ch_checkbox[param.name][brd].append(Checkbutton(ctab, variable=self.par_ch_svar[param.name][brd][ch]))
										self.par_ch_checkbox[param.name][brd][ch].place(relx=float(x)/sh.Win_Tabs_W, rely=float(yc)/sh.Win_Tabs_H) # x=x, y=yc)#
									else:
										if ch == 0: self.par_ch_entry[param.name].append([])
										self.par_ch_entry[param.name][brd].append(Entry(ctab, textvariable=self.par_ch_svar[param.name][brd][ch])) #, width=7
										self.par_ch_entry[param.name][brd][ch].place(relx=float(x)/sh.Win_Tabs_W, rely=float(yc)/sh.Win_Tabs_H, relwidth=0.11, relheight=0.041)#x=x, y=yc)#) # 
								Label(ctab, text='CH ' + str(ch)).place(relx=x/sh.Win_Tabs_W, rely=(5.*14.1/12.45)/sh.Win_Tabs_H) # relx=float(x+5)/sh.Win_Tabs_W, rely=5./sh.Win_Tabs_H) #
			

		for mode in params["AcquisitionMode"].options: # Set dictionary for GUI view
			self.AcqMode_Dict[mode] = str(params["AcquisitionMode"].options.index(mode))
		
		self.ypos_def = ypos

		for k, v in self.Mtabs.items():
			self.Mtabs_shown[k] = v

		# ***************************************************************************************
		# Add extra controls in some tabs
		# ***************************************************************************************
		# ------------------------------------------------------------
		# Connect
		# ------------------------------------------------------------
		self.BrdEnable = [IntVar() for i in sh.Boards]
		self.path_entry = []
		self.info_pid = []
		self.info_fpga_fwrev = []
		self.fpga_fwver = ["" for i in sh.Boards]
		self.info_board_model = []
		self.info_uc_fwrev = []
		self.brd_enable_cb = []
		# xl0, xl1, xl2, xl3 = 60, 220, 300, 450
		# xl = [60, 220, 301, 451]
		xl = [60, 220, 285, 354, 486]
		for i in range(len(xl)-1):
			xl[i+1] = xl[i+1] + 50
		xr=[xx/sh.Win_Tabs_W for xx in xl]	# for relative placement
		y0 = 5
		Label(self.Mtabs['Connect'], text='PATH').place(relx=xr[0], rely=y0/sh.Win_Tabs_H) # place(x=xl[0], y=y0)
		Label(self.Mtabs['Connect'], text='PID').place(relx=xr[1], rely=y0/sh.Win_Tabs_H) #   x=xl[1], y=y0)
		Label(self.Mtabs['Connect'], text='Brd Model').place(relx=xr[2], rely=y0/sh.Win_Tabs_H)
		Label(self.Mtabs['Connect'], text='FPGA FW Rev').place(relx=xr[3], rely=y0/sh.Win_Tabs_H) #x=xl[2], y=y0)
		Label(self.Mtabs['Connect'], text='uC FW Rev').place(relx=xr[4], rely=y0/sh.Win_Tabs_H) #x=xl[3], y=y0)

		x0 = 5
		y0 += 25
		ys = 27
		for b in sh.Boards:
			yb = y0 + b*ys
			if (self.conn_path[b].get() == ""):	self.BrdEnable[b].set(0)
			else: self.BrdEnable[b].set(1)
			self.brd_enable_cb.append(Checkbutton(self.Mtabs['Connect'], variable = self.BrdEnable[b], state=NORMAL))
			self.brd_enable_cb[b].place(relx=x0/sh.Win_Tabs_W, rely=(yb-3)/sh.Win_Tabs_H) #  x = x0, y = yb-3)
			Label(self.Mtabs['Connect'], text='%2s' % (str(b))).place(relx=35./sh.Win_Tabs_W, rely=yb/sh.Win_Tabs_H) #  x=35, y=yb)
			self.path_entry.append(Entry(self.Mtabs['Connect'], textvariable=self.conn_path[b]))  # , width=20+10
			self.path_entry[b].place(relx=xr[0], rely=yb/sh.Win_Tabs_H, relwidth=184/sh.Win_Tabs_W, relheight=0.039)  #  x = xl[0], y = yb)
			self.conn_path[b].trace('w', lambda name, index, mode, brd=b: self.update_conn_path(brd))
			self.BrdEnable[b].trace('w', lambda name, index, mode, brd=b: self.enable_conn_path(brd))
			if b > 0 and not self.BrdEnable[b].get(): 
				self.brd_enable_cb[b].config(state=DISABLED)
			
			self.info_pid.append(Label(self.Mtabs['Connect'], text="", anchor="w", relief = 'groove')) # , width = 10
			self.info_pid[b].place(relx=xr[1], rely=yb/sh.Win_Tabs_H, relwidth=60/sh.Win_Tabs_W, relheight=0.039) #  x=xl[1], y=yb)
			self.info_board_model.append(Label(self.Mtabs['Connect'], text="", anchor="w", relief = 'groove'))
			self.info_board_model[b].place(relx=xr[2], rely=yb/sh.Win_Tabs_H, relwidth=65/sh.Win_Tabs_W, relheight=0.039)
			self.info_fpga_fwrev.append(Label(self.Mtabs['Connect'], text="", anchor="w", relief = 'groove')) # , width = 20
			self.info_fpga_fwrev[b].place(relx=xr[3], rely=yb/sh.Win_Tabs_H, relwidth=126/sh.Win_Tabs_W, relheight=0.039) #x=xl[2], y=yb)
			self.info_uc_fwrev.append(Label(self.Mtabs['Connect'], text="", anchor="w", relief = 'groove')) # , width = 20
			self.info_uc_fwrev[b].place(relx=xr[4], rely=yb/sh.Win_Tabs_H, relwidth=136/sh.Win_Tabs_W, relheight=0.039) #)

		# ------------------------------------------------------------
		# Log
		# ------------------------------------------------------------
		x0 = 5
		self.Output = Text(self.Mtabs["Log"]) # , width=83, height=32
		self.Output.tag_configure('error', foreground='#FF0000')
		self.Output.tag_configure('warning', foreground='#FF8800')
		self.Output.tag_configure('normal', foreground='#000000')
		self.Output.tag_configure('empty', foreground='#CCBB00')
		self.Output.tag_configure('verbose', foreground='Violet')
		self.Output.place(relx=x0/sh.Win_Tabs_W, rely=5/sh.Win_Tabs_H, relwidth=0.984, relheight=0.975) #  x=x0, y=5)

		# ------------------------------------------------------------
		# Run Ctrl
		# ------------------------------------------------------------
		browse_button_outdir = Button(self.Mtabs["RunCtrl"], text='Browse', command=self.BrowseOutDir) # , width=14, height=1
		browse_button_outdir.place(relx=650/sh.Win_Tabs_W, rely=(ypos['DataFilePath']-3)/sh.Win_Tabs_H, relwidth=109/sh.Win_Tabs_W, relheight=0.049)  #  x=270, y=ypos['DataFilePath']-3)

		# Reset Jobs
		#last_col_idx = list(params.keys()).index("OutputFiles") - 2
		last_col_name = 'EnableJobs' #list(params.keys())[last_col_idx]
		self.reset_job = Button(self.Mtabs["RunCtrl"], text="Reset Job", command=lambda:comm.SendCmd('j'))
		self.reset_job.place(relx=140/sh.Win_Tabs_W, rely=(ypos[last_col_name]+50)/sh.Win_Tabs_H, relwidth=109/sh.Win_Tabs_W, relheight=0.049)
		self.button_names[last_col_name] = [self.reset_job, 170/sh.Win_Tabs_W, (ypos[last_col_name]+80)/sh.Win_Tabs_H, 80/sh.Win_Tabs_W, 0.049]
		tt.Tooltip(self.reset_job, text="Reset the Job. Active when jobs are enabled", wraplength=200)

		# ------------------------------------------------------------
		# Statistics
		# ------------------------------------------------------------
		maxbb = 0
		for i in range(sh.MaxBrd):
			if params['Open'].value[i] != "": maxbb += 1 
		self.ChCounts = []
		self.ChLabel = []
		y0 = 10
		ts = 85	
		self.StatsTypeLabel = Label(self.Mtabs["Statistics"], text = "", anchor="c", width = 18)
		self.StatsTypeLabel.place(relx=250/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) #  x = 250, y = y0)
		ttk.Checkbutton(self.Mtabs["Statistics"], text="All Boards Statistics", variable=self.change_statistics).place(relx=10/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x=10, y=y0)
		ttk.Checkbutton(self.Mtabs["Statistics"], text="Integral", variable=self.change_stat_integral).place(relx=140/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) 
		self.slabel = Label(self.Mtabs["Statistics"], text = "Brd", font=("Arial", 12))
		self.slabel.place(relx=(sh.Win_Tabs_W-100)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #   x =  sh.Win_Tabs_W - 100, y = y0)
		self.sbox = Spinbox(self.Mtabs["Statistics"], textvariable=self.ActiveBrd, from_=0, to=sh.MaxBrd-1, font=("Arial", 14), width=3)
		self.sbox.place(relx=(sh.Win_Tabs_W-65)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #  x = sh.Win_Tabs_W - 65, y = y0)
		y0 = y0 + 35
		for i in range(64):
			self.ChCounts.append(Label(self.Mtabs["Statistics"], width = 8, bg = 'White', font=("courier", 9), relief="groove"))
			self.ChCounts[i].place(relx=(21+ts*(i%8))/sh.Win_Tabs_W, rely=(y0+30*(int)(i/8))/sh.Win_Tabs_H)  #  x = 20 + ts * (i % 8), y = y0 + 30 * (int)(i / 8))
			self.ChLabel.append(Label(self.Mtabs["Statistics"], text=str(i), anchor="e", font=("courier", 9), width = 2))
			self.ChLabel[i].place(relx=(ts*(i%8))/sh.Win_Tabs_W, rely=(y0+30*(int)(i/8))/sh.Win_Tabs_H)  #  x = ts * (i % 8), y = y0 + 30 * (int)(i / 8))

		# global statistics (will be dynamically created by the DAQ)
		self.Gstats_y0 = 300  # position
		self.GStatsLabel = []  # label
		self.GStats = []  # value

		# alternative all boards statistics
		new_leg = ["Brd", "TStamp(s)", "Trigger-ID", "TrgRate(KHz)", "LostTrg(%)", "EvtBuild(%)", "DtRate(MB/s)"] #  # DNIN: checking for width
		for i in range(len(new_leg)):
			self.AllBrdLabel.append(Label(self.Mtabs["Statistics"], text=new_leg[i], anchor = "c", justify=CENTER, bg="light gray", font=("courier", 9))) # width=12, 
		for i in sh.Boards:
			self.AllBrdCounts[str(i)] = [] 
			for j in range(len(new_leg)): 
				self.AllBrdCounts[str(i)].append(Label(self.Mtabs["Statistics"], width = 12, bg = 'White', font=("courier", 9), relief="groove"))

		# ------------------------------------------------------------
		# AcqMode
		# ------------------------------------------------------------
		# self.maskch = Button(self.Mtabs["AcqMode"], text='CHANNEL MASK', command=lambda:self.OpenMask("CHANNEL MASK", "AcqMode", "ChEnableMask"), width=14, height=2)
		# self.maskch.place(relx=380/sh.Win_Tabs_W, rely=(ypos['ChEnableMask0'])/sh.Win_Tabs_H, relwidth=109/sh.Win_Tabs_W, relheight=0.08) # x = 380, y = ypos['ChEnableMask0'])
		maskch = [Button(self.BrdTabs["AcqMode"][i], text='CHANNEL MASK', command=lambda:self.OpenMask("CHANNEL MASK", "AcqMode", "ChEnableMask"), width=14, height=2) for i in range(sh.MaxBrd)]
		# maskch = []
		# for i in range(sh.NumBrd):
		# 	gianni = Button(self.BrdTabs["AcqMode"][i], text='CHANNEL MASK', command=lambda:self.OpenMask("CHANNEL MASK", "AcqMode", "ChEnableMask"), width=14, height=2)
		# 	maskch.add(gianni)
		
		# ------------------------------------------------------------
		# Discr
		# ------------------------------------------------------------
		maskqd = [Button(self.ChTabs["Discr"][i][j], text='Q-DISCR MASK', command=lambda:self.OpenMask("Q-DISCR MASK", "Discr", "Q_DiscrMask"), width=14, height=2) for i in sh.Boards for j in range(8)]
		# self.maskqd.place(relx=380/sh.Win_Tabs_W, rely=ypos['Q_DiscrMask0']/sh.Win_Tabs_H, relwidth=109/sh.Win_Tabs_W, relheight=0.08) # x = 380, y = ypos['Q_DiscrMask0'])
		masktd = [Button(self.ChTabs["Discr"][i][j], text='T-DISCR MASK', command=lambda:self.OpenMask("T-DISCR MASK", "Discr", "Tlogic_Mask"), width=14, height=2) for i in sh.Boards for j in range(8)]
		# self.masktd.place(relx=380/sh.Win_Tabs_W, rely=ypos['Tlogic_Mask0']/sh.Win_Tabs_H, relwidth=109/sh.Win_Tabs_W, relheight=0.08) # x = 380, y = ypos['Tlogic_Mask0'])

		# ------------------------------------------------------------
		# HV
		# ------------------------------------------------------------
		y0 = sh.Win_StatBar_Y
		x0 = 637
		Label(parent, text="HV").place(relx=x0/sh.Win_W, rely=y0/sh.Win_H) #  x=x0, y = y0)
		x0 += 23
		self.hvled = leds.Led(parent, 18)
		self.hvled.rel_place(x0/sh.Win_W, y0/sh.Win_H)  #   place(x0, y0)
		self.hvled.set_color("grey")
		self.hvbrdled = []

		self.hvon = [0 for i in sh.Boards]
		self.hvfail = [0 for i in sh.Boards]
		self.Vmon = [0 for i in sh.Boards]
		self.Imon = [0 for i in sh.Boards]
		self.DTemp = [0 for i in sh.Boards]
		self.HVTemp = [0 for i in sh.Boards]
		self.BTemp = [0 for i in sh.Boards]
		self.FPGATemp = [0 for i in sh.Boards]
		self.HVcb_status = [IntVar() for i in sh.Boards]
		self.HVcb = []
		self.HVupd = []
		for brd in sh.Boards:
			ym = 250
			y0 = ym
			x0 = 15
			xs = 130 # 80
			btab = self.BrdTabs["HV_bias"][brd]
			Label(btab, text='Vmon').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  #  x=x0, y=y0)
			self.Vmon[brd] = Label(btab, relief='groove') # , width = 14
			self.Vmon[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)
			y0 += 25
			Label(btab, text='Imon').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  # x=x0, y=y0)
			self.Imon[brd] = Label(btab, width = 14, relief='groove')
			self.Imon[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)x = x0 + xs, y = y0)
			y0 += 25
			Label(btab, text='Det Temp').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  # x=x0, y=y0)
			self.DTemp[brd] = Label(btab, width = 14, relief='groove')
			self.DTemp[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)x = x0 + xs, y = y0)
			y0 += 25
			Label(btab, text='HV Temp').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  # x=x0, y=y0)
			self.HVTemp[brd] = Label(btab, width = 14, relief='groove')
			self.HVTemp[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)x = x0 + xs, y = y0)
			y0 += 25
			Label(btab, text='FPGA Temp').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  # x=x0, y=y0)
			self.FPGATemp[brd] = Label(btab, width = 14, relief='groove')
			self.FPGATemp[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)x = x0 + xs, y = y0)
			y0 += 25
			Label(btab, text='Brd Temp').place(relx=x0/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H)  # x=x0, y=y0)
			self.BTemp[brd] = Label(btab, width = 14, relief='groove')
			self.BTemp[brd].place(relx=(x0+xs)/sh.Win_Tabs_W, rely=(y0*14.1/13.1)/sh.Win_Tabs_H, relwidth=169/sh.Win_Tabs_W, relheight=0.044)  #   x = x0 + xs, y = y0)x = x0 + xs, y = y0)
			y0 += 25
			#self.HVupd.append(Button(btab, text='Update\nHV Monitor', command=lambda brd = brd : comm.SendCmd('h ' + str(brd)), width=12, height = 4))
			#self.HVupd[brd].place(x = x0 + 180, y=ym)
			#xs = 80
			Label(btab, text='HV-ON').place(relx=(x0+490)/sh.Win_Tabs_W, rely=(ym*14.1/13.1)/sh.Win_Tabs_H)  #  x=x0 + 300, y=ym)
			self.HVcb.append(Checkbutton(btab, image=self.img_hvon, command=lambda brd = brd : self.HVonoff(brd), variable = self.HVcb_status[brd], indicatoron=0, height=30, width=30, relief = 'sunken', offrelief = 'groove'))
			self.HVcb[brd].place(relx=(x0+497)/sh.Win_Tabs_W, rely=((ym+20)*14.1/13.1)/sh.Win_Tabs_H, relwidth=0.095, relheight=0.082)  #   x = x0 + 303, y = ym + 20)

			# HV leds for each board
			hvbrdledtxt = 'B' + str(brd) 
			y_hvled = 420
			x0=10
			Label(self.Mtabs['HV_bias'], text = 'HV').place(relx=(x0-2)/sh.Win_Tabs_W, rely=(y_hvled-20)/sh.Win_Tabs_H)  #  x = x0, y = y_hvled-20)
			if brd < 10:
				Label(self.Mtabs['HV_bias'], text=hvbrdledtxt, font=('Arial',7)).place(relx=(x0+(brd-int(brd/8)*sh.NumBrd/2)*3*x0)/sh.Win_Tabs_W, rely=((y_hvled+int(brd/8)*40+5))/sh.Win_Tabs_H)  #   x = x0 + (brd-int(brd/8)*sh.NumBrd/2)*3*x0, y = y_hvled+int(brd/8)*40+5)
			else:
				Label(self.Mtabs['HV_bias'], text=hvbrdledtxt, font=('Arial',7)).place(relx=(10+(brd-int(brd/8)*sh.NumBrd/2)*3*x0-1)/sh.Win_Tabs_W, rely=(y_hvled+int(brd/8)*40+5)/sh.Win_Tabs_H)  #  x = 10 + (brd-int(brd/8)*sh.NumBrd/2)*3*x0-1, y = y_hvled+int(brd/8)*40+5)					
			self.hvbrdled.append(leds.Led(self.Mtabs['HV_bias'], 18))
			self.hvbrdled[brd].rel_place((10+(brd-int(brd/8)*sh.NumBrd/2)*3*x0)/sh.Win_Tabs_W, (y_hvled+20+int(brd/8)*40)/sh.Win_Tabs_H)  #  x = 10 + (brd-int(brd/8)*sh.NumBrd/2)*3*x0, y = y_hvled+20+int(brd/8)*40)
			self.hvbrdled[brd].set_color("grey")
		self.TabsUpdateStatus(sh.ACQSTATUS_DISCONNECTED)
		#self.DisableOnOffUpdateCnt = 0

		# ------------------------------------------------------------
		# Regs
		# ------------------------------------------------------------
		x0 = 10 
		y0 = 20
		self.reg_base = StringVar()
		self.reg_ch   = StringVar()
		self.reg_offs = StringVar()
		self.reg_addr = StringVar()
		self.reg_data = StringVar()
		self.cmd      = StringVar()

		Radiobutton(self.Mtabs["Regs"], text = 'COMM', variable = self.reg_base, value = '01').place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) #  x = x0, y = y0)
		Radiobutton(self.Mtabs["Regs"], text = 'INDIV', variable = self.reg_base, value = '02').place(relx=(x0+80)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x = x0+80, y = y0)
		Radiobutton(self.Mtabs["Regs"], text = 'BCAST', variable = self.reg_base, value = '03').place(relx=(x0+160)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x = x0+160, y = y0)
		self.reg_base.set('01')
		self.reg_base.trace('w', lambda name, index, mode: self.set_reg_addr())
		self.reg_ch.set(0)
		self.reg_ch.trace('w', lambda name, index, mode: self.set_reg_addr())
		vcmdch = (parent.register(self.validate_ch), '%P')
		Label(self.Mtabs["Regs"], text = "Ch", font=("Arial", 12)).place(relx=(x0+243)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) #  x =  x0 + 243, y = y0)
		Spinbox(self.Mtabs["Regs"], textvariable=self.reg_ch, from_=0, to=63, font=("Arial", 14), validate='key', validatecommand=vcmdch, width=3).place(relx=(x0+270)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x = x0 + 270, y = y0)
		Label(self.Mtabs["Regs"], text = "Brd", font=("Arial", 12)).place(relx=(x0+349)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x = x0 + 349, y = y0)
		Spinbox(self.Mtabs["Regs"], textvariable=self.ActiveBrd, from_=0, to=sh.MaxBrd-1, font=("Arial", 14), state='readonly', width=3).place(relx=(x0+380)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H) # x = x0 + 380, y = y0)
	
		y0 = y0 + 30
		self.reg_offs.set('0000')
		self.reg_offs.trace('w', lambda name, index, mode: self.set_reg_addr())
		Label(self.Mtabs["Regs"], text="Offset").place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #  x = x0, y = y0), width=12
		Entry(self.Mtabs["Regs"], textvariable=self.reg_offs).place(relx=(x0+60)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=76/sh.Win_Tabs_W, relheight=0.035)  #  x = x0+60, y = y0)
		y0 = y0 + 20
		self.reg_addr.set("01000000")
		Label(self.Mtabs["Regs"], text="Address").place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  # x = x0, y = y0)
		Entry(self.Mtabs["Regs"], textvariable=self.reg_addr, width=12).place(relx=(x0+60)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=76/sh.Win_Tabs_W, relheight=0.035)  #  x = x0+60, y = y0)
		y0 = y0 + 20
		self.reg_data.set('00000000')
		Label(self.Mtabs["Regs"], text="Data").place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #   x = x0, y = y0), width=12
		Entry(self.Mtabs["Regs"], textvariable=self.reg_data).place(relx=(x0+60)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=76/sh.Win_Tabs_W, relheight=0.035)  #  x = x0+60, y = y0)
		y0 = y0 + 20
		self.cmd.set('14')
		Label(self.Mtabs["Regs"], text="Cmd").place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #   x = x0, y = y0), width=12
		Entry(self.Mtabs["Regs"], textvariable=self.cmd).place(relx=(x0+60)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=76/sh.Win_Tabs_W, relheight=0.035)  #  x = x0+60, y = y0)
		y0 = y0 + 30
		Button(self.Mtabs["Regs"], text='Read',	command=self.read_reg, width= 12).place(relx=x0/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=93/sh.Win_Tabs_W, relheight=0.05)  #   x=x0, y=y0)
		Button(self.Mtabs["Regs"], text='Write',command=self.write_reg, width= 12).place(relx=(x0+100)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=93/sh.Win_Tabs_W, relheight=0.05)  #  x=x0+100, y=y0)
		Button(self.Mtabs["Regs"], text='Send Cmd',command=self.send_cmd, width= 12).place(relx=(x0+200)/sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H, relwidth=93/sh.Win_Tabs_W, relheight=0.05)  #  x=x0+200, y=y0)

		self.RWregLog = Text(self.Mtabs["Regs"]) # , width=83, height=20
		self.RWregLog.place(relx=5/sh.Win_Tabs_W, rely=190/sh.Win_Tabs_H, relwidth=0.984, relheight=0.61)  #  x=5, y=190)

		# *******************************************************************************************
		# Save Additional Widget button - ypos is not used anymore
		self.button_names.update({"DataFilePath" : [browse_button_outdir, 595/sh.Win_Tabs_W, (ypos['DataFilePath']-3)/sh.Win_Tabs_H, 80/sh.Win_Tabs_W, 0.049], 
							"ChEnableMask0" : [maskch, 200/sh.Win_Tabs_W, 2, 190/sh.Win_Tabs_W, 0.08], 
							"Q_DiscrMask0" : [maskqd, 200/sh.Win_Tabs_W, 2, 190/sh.Win_Tabs_W, 0.08], 
							"Tlogic_Mask0" : [masktd, 200/sh.Win_Tabs_W, 2, 190/sh.Win_Tabs_W, 0.08]})

		# **************************************************************************
		# Load the options for parameters affected by the GUI hide parameters file
		# **************************************************************************
		with open(sh.GUIParamOptions, "r") as f:
			self.new_options = json.load(f)

		self.StopUpdate = False



	# ***************************************************************************************
	# Update params
	# ***************************************************************************************
	def set_output_log(self, text, option='normal'):
		self.Output['state'] = NORMAL
		self.Output.insert(END, text, option)
		self.Output.yview_scroll(100, UNITS)
		self.Output['state'] = DISABLED


	# ***************************************************************************************
	# Update params
	# ***************************************************************************************
	def scale_ped_zs(self, param):	# scale the ZS and pedestal value according to the Ebin used
		shiftfact = {
					'8K': 1,
					'4K': 2,
					'2K': 4,
					'1K': 8,
					'512': 16,
					'256': 32
				}
		rf_HV = re.compile("^\d*\.?\d{0,10}$")	# float positive
		val_to_scale = ['Pedestal', 'ZS_Threshold_LG', 'ZS_Threshold_HG']
		# compact the code
		try:	# remove the .0's
			ff = float(shiftfact[param.default]/shiftfact[self.par_def_svar[param.name].get()])
		except:
			ff = 1

		to_scale = [names for names in val_to_scale if rf_HV.search(self.par_def_svar[names].get())]
		tvalt = [str(float(self.par_def_svar[names].get())*ff) for names in to_scale]
		tval = [tv.split('.')[0] if tv.split(".")[1] == '0' else tv for tv in tvalt]
		[self.par_def_svar[to_scale[i]].set(tval[i]) for i in range(len(to_scale))] 
		
		list_not_zero = {}
		for names in val_to_scale[1:]:
			list_not_zero[names] = [(i,j) for i in range(sh.MaxBrd) for j in range(sh.MaxCh) 
					if self.par_ch_svar[names][i][j].get() != "" and rf_HV.search(self.par_ch_svar[names][i][j].get())]
			isfloat = [str(float(self.par_ch_svar[names][a[0]][a[1]].get())*ff) for a in list_not_zero[names]]
			tval = [isf.split('.')[0] if isf.split(".")[1] == '0' else isf for isf in isfloat]
			[self.par_ch_svar[names][list_not_zero[names][i][0]][list_not_zero[names][i][1]].set(tval[i]) for i in range(len(list_not_zero[names]))]

	def isChanged(self, param, myval):
		if myval != param.default:
			return True
		else:
			return False
	
	def exadec_val(self, param):
		r = re.compile("[a-f0-9]", re.IGNORECASE)
		self.par_def_svar[param.name].set("0x" + (''.join(r.findall(self.par_def_svar[param.name].get()[2:10]))).upper())
		# if len(self.par_def_svar[param.name].get()) == 2: self.par_def_svar[param.name].set("0x0")
		return self.isChanged(param, self.par_def_svar[param.name].get())


	def set_units(self, prev_unit, upd_unit):	
		prev_unit = str(prev_unit)
		units = {}
		# if prev_unit == "m":
		# 	return prev_unit

		unit = ["second", "volt", "ampere"]
		units[unit[0]] = ["s", "ms", "us", "ns"]
		units[unit[1]] = ["V", "mV", "uV", "nV"]
		units[unit[2]] = ["A", "mA", "uA", "nA"]
		
		for u in unit:
			if prev_unit in units[u]:
				if len(upd_unit)>2:	# 3 letters: ending with the correct unit
					upd_unit = upd_unit[0] + upd_unit[units[u].index(units[u][0])]
				if upd_unit in units[u]: return upd_unit
				else: return prev_unit
		
		return upd_unit
			
	
	def val_with_unit(self, param):
		if len(self.par_def_svar[param.name].get().split(" ")) > 2:
			self.par_def_svar[param.name].set(param.default)
		if len(self.par_def_svar[param.name].get().split(" ")) > 1 and len(param.type) == 1:
			self.par_def_svar[param.name].set(param.default)

		mynum = self.par_def_svar[param.name].get().split(" ")[0]
		res = self.val_no_unit(param, mynum)	
		if len(param.type) == 2:	# process also the unit
			try:
				myunit = self.par_def_svar[param.name].get().split(" ")[1]	
				prev_unit = param.default.split(" ")[1]
				myunit = self.set_units(prev_unit, myunit)
				valtoset = res + " " + myunit	# param.default.split(" ")[1]
			except:
				valtoset = res + " "
		else: valtoset = res
		self.par_def_svar[param.name].set(valtoset)
		return  self.isChanged(param, valtoset)


	def remove_one_dot(self, param, tmpval, maxval, len_int=5, len_dec=14):
		old_pos = int(param.default.find('.'))	# remove the new dot 
		if tmpval[old_pos] != '.': return self.manage_float_format(param, (tmpval[::-1].replace('.', '', 1))[::-1], maxval, len_int, len_dec)
		else: return self.manage_float_format(param, tmpval.replace('.', '', 1), maxval, len_int, len_dec)


	def manage_float_format(self, param, tmpval, maxval, len_int=5, len_dec=14):	
		# tmp_pos = tmpval.find('.')
		if len(tmpval) < 3: return tmpval 	
		if abs(float(tmpval)) < maxval: return tmpval 	# control that is below the maximum
		else: return param.default.split(" ")[0]
		
		
	def val_no_unit(self, param, tmp_val):
		len_int = 5
		len_dec = 14
		max_val = 9999999999999999999999999
		# the length of the decimal part is defined here. No further control is applied
		# rd = re.compile("^-?\d{0,6}$")	# int pos/neg
		rf = re.compile("^-?\d*\.?\d{0,10}$")	# float pos/neg
		rd_HV = re.compile("^\d{0,10}$")	# int positive
		rf_HV = re.compile("^\d*\.?\d{0,10}$")	# float positive
		
		if param.name == "HV_Vbias": r1 = rf_HV		# HV and DAC step are defined positive
		elif param.name == "HV_IndivAdj": r1 = rd_HV
		#elif param.type == "d": r1 = rd
		else: r1 = rf

		tmp_val = str(tmp_val)
		if r1.match(tmp_val): return self.manage_float_format(param, tmp_val, max_val, len_int, len_dec)
		elif tmp_val.count('.') > 1: return self.remove_one_dot(param, tmp_val, max_val, len_int, len_dec)
		else: return param.default.split(" ")[0]

	def real_update_param(self, param):
		param.default = self.par_def_svar[param.name].get()
		self.UpdateVnom()
		self.CfgChanged.set(1)

	def update_def_param(self, param):
		if self.StopUpdate: return
		if param.name == "PresetTime": self.real_update_param(param)
		# elif param.name == "EHistoNbin": # DNIN may it is not needed since it is implemented similarly in paramparser.c
		# 	self.scale_ped_zs(param)
		# 	self.real_update_param(param)
		elif param.type[0] == 'd' or param.type[0] == 'f': # or param.type == 'u':
			if self.val_with_unit(param): self.real_update_param(param)
		elif param.type == 'h': 
			if self.exadec_val(param): self.real_update_param(param)
		else:
			self.real_update_param(param)

#	def update_def_param(self, param):
#		if self.StopUpdate: return
#		param.default = self.par_def_svar[param.name].get()
#		self.UpdateVnom()
#		self.CfgChanged.set(1)

	######################################################################
	# These controls prevent the GUI to crash
	######################################################################
	def set_Vbias(self, param, brd):	# should be integrated with the regex for default values
		tmpval = self.par_brd_svar[param.name][brd].get()
		rf_HV = re.compile("^\d*\.?\d{0,10}$")	# float positive
		if rf_HV.search(tmpval) or param.name != "HV_Vbias": # Trasparent if the format is correct or it is not HV_Vbias
			param.value[brd] = tmpval
			return 1
		elif tmpval.count('.') > 1: 
			old_pos = int(param.value[brd].find('.'))	# remove the new dot 
			if tmpval[old_pos] != '.': param.value[brd] = tmpval[::-1].replace('.', '', 1)[::-1]
			else: param.value[brd] = tmpval.replace('.', '', 1)
			self.par_brd_svar[param.name][brd].set(param.value[brd])
			return 0
		else: 
			self.par_brd_svar[param.name][brd].set(param.value[brd])
			return 0		

	def set_HVIndAdj(self, param, brd, ch):
		tmpval = self.par_ch_svar[param.name][brd][ch].get()
		rd_HV = re.compile("^\d{0,10}$")	# int positive
		if rd_HV.search(tmpval) or param.name != "HV_IndivAdj": 
			param.value[brd][ch] = tmpval
			return 1
		else:
			self.par_ch_svar[param.name][brd][ch].set(param.value[brd][ch]) 
			return 0

	#######################################################################
	#######################################################################
	#######################################################################

	def update_brd_param(self, param, brd):	# to DO
		if self.StopUpdate: return
		if (param.distr == 'b'):
			if(self.set_Vbias(param, brd)):
				self.UpdateVnom()
				self.CfgChanged.set(1)

	def update_ch_param(self, param, brd, ch):	# to DO
		if self.StopUpdate: return
		if (param.distr == 'c'):
			if(self.set_HVIndAdj(param, brd, ch)):
				self.UpdateVnom()
				self.CfgChanged.set(1)
		# 	param.value[brd][ch] = self.set_HVIndAdj(param, brd, ch)
		# 	# param.value[brd][ch] = self.par_ch_svar[param.name][brd][ch].get()
		# self.UpdateVnom()
		# self.CfgChanged.set(1)

	def enable_brd_cb(self, brd):	
		if brd > sh.MaxBrd-1: pass
		elif brd < 1: 
			self.brd_enable_cb[brd].config(state=NORMAL)
			self.enable_brd_cb(brd+1)
		elif self.conn_path[brd].get() != '' and (self.BrdEnable[brd-1].get() and self.brd_enable_cb[brd-1].cget('state')!=DISABLED):  
			self.brd_enable_cb[brd].config(state=NORMAL)
			self.enable_brd_cb(brd+1)
		else: 
			self.brd_enable_cb[brd].config(state=DISABLED)
			if self.BrdEnable[brd].get() == 1:
				self.BrdEnable[brd].set(0)
			self.enable_brd_cb(brd+1)

	def enable_conn_path(self, brd):
		if self.BrdEnable[brd].get() == 1: 
			params['Open'].value[brd] = self.conn_path[brd].get()
		else: 
			params['Open'].value[brd] = ''
		self.enable_brd_cb(brd)
		self.CfgChanged.set(1)		

	def update_conn_path(self, brd):	# DNIN: Is this reduntant?
		self.enable_brd_cb(brd)
		if self.BrdEnable[brd].get() == 1: 
			params['Open'].value[brd] = self.conn_path[brd].get()
		self.CfgChanged.set(1)

	def update_brd_info(self, infostr):
		bi = infostr.split(';')
		brd = int(bi[0])
		if brd >= 0 and brd < sh.MaxBrd:
			self.info_pid[brd].config(text = bi[1], bg = 'light blue')
			self.info_board_model[brd].config(text = bi[2], bg = 'light blue')
			self.info_fpga_fwrev[brd].config(text = bi[3], bg = 'light blue')
			self.fpga_fwver[brd] = bi[3].split(" ")[0]
			self.info_uc_fwrev[brd].config(text = bi[4], bg = 'light blue')

	def Params2Tabs(self, reloaded):  
		if not reloaded:
			return
		self.StopUpdate = True
		self.UpdateVnom()
		for param in params.values():
			if param.distr == '-' or param.type == 'm': continue
			if param.name == 'Open':
				if self.status_now != sh.ACQSTATUS_DISCONNECTED:
					continue
				for i in range(sh.MaxBrd): #len(param.value)):	
					self.conn_path[i].set(param.value[i])
					# if param.value[i] != '':
					if self.conn_path[i].get() != '' and self.status_now == sh.ACQSTATUS_DISCONNECTED: 
						self.BrdEnable[i].set(1)
					if self.status_now != sh.ACQSTATUS_DISCONNECTED:
						self.brd_enable_cb[i].config(state=DISABLED)
					# else: 
					# 	self.BrdEnable[i].set(0)
				
			else:
				self.par_def_svar[param.name].set(param.default)
				if (param.distr == 'b'):
					for brd in sh.Boards:
						self.par_brd_svar[param.name][brd].set(param.value[brd])
				if (param.distr == 'c'):
					for brd in sh.Boards:
						for ch in sh.Channels:
							self.par_ch_svar[param.name][brd][ch].set(param.value[brd][ch])
		self.StopUpdate = False	
		self.CfgChanged.set(1)				

	def TabsUpdateStatus(self, status):
		self.UpdateVnom()
		for brd in sh.Boards:
			if status == sh.ACQSTATUS_DISCONNECTED:  # disconnected (offline)
				self.HVcb[brd].config(state=DISABLED)
				self.HVcb_status[brd].set(0)
				self.hvbrdled[brd].set_color('grey')
				self.hvled.set_color('grey')
				self.Vmon[brd].config(text='')
				self.Imon[brd].config(text='')
				self.DTemp[brd].config(text='')
				self.BTemp[brd].config(text='')
				self.FPGATemp[brd].config(text='')
				self.HVTemp[brd].config(text='')
				self.info_pid[brd].config(text = "", bg = sh.BgCol)
				self.info_board_model[brd].config(text = "", bg = sh.BgCol)
				self.info_fpga_fwrev[brd].config(text = "", bg = sh.BgCol)
				self.fpga_fwver[brd] = ""
				self.info_uc_fwrev[brd].config(text = "", bg = sh.BgCol)
				self.enable_brd_cb(brd)	# enabling checkbox with some text, if consecutive to another one
				self.path_entry[brd].config(state=NORMAL)

				for i in range(64):
					self.ChCounts[i].config(bg='white')
				for b in range(sh.MaxBrd):
					for i in range(7):
						self.AllBrdCounts[str(b)][i].config(bg='white')
				# enable brd_cb
				# self.brd_enable_cb[brd].config(state=NORMAL)
			elif status == sh.ACQSTATUS_READY: # ready
				#self.HVupd[brd].config(state=NORMAL)
				if not self.offline: self.HVcb[brd].config(state=NORMAL)
				# self.enable_brd_cb(brd)
				self.path_entry[brd].config(state=DISABLED)
				self.brd_enable_cb[brd].config(state=DISABLED)
			elif status == sh.ACQSTATUS_RAMPING_HV:
				self.HVcb[brd].config(state=NORMAL)
				#self.DisableOnOffUpdateCnt = 3
			else: # running
				self.HVcb[brd].config(state=DISABLED)
				self.path_entry[brd].config(state=DISABLED)
				self.brd_enable_cb[brd].config(state=DISABLED)

	def set_reg_addr(self):
		if self.reg_base.get() == '02':
			base = '02'+str(self.reg_ch.get().rjust(2, '0'))
		else:
			base = self.reg_base.get() + '00'
		offs = self.reg_offs.get().rjust(4, '0')
		self.reg_addr.set(base + offs)

	def read_reg(self):
		comm.SendCmd('Rr' + self.reg_addr.get() + '\n')

	def write_reg(self):
		comm.SendCmd('Rw' + self.reg_addr.get() + '\n' + self.reg_data.get() + '\n')
		self.RWregLog.insert(END, "Wr-Reg: A=" + self.reg_addr.get() + " D=" + self.reg_data.get() + '\n')
		self.RWregLog.see(END)

	def send_cmd(self):
		comm.SendCmd('Rw 0x01008000' + self.cmd.get() + '\n')
		self.RWregLog.insert(END, "Send Command " + self.cmd.get() + '\n')
		self.RWregLog.see(END)

	def BrowseOutDir(self):
#		OutDir = filedialog.askdirectory()
		try:
			OutDir = askdirectory()
			OutDir = os.path.relpath(OutDir)
			self.par_def_svar['DataFilePath'].set(OutDir)
		except:
			return
	

	# ***************************************************************************************
	# Update HV Tab	 	
	# ***************************************************************************************
	def UpdateHVTab(self, hvfullstring):	# DNIN: control over the brd index you are trying to turn on
		# Take Num of board connected from connect tab
		# Divide the message in x blocks of that length (8: brd, status, vmon, imon, dtemp, itemp, fpgatemp)
		# From SW 3.5.0 (8: brd, status, vmon, imon, dtemp, itemp, fpgatemp, pcbtemp)
		# Do what is doing below
		if len(hvfullstring) == 0: return

		hvstring = hvfullstring.split("|")	# As many string as the board number 
		num_brd = int(len(hvstring))


		for i in range(num_brd):
			hvs = hvstring[i].split()
			brd = int(hvs[0])

			hv_on = int(hvs[1]) & 1
			self.hvfail[brd] = (int(hvs[1]) >> 1) & 1
			#if self.DisableOnOffUpdateCnt > 0:
			#	self.DisableOnOffUpdateCnt = self.DisableOnOffUpdateCnt - 1
			#else:	
			if hv_on == 1: self.HVcb_status[brd].set(1)
			else: self.HVcb_status[brd].set(0)
			self.Vmon[brd].config(text=hvs[2] + ' V')
			self.Imon[brd].config(text=hvs[3] + ' mA')
			if float(hvs[4]) >= 0 : 
				if float(hvs[4]) > 1: self.DTemp[brd].config(text=hvs[4] + ' degC')
				else: self.DTemp[brd].config(text='N.A.')
			if float(hvs[5]) >= 0 : self.HVTemp[brd].config(text=hvs[5] + ' degC')
			else: self.HVTemp[brd].config(text='N.A')
			if float(hvs[6]) >= 0 : self.FPGATemp[brd].config(text=hvs[6] + ' degC')
			else: self.FPGATemp[brd].config(text='N.A')
			if float(hvs[7]) >= 0 and float(hvs[7]) < 125: self.BTemp[brd].config(text=hvs[7] + ' degC')   # Janus 3.5.0 will send 8 ch anyway
			else: self.BTemp[brd].config(text='N.A.')
			
			vmon = float(hvs[2])
			# if vmon > 7: self.hvon[brd] = 1
			# else: self.hvon[brd] = 0
			# if 1 in self.hvfail: self.hvled.set_color("red")
			# elif 1 in self.hvon: self.hvled.set_color("green")
			# else: self.hvled.set_color("grey")
			# control hvbrdled
			try:
				if params['HV_Vbias'].value[brd] != "":
					vref = float(params['HV_Vbias'].value[brd].split(" ")[0])	
				else:
					vref = float(params['HV_Vbias'].default.split(" ")[0])
			except:
				vref = 20
			if self.hvfail[brd]: self.hvbrdled[brd].set_color("red")
			elif vmon > 0.95 * vref: 
				self.hvbrdled[brd].set_color("green")
				self.hvon[brd] = 1
			elif vmon < 22: 
				self.hvbrdled[brd].set_color("grey")
				self.hvon[brd] = 0
			else: 
				self.hvbrdled[brd].set_color("yellow")
				self.hvon[brd] = 2	# ramping up/down
			# control hvled
		if 1 in self.hvfail: self.hvled.set_color("red")
		elif 2 in self.hvon: self.hvled.set_color("yellow")
		elif 1 in self.hvon: self.hvled.set_color("green")
		else: self.hvled.set_color("grey")

	def HVonoff(self, brd):
		if self.HVcb_status[brd].get() == 1: # HV ON
			comm.SendCmd('H1 ' + str(brd))
		else: # HV OFF
			comm.SendCmd('H0 ' + str(brd))
		#self.DisableOnOffUpdateCnt = 3

	def UpdateVnom(self):
		if params['HV_Adjust_Range'].default == '4.5': 
			dacfs = 4.2
		elif params['HV_Adjust_Range'].default == '2.5': 
			dacfs = 2.5
		else:
			dacfs = 0
		stdunit = "V"	# DNIN: the manage of the unit is not yet implemented
		try:
			if params['HV_Vbias'].default.split()[1] == 'mV': stdunit = "mV"
			elif params['HV_Vbias'].default.split()[1] == 'uV': stdunit = "uV"
			elif params['HV_Vbias'].default.split()[1] == 'nV': stdunit = "nV"
			else: stdunit = "V"
		except:
			stdunit = "V"

		for brd in sh.Boards:
			for i in sh.Channels:
				if params['HV_Adjust_Range'].default == 'DISABLED': vdac = 4.5
				elif params['HV_IndivAdj'].value[brd][i] == '': 
					try:
						vdac = dacfs * float(255 - int(params['HV_IndivAdj'].default)) / 255
					except:
						vdac = 0	# default value
				else:	# control - overwrite if it is correct
					vdac = dacfs * float(255 - int(params['HV_IndivAdj'].value[brd][i])) / 255
				if params['HV_Vbias'].value[brd] == '': 
					try:
						vnom = float(params['HV_Vbias'].default.split()[0]) - vdac 
					except:
						vnom = 20	# default value
				else:	# control - overwrite if the value is correct
					vnom = float(params['HV_Vbias'].value[brd].split()[0]) - vdac
				self.par_ch_label['Vnom'][brd][i].config(text = '%.2f %s' % (vnom, stdunit)) 

	# ***************************************************************************************
	# Update Statistics Tab 
	# ***************************************************************************************
	def UpdateStatsTab(self, cmsg:str): 
		cmsg = cmsg.rstrip()
		if cmsg[0] == '0': return # exit, JanusC closed the comm forcibly 
		if cmsg[1] == 'b':	# write only if the active Brd is the one sending data
			if cmsg[2:] == str(self.ActiveBrd.get()): self.update_stats = True
			else: self.update_stats = False
		if cmsg[1] == 'c':  # channel value
			if (list(self.Mtabs_shown)[self.Mtabs_nb.index('current')] == 'Statistics'):
				for ch in range(64):
					ss = cmsg[8*ch+2:8*ch+10]
					if not self.update_stats: ss = '0.000'
					if ss.strip() == '0' or ss.strip() == '0.000': col = 'white'
					else: col = 'light yellow'
					self.ChCounts[ch].configure(text=ss, bg=col)
		# elif cmsg[1] == 'g': # globa value
		# 	if (list(self.Mtabs)[self.Mtabs_nb.index('current')] == 'Statistics'):
		# 		i = int(cmsg[2])
		# 		tmp_msg = cmsg[3:]
		# 		if not self.update_stats: tmp_msg = cmsg.replace(cmsg[3:],"")
		# 		if i < len(self.GStats):
		# 			self.GStats[i].configure(text = tmp_msg)
		elif cmsg[1] == 'G': # define global stat
			i = int(cmsg[2])
			# if self.update_stats: cmsg[3:] = cmsg[3:].re
			if i == len(self.GStats):
				self.GStats.append(Label(self.Mtabs["Statistics"], width = 20, bg = 'White', anchor="e", font=("courier", 9), relief="groove"))
				self.GStatsLabel.append(Label(self.Mtabs["Statistics"], text = cmsg[3:], anchor="w", width = 10))
				y0 = self.Gstats_y0 + 22 * i
				if not self.change_statistics.get(): self.GStatsLabel[i].place(relx=5./sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #  x = 5, y = y0)
				if not self.change_statistics.get(): self.GStats[i].place(relx=110./sh.Win_Tabs_W, rely=y0/sh.Win_Tabs_H)  #  x = 110, y = y0)
		elif cmsg[1] == 'g':
			if (list(self.Mtabs_shown)[self.Mtabs_nb.index('current')] == 'Statistics'):
				msg_spl = cmsg[2:].split("\t")
				self.StatsTypeLabel.configure(text = msg_spl[0], bg = "white")
				if not self.update_stats:
					for i in range(len(msg_spl)-1):
						self.GStats[i].configure(text = "")
				else:
					for i in range(len(msg_spl)-1):
						self.GStats[i].configure(text = msg_spl[i+1])
		elif cmsg[1] == 't': # channel Statistics title
			self.StatsTypeLabel.configure(text = cmsg[2:], bg = "light yellow")
		elif cmsg[1] == 'B':
			if (list(self.Mtabs_shown)[self.Mtabs_nb.index('current')] == 'Statistics'):
				msg_spl = cmsg[2:].split()
				msglen = len(self.AllBrdLabel)	# to shorten the variable name
				if len(msg_spl)%6 == 0: mlen = 6
				else: mlen = 7
				col = 'light yellow'
				if float(msg_spl[3]) < 1: col = 'white' 
				for b in range(int(len(msg_spl)/mlen)):
					if mlen == 6: msg_spl.insert(5+msglen*b, "-")
					for l in range(msglen): # adding the "-" in case of no tdl the actual len of the mesg read is = len(AllBrdLabel)
						self.AllBrdCounts[msg_spl[msglen*b]][l].config(text = msg_spl[msglen*b+l], bg = col)


	def ChgStatIntegr(self):
		if self.status_now == sh.ACQSTATUS_READY or self.status_now == sh.ACQSTATUS_RUNNING:
			comm.SendCmd("I{}".format(self.change_stat_integral.get()))	
	
	def ChgStatMode(self): 
		if self.status_now == sh.ACQSTATUS_READY or self.status_now == sh.ACQSTATUS_RUNNING:
			comm.SendCmd("\t{}".format(self.change_statistics.get()))	
		if self.change_statistics.get(): # remove the previous statistics
			self.sbox.place_forget()
			self.slabel.place_forget()
			for i in range(len(self.ChCounts)):
				self.ChCounts[i].place_forget()
				self.ChLabel[i].place_forget()
			if len(self.GStatsLabel) > 1:
				for i in range(len(self.GStatsLabel)):
					self.GStatsLabel[i].place_forget()
					self.GStats[i].place_forget()
			# place new statistic
			xnew = 660/len(self.AllBrdLabel)
			xwidth = 0.87 - len(self.AllBrdLabel)/100
			for i in range(len(self.AllBrdLabel)): self.AllBrdLabel[i].place(relx=(15+(xnew)*i)/sh.Win_Tabs_W, rely=45/sh.Win_Tabs_H, relwidth=0.9/(len(self.AllBrdLabel)))  #  x = 20+111*i, y = 45)
			for i in sh.Boards:
				for j in range(len(self.AllBrdLabel)): 
					self.AllBrdCounts[str(i)][j].place(relx=(20+(xnew)*j)/sh.Win_Tabs_W, rely=(75+25*i)/sh.Win_Tabs_H, relwidth=xwidth/(len(self.AllBrdLabel)))  #  x = 20+111*j, y = 75 + 25*i) 
		else: # remove
			for i in range(len(self.AllBrdLabel)):
				self.AllBrdLabel[i].place_forget()
				for j in sh.Boards:
					self.AllBrdCounts[str(j)][i].place_forget()
			# place back the old statistics
			self.sbox.place(relx=(sh.Win_Ctrl_W-65)/sh.Win_Tabs_W, rely=10./sh.Win_Tabs_H)   #   x = sh.Win_Tabs_W - 65, y = 10)
			self.slabel.place(relx=(sh.Win_Tabs_W-100)/sh.Win_Tabs_W, rely=10./sh.Win_Tabs_H)  #   x =  sh.Win_Tabs_W - 100, y = 10)
			for i in range(len(self.ChCounts)):
				self.ChCounts[i].place(relx=(21+85*(i%8))/sh.Win_Tabs_W, rely=(45+30*(int)(i/8))/sh.Win_Tabs_H)   #   x = 20 + 85 * (i % 8), y = 45 + 30 * (int)(i / 8))
				self.ChLabel[i].place(relx=(85*(i%8))/sh.Win_Tabs_W, rely=(45+30*(int)(i/8))/sh.Win_Tabs_H)  #  x = 85 * (i % 8), y = 45 + 30 * (int)(i / 8))
			if len(self.GStatsLabel) > 1:
				for i in range(len(self.GStatsLabel)):	# Those two placements needs to be checked
					self.GStatsLabel[i].place(relx=5/sh.Win_Tabs_W, rely=(300+22*i)/sh.Win_Tabs_H)  #  x = 5, y = 300+22*i)
					self.GStats[i].place(relx=110/sh.Win_Tabs_W, rely=(300+22*i)/sh.Win_Tabs_H)  #   x = 110, y = 300+22*i)
			

	# ***************************************************************************************
	# Enable/Disable combobox write
	# ***************************************************************************************
	def combobox_state(self, en_status):
		if en_status: mystate = "normal"
		else: mystate = 'readonly'
		for param in params.values():
			if param.type == 'c':
				self.par_def_combo[param.name].config(state = mystate)

	# ***************************************************************************************
	# Popup window for Mask Setting
	# ***************************************************************************************
	def OpenMask(self, title, section, param_name):
		if not os.path.isfile("pixel_map.txt"): return
		pm = open("pixel_map.txt", "r")
		self.pixmap = ["" for i in sh.Channels]
		for line in pm:
			p = line.split()
			ch = int(p[0])
			if ch >= 0 and ch < sh.MaxCh: self.pixmap[ch] = p[1]
		pm.close	

		self.en_pixel_map = IntVar()
		self.en_pixel_map.set(0)

		if self.MaskWinIsOpen: self.CloseMaskWin()
		xw = 215
		yw = 295
		self.MaskWin = Toplevel()
		self.MaskWin.geometry("{}x{}+{}+{}".format(xw, yw, 550, 300))
		#self.MaskWin.overrideredirect(1)  # no window bar
		self.MaskWin.wm_title("")
		self.MaskWin.protocol("WM_DELETE_WINDOW", self.CloseMaskWin)
		self.MaskWinIsOpen = True
		Frame(self.MaskWin, width=xw, height=yw, relief=RIDGE).place(x=0, y=0) # , bd=2

		cb = self.BrdTabs_nb[section].index('current')
		self.cbm = []
		self.BrdOpt = ['Global']
		for b in sh.Boards: self.BrdOpt.append(str(b))
		self.Mbrd = StringVar()
		self.Mbrd.set(str(cb))
		# print('Board mask = ' + self.Mbrd.get()) # DNIN: debug porpuse
		#self.Mbrd = self.BrdTabs_nb[section].index('current')
		self.par0 = param_name + "0"
		self.par1 = param_name + "1"

		x0, y0 = 5, 5
		sp = 26
		Label(self.MaskWin, text = title).place(relx=float(x0/xw), rely=float(y0/yw))  #  x = x0, y = y0)
		Label(self.MaskWin, text = "Brd").place(relx=float((x0+110)/xw), rely=float(y0/yw))  #  x = x0 + 110, y = y0)
		ttk.Combobox(self.MaskWin, values=self.BrdOpt, textvariable=self.Mbrd, state='readonly').place(relx=float(x0+138)/xw, rely=y0/yw, relwidth=0.317, relheight=0.075)  # , width=7 x=x0+138, y=y0)
		self.Mbrd.trace('w', lambda name, index, mode: self.GetBrdMask())

		y0 += 25
		Button(self.MaskWin, text='Enable all',  command=self.EnableAll).place(relx=float(x0/xw), rely=float(y0/yw), relwidth=0.475, relheight=0.087) # , height = 1 , width=13 x = x0, y = y0)
		Button(self.MaskWin, text='Disable all', command=self.DisableAll).place(relx=float((x0+sp*4)/xw), rely=float(y0/yw), relwidth=0.475, relheight=0.087) # , height = 1 , width=13 x = x0 + sp*4, y = y0)
		#self.no_update = True  # prevent the update while setting the mask with initial values
		for y in range(8):	
			for x in range(8):
				i = 8*y+x
				# self.Mask[i].trace('w', lambda name, index, mode: self.UpdateMask())
				self.cbm.append(Checkbutton(self.MaskWin, text=str(i), variable=self.Mask[i], indicatoron=0)) # , height = 1, width=2
				self.cbm[i].place(relx=float(x0+x*sp)/xw, rely=float(y0+(y+1)*sp)/yw, relwidth=float(sp-2)/xw, relheight=float(sp-2)/yw)  #DNIN: missing relwidth /height   x = x0+x*sp, y=y0+(y+1)*sp)
		Checkbutton(self.MaskWin, text='Pixel Map', variable=self.en_pixel_map, command=self.PixelMapTab(), indicatoron=0).place(relx=float(x0/xw), rely=float(1+y0+sp*9)/yw, relwidth=0.475, relheight=0.087) # , height=1, width=13 x=x0, y = 1 + y0 + sp*9)
		self.en_pixel_map.trace('w', lambda name, index, mode: self.PixelMapTab())
		Button(self.MaskWin, text='Done', command=self.CloseUpdateMaskWin, bg="light blue").place(relx=float(x0+sp*4)/xw, rely=float(1+y0+sp*9)/yw, relwidth=0.475, relheight=0.087) # , height = 1, width=13 x = x0+103, y = y0 + sp*9)
		#self.no_update = False
		self.GetBrdMask()
	
	def PixelMapTab(self):
		x0 = 5
		y0 = 30
		sp = 26
		for y in range(8):
			for x in range(8):
				i = 8*y+x
				if self.en_pixel_map.get() == 1:
					xp = ord(self.pixmap[i][0]) - ord('A')
					yp = 7 - (ord(self.pixmap[i][1]) - ord('1'))
				else:
					xp = x
					yp = y
				self.cbm[i].place(relx=float(x0+xp*sp)/215, rely=float(y0+(yp+1)*sp)/295)  #  x = x0+xp*sp, y=y0+(yp+1)*sp)

	def GetBrdMask(self):
		self.no_update = True
		if self.Mbrd.get() == 'Global': 
			mask0s = self.par_def_svar[self.par0].get()
			mask1s = self.par_def_svar[self.par1].get()
		else:	
			mask0s = self.par_brd_svar[self.par0][int(self.Mbrd.get())].get()
			mask1s = self.par_brd_svar[self.par1][int(self.Mbrd.get())].get()
		if mask0s == '': mask0s = "0x00000000"
		if mask0s[0:1] == '0x': mask0 = int(mask0s[2:], 16)
		else: mask0 = int(mask0s, 16)
		if mask1s == '': mask1s = "0x00000000"
		if mask1s[0:1] == '0x': mask1 = int(mask1s[2:], 16)
		else: mask1 = int(mask1s, 16)
		for y in range(8):
			for x in range(8):
				i = 8*y+x
				if (i<32): ec = (mask0 >> i) & 1
				else: ec = (mask1 >> (i - 32)) & 1	
				self.Mask[i].set(ec)
		self.no_update = False		

	def UpdateMask(self):
		if self.no_update: return
		mask0, mask1 = 0, 0
		for i in range(0, 64):
			if i < 32: mask0 += (2**i) * self.Mask[i].get()
			else: mask1 += (2**(i-32)) * self.Mask[i].get()
		if self.Mbrd.get() == 'Global': 
			self.par_def_svar[self.par0].set('0x'+hex(mask0)[2:].upper())
			self.par_def_svar[self.par1].set('0x'+hex(mask1)[2:].upper())
		else:	
			self.par_brd_svar[self.par0][int(self.Mbrd.get())].set('0x'+hex(mask0)[2:].upper())
			self.par_brd_svar[self.par1][int(self.Mbrd.get())].set('0x'+hex(mask1)[2:].upper())

	def EnableAll(self):
		self.no_update = True
		for i in range(64):
			self.Mask[i].set(1)
		self.no_update = False

	def DisableAll(self):
		self.no_update = True
		for i in range(64):
			self.Mask[i].set(0)
		self.no_update = False	

	def CloseMaskWin(self):
		# self.UpdateMask()
		self.MaskWin.destroy()
		# self.CfgChanged.set(1)
		self.MaskWinIsOpen = False

	def CloseUpdateMaskWin(self):
		self.UpdateMask()
		self.MaskWin.destroy()
		self.CfgChanged.set(1)
		self.MaskWinIsOpen = False	

	def CloseTab(self, parent):
		self.Mtabs_nb.destroy()


	# ----------------------------------------------------------------
	# BASIC / ADVANCED GUI view mode
	# ----------------------------------------------------------------
	# Change the options of combobox and spinbox from file
	def set_new_options(self, param, jobenabled):	
		if self.new_options[param.name]["type"] == 'job':
			try: new_option = self.new_options[param.name][jobenabled]
			except: new_option = []
		else: new_option = []

		if param.type == "c":
			self.par_def_combo[param.name]['values'] = new_option


	def forget_widget(self, param):
		if params[param.name].type == 'm' or param.name == 'Open': return
		if params[param.name].type == '-':
			if param.name.find("_BLANK") < 0: self.par_def_label[param.name].place_forget()
			return
		self.par_def_label[param.name].place_forget()
		if params[param.name].type == 'c': 
			if params[param.name].distr == 'g': self.par_def_combo[param.name].place_forget()
			elif params[param.name].distr == 'b': [self.par_brd_combo[param.name][brd].place_forget() for brd in range(sh.MaxBrd)]
			elif params[param.name].distr == 'c': [self.par_ch_combo[param.name][brd][ch].place_forget() for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)]
		elif params[param.name].type == 'b': 
			if params[param.name].distr == 'g': self.par_def_checkbox[param.name].place_forget()
			elif params[param.name].distr == 'b': [self.par_brd_checkbox[param.name][brd].place_forget() for brd in range(sh.MaxBrd)]
			elif params[param.name].distr == 'c': [self.par_ch_checkbox[param.name][brd][ch].place_forget() for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)]
		else:
			self.par_def_entry[param.name].place_forget()
			# TABMODE
			nch_t = 1
			if self.tabmode[param.section] == 2: nch_t = 8
			if params[param.name].distr == 'b': [self.par_brd_entry[param.name][brd][tch].place_forget() for brd in range(sh.MaxBrd) for tch in range(nch_t)]
			elif params[param.name].distr == 'c': [self.par_ch_entry[param.name][brd][ch].place_forget() for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)]


	def remove_tabs_widget(self):
		# Remove extra widgets
		for key, item in self.button_names.items():
			try: item[0].place_forget()
			except:
				[item[0][j].place_forget() for j in range(len(item))]
		
		# Remove widgets created with tabs
		for param in params.values(): self.forget_widget(param)
			
		for s in sections:
			self.Mtabs_nb.add(self.Mtabs[s], text=' ' + s + ' ')
			self.Mtabs_nb.forget(self.Mtabs[s])
		

	def load_par2remove(self, hiding_status, sel='h'):
		hideparam = []
		tabs_removal = []
		with open(sh.GuiModeFile, "r") as hidefile:
			hideparam_nu = hidefile.read().splitlines()	# does not work with breakpoint :-/
			for h in range(len(hideparam_nu)):	
				tt = hideparam_nu[h].split("\t")			# removing white space before and after the name
				tt = list(filter(("").__ne__, tt))
				if len(tt) != 2 or tt[0][0] == "#": continue		# removing comments
				if not hiding_status.search(tt[1]): continue 			# check if the hide attribute contains the hiding status
				h_n = tt[0].strip()	# Hide Name Par
				if h_n in self.rename_parname:					# check if the name is a real parameter
					if "Ch Enable Mask Chip" in h_n or "QD Mask Chip" in h_n or "TLogic Mask Chip" in h_n:	# the parameter is splitted for 2 chips, but removed together
						hideparam.append(self.rename_parname[h_n[:-1] + "0"])
						hideparam.append(self.rename_parname[h_n[:-1] + "1"])
					else: 
						hideparam.append(self.rename_parname[h_n]) 				
				elif h_n in self.rename_parname.values():	#	Accept both defs or renamed parameters name
					if "ChEnableMask" in h_n or "QDDiscrMask" in h_n or "Tlogic_Mask" in h_n:
						hideparam.append(h_n[0][:-1] + "0")
						hideparam.append(h_n[:-1] + "1")
					else:
						hideparam.append(h_n)					
				elif tt[0] in sh.sections:	# rem
					tabs_removal.append(h_n)
		if self.offline: tabs_removal.append('Regs')
		if sel=='h': return hideparam
		else: return tabs_removal


	def place_widgets(self, param, hideparam, yrow, x_def, x_brd):
		ypos = {}
		if param.name in hideparam: return
		if params[param.name].type == 'm' or param.name == 'Open': return
		# Re-define the widget position
		yd = yrow[param.section]  	# y position for parameter label and control (default setting)
		yb = yd #*14.1/13.1 - 24.756  # y position for relative placement (board setting)  yb = yd - 23 # y position for parameter label and control (board setting)
		yc = yd #*14.1/12.45 - 53.23  # y position for relative placement (channel setting) yd - 45 # y position for parameter label and control (channel setting)
		yrow[param.section] += 25

		m_xdef = x_def 
		lx = 0
		kk = yd
		if param.name == "OutputFiles":
			yrow[param.section] = 10
			yd = yrow[param.section]
			yrow[param.section] += 25
			lx = 330
		if param.name == "DataAnalysis":  # "DataFilePath":
			kk = yd-3
			m_xdef = 480 #465
			lx = 330
		if "OF_" in param.name or 'DataFilePath' in param.name:
			if 'FileUnit' in param.name: m_xdef = 480 #465 # or \
				# 'OF_MaxSize' in param.name or \
				# 'OF_ListLL' in param.name: m_xdef = 465
			else: m_xdef = 480 #520
			lx = 330
		# Replace everything except hideparam!
		if param.name in self.button_names:
			try: self.button_names[param.name][0].place(relx=self.button_names[param.name][1], rely=(kk+self.button_names[param.name][2])/sh.Win_Tabs_H, relwidth=self.button_names[param.name][3], relheight=self.button_names[param.name][4]) # x=self.button_names[param.name][1], y=yd)
			except:
				for click in self.button_names[param.name][0]:
					click.place(relx=self.button_names[param.name][1], rely=(kk+self.button_names[param.name][2])/sh.Win_Tabs_H, relwidth=self.button_names[param.name][3], relheight=self.button_names[param.name][4])

		if params[param.name].type == '-':
			if param.name.find("_BLANK") < 0: self.par_def_label[param.name].place(relx=lx/sh.Win_Tabs_W, rely=yd/sh.Win_Tabs_H) # x=0, y=yd)
			return
		self.par_def_label[param.name].place(relx=lx/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H)  #   x=0, y=yd) x=0, y=yd)
		if params[param.name].type == 'c': 
			if params[param.name].distr == 'g': self.par_def_combo[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H, relwidth=0.163, relheight=0.039)  #   x=x_def, y=yd)
			elif params[param.name].distr == 'b': [self.par_brd_combo[param.name][brd].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Nb_H, relwidth=0.2, relheight=0.039) for brd in range(sh.MaxBrd)] #x=x_brd, y=self.yb) 
			elif params[param.name].distr == 'c': [self.par_ch_combo[param.name][brd][ch].place(relx=(x_brd+ch%8*81.5)/sh.Win_Tabs_W, rely=yc/sh.Win_Tabs_H, relwidth=0.11, relheight=0.041) for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)] # x=2 + ch%8 * 50, y=self.yc) 
		elif params[param.name].type == 'b':
			# if "OF_" in param.name:
			# 	m_xdef = 500
			if params[param.name].distr == 'g': self.par_def_checkbox[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H)  #   x=x_def, y=yd)
			elif params[param.name].distr == 'b': [self.par_brd_checkbox[param.name][brd].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Nb_H) for brd in range(sh.MaxBrd)] #x=x_brd, y=yb) 
			elif params[param.name].distr == 'c': [self.par_ch_checkbox[param.name][brd][ch].place(relx=(x_brd+ch%8*81.5)/sh.Win_Tabs_W, rely=yc/sh.Win_Tabs_H) for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)] #x=2 + ch%8 * 50, y=yc) 
		else:
			nch_t = 1
			if self.tabmode[param.section] == 2: nch_t = 8
			self.par_def_entry[param.name].place(relx=float(m_xdef)/sh.Win_Tabs_W, rely=float(yd)/sh.Win_Tabs_H, relwidth=0.163, relheight=0.039)  #   x=x_def, y=yd)
			if params[param.name].distr == 'b': [self.par_brd_entry[param.name][brd][ch].place(relx=float(x_brd)/sh.Win_Tabs_W, rely=float(yb)/sh.Win_Nb_H, relwidth=0.27, relheight=0.041) for brd in range(sh.MaxBrd) for ch in range(nch_t)] #x=x_brd, y=ybx=x_brd, y=yb) 
			elif params[param.name].distr == 'c': [self.par_ch_entry[param.name][brd][ch].place(relx=(x_brd+ch%8*81.5)/sh.Win_Tabs_W, rely=yc/sh.Win_Tabs_H, relwidth=0.11, relheight=0.041) for brd in range(sh.MaxBrd) for ch in range(sh.MaxCh)] #x=2 + ch%8 * 50, y=yc) 
		
	def remove_label(self, hideparam):
		en_button = {		# to enable the widget added to the GUI
			"Q_DiscrMask0": ["Q_DiscrMask0", "Q_DiscrMask1", "QD_CoarseThreshold", 
							 "QD_FineThreshold", "Q-Discriminators"],
			"Tlogic_Mask0": ["FastShaperInput", "TD_CoarseThreshold", "TD_FineThreshold",
							 "Trg_HoldOff", "Tlogic_Mask0", "Tlogic_Mask1", "T-Discriminators"]
		}

		for item in en_button.values():
			val = item[0] in hideparam
			for i in range(len(item)-2):
				val = val and (item[i] in hideparam)
			if val:
				self.par_def_label[item[-1]].place_forget()

	def load_tabs_widget(self, basic_advanced, acqmode, jobenabled):
		x_def = 140	# x-pos of default entry/combo
		x_brd = 3	# x-pos of board entry/combo
		x_ch = 300	# x-pos of channel entry/combo
		yrow = {s: 10 for s in sections} # initial Y-position for default and channel rows (one variable per section)
		tabmodel = {s : 0 for s in sections}
		for param in params.values():
			if param.distr == 'c': 
				tabmodel[param.section] = 2
				yrow[param.section] = 40
			if param.distr == 'b' and tabmodel[param.section] == 0: 
				tabmodel[param.section] = 1
				yrow[param.section] = 10
				
		# Load inverse of rename (it can be with enumerate(self.param_rename))
		self.rename_parname = {}
		hidetab = []
		hideparam = []
		
		for key, val in self.param_rename.items(): # inverse rename parameters GUI name: C name
			self.rename_parname[val] = str(key)
		
		my_hiding_status = re.compile("["+basic_advanced + self.AcqMode_Dict[acqmode]+"]")
		
		hideparam = self.load_par2remove(my_hiding_status, 'h')	# get name to remove
		hidetab = self.load_par2remove(my_hiding_status, 't')
		for param in params.values():	# place everything that is not in gui file
			self.place_widgets(param, hideparam, yrow, x_def, x_brd)
			if param.name in list(self.new_options.keys()):
				self.set_new_options(param, jobenabled)
			
		self.remove_label(hideparam)
		self.Mtabs_shown.clear()
		
		for s in sections:	# remove the sections that are in the hiddenfile
			if s in hidetab: continue # and basic_advanced == '0': continue
			self.Mtabs_nb.add(self.Mtabs[s], text=' ' + s + ' ')
			self.Mtabs_shown[s] = self.Mtabs[s]

		
	def update_guimode(self, basic_advanced, not_acq_ch=1):	
		# remove every widget - contorl if the GUIbasic/advanced file exists
		# If the entries related to a button are missing the button is removed 
		if not os.path.exists(sh.GuiModeFile):
			with open(sh.GuiModeFile, 'w') as f:
				f.write("# List of the parameters that won't be visualized in the different GUI views\n")
				f.write("# xNNN, where x=a, b, N=0 to 5 (multiple N allowed)\n")
				f.write("# x: a=Advanced, b=Basic\n")
				f.write("# N: SPECT=0, SPECT_TIME=1, TIME_CSTART=2, TIME_CSTOP=3, COUNT=4, WAVE=5 (see par_defs.txt for details)\n")
				f.writE("# E: JobEnabled=J, JobNotEnabled=N")
				f.write("# Es: Run Sleep			b01 (parameter removed in basic, spect and specttime modes\n")
				f.write("# Please, use 'tab' for spacing param_name and hiding attriute\n")
				f.write("# Param name		Hiding Attribute\n")

		acqmode = self.par_def_svar["AcquisitionMode"].get()
		if acqmode not in self.AcqMode_Dict: return
		tab_idx = self.Mtabs_nb.select() # get the tab currently selected

		if int(self.par_def_svar["EnableJobs"].get()): jobenabled = 'J'
		else: jobenabled = 'N'

		#if not_acq_ch:
		#	if basic_advanced == 'a': 
		#		self.Output.insert(END, "Switch to Advanced GUI view\n")
		#	elif basic_advanced == 'b': 
		#		self.Output.insert(END, "Switch to Basic GUI view\n")
		#else:
		#	self.Output.insert(END, "Switch to " +  acqmode + " view\n")
		#self.Output.yview_scroll(100, UNITS)

		self.remove_tabs_widget()
		self.load_tabs_widget(basic_advanced, acqmode, jobenabled)

		# set tab Notebook to the last active one 
		self.Mtabs_nb.select(tab_idx)

	

