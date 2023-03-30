# ------------------------------------------------------------------
# global variables and data structures
# ------------------------------------------------------------------

from tkinter import *
import sys

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
Win_Tabs_X = 0
Win_Tabs_W = Win_W-5

BgCol = 'grey95'  # default background color
ErCol = 'red'     # default error color
WrCol = 'yellow'  # defualt warning color
OkCol = 'green'   # defualt 'is fine' color

# Versions
Version = "5202"
Release = "3.0.3 - 29/09/2022"

# Ranges
MaxCh = 64
MaxBrd = 16
NumCh = 64
NumBrd = 16
Channels = range(NumCh)
Boards = range(NumBrd)

# acquisition variables
ImgPath = "..\\img\\"
CfgFile = "Janus_Config.txt"
GuiModeFile = "GUI_hide_parameters.txt"
ActiveCh = 0
ActiveBrd = 0
params = {}  # dictionary of all parameters
sections = []  # list of section

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
ACQSTATUS_ERROR = -1			# Error

