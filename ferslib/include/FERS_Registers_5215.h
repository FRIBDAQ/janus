/******************************************************************************
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

#ifndef _REGISTERS_5215_H
#define _REGISTERS_5215_H				// Protect against multiple inclusion

// *****************************************************************
// Virtual Registers of the concentrator
// *****************************************************************
// Register virtual address map
#define VR_IO_STANDARD				0	// IO standard (NIM or TTL). Applies to all I/Os
#define VR_IO_FA_DIR				1	// Direction of FA
#define VR_IO_FB_DIR				2
#define VR_IO_RA_DIR				3
#define VR_IO_RB_DIR				4
#define VR_IO_FA_FN					5	// Function of FA when used as output
#define VR_IO_FB_FN					6
#define VR_IO_RA_FN					7
#define VR_IO_RB_FN					8
#define VR_IO_FOUT_FN				9	// Funtion of the eigth F_OUT
#define VR_IO_VETO_FN				10	// Veto source
#define VR_IO_FIN_MASK				11	// F_IN enable mask
#define VR_IO_FOUT_MASK				12	// F_OUT enable mask
#define VR_IO_REG_VALUE				13	// Register controlled I/O
#define VR_IO_TP_PERIOD				14	// Internal Test Pulse period
#define VR_IO_TP_WIDTH				15	// Internal Test Pulse width
#define VR_IO_CLK_SOURCE			16	// Clock source
#define VR_IO_SYNC_OUT_A_FN			17	// Function of SYNC_OUT (RJ45)
#define VR_IO_SYNC_OUT_B_FN			18
#define VR_IO_SYNC_OUT_C_FN			19
#define VR_IO_MASTER_SALVE			20	// Board is master or slave
#define VR_IO_SYNC_PULSE_WIDTH		21	// Sync pulse width
#define VR_IO_SYNC_SEND				22	// Send a sync pulse - Registro da scrivere 
#define VR_SYNC_DELAY				23	// node to node delay in the daisy chain (used to fine tune the sync skew)
#define VR_FIN_MAJORITY				24
#define VR_IO_PPS_SOURCE			25
//enum class IO_PPSSOURCE{
#define VR_PPS_DISABLE              5
#define VR_PPS_GPS                  0
#define VR_PPS_SYNC_A               1
#define VR_PPS_SYNC_B               2
#define VR_PPS_SYNC_C               3
#define VR_PPS_RB                   4
//};

#define VR_IO_PLL_STATUS			26
#define VR_IO_LED_TEST_1			27
#define VR_IO_LED_TEST_2			28
#define VR_DMA_FLAGS_INFO			29  // Almost Full level of the INFO buffer (event descriptor table)
#define VR_DMA_FLAGS_DATA			30  // Almost Full level of the DATA buffer (event payload)
#define VR_ENABLED_LINKS			31  // Enable mask of the optical links
#define VR_IO_DEBUG					32  // Digital Probes (allows some internal signals to be routed to the FA/FB outputs)
#define VR_IO_CMD                   33  // Configure the io triggering the command 
                                        // bit 5 => 0: disable, 1: enable
                                        // bit 4 => 0: rising edge, 1: falling edge
                                        // bit 3 ... 0 => 
                                        // 0 : F_A
                                        // 1 : F_B
                                        // 2 : R_A
                                        // 3 : R_B
                                        // 4 : F_IN(0)
                                        // 5 : SYNC_A
                                        // 6 : SYNC_B
                                        // 7 : SYNC_C
                                        // 8 : GPS_PPS
                                        // 9 : OR_F_IN
                                        // A : AND_F_IN
                                        // B : MAJ_F_IN
                                        // C : TEST_PULSE
                                        // D : SYNC
                                        // E : GPS_PULSE
                                        // F : reserved


// In order to send a delayed command send command with DCMD and not FCMD
// with the same syntax as FCMD


#define VR_SET_TARGET_EPOCH         34
//#define VR_GET_EPOCH                35
//#define VR_GPS_STATUS               36
#define VR_GET_EPOCH_FPGA   35
#define VR_GET_EPOCH_GPS    36
#define VR_GET_PPS_FPGA     37
#define VR_GPS_STATUS       38
#define VR_GPS_LAT          39
#define VR_GPS_LONG         40
#define VR_GPS_ALT          41
#define VR_GPS_SPEED        42

// Register values - Registro 18
#define VR_IO_SYNCSOURCE_ZERO		0
#define VR_IO_SYNCSOURCE_SW_PULSE	1
#define VR_IO_SYNCSOURCE_SW_REG		2
#define VR_IO_SYNCSOURCE_FA			3
#define VR_IO_SYNCSOURCE_FB			4
#define VR_IO_SYNCSOURCE_RA			5
#define VR_IO_SYNCSOURCE_RB			6
#define VR_IO_SYNCSOURCE_GPS_START	7
#define VR_IO_SYNCSOURCE_CLK_REF	8
#define VR_IO_SYNCSOURCE_SYNCB		9
#define VR_IO_SYNCSOURCE_DELAY		10
// Questi sono i valori che si possono selezionare e attaccare al SyncInB


#define VR_IO_BOARD_MODE_MASTER		0
#define VR_IO_BOARD_MODE_SLAVE		1

#define VR_IO_STANDARD_IO_NIM		0
#define VR_IO_STANDARD_IO_TTL		1

#define VR_IO_DIRECTION_OUT			0
#define VR_IO_DIRECTION_IN_R50		1
#define VR_IO_DIRECTION_IN_HIZ		2

#define VR_IO_FUNCTION_REGISTER		0
#define VR_IO_FUNCTION_LOGIC_OR		1
#define VR_IO_FUNCTION_LOGIC_AND	2
#define VR_IO_FUNCTION_MAJORITY		3
#define VR_IO_FUNCTION_TEST_PULSE	4
#define VR_IO_FUNCTION_SYNC			5
#define VR_IO_FUNCTION_FA_IN		6
#define VR_IO_FUNCTION_FB_IN		7
#define VR_IO_FUNCTION_RA_IN		8
#define VR_IO_FUNCTION_RB_IN		9
#define VR_IO_FUNCTION_ZERO			15

#define VR_IO_CLKSOURCE_INTERNAL	5   //0
#define VR_IO_CLKSOURCE_LEMO		0   //1 ClockIN 10 MHz
#define VR_IO_CLKSOURCE_SYNC		1   //2
#define VR_IO_CLKSOURCE_RA			4   //3

// Digital Probes (for debug)
#define VR_DPROBE_MON_TX_SYNC_SHORT         0x01  //!< sending periodic synch (short)
#define VR_DPROBE_MON_TX_SYNC_ALIGN         0x02  //!< link alignment in progress
#define VR_DPROBE_MON_TX_SYNC_LONG          0x03  //!< sending periodic synch (long)
#define VR_DPROBE_MON_TX_TIMING_SEND        0x04  //!< sending Timing *T0
#define VR_DPROBE_MON_TX_NORMAL_SEND        0x05  //!< FSM low level TX is in the state ready to transmit standard packets (reg/cmd)
#define VR_DPROBE_MON_TX_TLAST_SEND         0x06  //!< FSM low level TX is transmetting last word
#define VR_DPROBE_MON_TX_DATA_SEND          0x07  //!< FSM low level TX is transmetting a standard packets (reg/cmd)
#define VR_DPROBE_MON_TX_SYNC_PENDING       0x08  //!< Periodic synch is pending
#define VR_DPROBE_MON_TX_FIFOFULL           0x09  //!< TX fifo full, commands and re access will be lost
#define VR_DPROBE_MON_TX_TVALID             0x0A  //!< tvalid fifo tx
#define VR_DPROBE_MON_TX_TREADY             0x0B  //!< tready fifo tx
#define VR_DPROBE_SC_BUSY                   0x10  //!< register cmd interface busy
#define VR_DPROBE_SC_DATA_WR                0x11  //!< data to write 
#define VR_DPROBE_SC_INT_RD                 0x12  //!< read operation request
#define VR_DPROBE_SC_INT_WR                 0x13  //!< write operation request
#define VR_DPROBE_SC_COMMIT                 0x14  //!< packet commit 
#define VR_DPROBE_SC_ACK                    0x15  //!< ack to commit request
#define VR_DPROBE_SC_NACK                   0x16  //!< nack to commit request
#define VR_DPROBE_STR_SOP                   0x17  //!< start incoming data packet 
#define VR_DPROBE_STR_EOP                   0x18  //!< end incoming data packet 
#define VR_DPROBE_STR_EOB                   0x19  //!< end of train
#define VR_DPROBE_STR_DATA_VALID            0x20  //!< data valid (read)
#define VR_DPROBE_CMD_BUSY                  0x21  //!< command interface busy
#define VR_DPROBE_CMD_COMMIT                0x22  //!< sending command from CPU 
#define VR_DPROBE_ENUMERATEBOARD            0x23  //!< request of enumeration
#define VR_DPROBE_ENUMERATION_COMPLETED     0x24  //!< enumeration completed
#define VR_DPROBE_SEND_T0                   0x25  //!< send T0
#define VR_DPROBE_G_REINIT_LINK             0x26  //!< reinitialize link
#define VR_DPROBE_READOUT_READY             0x27  //!< readout ready
#define VR_DPROBE_MON_AXSM_WRITEREG         0x30  //!< FSM protocol is sending a reg read 
#define VR_DPROBE_MON_AXSM_READREG          0x31  //!< FSM protocol is sending a reg write
#define VR_DPROBE_MON_AXSM_SENDCMD          0x32  //!< FSM protocol is sending a command 
#define VR_DPROBE_MON_AXSM_SENDTOKEN        0x33  //!< FSM protocol is sending a token (start of train)
#define VR_DPROBE_MON_AXSM_PENDING_TOKEN    0x34  //!< a token is pending (= is circulating)
#define VR_DPROBE_MON_AXSM_PENDING_REG      0x35  //!< pending send register

// VR_IO_CMD (address 33)
#define VR_IO_CMD_F_A			0x0
#define VR_IO_CMD_F_B			0x1
#define VR_IO_CMD_R_A			0x2
#define VR_IO_CMD_R_B			0x3
#define VR_IO_CMD_F_IN  		0x4
#define VR_IO_CMD_SYNC_A		0x5
#define VR_IO_CMD_SYNC_B		0x6
#define VR_IO_CMD_SYNC_C		0x7
#define VR_IO_CMD_GPS_PPS		0x8
#define VR_IO_CMD_OR_F_IN		0x9
#define VR_IO_CMD_AND_F_IN		0xA
#define VR_IO_CMD_MAJ_F_IN		0xB
#define VR_IO_CMD_TEST_PULSE	0xC
#define VR_IO_CMD_SYNC			0xF  // LA NUOVA VERSIONE DI FW PREVEDE CHE SI USI F PER QUESTO SCOPO. 
#define VR_IO_CMD_GPS_PULSE		0xE

// VR_GPS_STATUS (address 36)

// Registers to read concentrator K and CRC errors 
#define VR_ERROR_K_CH0          43
#define VR_ERROR_K_CH1          44
#define VR_ERROR_K_CH2          45
#define VR_ERROR_K_CH3          46
#define VR_ERROR_K_CH4          47
#define VR_ERROR_K_CH5          48
#define VR_ERROR_K_CH6          49
#define VR_ERROR_K_CH7          50

#define VR_CRC_ERROR_REGS_CH0   51
#define VR_CRC_ERROR_REGS_CH1   52
#define VR_CRC_ERROR_REGS_CH2   53
#define VR_CRC_ERROR_REGS_CH3   54
#define VR_CRC_ERROR_REGS_CH4   55
#define VR_CRC_ERROR_REGS_CH5   56
#define VR_CRC_ERROR_REGS_CH6   57
#define VR_CRC_ERROR_REGS_CH7   58

#define VR_CRC_ERROR_DATA_CH0   59
#define VR_CRC_ERROR_DATA_CH1   60
#define VR_CRC_ERROR_DATA_CH2   61
#define VR_CRC_ERROR_DATA_CH3   62
#define VR_CRC_ERROR_DATA_CH4   63
#define VR_CRC_ERROR_DATA_CH5   64
#define VR_CRC_ERROR_DATA_CH6   65
#define VR_CRC_ERROR_DATA_CH7   66

#define VR_PROTOCOL_ERROR_REGS_CH0  67
#define VR_PROTOCOL_ERROR_REGS_CH1  68
#define VR_PROTOCOL_ERROR_REGS_CH2  69
#define VR_PROTOCOL_ERROR_REGS_CH3  70
#define VR_PROTOCOL_ERROR_REGS_CH4  71
#define VR_PROTOCOL_ERROR_REGS_CH5  72
#define VR_PROTOCOL_ERROR_REGS_CH6  73
#define VR_PROTOCOL_ERROR_REGS_CH7  74

// TDlink error codes and status bits
#define CNC_STATUS_INVALID_CHAIN						1		//!<
#define CNC_STATUS_INVALID_BOARD						2		//!<
#define CNC_STATUS_INVALID_FIRMWARE						3		//!<
#define CNC_STATUS_CHAIN_NOT_INITIALIZED				4		//!<
#define CNC_STATUS_UNABLE_TO_MAP_MEMORY_TO_USER_SPACE	5		//!<
#define CNC_STATUS_UNABLE_TO_OPEN_DEV_MEM				6		//!<
#define CNC_STATUS_UNABLE_TO_ALLOCATE_USER_MEMORY		7		//!<
#define CNC_STATUS_UANBLE_TO_ALLOCATE_KERNEL_MEMORY		8		//!<
#define CNC_STATUS_MEMORY_NOT_MAPPED					9		//!<
#define CNC_STATUS_ADDRESS_OUT_OF_MEMORY				10		//!<
#define CNC_STATUS_NO_MORE_SPACE						11		//!<
#define CNC_STATUS_EMPTY								12		//!<
#define CNC_STATUS_FULL									13		//!<
#define CNC_STATUS_INVALID_SETTINGS						14		//!<
#define CNC_STATUS_NO_MORE_SPACE_IN_THE_LIST			15		//!<
#define CNC_STATUS_LIST_ALMOST_FULL						16		//!<
#define CNC_STATUS_DMA_FAILED							17		//!<
#define CNC_STATUS_CHAIN_DOWN							18		//!<
#define CNC_STATUS_INVALID_IO_MODE						19		//!<
#define CNC_STATUS_FPGA_IO_ERROR						20		//!<
#define CNC_STATUS_IIC_IO_ERROR							21		//!<
#define CNC_STATUS_INVALID_REGISTER						22		//!<
#define CNC_STATUS_LINK_ERROR							23		//!<
#define CNC_STATUS_UNABLE_TO_SET_LINK_PROP_DELAY		24		//!<
#define CNC_STATUS_CHAIN_DISABLED						25		//!<
#define CNC_STATUS_TIMEOUT								26		//!<
#define CNC_STATUS_SFP_LOS								28		//!<
#define CNC_STATUS_SFP_FAULT							29		//!<

#endif
