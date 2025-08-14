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

from PIL import ImageTk, Image
#import tk_tools

import shared as sh
import cfgfile_rw as cfg
import socket2daq as comm
import leds as leds
import ctrl as ctrl
import tabs as tabs
import ctypes



if sys.platform.find('win') == 0:
	ctypes.windll.shcore.SetProcessDpiAwareness(2) # PROCESS_PER_MONITOR_DPI_AWARE = 2

params = sh.params
sections = sh.sections

ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

# ------------------------------------------------------------------
class Open_GUI(Frame):
	def __init__(self, master):
		Frame.__init__(self, master)
		self.master = master
		# self.style = Style() # DNIN: can be useful for deal with Linux GUI?

		# images and logos
		if sys.platform.find('win') < 0:
			sh.ImgPath = '../img/'
		
		# print(os.getcwd())
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

		# Image for error and warning no blocking pop-up
		self.warning_image = ImageTk.PhotoImage(Image.open(fp="{}warning.png".format(sh.ImgPath),mode='r').resize((45,41)))  #reduce(133))  # Computed as img.height/40
		self.error_image = ImageTk.PhotoImage(Image.open(fp="{}error.png".format(sh.ImgPath),mode='r').resize((50,50)))   #reduce(12))	# Computed as img.height/50

		self.Ctrl = ctrl.CtrlPanel()
		self.Ctrl.OpenControlPanel(self.master)
		self.Ctrl.plugged.trace('w', lambda name, index, mode: self.DAQconnect())

		self.Tabs = tabs.TabsPanel()
		self.Tabs.OpenTabs(self.master)
		self.Tabs.combobox_state(1)

		self.Tabs.CfgChanged.trace('w', lambda name, index, mode:self.Set_b_apply()) # To prevent overwrite of Janus_Config when a run of a job is performed
		self.Tabs.ActiveBrd.trace('w', lambda name, index, mode: self.AssignActiveBrd())  # self.Ctrl.active_board.set(str(self.Tabs.ActiveBrd.get())))
		# self.Ctrl.bstart.trace('w', lambda name, index, mode:self.Set_b_apply())
		self.Ctrl.CfgReloaded.trace('w', lambda name, index, mode:self.Tabs.Params2Tabs(self.Ctrl.CfgReloaded.get()))
		self.Ctrl.CfgFileName.trace('w', lambda name, index, mode:self.CfgLoadOnLog(self.Ctrl.CfgFileName.get())) 
		self.Ctrl.CfgNameSaved.trace('w', lambda name, index, mode:self.CfgSaveOnLog(self.Ctrl.CfgNameSaved.get()))
		self.Ctrl.CfgWarning.trace('w', lambda name, index, mode:self.WrittenEmptyEntries())
		self.Ctrl.ConvCsvTrace.trace('w', lambda name, index, mode:self.NotifyCsvStarted())
		# self.Ctrl.combobox_writing.trace('w', lambda name, index, mode: self.Tabs.combobox_state(self.Ctrl.combobox_writing.get()))

		# set gui apparence according to acq mode		
		self.guimodegui = StringVar()
		self.guimodegui.set(self.Ctrl.guimode.get())   #  'a') 
		self.guimodegui.trace('w', lambda name, index, mode:self.update_guimode())
		self.Tabs.par_def_svar["AcquisitionMode"].trace('w', lambda name, index, mode:self.Tabs.update_guimode(self.guimodegui.get(), 0))
		self.Tabs.par_def_svar["EnableJobs"].trace("w", lambda name, index, mode: self.Tabs.update_guimode(self.guimodegui.get()))
		self.Tabs.update_guimode(self.guimodegui.get())
		
		self.verbose = {}
		self.verbose['socket'] = IntVar()
		self.verbose['socket'].set(0)
		self.verbose['service'] = IntVar()
		self.verbose['service'].set(0)
		self.OfflineJanus = False	# Raw data process only
		# self.DebugGUI.trace('w', lambda name, index, mode: self.LibDumpMsg())

		self.OpenAndFWupg = IntVar()
		self.OpenAndFWupg.set(0)
		self.FWUpgrading = IntVar(value=0)

		self.AddMenu()
		self.update_guimode()
		self.bglabel.place_forget()

		# start thread for reading messages from the client and print to output window
		# self.stop_thread = False
		# self.t = Thread(target=self.ClientMsg)
		# self.t.daemon = True # thread dies with the program
		# self.t.start()

	
	# *******************************************************************************
	# Assign Active brd using 'trace' function with try/except syntax
	# *******************************************************************************
	def AssignActiveBrd(self):
		try: self.Ctrl.active_board.set(str(self.Tabs.ActiveBrd.get()))
		except: pass


	# *******************************************************************************
	# Change guimode visualization
	# *******************************************************************************
	def update_guimode(self):
		""" In basic mode 'Verbose socket message' is not present 
			and should be removed. Must be added in advanced mode """
		if (self.guimodegui.get() == 'b'):
			# Cannot delete Verbose socket more if not present
			if "Verbose socket messages" == self.menu_mode.entrycget(self.menu_mode.index("end"), "label"):
				self.menu_mode.delete("Verbose socket messages")
		if (self.guimodegui.get() == 'a' and 
	  		"Verbose service event" == self.menu_mode.entrycget(self.menu_mode.index("end"), "label")):
			self.menu_mode.add_checkbutton(label="Verbose socket messages", variable=self.verbose['socket'], command=lambda: self.check_radioVerbose('socket'))

		self.Ctrl.guimode.set(self.guimodegui.get())
		self.Tabs.update_guimode(self.guimodegui.get())


	def check_radioVerbose(self, key1: str):
		if key1 == 'service': key2 = 'socket'
		else: key2 = 'service'
		if self.verbose[key1].get() and self.verbose[key2].get():
			self.verbose[key2].set(0)


	# *******************************************************************************
	# Activate LowLevel dump messages with the GUI verbose
	# *******************************************************************************
	# def LibDumpMsg(self):
	# 	if self.DebugGUI.get():
	# 		cfg.cfg_file_list.append(sh.LLDumpMsg)
	# 	else:
	# 		cfg.cfg_file_list.remove(sh.LLDumpMsg)
		
	# 	self.Ctrl.len_macro.set(len(cfg.cfg_file_list))
	# 	self.Ctrl.SaveCfgFile()
	# 	return 0	

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
		self.Tabs.set_output_log("Load configuration from '" + myname + "'\n")


	def CfgSaveOnLog(self, myname):
		if myname.find("ACOPY_") != -1:	# not used at the moment
			myname = myname.replace("ACOPY_", "")
			self.Tabs.set_output_log("Save a configuration copy as '" + myname + "'\n")
		else:
			self.Tabs.set_output_log("Save configuration as '" + myname + "'\n")
			print("Save configuration as '" + myname + "'\n")


	def NotifyCsvStarted(self):
		mytext = "Converting to CSV format the following binary files:\n"
		for name in self.Ctrl.Bin_fname:
			if len(name.get()) > 0:
				mytext = mytext + " - " + name.get() + "\n"
		mytext = mytext + "Please, check the opened shell to monitor the CSV conversion status ...\n"

		self.Tabs.set_output_log(mytext)


	# GUI self warning
	def WrittenEmptyEntries(self):
		wrmsg = ""
		if len(cfg.gain_check) > 0 or len(cfg.empty_field) > 0:
			for empt in cfg.empty_field:
				empt = self.Tabs.param_rename[empt]
				mymsg = empt + " entry value is empty. Janus will use its default value\n"
				wrmsg = wrmsg + empt + "\n"
				self.Tabs.set_output_log(mymsg, 'empty')
			if len(cfg.empty_field) > 1: wrmsg = wrmsg + " entries value are empty.\nJanus will use its default values\n\n" 
			elif len(cfg.empty_field) > 0: wrmsg = wrmsg + " entry value is empty.\nJanus will use its default value\n\n" 
			if len(cfg.gain_check) > 0:
				mm = "".join(gcheck for gcheck in cfg.gain_check)
				self.Tabs.set_output_log(mm, 'warning')
				self.Ctrl.RisedWarning.set(1)
				wrmsg = wrmsg + mm
			if self.Ctrl.show_warning.get(): 
				messagebox.showwarning(title=None, message=wrmsg)

		if len(cfg.jobs_check) > 0: # Last job < first job 
			self.Tabs.set_output_log(cfg.jobs_check[0], 'empty')
			## wrmsg = wrmsg + cfg.jobs_check[0]
			self.Tabs.par_def_svar['JobLastRun'].set(params['JobLastRun'].default)
			return
		if len(cfg.cfg_file_list) > 0:
			mmsg = "You are overwriting the paramters with other cfg file:\n"
			for cfile in cfg.cfg_file_list:
				mmsg = mmsg + " - " + cfile + "\n"
			mmsg = mmsg + "The previous parameter values are lost, the new ones are displaied on GUI!\n"	
			# mmsg = mmsg + "The new parameter values are loaded on JanusC but not shown on GUI!"
			self.Tabs.set_output_log(mmsg, 'empty')
			return

		
	# *******************************************************************************
	# Menu
	# *******************************************************************************
	def AddMenu(self):
		self.mGuiMenu = Menu(self.master)
		self.menu_file = Menu(self.mGuiMenu, tearoff=0)
		self.menu_file.add_command(label='Load Config File', command=self.Ctrl.ReadCfgFile)
		# self.menu_file.add_command(label='Save Config File', command=self.Ctrl.SaveCfgFile)
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
		self.menu_mode.add_checkbutton(label="Verbose service event", variable=self.verbose['service'], command=lambda: self.check_radioVerbose('service'))
		# if self.Ctrl.guimode.get() == 'a':
		self.menu_mode.add_checkbutton(label="Verbose socket messages", variable=self.verbose['socket'], command=lambda: self.check_radioVerbose('socket'))
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
			self.stop_thread = False
			self.t = Thread(target=self.ClientMsg)
			self.t.daemon = True # thread dies with the program
			self.t.start()
			self.Ctrl.SetAcqStatus(1, 'Connecting JanusC...')
			self.Tabs.status_now = 1
			cfg.status = 1
			self.Tabs.TabsUpdateStatus(1)
			t1 = Thread(target=self.DAQconnect_Thread)
			t1.daemon = True # thread dies with the program
			t1.start()
		else:	
			self.stop_thread = True
			comm.SendCmd('V0')	# Stop updating
			time.sleep(0.2)		# 			
			comm.SendCmd('q0')	# Closing
			time.sleep(0.2)
			self.CheckHVIsOn()	# Check if HV is still on
			time.sleep(0.1)
			# comm.SendCmd('q')
			self.Tabs.TabsUpdateStatus(0)
			comm.Close()
			self.Ctrl.SetAcqStatus(0, 'offline')
			self.Tabs.status_now = 0
			cfg.status = 0
			self.mGuiMenu.entryconfig("FWupgrade", state="disabled")
			

	def DAQconnect_Thread(self):
		if sys.platform.find('win') < 0: exe_name = "JanusC"
		else: exe_name = "JanusC.exe"

		if not sh.is_exe():
			fname = os.path.join(sh.cfgfile_path, exe_name)
			wdir = sh.cfgfile_path
		else:
			fname = exe_name
			wdir = os.path.curdir

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
				subprocess.Popen(['./JanusC','-g','2>','/dev/null'], cwd=wdir, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=2, close_fds=ON_POSIX)
			else: 
				subprocess.Popen('{} -g 2> nul'.format(fname), cwd=wdir, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1, close_fds=ON_POSIX)
			comm.Open()
			timeout = 0
			while timeout < 20:
				if comm.SckConnected: break
				time.sleep(0.1)

	def CheckHVIsOn(self):
		# brd_hvon = [i for i in range(sh.MaxBrd) if self.Tabs.HVcb_status[i].get()]
		mmsg = ""
		timeout2 = time.time() + 4 # in case "Quitting" got lost during communication
		while "WARNING" not in mmsg:
			try: 
				mmsg = comm.GetString()
				if "Quit" in mmsg: break
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
		try: 
			if self.t.is_alive(): self.t.join()
		except: pass
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
		if any(list(tval.get() for tval in self.verbose.values())):   #self.verbose['socket'].get() or self.verbose['service'].get():
			with open("JanusPyLog.log", "w") as f:
				for line in self.Tabs.Output.dump(1.0, END, text=True):
					f.write(line[1])

		self.quit()


	# *******************************************************************************
	# Customized not blocking message box for warning during run 
	# *******************************************************************************
	# Insert \n in a string in order to fit a label of a given width
	def parse_string(self, mmsg: str, lab_len:int, mframe: Frame):
		# Create temp label to get the string dimension inside a label
		temp_label = Label(mframe, text=mmsg)
		temp_label.update_idletasks()  # Update the label to ensure it's fully configured
		tlw = temp_label.winfo_reqwidth()
		temp_label.destroy()
		if (tlw > lab_len):
			r=(lab_len)/tlw
			inLen = int(len(mmsg)*r)  # The string position where to cut
			# Find the closest space before the cut position:
			for pos in range(inLen,-1,-1):
				if mmsg[pos].isspace():
					r = pos
					break
		else: return mmsg

		nmsg = mmsg[0:r] + "\n" + self.parse_string(mmsg[r+1:], lab_len, mframe)
		return nmsg
	
	# Create a no-blocking messagebox of error and warning with fixed dimension
	def j_messagebox(self, type: str, msg: str):
		selfJanusMsgBox = Toplevel()
		self.t_fname = "."
		x_l = 450
		y_l = 180
		selfJanusMsgBox.geometry("{}x{}+{}+{}".format(x_l, y_l, 150, 400))
		selfJanusMsgBox.wm_title(type.upper())
		selfJanusMsgBox.resizable(False, False)
		# selfJanusMsgBox.attributes('-toolwindow', True)
		# selfJanusMsgBox.grab_set()
		selfJanusMsgBox.protocol("WM_DELETE_WINDOW", selfJanusMsgBox.destroy)  # self.CloseJanusMsgBox)
		# Pop-up frames
		msgFrame = Frame(selfJanusMsgBox, bg='white')
		msgFrame.place(relx=0, rely=0, relwidth=1, relheight=1)
		btnFrame = Frame(selfJanusMsgBox)
		btnFrame.place(relx=0, y=y_l-40, relwidth=1, relheight=0.35)
		# Message label and OK button
		nmsg = self.parse_string(msg, x_l-90, msgFrame)
		msgLabel = Label(msgFrame, text = nmsg, justify=LEFT, bg='white')
		msgLabel.place(x=81, rely=0.05, relheight=0.68, width=x_l-90)
		Button(btnFrame, text='OK', bg='white', relief=GROOVE, command=selfJanusMsgBox.destroy).place(x=x_l-100, y=10, height=22, width=80)
		
		# Image of warning and error
		if type.upper() == 'WARNING':
			img_label = Label(msgFrame, image=self.warning_image, bg='white')
			img_label.image = self.warning_image
			img_label.place(relx=0.035,rely=0.28, width=50, height=50)
		elif type.upper() == 'ERROR':
			img_label = Label(msgFrame, image=self.error_image, bg='white')
			img_label.image = self.error_image
			img_label.place(relx=0.04,rely=0.28, width=50, height=50)


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
				try: self.Tabs.Mtabs_nb.index('current')
				except: 
					time.sleep(100)
					continue
				if (list(self.Tabs.Mtabs)[self.Tabs.Mtabs_nb.index('current')] == 'HV_bias'):
					if enable_hvmon == 0: comm.SendCmd('V1')
					enable_hvmon = 1
				else:	
					if enable_hvmon == 1: comm.SendCmd('V0')
					enable_hvmon = 0
			
			cmsg_ansi = comm.GetString()  # server.recv_data() from socket
			cmsg = ansi_escape.sub('', cmsg_ansi)
			if len(cmsg) > 0:
				if any(list(tval.get() for tval in self.verbose.values())):  	# self.verbose['socket'].get() or self.verbose['service'].get():
					if cmsg[0] != 'h' and self.verbose['service'].get(): pass	# print only serivice event
					else:
						print("Message from board: ", cmsg)	# debug
						forgrd = "verbose"
						with open("JanusPylog.log", "a") as f:
							f.write(f"Message from board: {cmsg}")
						if cmsg[0] != 'm' or cmsg != 'w':	# Verbose message
							self.Tabs.set_output_log(cmsg[1:] + "\n", forgrd)
				
				if cmsg[0] != 'w' and pcmsg == 'w':	# print Warning on LOG and raise a warning pop-up
					if self.Ctrl.show_warning.get():
						self.j_messagebox('warning', "WARNING(s)!!!\n" + wmsg.rstrip())  #self.Tabs.Mtabs_nb.select('.!notebook.!frame10')
						# if self.Tabs.status_now == sh.ACQSTATUS_RUNNING:	# During Run does not show tkinter pop-up
						# 	self.j_messagebox('warning', "WARNING(s)!!!\n\n" + wmsg.rstrip())  #self.Tabs.Mtabs_nb.select('.!notebook.!frame10')
						# else:
						# 	self.launch_popup('warning', "WARNING(s)!!!\n\n" + wmsg.rstrip()) 
						# 	# messagebox.showwarning(title=None, message="WARNING(s)!!!\n\n" + wmsg)#, parent=self.master)

					if 'Service Event' in wmsg or 'OVERHEATING' in wmsg:
						self.Ctrl.statled.set_color('yellow')
					else:
						self.Ctrl.RisedWarning.set(1)
					
					self.Tabs.set_output_log("WARNING(s)!!!\n" + wmsg, 'warning')
					wmsg = ""					
					pcmsg = cmsg[0]
				#dest = sdata[0].decode('utf-8')
				if cmsg[0] == 'Q':
					# print("I am going to Close everything")
					self.Ctrl.plugged.set(0)
				if cmsg[0] == 'm':  # log message (to LogMsg panel)
					forgrd = 'normal'
					if cmsg.find("ERROR") != -1: 
						forgrd = 'error'  
						cmsg = cmsg[:-1]	# DNIN: to be tested
					self.Tabs.set_output_log(cmsg[1:], forgrd)
				elif cmsg[0] == 'a': # acquisition status
					status = int(cmsg[1:3])
					status_msg = cmsg[3:]
					status_msg = status_msg.split("\n")[0] # Only the Error msg due to missing firmware is filtered
					pr_status = self.Tabs.status_now # DNIN: Can be done simpler?
					self.Tabs.status_now = status
					cfg.status = status
					self.Ctrl.SetAcqStatus(status, status_msg, self.OfflineJanus)	# With cmsg[0]=='R' there is a redundance
					self.Tabs.TabsUpdateStatus(status)
					if status == sh.ACQSTATUS_HW_CONNECTED:
						self.Tabs.update_guimode(self.guimodegui.get())
					if status == sh.ACQSTATUS_ERROR: 
						self.Tabs.set_output_log('\n', 'normal')
						self.Tabs.set_output_log(cmsg[3:], 'error')
						self.j_messagebox("error", cmsg[3:].rstrip())
						# messagebox.showerror("ERROR", cmsg[3:])
						comm.SendCmd("K") # Reset error status
					if status == sh.ACQSTATUS_READY: 
						if not self.OfflineJanus: 
							self.mGuiMenu.entryconfig("FWupgrade", state="normal")
						if pr_status == 2: #!= status: # Only when connect 
							comm.SendCmd("\t{}".format(self.Tabs.change_statistics.get()))
							time.sleep(1)
							comm.SendCmd("I{}".format(self.Tabs.change_stat_integral.get()))
					# if status != sh.ACQSTATUS_READY: self.mGuiMenu.entryconfig("FWupgrade", state="disabled")
					if status == sh.ACQSTATUS_UPGRADING_FW:
						if "Progress" in status_msg:
							self.UpgStat.configure(text = f"{status_msg}%")
							msg_split = status_msg[3:].split() # Brd%02d Progress: %03d
							brd_upg = int(re.search(r'\d+(\.\d+)?', msg_split[0]).group(0))
							self.UpgBrdPercent[brd_upg]['text'] = msg_split[2]+" %"
							self.upg_progress['value'] = int(msg_split[2]) + 100*self.nb2upgrade.index(brd_upg)
							# if self.upg_progress['value'] >= self.upg_progress['maximum']:
						else:
							self.Tabs.set_output_log(status_msg)
							self.Tabs.set_output_log("\n")							
							try: self.UpgStat.configure(text = status_msg)
							except: pass
						# s = status_msg.split()
						# if len(s) > 1 and s[0].find('Progress') >= 0:
						# 	if float(s[1]) >= 100: self.notify_succesfull("Firmware Upgrade") # self.CloseUpgradeWin()
						# 	else: self.upg_progress['value'] = float(s[1])
				elif cmsg[0] == 'F': # Firmware not found in the board
					ret = messagebox.askyesno("FPGA Firmware not found", cmsg[1:-8])
					brd_noFW = list(filter(lambda x: x.isdigit() ,cmsg.replace(".", "").split()))[0]			
					if ret: 
						self.OpenAndFWupg.set(1)
						comm.SendCmd('y')
						self.FPGAupgrade(brd_noFW)
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
				elif cmsg[0] == 'e':
					self.Tabs.set_output_log('\n', 'normal')
					self.Tabs.set_output_log(cmsg[1:], 'error')
					self.j_messagebox("ERROR", cmsg[3:])
					# messagebox.showerror("ERROR", cmsg[3:])
				elif cmsg[0] == 'w': # Create the Warning message	
					pcmsg = 'w'
					my_msg = cmsg[10:].split(':')
					if len(my_msg) > 1:
						try:
							wmsg = wmsg + self.Tabs.param_rename[my_msg[0]] + ":" + my_msg[1]
						except:
							wmsg = wmsg + my_msg[0] + ":" + my_msg[1]
					else:
						wmsg += my_msg[0]
					# wmsg = wmsg + cmsg[10:]
				elif cmsg[0] == "u": # Messages related to frmware upgrade fails
					if "LED" in cmsg[1:]:
						brd_status = cmsg[1:].replace("LED", "").split('-')
						if int(brd_status[1]) == 1: self.UpgStatusLed[int(brd_status[0])].set_color(sh.WrCol)
						if int(brd_status[1]) < 0: self.UpgStatusLed[int(brd_status[0])].set_color(sh.ErCol)
						if int(brd_status[1]) == 0: self.UpgStatusLed[int(brd_status[0])].set_color(sh.OkCol)
					elif "END" in cmsg[1:]:
						mstr = cmsg[1:].replace("END", "")
						self.notify_succesfull(mstr)
				elif cmsg[0] == 's':
					if 'offline' in cmsg[1:]:
						self.OfflineJanus = True
						self.Tabs.offline = True
						self.Tabs.button_names['EnableJobs'][0]['state'] = DISABLED
					if 'online' in cmsg[1:]:
						self.OfflineJanus = False
						self.Tabs.offline = False
						self.Tabs.button_names['EnableJobs'][0]['state'] = NORMAL
				elif cmsg[0] == 'M':
					tmp_par = cmsg[1:].rstrip().split(':') # [Par_Name, Par_Values]
					tmp_val = ""
					tmp_lval = {}
					tmp_type = "g"
					if tmp_par[0] == "StartRunMode": tmp_val = sh.STARTRUN_MODE[int(tmp_par[1])]
					if tmp_par[0] == "HV_Vbias": 
						hv_set = {index:value for index, value in enumerate(self.Tabs.par_brd_svar[tmp_par[0]]) if len(value.get().strip()) > 0}
						if len(hv_set) == 0: # Just global HV bias set
							tmp_val = tmp_par[1].split(',')[0].split()[1] + "V" # Values are NAMEPAR: brd val, brd val, brd val ...
							tmp_type = 'g'
						else:	# Board set in python
							if float(self.Tabs.par_def_svar[tmp_par[0]].get().split()[0]) < 20: self.Tabs.par_def_svar[tmp_par[0]].set("20 V")
							elif float(self.Tabs.par_def_svar[tmp_par[0]].get().split()[0]) > 85: self.Tabs.par_def_svar[tmp_par[0]].set("85 V")
							tmp_lval = {int(t.split()[0]):int(t.split()[1]) for t in tmp_par[1].split(',') if len(t) > 0}
							tmp_type = 'b'							
							
					if tmp_type == "g": # global parameter
						self.Tabs.par_def_svar[tmp_par[0]].set(tmp_val)
					elif tmp_type == 'b': # board parameter
						for b,v in tmp_lval.items():
							if len(self.Tabs.par_brd_svar[tmp_par[0]][b].get()):	# Overwrite only when values are actually written
								self.Tabs.par_brd_svar[tmp_par[0]][b].set(str(v))
					self.Ctrl.SaveCfgFile()
				elif cmsg[0] == 'd':	# Get the time to attach JanusC for debugging with GUI
					ret = messagebox.showinfo("JanusC", cmsg[1:])
					comm.SendCmd('1')
				elif cmsg[0] == 'p':   # Force print on log tab. This as workaround to not show the missing SrvEnt pop up
					self.Tabs.set_output_log(cmsg[1:], "normal")
			else:	
				time.sleep(0.1)


	# *******************************************************************************
	# Firmware Upgrade
	# *******************************************************************************
	def write_uBlaze_version(self, smsg:str):
		if 'key' in smsg or 'version' in smsg or 'timestamp' in smsg:
			self.Tabs.set_output_log("\nuBlaze version:\n" + smsg + "\n", "normal")


	def notify_succesfull(self, func):	# Not correct, it is just notified
		self.Ctrl.AcqStatus.delete(1.0, END)
		self.Ctrl.AcqStatus.insert(INSERT, func)
		messagebox.showinfo(title="Firmware Upgrade", message=func)
		try: 
			for el in self.CheckBrd:
				el['state'] = NORMAL
		except: pass
		self.FWUpgBrowseButton['state'] = NORMAL
		self.FWUpgUpgradeButton['state'] = NORMAL
		self.CloseUpgradeWin()


	def uCupgrade(self):
		os.startfile("AllInOneJar.jar")


	def RestoreIP(self):
		if (self.Tabs.BrdEnable[1].get() == 0) and ("usb" in self.Tabs.conn_path[0].get()):
			comm.SendCmd('!')
		else:
			messagebox.showwarning(title=None, message="IP restore only with single board connected via USB")


	def FPGAupgrade(self, brd:str = '0'):
		if self.UpgradeWinIsOpen: self.CloseUpgradeWin()
		self.UpgradeWin = Toplevel()
		self.t_fname = "."
		y0 = 0
		x0 = 5
		y_0 = 170

		x_l = 800
		y_0 = 170

		brd_connected = sum(1 for cn in self.Tabs.BrdEnable if cn.get() == 1)
		y_l = y_0 + 20 * (brd_connected-1)

		self.UpgradeWin.geometry("{}x{}+{}+{}".format(x_l, y_l, 150, 400))
		self.UpgradeWin.wm_title("FPGA Firmware Upgrade")
		self.UpgradeWin.grab_set()
		# self.UpgradeWin.attributes('-topmost', 'true')
		self.UpgradeWin.protocol("WM_DELETE_WINDOW", self.CloseUpgradeWin)
		self.UpgradeWinIsOpen = True
		self.FWupg_fname = ''
		self.cBrd = [IntVar() for i in range(brd_connected)]
		if brd_connected == 1: self.cBrd[0].set(1)
		self.UpgStatusLed = [leds.Led(self.UpgradeWin, 18) for i in range(brd_connected)]
		self.Tbrd = StringVar()
		self.Tbrd.set(brd)
		#if os.path.isfile("FWupgfile.txt"):
		#	ff = open("FWupgfile.txt", "r")
		#	self.FWupg_fname = ff.readline()
		#	ff.close()

		# x0 = 5
		y0 = 5
		xl = [30, 60, 115, 240, 380]
		xr = [xx/x_l for xx in xl]
		# Legend: Brd - PID - Brd Model - Current FW Rev - New FW Rev - Upgrade Status
		Label(self.UpgradeWin, text='Brd').place(relx=xr[0], rely=y0/y_l) # Brd
		Label(self.UpgradeWin, text='PID').place(relx=xr[1], rely=y0/y_l) # PID
		Label(self.UpgradeWin, text='Current FW Rev').place(relx=xr[2], rely=y0/y_l) # Current FW
		Label(self.UpgradeWin, text='New FW Rev').place(relx=xr[3], rely=y0/y_l) # New FW
		Label(self.UpgradeWin, text='Upgrade Status').place(relx=xr[4], rely=y0/y_l) # Status
		self.NewFwVersion = []
		self.UpgBrdPercent = []
		self.CheckBrd = []
		y0 += 20 
		for bb in range(brd_connected):
			self.CheckBrd.append(Checkbutton(self.UpgradeWin, variable = self.cBrd[bb], state=NORMAL))
			self.CheckBrd[bb].place(relx=x0/x_l, rely=y0/y_l)
			if self.OpenAndFWupg.get():
				Label(self.UpgradeWin, text='%2s' % (str(self.Tbrd.get()))).place(relx=xr[0], rely=y0/y_l) # Brd
			else: 
				Label(self.UpgradeWin, text='%2s' % (str(bb))).place(relx=xr[0], rely=y0/y_l) # Brd
			Label(self.UpgradeWin, text=self.Tabs.info_pid[bb].cget('text'), anchor="w", relief = 'groove').place(relx=xr[1], rely=(y0+2)/y_l, relwidth=50/x_l, relheight=0.11*(y_0/y_l)) # PID
			Label(self.UpgradeWin, text=self.Tabs.info_fpga_fwrev[bb].cget('text'), anchor="w", relief = 'groove').place(relx=xr[2], rely=(y0+2)/y_l, relwidth=120/x_l, relheight=0.11*(y_0/y_l)) # Current
			self.NewFwVersion.append(Label(self.UpgradeWin, text="", anchor="w", relief = 'groove', fg="#990000")) # New
			self.NewFwVersion[bb].place(relx=xr[3], rely=(y0+2)/y_l, relwidth=120/x_l, relheight=0.11*(y_0/y_l)) 
			self.UpgBrdPercent.append(Label(self.UpgradeWin, text="", anchor="e", justify=RIGHT, relief="groove"))
			self.UpgBrdPercent[bb].place(relx=xr[4], rely=(y0/y_l), relwidth=50/x_l, relheight=0.11*(y_0/y_l))
			self.UpgStatusLed[bb].rel_place(xr[4]+60/x_l, (y0+0)/y_l)
			self.UpgStatusLed[bb].set_color('grey')
			y0 += 20

		y0 += 20
		if self.OpenAndFWupg.get():
			self.CheckBrd[0]['state'] = DISABLED
		Label(self.UpgradeWin, text = "Upgrade File").place(relx=x0/x_l, rely=y0/y_l) # x = x0, y = y0)
		y0 += 20
		self.fnlabel = Text(self.UpgradeWin, bg = 'white') #, height=1, width = 60)
		self.fnlabel.place(relx=x0/x_l, rely=y0/y_l, relheight=0.125*(y_0/y_l), relwidth=0.85) #  x = x0, y = y0)
		self.FWUpgBrowseButton=Button(self.UpgradeWin, text='Browse', command=self.SelectUpgFile)
		self.FWUpgBrowseButton.place(relx=0.87, rely=(y0-4)/y_l, relwidth=0.12)  #  x = x0 + 487, y = y0-4)

		y0 += 25
		self.FWUpgUpgradeButton = Button(self.UpgradeWin, text='Upgrade', command=self.DoUpgrade)
		self.FWUpgUpgradeButton.place(relx=0.87, rely=y0/y_l, relwidth=0.12)  #  x = x0, y = y0)
		self.upg_progress = Progressbar(self.UpgradeWin, orient = HORIZONTAL, length = 474, mode = 'determinate') 
		self.upg_progress.place(relx=x0/x_l, rely=(y0+3)/y_l, relwidth=0.85, relheight=0.125*(y_0/y_l))  #  x = x0 + 75, y = y0+3)

		y0 += 30
		Label(self.UpgradeWin, text = "Messages", relief=GROOVE, justify=CENTER).place(relx=0.87, rely=y0/y_l, relwidth=0.12)  #  x = x0, y = y0)
		self.UpgStat = Label(self.UpgradeWin, text = "", anchor = "w", relief = 'groove')
		self.UpgStat.place(relx=x0/x_l, rely=y0/y_l, relheight=0.125*(y_0/y_l), relwidth=0.85) # x = x0 + 75, y = y0)


	def ChangeBrd(self):
		self.CurrVers.configure(text = "Current Version: " + self.Tabs.info_fpga_fwrev[int(self.Tbrd.get())].cget('text'))


	def DoUpgrade(self):
		if len(self.FWupg_fname) < 3: return

		self.nb2upgrade = [idx for idx in range(len(self.cBrd)) if self.cBrd[idx].get() == 1]
		self.upg_progress['maximum'] = 100*len(self.nb2upgrade)
		if not self.OpenAndFWupg.get():
			for el in self.CheckBrd:
				el['state'] = DISABLED
			if (len(self.nb2upgrade) >= 1):
				comm.SendCmd('U')
				time.sleep(0.01)
				tlist = ",".join(map(str, self.nb2upgrade))
				comm.SendString(tlist)
				time.sleep(0.01)
				comm.SendString(self.FWupg_fname)
			# comm.SendString(self.Tbrd.get())
			# comm.SendString(f'{self.Tbrd.get()}{self.FWupg_fname}')
		else:
			comm.SendString(f'{self.FWupg_fname}')

		self.FWUpgBrowseButton['state'] = DISABLED
		self.FWUpgUpgradeButton['state'] = DISABLED
		self.FWUpgrading.set(1)
		#self.OpenAndFWupg.set(0)


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
					self.info_new_fw.append(str(fw_file.readline()).split()[0].split("'")[1])	# Removed \r\n
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
		#self.NewVers = Label(self.UpgradeWin, text = "New Version: -", fg="#990000")
		#self.NewVers.place(relx=0.62, rely=55/170.)  #  x=350, y=55)
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
				for newFW_label in self.NewFwVersion:
					if self.cBrd[self.NewFwVersion.index(newFW_label)].get() == 1:
						newFW_label['text'] = str(self.info_new_fw[0]) + " (Build = " + str(self.info_new_fw[1]) + ")"
				# self.NewVers['text'] = "New Version: " + str(self.info_new_fw[0]) + " (Build = " + str(self.info_new_fw[1]) + ")"
				# self.NewVers.config(font=("Arial", 10))


	def CloseUpgradeWin(self):
		self.UpgradeWin.destroy()
		self.UpgradeWinIsOpen = False
		if self.FWUpgrading.get()==1:
			comm.SendCmd('q')
			self.OpenAndFWupg.set(0)
			self.FWUpgrading.set(0)

# *****************************************************************************
# Main
# *****************************************************************************
if sh.is_exe():
	sh.cfgfile_path = ""
else:
	sh.cfgfile_path = os.path.join("..", "bin")

pardef_file = os.path.join(sh.cfgfile_path, "param_defs.txt")
sh.CfgFile = os.path.join(sh.cfgfile_path, sh.CfgFile)
sh.GuiModeFile = os.path.join(sh.cfgfile_path, sh.GuiModeFile)
sh.GUIParamOptions = os.path.join(sh.cfgfile_path, sh.GUIParamOptions)
sh.RunVars = os.path.join(sh.cfgfile_path, sh.RunVars)
sh.ParRename = os.path.join(sh.cfgfile_path, sh.ParRename)
sh.PixelMap = os.path.join(sh.cfgfile_path, sh.PixelMap)


sh.Version = cfg.ReadParamDescription(pardef_file, sh.sections, sh.params)
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



