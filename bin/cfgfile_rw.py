# ------------------------------------------------------------------
# Read/write config file and create parameters
# ------------------------------------------------------------------

from tkinter import IntVar, messagebox
import tkinter
import shared as sh

debug = 0
empty_field = []
jobs_check = []
gain_check = []
cfg_file_list = []
numfield = 0
num_empty_field = 0
status = sh.ACQSTATUS_DISCONNECTED

# Basic - Advance GUI mode
guimode = "basic"

# ------------------------------------------------------------------
class param:
	""" config parameter """
	def __init__(self, name, default, section, distr, type):

		self.name = name         # parameter name (same as dictionary key)
		self.default = default   # default value
		self.descr = ""          # param description (used for tooltip and comments)
		self.section = section   # section in the config file and GUI tab
		self.type = type         # param type: 'd' = int decimal, 'h' = int hex, 'f' = float, 's' = string, 'c' = combo, 'b' = boolean, '-' = separator
		self.options = []        # options for combobox
		self.distr = distr       # parameter distribution: 'c' = channel-wise, 'b' = board-wise, 'g' = global only
		self.set_default()

	def add_option(self, option):
		self.options.append(option)

	def add_description(self, descr):
		self.descr = descr

	def set_default(self):
		if (self.distr == 'c'):
			self.value = [['' for c in sh.Channels] for b in sh.Boards]
		elif (self.distr == 'b'):
			self.value = ['' for b in sh.Boards]
		else:
			self.value = self.default


# ------------------------------------------------------------------
# Read param description
# ------------------------------------------------------------------
def ReadParamDescription(filename, sections, params):
	pardef = open(filename, "r")
	blankn = 1
	lastp = ""
	version = "5202"
	for line in pardef:
		if '\"' in line:  # find strings enclosed in "" (don't split them)
			p = []
			p1 = line.split('"')
			p.append(p1[0].strip())  # remove spaces and tabs
			p.append(p1[1])
			p2 = p1[2].split()
			for pp in p2:
				p.append(pp)
		else:
			p = line.split()
		if (len(p) == 0) or (len(p[0]) == 0) or (p[0][0] == '#'):
			continue
		if p[0].upper() == 'VERS':
			vers = int(p[1])
			continue
		if p[0] == "Board_version":
			version = p[1]
		if p[0][0] == '[':
			sections.append(p[0][1:-1])
		elif p[0][0] == '-':
			params[lastp].add_option(p[1])
		else:
			if p[0] == '_BLANK':
				p[0] = p[0] + str(blankn)
				blankn += 1
			params[p[0]] = param(p[0], p[1], sections[-1], p[2], p[3]) # Here were 
			params[p[0]].add_description(line[line.find('#'):][1:].strip())
			lastp = p[0]
	pardef.close()
	return version
	


