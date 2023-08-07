# ------------------------------------------------------------------
# python GUI for PyCROS (FERS readout software by Tintori)
# ------------------------------------------------------------------

import sys
import subprocess
import time
import os
import re

from threading import Thread, Lock

from tkinter import *
from tkinter import ttk
from tkinter import font
from tkinter import messagebox
from tkinter.commondialog import Dialog
from tkinter.ttk import Progressbar, Style
from tkinter.filedialog import askopenfilename, asksaveasfilename, askdirectory
#import tk_tools

import shared as sh
import cfgfile_rw as cfg
import socket2daq as comm
import ctrl as ctrl
import tabs as tabs
import ctypes

if sys.platform.find('win') == 0:
	ctypes.windll.shcore.SetProcessDpiAwareness(2) # PROCESS_PER_MONITOR_DPI_AWARE = 2

params = sh.params
sections = sh.sections

# ------------------------------------------------------------------
class Open_GUI(Frame):
	def __init__(self, master):
		Frame.__init__(self, master)
		self.master = master
		# self.style = Style() # DNIN: can be useful for deal with Linux GUI?

		# images and logos
		if sys.platform.find('win') < 0:
			sh.ImgPath = '../img/'
		self.img_logo = PhotoImage(file=sh.ImgPath + "logo.png").subsample(3, 3)

		# define main window properties
		self["width"] = sh.Win_W
		self["height"] = sh.Win_H
		self.place(x=0, y=0)
		self.place
		self.tk.call('wm', 'iconphoto', master._w, self.img_logo)
		self.master.protocol("WM_DELETE_WINDOW", self.CloseAndQuit)
		self.master.wm_iconphoto(True, self.img_logo)

		self.img_bg = PhotoImage(file=sh.ImgPath + "Janus.png").subsample(2, 2)
		logo_W = 150
		if sys.platform.find('win') < 0: logo_W = 190
		self.bglabel = Label(self.master, image = self.img_bg)  #	 x=150, y=200)
		self.bglabel.place(relx=logo_W/sh.Win_W, rely=200/sh.Win_H)
		self.master.update()

		self.Ctrl = ctrl.CtrlPanel()
		self.Ctrl.OpenControlPanel(self.master)
		self.Ctrl.plugged.trace('w', lambda name, index, mode: self.DAQconnect())

		self.Tabs = tabs.TabsPanel()
		self.Tabs.OpenTabs(self.master)
		self.Tabs.combobox_state(1)

		self.Tabs.CfgChanged.trace('w', lambda name, index, mode:self.Set_b_apply()) # To prevent overwrite of Janus_Config when a run of a job is performed
		self.Tabs.ActiveBrd.trace('w', lambda name, index, mode: self.AssignActiveBd())  # self.Ctrl.active_board.set(str(self.Tabs.ActiveBrd.get())))
		# self.Ctrl.bstart.trace('w', lambda name, index, mode:self.Set_b_apply())
		self.Ctrl.CfgReloaded.trace('w', lambda name, index, mode:self.Tabs.Params2Tabs(self.Ctrl.CfgReloaded.get()))
		self.Ctrl.CfgFileName.trace('w', lambda name, index, mode:self.CfgLoadOnLog(self.Ctrl.CfgFileName.get())) 
		self.Ctrl.CfgNameSaved.trace('w', lambda name, index, mode:self.CfgSaveOnLog(self.Ctrl.CfgNameSaved.get()))
		self.Ctrl.CfgWarning.trace('w', lambda name, index, mode:self.WrittenEmptyEntries())
		self.Ctrl.ConvCsvTrace.trace('w', lambda name, index, mode:self.NotifyCsvStarted())
		# self.Ctrl.combobox_writing.trace('w', lambda name, index, mode: self.Tabs.combobox_state(self.Ctrl.combobox_writing.get()))

		# set gui apparence according to acq mode		
		self.guimodegui = StringVar()
		self.guimodegui.set('a')
		self.guimodegui.trace('w', lambda name, index, mode:self.Tabs.update_guimode(self.guimodegui.get()))
		self.Tabs.par_def_svar["AcquisitionMode"].trace('w', lambda name, index, mode:self.Tabs.update_guimode(self.guimodegui.get(), 0))
		self.Tabs.update_guimode(self.guimodegui.get())
		
		self.DebugGUI = IntVar()
		self.DebugGUI.set(0)

		self.OpenAndFWupg = IntVar()
		self.OpenAndFWupg.set(0)

		self.AddMenu()
		self.bglabel.place_forget()

		# start thread for reading messages from the client and print to output window
		self.stop_thread = False
		self.t = Thread(target=self.ClientMsg)
		self.t.daemon = True # thread dies with the program
		self.t.start()

	
	# *******************************************************************************
	# Assign Active brd using 'trace' function with try/except syntax
	def AssignActiveBrd(self):
		try: self.Ctrl.active_board.set(str(self.Tabs.ActiveBrd.get()))
		except: pass

	# *******************************************************************************

	# Write on Log Tab
	# *******************************************************************************
	def Set_b_apply(self):
		self.Ctrl.b_apply.configure(bg = "red", state = NORMAL)
		# To prevent overwrite of Janus_Config when a run of a job is performed
		# if self.Tabs.par_def_svar["EnableJobs"].get() == "1" and self.Ctrl.bstart['state'] == 'disabled': 
		# 	self.Ctrl.b_apply.configure(bg = "red", state = DISABLED)
		# else:
		# 	self.Ctrl.b_apply.configure(bg = "red", state = NORMAL)

	def CfgLoadOnLog(self, myname):
		print("Load configuration from '" + myname + "'\n")
		self.Tabs.Output.insert(END, "Load configuration from '" + myname + "'\n")
		self.Tabs.Output.yview_scroll(100, UNITS)

	def CfgSaveOnLog(self, myname):
		if myname.find("ACOPY_") != -1:	# not used at the moment
			myname = myname.replace("ACOPY_", "")
			self.Tabs.Output.insert(END, "Save a configuration copy as '" + myname + "'\n")
		else:
			self.Tabs.Output.insert(END, "Save configuration as '" + myname + "'\n")
			print("Save configuration as '" + myname + "'\n")
		self.Tabs.Output.yview_scroll(100, UNITS)

	def NotifyCsvStarted(self):
		mytext = "Converting to CSV format the following binary files:\n"
		for name in self.Ctrl.Bin_fname:
			if len(name.get()) > 0:
				mytext = mytext + " - " + name.get() + "\n"
		mytext = mytext + "Please, check the opened shell to monitor the CSV conversion status ...\n"

		self.Tabs.Output.insert(END, mytext)
		self.Tabs.Output.yview_scroll(100, UNITS)

	# GUI self warning
	def WrittenEmptyEntries(self):
		wrmsg = ""
		if len(cfg.gain_check) > 0 or len(cfg.empty_field) > 0:
			for empt in cfg.empty_field:
				empt = self.Tabs.param_rename[empt]
				mymsg = empt + " default entry is left blank. Janus will use its default value\n"
				wrmsg = wrmsg + empt + "\n"
				self.Tabs.Output.insert(END, mymsg, 'empty')
				self.Tabs.Output.yview_scroll(100, UNITS)
			if len(cfg.empty_field) > 1: wrmsg = wrmsg + "default entries are left Blank.\nJanus will use its default values\n\n" 
			elif len(cfg.empty_field) > 0: wrmsg = wrmsg + "default entry is left Blank.\nJanus will use its default value\n\n" 
			if len(cfg.gain_check) > 0:
				mm = "".join(gcheck for gcheck in cfg.gain_check)
				self.Tabs.Output.insert(END, mm, 'warning')
				self.Ctrl.RisedWarning.set(1)
				self.Tabs.Output.yview_scroll(100, UNITS)
				wrmsg = wrmsg + mm
			if self.Ctrl.show_warning.get(): 
				messagebox.showwarning(title=None, message=wrmsg)

		if len(cfg.jobs_check) > 0: # Last job < first job 
			self.Tabs.Output.insert(END, cfg.jobs_check[0], 'empty')
			self.Tabs.Output.yview_scroll(100, UNITS)
			# wrmsg = wrmsg + cfg.jobs_check[0]
			self.Tabs.par_def_svar['JobLastRun'].set(params['JobLastRun'].default)
			return
		if len(cfg.cfg_file_list) > 0:
			mmsg = "You are overwriting the paramters with other cfg file:\n"
			for cfile in cfg.cfg_file_list:
				mmsg = mmsg + " - " + cfile + "\n"
			mmsg = mmsg + "The previous parameter values are lost, the new ones are displaied on GUI!\n"	
			# mmsg = mmsg + "The new parameter values are loaded on JanusC but not shown on GUI!"
			self.Tabs.Output.insert(END, mmsg, 'empty')
			return

		
	# *******************************************************************************
	# Menu
	# *******************************************************************************
	def AddMenu(self):
		self.mGuiMenu = Menu(self.master)
		self.menu_file = Menu(self.mGuiMenu, tearoff=0)
		self.menu_file.add_command(label='Load Config File', command=self.Ctrl.ReadCfgFile)
		self.menu_file.add_command(label='Save Config File', command=self.Ctrl.SaveCfgFile)
		self.menu_file.add_command(label='Save Config File As', command=self.Ctrl.SaveCfgFileAs)
		self.menu_file.add_command(label="Load Macro", command=self.Ctrl.OpenExternalCfg)
		# self.menu_file.add_command(label="Reset Plotter", command=lambda:comm.SendCmd('G'))
		self.menu_file.add_separator()
		self.menu_file.add_command(label='Quit', command=self.CloseAndQuit)
		self.menu_FWupgrade = Menu(self.mGuiMenu, tearoff=0)
		self.menu_FWupgrade.add_command(label='Upgrade FPGA', command=self.FPGAupgrade)
		# self.menu_FWupgrade.add_command(label='Upgrade uC', command=self.uCupgrade)
		self.menu_FWupgrade.add_command(label='Restore IP 192.168.50.3', command=self.RestoreIP)
		self.menu_mode = Menu(self.mGuiMenu, tearoff=0)
		self.menu_mode.add_radiobutton(label='Basic', variable=self.guimodegui, value='b')
		self.menu_mode.add_radiobutton(label='Advanced',variable=self.guimodegui, value='a')
		self.menu_mode.add_checkbutton(label="Show warning pop-up", variable=self.Ctrl.show_warning)
		self.menu_mode.add_checkbutton(label="Verbose socket messages", variable=self.DebugGUI)
		self.menu_help = Menu(self.mGuiMenu, tearoff=0)
		self.menu_help.add_command(label='About', command=self.Help_About)

		self.mGuiMenu.add_cascade(label='File', menu=self.menu_file)
		self.mGuiMenu.add_cascade(label='FWupgrade', menu=self.menu_FWupgrade)
		self.mGuiMenu.add_cascade(label='GUI Mode', menu=self.menu_mode)
		self.mGuiMenu.add_cascade(label='Help', menu=self.menu_help)
		# Add menu mode Basic / Expert
		self.master.config(menu=self.mGuiMenu)
		self.mGuiMenu.entryconfig("FWupgrade", state="disabled") # DNIN: commented for debug
		self.UpgradeWinIsOpen = False

	def Help_About(self):
		messagebox.showinfo("", "JanusPy Rel. " + sh.Release)

	# *******************************************************************************
	# Connect / Disconnect function
	# *******************************************************************************
	def DAQconnect(self):
		if self.Ctrl.plugged.get() == 1:
			self.Ctrl.SetAcqStatus(1, 'Connecting JanusC...')
			self.Tabs.status_now = 1
			cfg.status = 1
			self.Tabs.TabsUpdateStatus(1)
			t = Thread(target=self.DAQconnect_Thread)
			t.daemon = True # thread dies with the program
			t.start()
		else:	
			comm.SendCmd('V0')	# Stop updating
			time.sleep(0.1)		# 			
			comm.SendCmd('q0')	# Closing
			time.sleep(0.1)
			self.CheckHVIsOn()	# Check if HV is still on
			time.sleep(0.1)
			# comm.SendCmd('q')
			self.Tabs.TabsUpdateStatus(0)
			comm.Close()
			self.Ctrl.SetAcqStatus(0, 'offline')
			self.Tabs.status_now = 0
			cfg.status = 0
			
	def DAQconnect_Thread(self):
		fname = "JanusC.exe"
		if sys.platform.find('win') < 0: fname = "JanusC"
		if not os.path.exists(fname):	#   Error: JanusC is not in the folder and cannot be launched
			Jmsg="Warning, JanusC executable is missing!!!\nPlease, check if the antivirus cancel it during the unzip (Windows)"
			Jmsg=Jmsg+"or run make from the main folder (Linux)"
			messagebox.showwarning(title=None, message=Jmsg)
			self.Ctrl.plugged.set(0)
			return

		if comm.SckConnected == 0:
			# start PyCROS
			ON_POSIX = 'posix' in sys.builtin_module_names
			if sys.platform.find('win') < 0: 
				subprocess.Popen(['./JanusC','-g','2>','/dev/null'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=2, close_fds=ON_POSIX)
			else: 
				subprocess.Popen('JanusC.exe -g 2> nul', shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1, close_fds=ON_POSIX)
			comm.Open()
			timeout = 0
			while timeout < 20:
				if comm.SckConnected: break
				time.sleep(0.1)

	def CheckHVIsOn(self):
		# brd_hvon = [i for i in range(sh.MaxBrd) if self.Tabs.HVcb_status[i].get()]
		mmsg = ""
		timeout2 = time.time() + 4 # in case "Quitting" got lost during communication
		while "WARNING" not in mmsg and "Quitting" not in mmsg:
			try: mmsg = comm.GetString()
			except: break
			if time.time() > timeout2: break	
		if "WARNING" in mmsg:
			# print("\a")
			res = messagebox.askyesno("Shut HV down?", mmsg[1:])
			if res: comm.SendCmd("y") # DNIN: a confirm of the turn off is needed?
			else: comm.SendCmd("n")	

	def CloseAndQuit(self):
		if cfg.status == 4: # JanusC is in Running
			comm.SendCmd('S')
			time.sleep(0.1)
	
		comm.SendCmd('V0')	# Stop update 
		self.stop_thread = True
		time.sleep(0.1)
		if self.t.is_alive(): self.t.join()
		if comm.SckConnected and not comm.SckError:
			comm.SendCmd('q0')
			time.sleep(0.1)
			self.CheckHVIsOn()
			time.sleep(0.1)
			comm.Close()
		if self.Ctrl.b_apply['bg'] == 'red':	# Changes not applied
			# print("\a") # seems it doesn't work
			res = messagebox.askyesnocancel('Save Changes', 'Do you want to save current configuration?')
			if res == None: return False
			if res: 
				self.Ctrl.SaveCfgFileAs()
				self.Ctrl.plugged.set(0)

		# Dump on file LogGUI
		if self.DebugGUI.get():
			with open("JanusPyLog.log", "w") as f:
				for line in self.Tabs.Output.dump(1.0, END, test=True):
					f.write(line[1])

		self.quit()


	# *******************************************************************************
	# Thread that manages the messages coming from client
	# *******************************************************************************
	def ClientMsg(self):
		enable_hvmon = 0
		pcmsg = ""
		wmsg = ""
		while not self.stop_thread:
			if not comm.SckConnected:
				time.sleep(0.1)
				enable_hvmon = 0
				continue
			if comm.SckError:
				comm.SckConnected = False
				self.Ctrl.SetAcqStatus(-1, "Connection Error!")
				self.Tabs.status_now = -1
				cfg.status = -1
				self.Tabs.UpdateStatsTab('0')
				continue
			if comm.SckConnected:
				if (list(self.Tabs.Mtabs)[self.Tabs.Mtabs_nb.index('current')] == 'HV_bias'):
					if enable_hvmon == 0: comm.SendCmd('V1')
					enable_hvmon = 1
				else:	
					if enable_hvmon == 1: comm.SendCmd('V0')
					enable_hvmon = 0
			
			cmsg = comm.GetString()  # server.recv_data() from socket
			if len(cmsg) > 0:
				if self.DebugGUI.get():
					print("Message from board: ", cmsg)	# debug
					forgrd = "verbose"
					with open("JanusPylog.log", "a") as f:
						f.write("Message from board: ", cmsg)
					if cmsg[0] != 'm' or cmsg != 'w':	# Verbose message
						self.Tabs.Output.insert(END, cmsg[1:] + "\n", forgrd)
						self.Tabs.Output.yview_scroll(100, UNITS)
				if cmsg[0] != 'w' and pcmsg == 'w':	# print Warning on LOG and raise a warning pop-up
					if self.Ctrl.show_warning.get(): messagebox.showwarning(title=None, message="WARNING(s)!!!\n\n" + wmsg, parent=self.master)
					self.Ctrl.RisedWarning.set(1)
					self.Tabs.Output.insert(END, "WARNING(s)!!!\n" + wmsg, 'warning')
					self.Tabs.Output.yview_scroll(100, UNITS)
					wmsg = ""					
					pcmsg = cmsg[0]
				#dest = sdata[0].decode('utf-8')
				if cmsg[0] == 'm':  # log message (to LogMsg panel)
					forgrd = 'normal'
					if cmsg.find("ERROR") != -1: 
						forgrd = 'error'  
						cmsg = cmsg[:-1]	# DNIN: to be tested
					self.Tabs.Output.insert(END, cmsg[1:], forgrd)
					self.Tabs.Output.yview_scroll(100, UNITS)
				elif cmsg[0] == 'a': # acquisition status
					status = int(cmsg[1:3])
					status_msg = cmsg[3:]
					status_msg = status_msg.split("\n")[0] # Only the Error msg due to missing firmware is filtered
					pr_status = self.Tabs.status_now # DNIN: Can be done simpler?
					self.Tabs.status_now = status
					cfg.status = status
					self.Ctrl.SetAcqStatus(status, status_msg)	# With cmsg[0]=='R' there is a redundance
					self.Tabs.TabsUpdateStatus(status)
					if status == sh.ACQSTATUS_ERROR: 
						self.Tabs.Output.insert(END, '\n', 'normal')
					if status == sh.ACQSTATUS_READY: 
						self.mGuiMenu.entryconfig("FWupgrade", state="normal")
						if pr_status == 2: #!= status: # Only when connect 
							comm.SendCmd("\t{}".format(self.Tabs.change_statistics.get()))
							time.sleep(1)
							comm.SendCmd("I{}".format(self.Tabs.change_stat_integral.get()))
					else: self.mGuiMenu.entryconfig("FWupgrade", state="disabled")
					if status == sh.ACQSTATUS_UPGRADING_FW:
						if "Progress" in status_msg:
							self.UpgStat.configure(text = f"{status_msg}%")
						else:
							self.UpgStat.configure(text = status_msg)
						s = status_msg.split()
						if len(s) > 1 and s[0].find('Progress') >= 0:
							if float(s[1]) >= 100: self.notify_succesfull("Firmware Upgrade") # self.CloseUpgradeWin()
							else: self.upg_progress['value'] = float(s[1])
				elif cmsg[0] == 'F': # Firmware not found
					ret = messagebox.askyesno("FPGA Firmware not found", cmsg[1:-8])
					if ret: 
						self.OpenAndFWupg.set(1)
						comm.SendCmd('y')
						self.FPGAupgrade()
						# self.mGuiMenu.entryconfig("FWupgrade", state="normal")
					else: comm.SendCmd('n')				
				elif cmsg[0] == 'i': # board info
					self.Tabs.update_brd_info(cmsg[1:])
				#elif cmsg[0] == 'p': # Plot Type
				#	self.Ctrl.plot_type.set(self.Ctrl.plot_options[int(cmsg[1])])
				elif cmsg[0] == 'c': # Active Channel/Board
					self.Ctrl.active_channel.set(int(cmsg[1:]))
				elif cmsg[0] == 'r': # read Register
					self.Tabs.reg_data.set(cmsg[1:])
					self.Tabs.RWregLog.insert(END, "Rd-Reg: A=" + self.Tabs.reg_addr.get() + " D=" + cmsg[1:] + '\n')
					self.Tabs.RWregLog.see(END)
				elif cmsg[0] == 'h': # HV settings
					self.Tabs.UpdateHVTab(cmsg[1:])
					if cfg.status == sh.ACQSTATUS_HW_CONNECTED:
						hvs = cmsg[1:].split()
						brd = int(hvs[0])
						hv_on = int(hvs[1]) & 1
						self.Ctrl.HV_ON[brd] = hv_on
				elif cmsg[0] == 'S': # strings with statistics info (to Stats panel)
					self.Tabs.UpdateStatsTab(cmsg)
				elif cmsg[0] == 'R' and (not int(sh.params['EnableJobs'].default)): # Decoupling RunNumber in enable job
					self.Ctrl.RunNumber.set(int(cmsg[1:]))
				elif cmsg[0] == 'w': # Create the Warning message	
					pcmsg = 'w'
					my_msg = cmsg[10:].split(':')
					try:
						wmsg = wmsg + self.Tabs.param_rename[my_msg[0]] + ":" + my_msg[1]
					except:
						wmsg = wmsg + my_msg[0] + ":" + my_msg[1]
					# wmsg = wmsg + cmsg[10:]
				elif cmsg[0] == "u": # Messages related to frmware upgrade fails
					if self.Ctrl.show_warning.get(): messagebox.showwarning(title=None, message=cmsg[1:] + wmsg, parent=self.master)
			else:	
				time.sleep(0.1)


	# *******************************************************************************
	# Firmware Upgrade
	# *******************************************************************************
	def notify_succesfull(self, func):
		messagebox.showinfo(title=func, message=f"{func} succesfully completed!")
		if self.OpenAndFWupg.get():
			self.Ctrl.plugged.set(0)
			self.OpenAndFWupg.set(0)

		self.CloseUpgradeWin()


	def uCupgrade(self):
		os.startfile("AllInOneJar.jar")

	def RestoreIP(self):
		if (self.Tabs.BrdEnable[1].get() == 0) and ("usb" in self.Tabs.conn_path[0].get()):
			comm.SendCmd('!')
		else:
			messagebox.showwarning(title=None, message="IP restore only with single board connected via USB")
		
	def FPGAupgrade(self):
		if self.UpgradeWinIsOpen: self.CloseUpgradeWin()
		self.UpgradeWin = Toplevel()
		self.t_fname = "."
		x_l = 700
		self.UpgradeWin.geometry("{}x{}+{}+{}".format(x_l, 170, 150, 400))
		self.UpgradeWin.wm_title("FPGA Firmware Upgrade")
		self.UpgradeWin.grab_set()
		# self.UpgradeWin.attributes('-topmost', 'true')
		self.UpgradeWin.protocol("WM_DELETE_WINDOW", self.CloseUpgradeWin)
		self.UpgradeWinIsOpen = True
		self.FWupg_fname = ''
		self.Tbrd = StringVar()
		self.Tbrd.set('0')
		#if os.path.isfile("FWupgfile.txt"):
		#	ff = open("FWupgfile.txt", "r")
		#	self.FWupg_fname = ff.readline()
		#	ff.close()
		x0 = 5
		y0 = 5
		Label(self.UpgradeWin, text = "Upgrade File").place(relx=x0/x_l, rely=y0/170.) # x = x0, y = y0)
		y0 += 25
		self.fnlabel = Text(self.UpgradeWin, bg = 'white') #, height=1, width = 60)
		self.fnlabel.place(relx=x0/x_l, rely=y0/170., relheight=0.13, relwidth=0.85) #  x = x0, y = y0)
		Button(self.UpgradeWin, text='Browse', command=self.SelectUpgFile).place(relx=0.87, rely=(y0-4)/170., relwidth=0.12)  #  x = x0 + 487, y = y0-4)

		y0 += 30
		Label(self.UpgradeWin, text = "Brd", font=("Arial", 12)).place(relx=x0/x_l, rely=y0/170.)  #  x = x0, y = y0)
		tmp0=[self.Tabs.info_fpga_fwrev[int(self.Tbrd.get())].cget('text'), self.Tabs.info_board_model[int(self.Tbrd.get())].cget('text')]
		self.CurrVers = Label(self.UpgradeWin, text = "Current Version: " + tmp0[0] + "\nBoard Model: " + tmp0[1], font=("Arial", 10), justify=LEFT)
		self.CurrVers.place(relx=(x0+100)/x_l, rely=(y0-5)/170.)  #  x = x0 + 100, y = y0-5)
		Spinbox(self.UpgradeWin, textvariable=self.Tbrd, command=self.ChangeBrd, from_=0, to=sh.MaxBrd-1, font=("Arial", 14), width=4).place(relx=(x0+35)/x_l, rely=y0/170.)  #  x = x0 + 35, y = y0)
		
		y0 += 40
		Button(self.UpgradeWin, text='Upgrade', command=self.DoUpgrade).place(relx=0.87, rely=y0/170., relwidth=0.12)  #  x = x0, y = y0)
		self.upg_progress = Progressbar(self.UpgradeWin, orient = HORIZONTAL, length = 474, mode = 'determinate') 
		self.upg_progress.place(relx=x0/x_l, rely=(y0+3)/170., relwidth=0.85, relheight=0.125)  #  x = x0 + 75, y = y0+3)

		y0 += 35
		Label(self.UpgradeWin, text = "Messages", relief=GROOVE, justify=CENTER).place(relx=0.87, rely=y0/170., relwidth=0.12)  #  x = x0, y = y0)
		self.UpgStat = Label(self.UpgradeWin, text = "", anchor = "w", relief = 'groove')
		self.UpgStat.place(relx=x0/x_l, rely=y0/170., relheight=0.125, relwidth=0.85) # x = x0 + 75, y = y0)


	def ChangeBrd(self):
		self.CurrVers.configure(text = "Current Version: " + self.Tabs.info_fpga_fwrev[int(self.Tbrd.get())].cget('text'))

	def DoUpgrade(self):
		if len(self.FWupg_fname) < 3: return
		#ff = open("FWupgfile.txt", "w")
		#ff.write(self.FWupg_fname)
		#ff.close()
		if not self.OpenAndFWupg.get():
			comm.SendCmd('U')
			comm.SendString(self.Tbrd.get())
			# comm.SendString(f'{self.Tbrd.get()}{self.FWupg_fname}')
			comm.SendString(self.FWupg_fname)
		else:
			comm.SendString(f'U{self.FWupg_fname}')


	def read_header(self):
		self.info_new_fw = []
		# r = re.compile("[a-f0-9]", re.IGNORECASE) # hex filter for firmware Build
		with open(self.FWupg_fname,"rb") as fw_file:
			first_line = fw_file.readline().split()[0]
			if b"$$$$CAEN-Spa" not in first_line:
				return 1
			line = fw_file.readline()	# each line is in format: b'#### \r\n'
			for cnt in range(10):
				if b"Rev" in line:
					self.info_new_fw.append(str(fw_file.readline()).split()[0].split("'")[1][:-4])	# Removed \r\n
				if b"Build" in line:
					self.info_new_fw.append(''.join(filter(lambda x: x.isalnum(), str(fw_file.readline())[1:-4])))
				if b"Board" in line:
					tmp_line = str(fw_file.readline()).split("'")[1].split("\\")[0].split()	
					self.info_new_fw.append(tmp_line)
				line = fw_file.readline()
				if b"$$$$" in line: return 0

			return 2

		# 	while b"$$$$" not in line:
		# 		if b"Rev" in line:
		# 			self.info_new_fw.append(str(fw_file.readline()).split()[0].split("'")[1][:-4])	# Removed \r\n
		# 		if b"Build" in line:
		# 			self.info_new_fw.append(''.join(filter(lambda x: x.isalnum(), str(fw_file.readline())[1:-4])))
		# 		if b"Board" in line:
		# 			tmp_line = str(fw_file.readline()).split("'")[1].split("\\")[0].split()	
		# 			self.info_new_fw.append(tmp_line)
		# 		line = fw_file.readline()
		# return 0

	def SelectUpgFile(self):
		self.FWupg_fname = askopenfilename(initialdir=self.t_fname, filetypes=(("Binary files", "*.bin *.ffu"), ("All Files", "*.*")), title="Choose a file.")
		try:
			self.FWupg_fname = os.path.normpath(self.FWupg_fname)
		except:
			self.FWupg_fname = ""
		self.t_fname = self.FWupg_fname
		#p = os.path.split(self.FWupg_fname)
		self.fnlabel.delete(1.0, END)
		self.fnlabel.insert(INSERT, self.FWupg_fname)
		self.NewVers = Label(self.UpgradeWin, text = "New Version: -", fg="#990000")
		self.NewVers.place(relx=0.62, rely=55/170.)  #  x=350, y=55)
		if len(self.FWupg_fname)>3:
			if self.read_header() == 1 or len(self.info_new_fw)<3: 
				mmsg="WARNING: the firmware selected does not contain any check on boards compatility."
				mmsg = mmsg + "\nContinue?"
				# Set yes/no box
				ret = messagebox.askyesno("WARNING", mmsg)
				if not ret:
					self.fnlabel.delete(1.0, END)
					self.FWupg_fname = ""
			elif self.read_header() == 2:
				mmsg="ERROR: the ffu Header cannot be properly read."
				mmsg=mmsg+"\nExiting ..."
				messagebox.showwarning(title=None, message=mmsg, parent=self.master)
				self.fnlabel.delete(1.0, END)
				self.FWupg_fname = ""
				return -1
			elif ''.join(filter(lambda x: x.isdigit(), self.Tabs.info_board_model[int(self.Tbrd.get())].cget('text'))) not in str(self.info_new_fw[2]):
				mmsg="ERROR: the firmware you want to upgrade (valid for FERS "+','.join(self.info_new_fw[2])+") is not suitable for the board selected ("+self.Tabs.info_board_model[int(self.Tbrd.get())].cget('text')+")."
				mmsg=mmsg+"\nExiting ..."
				messagebox.showwarning(title=None, message=mmsg, parent=self.master)
				self.fnlabel.delete(1.0, END)
				self.FWupg_fname = ""
				return -1
			else:
				self.NewVers['text'] = "New Version: " + str(self.info_new_fw[0]) + " (Build = " + str(self.info_new_fw[1]) + ")"
				self.NewVers.config(font=("Arial", 10))

	def CloseUpgradeWin(self):
		self.UpgradeWin.destroy()
		self.UpgradeWinIsOpen = False

# *****************************************************************************
# Main
# *****************************************************************************
sh.Version = cfg.ReadParamDescription("param_defs.txt", sh.sections, sh.params)
cfg.ReadConfigFile(sh.params, sh.CfgFile, 0)
cfg.WriteConfigFile(sections, sh.params, sh.CfgFile, 1)

# create GUI
mGui = Tk()
Gui_W = sh.Win_W
if sys.platform.find('win') < 0: Gui_W += 150 
mGui.geometry("{}x{}+{}+{}".format(Gui_W, sh.Win_H, 10, 10)) # sh.Win_W
# mGui.geometry("{}x{}+{}+{}".format(Gui_W, 1000, 10, 10)) # sh.Win_W
# mGui.tk.call('tk', 'scaling', 1.0) # test


mGui.title('Janus')
mGui.resizable(True, True)

app = Open_GUI(master=mGui)	# ok it works 

mGui.mainloop()

