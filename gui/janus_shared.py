# ------------------------------------------------------------------
# global variables and data structures
# ------------------------------------------------------------------

from tkinter import *
import os
import sys
import math

SocketLog = False

# -------------------------------------------------------
# Sizes and positions (GUI layout)
# W=width, H=height, X=x-pos, Y=y-pos, Xm=x-margin, Ym=y-margin
# -------------------------------------------------------
# Main Window Width and Height
Win_W = 685   
# Win_H = 720
Win_H = 760    

# Run Control Frame
Win_Ctrl_Y = 50
Win_Ctrl_H = 120
Win_Ctrl_X = 0
Win_Ctrl_W = Win_W

# Status Bar
Win_StatBar_Y = Win_H - 30
Win_StatBar_X = 0
Win_StatBar_H = 30

# Tabs Frame
Win_Tabs_Y = Win_Ctrl_Y + Win_Ctrl_H
Win_Tabs_H = Win_H - Win_Tabs_Y - Win_StatBar_H - 30
Win_Nb_H = Win_Tabs_H
Win_Tabs_X = 0
Win_Tabs_W = Win_W-5

BgCol = 'grey95'  # Default background color
ErCol = 'red'     # Default error color
WrCol = 'yellow'  # Defualt warning color
OkCol = 'green'   # Defualt 'is fine' color

# Versions
Version = "5202"
Release = "5.0.1 - 21/07/2026"

# Ranges
MaxCh = 64
MaxBrd = 32
NumCh = 64
NumBrd = MaxBrd
MaxBrdPerPage = 16
Pages = math.ceil(NumBrd/MaxBrdPerPage)
Channels = range(NumCh)
Boards = range(NumBrd)

# acquisition variables
ImgPath = os.path.join("..", "img", "")
CfgFile = "Janus_Config.txt"
GuiModeFile = "GUI_hide_parameters.txt"
GUIParamOptions = "GUI_param_options.json"
RunVars = "RunVars.txt"
PixelMap = "pixel_map.txt"
JanusCexe = "JanusC.exe"
ParRename = "param_rename.txt"
ActiveCh = 0
ActiveBrd = 0
params = {}  # dictionary of all parameters
sections = []  # list of section

# Macros
LLDumpMsg = os.path.join("..", "macros", "LL_dump_msg.txt")

InverValues = {
	"StartRunMode": {
 		0: "ASYNC",
	    int(0x11): "TDL",
	    int(0x12): "TDL_EXTRUN",
	    int(0x13): "TDL_EXTRUN_EXTCLK",
	    4: "CHAIN_T0",
	    5: "CHAIN_T1",
	    int(0x16): "TDL_GPS"
	},
	"TrefSource": {
	    int(0x1): "T0-IN",
	    int(0x2): "T1-IN",
	    int(0x4): "Q-OR",
	    int(0x8): "T-OR",
	    int(0x10): "PTRG",
	    int(0x40): "TLOGIC"
	},
	"StopRunMode": {
	    int(0x0): "MANUAL",
	    int(0x1): "PRESET_TIME",
	    int(0x2): "PRESET_COUNTS"
	}
}


ACQSTATUS_DISCONNECTED = 0	    # offline
ACQSTATUS_SOCK_CONNECTED = 1	# GUI connected through socket
ACQSTATUS_HW_CONNECTED = 2		# Hardware connected
ACQSTATUS_READY = 3				# ready to start (HW connected, memory allocated and initialized)
ACQSTATUS_RUNNING = 4			# acquisition running (data taking)
ACQSTATUS_RESTARTING = 5		# Restarting acquisition
ACQSTATUS_STAIRCASE = 10		# Running Staircase
ACQSTATUS_RAMPING_HV = 11		# Switching HV ON or OFF
ACQSTATUS_UPGRADING_FW = 12		# Upgrading the FW
ACQSTATUS_HOLDSCAN = 13		    # Running Scan Hold
ACQSTATUS_WAITING_EXTSTART = 14	# Waiting for EXTRUN trigger
ACQSTATUS_RESTARTINGJANUS = 15	# Janus is restarting
ACQSTATUS_ERROR = -1			# Error


cfgfile_path = os.path.join('..', 'bin')

##########################################################
##     FUNCTIONS
##########################################################
def multiplatform_width(width, percent_linux_increase=10):
	if (sys.platform.find('win') < 0):
		return width + round(width*percent_linux_increase/100)
	else:
		return width


# Check if is executable
def is_exe():
    return getattr(sys, 'frozen', False)


# Trace bindings
def trace_bind(var, mode, callback):
	ta_mode = {"w": "write", "r": "read", "u": "unset"}.get(mode, mode)
	t_mode = {"write": "w", "read": "r", "unset": "u"}.get(ta_mode, ta_mode)
	if hasattr(var, "trace_add"):
		return var.trace_add(ta_mode, callback)
	elif hasattr(var, "trace"):
		return var.trace(t_mode, callback)
	else:
		raise AttributeError(
        	"The tkinter variable has neither 'trace' nor 'trace_add' methods."
    	)