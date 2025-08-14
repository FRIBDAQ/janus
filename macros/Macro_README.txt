------------------------------------------------------------------------------
  CAEN SpA
  Via Vetraia, 11 - 55049 - Viareggio ITALY
  +390594388398 - www.caen.it
------------------------------------------------------------------------------

------------------------------------------------------------------------------

Janus 5202 macros README
    
------------------------------------------------------------------------------

Macros folder content:

- leds_off.txt:  			Turn off the boards rear panel LEDs. In case of naked board, the two LEDs on the upper side remains on. 
							This macro is useful in case a dark environment is required.


- TimeReset.txt: 			Enables the second Time Stamp in Spectroscopy, Spect+Timing and Counting Acquisition modes, 
							as to permit the timestamp reset via a Tref. This functionality is needed to correlated acquired events with an external time reference.


- FERSlib_msg.txt:			A dump of the FERS library messages is saved in the bin/FERSlib_msg.txt file. Especially useful for debugging purposes.


- Dump_RawData.txt:			A dump of the raw data from each board, with no processing performed, is saved in the bin/ll_log_BRD.txt file, 
							where BRD is the board number. Especially useful for debugging purposes.


- Dump_ParamAndConfig,txt	A dump of the FERSlib parameters and of the board registers are saved in bin folder. Expecially useful for debugging purposes.


- Set_FiberDelayAdjust.txt	Adjusts for the delay due to unequal fiber lengths between boards. See the macro for usage details."