# ------------------------------------------------------------------
# Read Config File
# ------------------------------------------------------------------	
def ReadConfigFile(params, filename, reloaded=1, our_cfg=0): # DNIN: need to pass show_warning, to enabling the Yes/No messagebox
	# BASIC / ADVANCED version
	# with open("basic.txt", "r") as hidefile:
	# 	hideparam = hidefile.readlines()

	brd, ch, lev = 0, 0, 0

	HV_bias_setting = {}
	HV_bias_setting['default'] = params['HV_Vbias'].default
	HV_bias_setting['value'] = params['HV_Vbias'].value
	# DNIN: is not executed for free writes, can be implemented if needed
	if not our_cfg:
		cfg_file_list.clear()	
		cfg_file_list.clear()
		for key in params.keys():	# Reset all the entries
			if status != sh.ACQSTATUS_DISCONNECTED and key == 'Open':
				continue
			params[key].set_default()
	
		if status == sh.ACQSTATUS_DISCONNECTED:
			for b in range(sh.MaxBrd):	# Reset connection path value
				params['Open'].value[b] = ''

	print("Reading " + filename + "\n")
	try: cf = open(filename, "r")
	except: 	# Check when you load not standard cfg file or cfg file with 'Load' keys
		root = tkinter.Tk()
		root.withdraw()
		messagebox.showinfo("Not found", "The macro file \"{0}\" has not been found. " 
		"Macro reading is skipped and macro load is removed from Janus_Config.txt".format(filename))
		root.destroy()
		return 1

	for line in cf:
		line = line.split('#')[0]  # remove comments
		p = line.split()
		i = 0
		lev = 0
		while i < len(p) and len(p[i]) > 0:
			if '[' in p[i]:
				p2 = p[i].split('[')
				if len(p2) == 2:
					brd = int(p2[1].split(']')[0])
					lev = 1
				elif len(p2) == 3:
					brd = int(p2[1].split(']')[0])
					ch = int(p2[2].split(']')[0])
					lev = 2
				key = p2[0]
			else:
				key = p[i]
			# not overwriting connection if boards are already connected - 
			if key == 'Open' and status != sh.ACQSTATUS_DISCONNECTED:
				break
			if key in params.keys():
				i += 1
				val = p[i]
				i += 1
				while i < len(p):
					val = val + ' ' + p[i]
					i += 1
				if debug == 1: print('Reading ' + key + ' = ' + val)
				if lev == 0:
					# if (key == 'HG_Gain' or key == 'LG_Gain') and int(val,10) >= 64: # DNIN: is this correct and/or useful?
					# 	continue
					try:
						if key == 'JobLastRun' and int(val,10)<int(params['JobFirstRun'].default,10):
							val = str(int(params['JobFirstRun'].default, 10) + 1)
					except:
						continue
					if(key == 'HV_Vbias' and reloaded):	# check if Vbias has changed 
						if HV_bias_setting['default'] != val:
							ret = messagebox.askyesno("Warning", "HV VBias default has changed from "+HV_bias_setting['default']+" to " +val+"\nContinue?")
							#DNIN: I'd like to add sounds, seems it depends from th OS, and windows gave sounds on WRNG, ERR, INFO
							if not ret: 
								continue
					params[key].default = val
				elif lev == 1:
					if params[key].distr == 'b':
						if brd < sh.MaxBrd:
							params[key].value[brd] = val
					else:
						params[key].default = val
				else:
					if brd < sh.MaxBrd and ch < sh.MaxCh:
						params[key].value[brd][ch] = val
				break
			if key == "Load": # DNIN: To show the new params on gui, but then when SAVE you'll save with the new CFG ... 
				# print(p)	  # but then you do not have control on the new par with gui... so HV control from C and communicate? .. left
				# and then discuss with Carlo and Lorenzo ... 
				ret = ReadConfigFile(params, p[1], reloaded, 1) # You should do a new default value ...
				if not ret: cfg_file_list.append(p[1])
				break
			else:
				break

	# DNIN: HV board parameters are read only if they are in cfg file
	# if they are removed the check is not done
	# range(16 - params['Open'].value.count("")) if the loop for only the board connected
	# You may take some actions to check if the brd settings are zero, and collect them 
	# in a single warning msg
	for b in sh.Boards:
		this_val = HV_bias_setting['value'][b]
		val = params['HV_Vbias'].value[b]
		if this_val != val and reloaded:
			if val == "": val = params["HV_Vbias"].default
			if this_val == "": this_val = HV_bias_setting['default']
			ret = messagebox.askyesno("Warning", "HV VBias for board "+str(b)+" has changed from "+this_val+" to " +val+"\nContinue?")
			if not ret:
				params['HV_Vbias'].value[b] = this_val

	cf.close()
	return 0

# ------------------------------------------------------------------
# Write Config File
# ------------------------------------------------------------------	
def manage_empty_entries(pp, p, popup):
	if p == 'JobLastRun' and 'JobFirstRun' not in empty_field:	# DNIN: can it be managed in a different function?
		mmsg = "Job First Run is greater than Job Last Run. Do you want Janus to fix it? (JobLastRun=JobFirstRun+1)\n"
		if popup: ret = messagebox.askyesno(title=None, message=mmsg)
		else: ret = 0
		if ret: 
			jobs_check.append("DO IT")
			return 0
		else:
			empty_field.append(pp[p].name)
			return 1
	else:
		empty_field.append(pp[p].name)
		return 1

def manage_jobrun(pp, popup):
	if 'JobFirstRun' in empty_field:
		return 0
	elif int(pp['JobLastRun'].default,10)<int(pp['JobFirstRun'].default,10):
		mmsg = "Job First Run is greater than Job Last Run. Do you want Janus to fix it? (JobLastRun=JobFirstRun+1)\n"
		if popup: ret = messagebox.askyesno(title=None, message=mmsg)
		else: ret = 0
		if ret: 
			jobs_check.append("DO IT")
			return 1		
		else: return 0
	else: return 0

def WriteConfigFile(sections, params, filename, show_popup): # DNIN: need to pass show_warning, to enabling the Yes/No messagebox
	num_empty_field = 0
	empty_field.clear()
	jobs_check.clear()
	gain_check.clear()

	pf, cf = 35, 20
	print("Writing " + filename + "\n")
	ff = open(filename, "w")
	ff.write("# ******************************************************************************************\n")
	ff.write("# params File generated by Python\n")
	ff.write("# ******************************************************************************************\n")

	# Connection params
	ff.write("# ------------------------------------------------------------------------------------------\n")
	ff.write("# " + params['Open'].section + "\n")
	ff.write("# ------------------------------------------------------------------------------------------\n")
	for b in sh.Boards:
		if (params['Open'].value[b] != ''):
			ff.write("Open[" + str(b) + "] " + str(params['Open'].value[b]) + "\n")
	ff.write("\n\n")

	# Common (and default) params
	ff.write("# ******************************************************************************************\n")
	ff.write("# Common and Default settings\n")
	ff.write("# ******************************************************************************************\n\n")
	for s in sections:
		if (s == params['Open'].section): continue
		# check for empty sections (no parameters)
		cnt = 0
		for p in params.keys():
			if (params[p].section == s) and (params[p].type != '-') and (params[p].type != 'm'): cnt += 1
		if cnt == 0: continue

		ff.write("# ------------------------------------------------------------------------------------------\n")
		ff.write("# " + s + "\n")
		ff.write("# ------------------------------------------------------------------------------------------\n")
		for p in params.keys():
			if (params[p].section == s) and (params[p].type != '-') and (params[p].type != 'm'):
				if params[p].default.split(" ")[0] == "":
					if manage_empty_entries(params, p, show_popup):
						num_empty_field += 1
						continue
					else:
						params[p].default = str(int(params['JobFirstRun'].default, 10) + 1)
				# Manage JobFirst/Last Run
				if p == 'JobLastRun': #  here JobLastRun is not in empty_list or has been already changed
					if manage_jobrun(params, show_popup):
						params[p].default = str(int(params['JobFirstRun'].default, 10) + 1)
				if params[p].type == 'h' and params[p].default.split('x')[1] == "":
					num_empty_field += 1
					empty_field.append(params[p].name)
					continue
				ff.write(p.ljust(pf) + str(params[p].default).ljust(cf) + ' # ' + params[p].descr)
				if debug == 1: print(p.ljust(pf), str(params[p].default).ljust(cf) + ' # ' + params[p].descr)
				if params[p].type == 'c':
					ff.write('. Options: ')
					for opt in params[p].options: 
						ff.write(opt)
						if (opt != params[p].options[-1]): ff.write(', ')
				if params[p].type == 'x':
					ff.write('. Min={}, Max={}'.format(params[p].options[0], params[p].options[1]))
				ff.write("\n")
		ff.write("\n")
	ff.write("\n\n")

	# Individual (channel wise) params
	ff.write("# ******************************************************************************************\n")
	ff.write("# Board and Channel settings (overwrite default settings)\n")
	ff.write("# ******************************************************************************************\n")
	for b in sh.Boards:
		#if (params['Open'].value[b] == ''): continue
		for p in params.keys():
			if p == 'Open': continue
			if (params[p].distr == 'b'):
				if (params[p].type != '-') and (params[p].type != 'm'):
					# if (params[p].value[b] != '' and ):
					if (len(params[p].value[b].split())>0):	# better to avoid writing blank entries (no text or only spaces)
						pn = p + "[" + str(b) + "]"
						ff.write(pn.ljust(pf) + str(params[p].value[b]) + "\t\t\t#\n")
				else:
					ff.write("\n")

		for c in sh.Channels:
			for p in params.keys():
				if (params[p].distr == 'c') and (params[p].type != '-') and (params[p].type != 'm'):
					# if (params[p].value[b][c] != ''):
					if (len(params[p].value[b][c].split())>0):
						pn = p + "[" + str(b) + "]"  + "[" + str(c) + "]"
						ff.write(pn.ljust(pf) + str(params[p].value[b][c]) + "\t\t\t#\n")		
	
	ff.write("\n")

	# External Overwrite Cfg File
	if len(cfg_file_list) > 0:
		ff.write("# ******************************************************************************************\n")
		ff.write("# Additional Configuration Files (might overwrite GUI settings)\n")
		ff.write("# ******************************************************************************************\n")
		for cfile in cfg_file_list:
			ff.write("Load\t\t\t\t\t\t\t" + cfile + "\n")
	
		ff.write("\n")
	
	ff.close()

	# Create a warning message that is managed in JanusPy	
	# DNIN:ATTENTION!!!! These checks do not prevent from writing wrong cfg.
	#      The control must be improved preventing writing JobFirst>JobLast in 
	#	   JanusC. Maybe actions can be taken there
	# if 'JobFirstRun' not in empty_field and 'JobLastRun' not in empty_field:	# DNIN: not anymore useful 
	# 	if (int(params['JobFirstRun'].default,10)>int(params['JobLastRun'].default,10)):
	# 		jobs_check.append("Job First Run is greater than Job Last Run\n")
	
	if 'LG_Gain' not in empty_field:
		if int(params['LG_Gain'].default,10) > 63:
			gain_check.append("LG Gain is out of boundaries\n")

	if 'HG_Gain' not in empty_field:
		if int(params['HG_Gain'].default,10) > 63:
			gain_check.append("HG Gain is out of boundaries\n")
					

	
