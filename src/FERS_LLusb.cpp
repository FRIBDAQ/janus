// WinUSBConsoleApplication.cpp : Defines the entry point for the console application.
//

#include "FERS_LLusb.h"

#ifdef _WIN32
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#endif

// *********************************************************************************************************
// Functions related to USB driver and low level communication. This part is C++.
// *********************************************************************************************************
using namespace std;

#define TIMEOUT  1000

#ifdef WIN32

												   //Modify this value to match the VID and PID in your USB device descriptor.
												   //Use the formatting: "Vid_xxxx&Pid_xxxx" where xxxx is a 16-bit hexadecimal number.
#define MY_DEVICE_ID  L"Vid_04d8&Pid_0053"		   //Change this number (along with the corresponding VID/PID in the
												   //microcontroller firmware, and in the driver installation .INF
												   //file) before moving the design into production.


		BOOL	USBDEV::CheckIfPresentAndGetUSBDevicePath(DWORD InterfaceIndex)	{

			GUID InterfaceClassGuid = { 0x58D07210, 0x27C1, 0x11DD, 0xBD, 0x0B, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66 };

			HDEVINFO DeviceInfoTable = INVALID_HANDLE_VALUE;
			PSP_DEVICE_INTERFACE_DATA InterfaceDataStructure = new SP_DEVICE_INTERFACE_DATA;
			//		PSP_DEVICE_INTERFACE_DETAIL_DATA DetailedInterfaceDataStructure = new SP_DEVICE_INTERFACE_DETAIL_DATA;	//Global
			SP_DEVINFO_DATA DevInfoData;

			//DWORD InterfaceIndex = 0;
			DWORD StatusLastError = 0;
			DWORD dwRegType;
			DWORD dwRegSize;
			DWORD StructureSize = 0;
			PBYTE PropertyValueBuffer;
			bool MatchFound = false;
			DWORD ErrorStatus;
			BOOL BoolStatus = FALSE;
			DWORD LoopCounter = 0;

			wstring DeviceIDToFind = MY_DEVICE_ID;

			//First populate a list of plugged in devices (by specifying "DIGCF_PRESENT"), which are of the specified class GUID.
			DeviceInfoTable = SetupDiGetClassDevs(&InterfaceClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

			//Now look through the list we just populated.  We are trying to see if any of them match our device.
			while (true)
			{
				InterfaceDataStructure->cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
				if (SetupDiEnumDeviceInterfaces(DeviceInfoTable, NULL, &InterfaceClassGuid, InterfaceIndex, InterfaceDataStructure))
				{
					ErrorStatus = GetLastError();
					if (ErrorStatus == ERROR_NO_MORE_ITEMS)	//Did we reach the end of the list of matching devices in the DeviceInfoTable?
					{	//Cound not find the device.  Must not have been attached.
						SetupDiDestroyDeviceInfoList(DeviceInfoTable);	//Clean up the old structure we no longer need.
						return FALSE;
					}
				}
				else	//Else some other kind of unknown error ocurred...
				{
					ErrorStatus = GetLastError();
					SetupDiDestroyDeviceInfoList(DeviceInfoTable);	//Clean up the old structure we no longer need.
					return FALSE;
				}

				//Now retrieve the hardware ID from the registry.  The hardware ID contains the VID and PID, which we will then
				//check to see if it is the correct device or not.

				//Initialize an appropriate SP_DEVINFO_DATA structure.  We need this structure for SetupDiGetDeviceRegistryProperty().
				DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
				SetupDiEnumDeviceInfo(DeviceInfoTable, InterfaceIndex, &DevInfoData);

				//First query for the size of the hardware ID, so we can know how big a buffer to allocate for the data.
				SetupDiGetDeviceRegistryProperty(DeviceInfoTable, &DevInfoData, SPDRP_HARDWAREID, &dwRegType, NULL, 0, &dwRegSize);

				//Allocate a buffer for the hardware ID.
				PropertyValueBuffer = (BYTE *)malloc(dwRegSize);
				if (PropertyValueBuffer == NULL)	//if null, error, couldn't allocate enough memory
				{	//Can't really recover from this situation, just exit instead.
					SetupDiDestroyDeviceInfoList(DeviceInfoTable);	//Clean up the old structure we no longer need.
					return FALSE;
				}

				//Retrieve the hardware IDs for the current device we are looking at.  PropertyValueBuffer gets filled with a
				//REG_MULTI_SZ (array of null terminated strings).  To find a device, we only care about the very first string in the
				//buffer, which will be the "device ID".  The device ID is a string which contains the VID and PID, in the example
				//format "Vid_04d8&Pid_003f".
				SetupDiGetDeviceRegistryProperty(DeviceInfoTable, &DevInfoData, SPDRP_HARDWAREID, &dwRegType, PropertyValueBuffer, dwRegSize, NULL);

				//Now check if the first string in the hardware ID matches the device ID of my USB device.
#ifdef UNICODE
				wstring *DeviceIDFromRegistry = new wstring((wchar_t*)PropertyValueBuffer);
#else
				string DeviceIDFromRegistry = new string((char *)PropertyValueBuffer);
#endif

				free(PropertyValueBuffer);		//No longer need the PropertyValueBuffer, free the memory to prevent potential memory leaks

												//Convert both strings to lower case.  This makes the code more robust/portable accross OS Versions
				std::transform(DeviceIDFromRegistry->begin(), DeviceIDFromRegistry->end(), DeviceIDFromRegistry->begin(),
					[](unsigned char c) { return std::tolower(c); });
				std::transform(DeviceIDToFind.begin(), DeviceIDToFind.end(), DeviceIDToFind.begin(),
					[](unsigned char c) { return std::tolower(c); });

				//DeviceIDFromRegistry = DeviceIDFromRegistry ->ToLowerInvariant();
				//DeviceIDToFind = DeviceIDToFind->ToLowerInvariant();
				//Now check if the hardware ID we are looking at contains the correct VID/PID
				if (DeviceIDFromRegistry->find(DeviceIDToFind) != std::wstring::npos) {
					MatchFound = true;
				}

				if (MatchFound == true)
				{
					//Device must have been found.  Open WinUSB interface handle now.  In order to do this, we will need the actual device path first.
					//We can get the path by calling SetupDiGetDeviceInterfaceDetail(), however, we have to call this function twice:  The first
					//time to get the size of the required structure/buffer to hold the detailed interface data, then a second time to actually
					//get the structure (after we have allocated enough memory for the structure.)
					DetailedInterfaceDataStructure->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
					//First call populates "StructureSize" with the correct value
					SetupDiGetDeviceInterfaceDetail(DeviceInfoTable, InterfaceDataStructure, NULL, NULL, &StructureSize, NULL);
					DetailedInterfaceDataStructure = (PSP_DEVICE_INTERFACE_DETAIL_DATA)(malloc(StructureSize));		//Allocate enough memory
					if (DetailedInterfaceDataStructure == NULL)	//if null, error, couldn't allocate enough memory
					{	//Can't really recover from this situation, just exit instead.
						SetupDiDestroyDeviceInfoList(DeviceInfoTable);	//Clean up the old structure we no longer need.
						return FALSE;
					}
					DetailedInterfaceDataStructure->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
					//Now call SetupDiGetDeviceInterfaceDetail() a second time to receive the goods.
					SetupDiGetDeviceInterfaceDetail(DeviceInfoTable, InterfaceDataStructure, DetailedInterfaceDataStructure, StructureSize, NULL, NULL);

					//We now have the proper device path, and we can finally open a device handle to the device.
					//WinUSB requires the device handle to be opened with the FILE_FLAG_OVERLAPPED attribute.
					SetupDiDestroyDeviceInfoList(DeviceInfoTable);	//Clean up the old structure we no longer need.
					return TRUE;
				}

				InterfaceIndex++;
				//Keep looping until we either find a device with matching VID and PID, or until we run out of devices to check.
				//However, just in case some unexpected error occurs, keep track of the number of loops executed.
				//If the number of loops exceeds a very large number, exit anyway, to prevent inadvertent infinite looping.
				LoopCounter++;
				if (LoopCounter == 10000000)	//Surely there aren't more than 10 million devices attached to any forseeable PC...
				{
					return FALSE;
				}

			}//end of while(true)
		}

		int USBDEV::open_connection(int index) {
			//Now perform an initial start up check of the device state (attached or not attached), since we would not have
			//received a WM_DEVICECHANGE notification.
			if (CheckIfPresentAndGetUSBDevicePath((DWORD)index)) {	//Check and make sure at least one device with matching VID/PID is attached
				//We now have the proper device path, and we can finally open a device handle to the device.
				//WinUSB requires the device handle to be opened with the FILE_FLAG_OVERLAPPED attribute.
				MyDeviceHandle = CreateFile((DetailedInterfaceDataStructure->DevicePath), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
				DWORD ErrorStatus = GetLastError();
				if (ErrorStatus == ERROR_SUCCESS) {
					//Now get the WinUSB interface handle by calling WinUsb_Initialize() and providing the device handle.
					BOOL BoolStatus = WinUsb_Initialize(MyDeviceHandle, &MyWinUSBInterfaceHandle);
					if (BoolStatus == TRUE)	{
						ULONG timeout = 100; // ms
						WinUsb_SetPipePolicy(MyWinUSBInterfaceHandle, 0x81, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout);
						WinUsb_SetPipePolicy(MyWinUSBInterfaceHandle, 0x82, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout);
						if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO] Device found on USB\n");
					}
				}
				IsOpen = TRUE;
			} else {	// Device must not be connected (or not programmed with correct firmware)
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Device not found on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		int USBDEV::set_service_reg(uint32_t address, uint32_t data) {
			BOOL Status;
			unsigned char OUTBuffer[512];		//BOOTLOADER HACK
			unsigned char INBuffer[512];		//BOOTLOADER HACK
			ULONG LengthTransferred;
			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &data, 4);

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0x10;	//0x80 in byte 0 position is the "Toggle LED" command in the firmware

									//Now send the USB packet data in the OUTBuffer[] to the device firmware.
			Status = WinUsb_WritePipe(MyWinUSBInterfaceHandle, 0x01, &OUTBuffer[0], 1 + 4 + 4 , &LengthTransferred, NULL); //BOOTLOADER HACK
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}

			Status = WinUsb_ReadPipe(MyWinUSBInterfaceHandle, 0x81, &INBuffer[0], 4, &LengthTransferred, NULL);
			/*if (Status == FALSE) {  // CTIN: the ReadPipe returns an error code, but the IP reset works fine. Can we ignore it?
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] read pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}*/
			return 0;
		}

		int USBDEV::write_mem(uint32_t address, uint32_t length,  char *data) {
			BOOL Status;
			unsigned char OUTBuffer[512];
			unsigned char INBuffer[512];
			ULONG LengthTransferred;
			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &length, 4);
			memcpy(&OUTBuffer[9], data, length);

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0x80;	//0x80 in byte 0 position is the "Toggle LED" command in the firmware
			// Now send the USB packet data in the OUTBuffer[] to the device firmware.
			Status = WinUsb_WritePipe(MyWinUSBInterfaceHandle, 0x01, &OUTBuffer[0], 1+4+4+length, &LengthTransferred, NULL);
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			//We successfully sent the request to the firmware, it is now time to
			//try to read the response IN packet from the device.
			Status = WinUsb_ReadPipe(MyWinUSBInterfaceHandle, 0x81, &INBuffer[0], 4, &LengthTransferred, NULL);
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] read pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		int USBDEV::read_mem(uint32_t address, uint32_t length, char *data) {
			volatile unsigned char PushbuttonStateByte = 0xFF;
			unsigned char OUTBuffer[512];
			unsigned char INBuffer[512];
			ULONG BytesTransferred;
			BOOL Status;
			//send a packet back IN to us, with the pushbutton state information
			OUTBuffer[0] = 0x81;	//0x81
			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &length, 4);
			Status = WinUsb_WritePipe(MyWinUSBInterfaceHandle, 0x01, &OUTBuffer[0], 9, &BytesTransferred, NULL);
			// Do error case checking to verify that the packet was successfully sent
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			//We successfully sent the request to the firmware, it is now time to
			//try to read the response IN packet from the device.
			Status = WinUsb_ReadPipe(MyWinUSBInterfaceHandle, 0x81, &INBuffer[0], length, &BytesTransferred, NULL);
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] read pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			memcpy(data, INBuffer, length);
			return 0;
		}

		int USBDEV::write_reg(uint32_t address, uint32_t data) {
			return write_mem(address, 4, (char *) &data);
		}

		int USBDEV::read_reg(uint32_t address, uint32_t *data) {
			return read_mem(address, 4, (char*)data);
		}

		int USBDEV::stream_enable(bool enable) {
			BOOL Status;
			unsigned char OUTBuffer[64];
			ULONG LengthTransferred;

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0xFA;	//0x79 Stream control
			OUTBuffer[1] = enable ? 0x01: 0x00;	//enable

			Status = WinUsb_WritePipe(MyWinUSBInterfaceHandle, 0x01, &OUTBuffer[0], 2, &LengthTransferred, NULL); // DNIN: READING error @ high freq
			if (Status == FALSE) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		int USBDEV::read_pipe(char *buff, int size, int *nb) {
			ULONG BytesTransferred;
			BOOL Status;

			*nb = 0;
			if (size == 0) return 0;
			//Stream read on ep 0x82
			Status = WinUsb_ReadPipe(MyWinUSBInterfaceHandle, 0x82, (UCHAR *)buff, size, &BytesTransferred, NULL);
			//if ((int)BytesTransferred > size) printf("ERROR: ReqSize = %d, NB = %d\n", size, BytesTransferred);
			if (Status == FALSE) // no data
				return 0;
			*nb = (int)BytesTransferred;
			return 0;
		}

#else

	libusb_context* USBDEV::ctx = nullptr;
	unsigned int USBDEV::occurency = 0;


		bool	USBDEV::CheckIfPresentAndGetUSBDevicePath(int InterfaceIndex, libusb_device_handle **dev_handle)	{
			libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
			int r; //for return values
			int i = 0, fersIdx = 0;
			ssize_t cnt; //holding number of devices in list
			if (ctx == nullptr) {
			r = libusb_init(&ctx); //initialize a library session
				if(r < 0) {
							return false;
				}
			}
			cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
			if(cnt < 0) {
				libusb_free_device_list(devs, 1); //free the list, unref the devices in it
				libusb_exit(ctx); //close the session
				return false;
			}
			if (InterfaceIndex >= cnt) {
				libusb_free_device_list(devs, 1); //free the list, unref the devices in it
				libusb_exit(ctx); //close the session
				return false;
			}
			libusb_device_descriptor desc;
			while (i < cnt) {
				r = libusb_get_device_descriptor(devs[i], &desc);
				if (r < 0) {
					libusb_free_device_list(devs, 1); //free the list, unref the devices in it
					libusb_exit(ctx); //close the session
					return false;
				}
				if ((desc.idVendor == 0x04D8) && (desc.idProduct == 0x53)) {
					if (InterfaceIndex != fersIdx) {
						++fersIdx;
						++i;
						continue;
					}
					r = libusb_open((libusb_device *) (devs[i]),dev_handle);
					if (r != 0) {
						libusb_free_device_list(devs, 1); //free the list, unref the devices in it
						if (InterfaceIndex == 0) libusb_exit(ctx); //close the session
						return false;
					}
					if(libusb_kernel_driver_active(*dev_handle, 0) == 1) { //find out if kernel driver is attached
						if(libusb_detach_kernel_driver(*dev_handle, 0) != 0) {
							libusb_close(*dev_handle);
							libusb_free_device_list(devs, 1); //free the list, unref the devices in it
							if (InterfaceIndex == 0) {
								libusb_exit(ctx); //close the session
								ctx = nullptr;
							}
							return false;
						}
					}
					r = libusb_claim_interface(*dev_handle, 0);
					if(r < 0) {
						libusb_close(*dev_handle);
						libusb_free_device_list(devs, 1); //free the list, unref the devices in it
						if (InterfaceIndex == 0) {
							libusb_exit(ctx); //close the session
							ctx = nullptr;
						}
						return false;
					}
					if (InterfaceIndex == fersIdx) {
						occurency++;
						libusb_free_device_list(devs, 1); //free the list, unref the devices in it
						return true;
					}
					//else {
					//	libusb_close(*dev_handle);
					//	fersIdx++;
					//}
				}
				i++;
			}
			libusb_free_device_list(devs, 1); //free the list, unref the devices in it
			if (occurency == 0) {
				libusb_exit(ctx); //close the session
				ctx = nullptr;
			}
			return false;
		}

		int USBDEV::USBSend(unsigned char* outBuffer, unsigned char* inBuffer, int outsize, int insize) {
			int r;
			int actual; //used to find out how many bytes were written

			r = libusb_bulk_transfer(dev_handle, 0x1, outBuffer, outsize, &actual, TIMEOUT);
			if (!(r == 0 && actual == outsize)) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			r = libusb_bulk_transfer(dev_handle, 0x81, inBuffer, insize, &actual, TIMEOUT);
			if (!(r == 0 && actual == insize)) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] read pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		USBDEV::USBDEV() {
		}

		bool IsOpen = 0;

		int USBDEV::open_connection(int index) {
			if (USBDEV::CheckIfPresentAndGetUSBDevicePath(index, &dev_handle)) {	//Check and make sure at least one device with matching VID/PID is attached
				IsOpen = true;
			} else {	// Device must not be connected (or not programmed with correct firmware)
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] Device not found on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		void USBDEV::close_connection() {
			if (IsOpen) {
				occurency--;
			}
			if (occurency == 0)
				if (ctx != nullptr) libusb_exit(ctx); //close the session
			ctx = nullptr;
		}

		int USBDEV::set_service_reg(uint32_t address, uint32_t data) {
			unsigned char OUTBuffer[512];		//BOOTLOADER HACK
			unsigned char INBuffer[512];		//BOOTLOADER HACK
			int actual; //used to find out how many bytes were written

			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &data, 4);

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0x10;	//0x80 in byte 0 position is the "Toggle LED" command in the firmware

			//Now send the USB packet data in the OUTBuffer[] to the device firmware.
			return USBDEV::USBSend(OUTBuffer,INBuffer,1 + 4 + 4,4);
		}

		int USBDEV::write_mem(uint32_t address, uint32_t length,  char *data) {
			unsigned char OUTBuffer[512];
			unsigned char INBuffer[512];
			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &length, 4);
			memcpy(&OUTBuffer[9], data, length);

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0x80;	//0x80 in byte 0 position is the "Toggle LED" command in the firmware
			// Now send the USB packet data in the OUTBuffer[] to the device firmware.
			return USBSend(OUTBuffer,INBuffer,1+4+4+length,4);
		}

		int USBDEV::read_mem(uint32_t address, uint32_t length, char *data) {
			volatile unsigned char PushbuttonStateByte = 0xFF;
			unsigned char OUTBuffer[512];
			unsigned char INBuffer[512];

			//send a packet back IN to us, with the pushbutton state information
			OUTBuffer[0] = 0x81;	//0x81
			memcpy(&OUTBuffer[1], &address, 4);
			memcpy(&OUTBuffer[5], &length, 4);

			if (USBDEV::USBSend(OUTBuffer,INBuffer,1+4+4,length) == 0) {
				memcpy(data, INBuffer, length);
				return 0;
			}
			return FERSLIB_ERR_COMMUNICATION;
		}

		int USBDEV::write_reg(uint32_t address, uint32_t data) {
			return write_mem(address, 4, (char *) &data);
		}

		int USBDEV::read_reg(uint32_t address, uint32_t *data) {
			return read_mem(address, 4, (char*)data);
		}

		int USBDEV::stream_enable(bool enable) {
			unsigned char OUTBuffer[64];
			int r;
			int actual; //used to find out how many bytes were written

			//Prepare a USB OUT data packet to send to the device firmware
			OUTBuffer[0] = 0xFA;	//0x79 Stream control
			OUTBuffer[1] = enable ? 0x01: 0x00;	//enable

			r = libusb_bulk_transfer(dev_handle, 0x1, OUTBuffer, 2, &actual, TIMEOUT);
			if (!(r == 0 && actual == 2)) {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] write pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			return 0;
		}

		int USBDEV::read_pipe(char *buff, int size, int *nb) {
			int r;
			int actual;

			*nb = 0;
			if (size == 0) return 0;
			//Stream read on ep 0x82
			r = libusb_bulk_transfer(dev_handle, 0x82, (unsigned char*) buff, size, &actual, TIMEOUT);
			if ((r != 0) && (r != -7))  {
				if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] read pipe failed on USB\n");
				return FERSLIB_ERR_COMMUNICATION;
			}
			*nb = actual;
			return 0;
		}
#endif


// *********************************************************************************************************
// END OF C++ SECTION
// *********************************************************************************************************

// *********************************************************************************************************
// Global variables
// *********************************************************************************************************
static USBDEV FERS_usb[FERSLIB_MAX_NBRD];
static char *RxBuff[FERSLIB_MAX_NBRD][2] = { NULL };	// Rx data buffers (two "ping-pong" buffers, one write, one read)
static uint32_t RxBuff_rp[FERSLIB_MAX_NBRD] = { 0 };	// read pointer in Rx data buffer
static uint32_t RxBuff_wp[FERSLIB_MAX_NBRD] = { 0 };	// write pointer in Rx data buffer
static int RxB_w[FERSLIB_MAX_NBRD] = { 0 };				// 0 or 1 (which is the buffer being written)
static int RxB_r[FERSLIB_MAX_NBRD] = { 0 };				// 0 or 1 (which is the buffer being read)
static int RxB_Nbytes[FERSLIB_MAX_NBRD][2] = { 0 };		// Number of bytes written in the buffer
static int WaitingForData[FERSLIB_MAX_NBRD] = { 0 };	// data consumer is waiting fro data (data receiver should flush the current buffer)
static int RxStatus[FERSLIB_MAX_NBRD] = { 0 };			// 0=not started, 1=idle (wait for run), 2=running (taking data), 3=closing run (reading data in the pipes)
static int QuitThread[FERSLIB_MAX_NBRD] = { 0 };		// Quit Thread
static f_thread_t ThreadID[FERSLIB_MAX_NBRD];			// RX Thread ID
static mutex_t RxMutex[FERSLIB_MAX_NBRD];				// Mutex for the access to the Rx data buffer and pointers
static FILE *Dump[FERSLIB_MAX_NBRD] = { NULL };			// low level data dump files (for debug)
static uint8_t ReadData_Init[FERSLIB_MAX_NBRD] = { 0 }; // Re-init read pointers after run stop

#define USB_BLK_SIZE  (512 * 8)					// Max size of one USB packet (4k)
#define RX_BUFF_SIZE  (16 * USB_BLK_SIZE)		// Size of the local Rx buffer


// *********************************************************************************************************
// R/W Memory and Registers
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Write a memory block to the FERS board
// Inputs:		bindex = FERS index
//				address = mem address (1st location)
//				data = reg data
//				size = num of bytes being written
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_WriteMem(int bindex, uint32_t address, char *data, uint16_t size) {
	return FERS_usb[bindex].write_mem(address, size, data);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Read a memory block from the FERS board 
// Inputs:		bindex = FERS index
//				address = mem address (1st location)
//				size = num of bytes being written
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_ReadMem(int bindex, uint32_t address, char *data, uint16_t size) {
	return FERS_usb[bindex].read_mem(address, size, data);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Write a register of the FERS board 
// Inputs:		bindex = FERS index
//				address = reg address 
//				data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_WriteRegister(int bindex, uint32_t address, uint32_t data) {
	return FERS_usb[bindex].write_reg(address, data);
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Read a register of the FERS board 
// Inputs:		bindex = FERS index
//				address = reg address 
// Outputs:		data = reg data
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_ReadRegister(int bindex, uint32_t address, uint32_t *data) {
	return FERS_usb[bindex].read_reg(address, data);
}


// *********************************************************************************************************
// Raw data readout
// *********************************************************************************************************
// Thread that keeps reading data from the data socket (at least until the Rx buffer gets full)
static void *usb_data_receiver(void *params) {
	int bindex = *(int *)params;
	int nbreq, nbrx, nbfree, stream=0, ret, nodata=0, empty=0;
	int FlushBuffer = 0;
	int WrReady = 1;
	char *wpnt;
	uint64_t ct, pt, tstart, tstop, tdata=0;

	if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[INFO][BRD %02d] Data receiver thread is started\n", bindex);
	if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "RX thread started\n");
	lock(RxMutex[bindex]);
	RxStatus[bindex] = RXSTATUS_IDLE;
	unlock(RxMutex[bindex]);

	ct = get_time();
	pt = ct;
	while(1) {
		ct = get_time();
		lock(RxMutex[bindex]);
		if (QuitThread[bindex]) break;
		if (RxStatus[bindex] == RXSTATUS_IDLE) {
			if (stream == 0) {
				FERS_usb[bindex].stream_enable(true);
				stream = 1;
			}
			if ((FERS_ReadoutStatus == ROSTATUS_RUNNING) && empty) {  // start of run
				// Clear Buffers
				ReadData_Init[bindex] = 1;
				RxBuff_rp[bindex] = 0;
				RxBuff_wp[bindex] = 0;
				RxB_r[bindex] = 0;
				RxB_w[bindex] = 0;
				WaitingForData[bindex] = 0;
				WrReady = 1;
				FlushBuffer = 0;
				for(int i=0; i<2; i++)
					RxB_Nbytes[bindex][i] = 0;
				tstart = ct;
				tdata = ct;
				if (DebugLogs & DBLOG_LL_MSGDUMP) {
					char st[100];
					time_t ss;
					time(&ss);
					strcpy(st, asctime(gmtime(&ss)));
					st[strlen(st) - 1] = 0;
					fprintf(Dump[bindex], "\nRUN_STARTED at %s\n", st);
					fflush(Dump[bindex]);
				}
				RxStatus[bindex] = RXSTATUS_RUNNING;
				lock(FERS_RoMutex);
				FERS_RunningCnt++;
				unlock(FERS_RoMutex);
			} else {
				unlock(RxMutex[bindex]);
				// make "dummy" reads while not running to prevent the USB FIFO to get full and become insensitive to any reg access (regs and data pass through the same pipe)
				if (!empty) {
					ret = FERS_usb[bindex].read_pipe(RxBuff[bindex][0], USB_BLK_SIZE, &nbrx);
					if (nbrx == 0) empty = 1;
					if (!empty && (DebugLogs & DBLOG_LL_MSGDUMP)) fprintf(Dump[bindex], "Reading old data...\n");
				}
				Sleep(10);
				continue;
			}
		}

		if ((RxStatus[bindex] == RXSTATUS_RUNNING) && (FERS_ReadoutStatus != ROSTATUS_RUNNING)) {  // stop of run 
			tstop = ct;
			RxStatus[bindex] = RXSTATUS_EMPTYING;
			if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Board Stopped. Emptying data (T=%.3f)\n", 0.001 * (tstop - tstart));
		}

		if (RxStatus[bindex] == RXSTATUS_EMPTYING) {
			// stop RX for one of these conditions:
			//  - flush command 
			//  - there is no data for more than NODATA_TIMEOUT
			//  - STOP_TIMEOUT after the stop to the boards
			if ((FERS_ReadoutStatus == ROSTATUS_FLUSHING) || ((ct - tdata) > NODATA_TIMEOUT) || ((ct - tstop) > STOP_TIMEOUT)) {  
				RxStatus[bindex] = RXSTATUS_IDLE;
				lock(FERS_RoMutex);
				if (FERS_RunningCnt > 0) FERS_RunningCnt--;
				unlock(FERS_RoMutex);
				if (DebugLogs & DBLOG_LL_MSGDUMP) fprintf(Dump[bindex], "Run stopped (T=%.3f)\n", 0.001 * (ct - tstart));
				empty = 0;
				unlock(RxMutex[bindex]);
				continue;
			}
		}
		if (!WrReady) {  // end of current buff reached => switch to the other buffer (if available for writing)
			if (RxB_Nbytes[bindex][RxB_w[bindex]] > 0) {  // the buffer is not empty (being used for reading) => retry later
				unlock(RxMutex[bindex]);
				Sleep(10);
				continue;
			}
			WrReady = 1;
			if (!stream) {
				FERS_usb[bindex].stream_enable(true);
				stream = 1;
			}
		} 

		wpnt = RxBuff[bindex][RxB_w[bindex]] + RxBuff_wp[bindex];
		nbfree = RX_BUFF_SIZE - RxBuff_wp[bindex];  // bytes free in the buffer
		nbreq = min(nbfree, USB_BLK_SIZE);

		unlock(RxMutex[bindex]);
		ret = FERS_usb[bindex].read_pipe(wpnt, nbreq, &nbrx);
		if (ret) {
			lock(RxMutex[bindex]);
			RxStatus[bindex] = -1;
			if (ENABLE_FERSLIB_LOGMSG) FERS_LibMsg("[ERROR] usb read pipe failed in data receiver thread (board %d)\n", bindex);
			break;
		}
		if (nbrx > 0) tdata = ct;
		if ( (nbfree < 4*USB_BLK_SIZE) && (stream == 1) ) FlushBuffer = 1;  // switch buffer if it has no space for at least 4 blocks

		lock(RxMutex[bindex]);
		RxBuff_wp[bindex] += nbrx;
		if ((ct - pt) > 10) {  // every 10 ms, check if the data consumer is waiting for data or if the thread has to quit
			if (QuitThread[bindex]) break;  
			if (WaitingForData[bindex] && (RxBuff_wp[bindex] > 0)) FlushBuffer = 1;
			pt = ct;
		}

		if ((RxBuff_wp[bindex] == RX_BUFF_SIZE) || FlushBuffer) {  // the current buffer is full or must be flushed
			RxB_Nbytes[bindex][RxB_w[bindex]] = RxBuff_wp[bindex];
			RxB_w[bindex] ^= 1;  // switch to the other buffer
			RxBuff_wp[bindex] = 0;
			FERS_usb[bindex].stream_enable(false);
			stream = 0;
			WrReady = 0;
			FlushBuffer = 0;
		}
		// Dump data to log file (for debugging)
		if ((DebugLogs & DBLOG_LL_DATADUMP) && (nbrx > 0) && (Dump[bindex] != NULL)) {
			//fprintf(Dump[bindex], "Wpage=%d Wpnt=%d\n", RxB_w[bindex], RxBuff_wp[bindex]);
			for(int i=0; i<nbrx; i+=4) {
				uint32_t *d32 = (uint32_t *)(wpnt + i);
				fprintf(Dump[bindex], "%08X\n", *d32);
			}
			fflush(Dump[bindex]);
		}
		unlock(RxMutex[bindex]);
	}
	unlock(RxMutex[bindex]);
	return NULL;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Copy a data block from RxBuff of the FERS board to the user buffer 
// Inputs:		bindex = board index
//				buff = user data buffer to fill
//				maxsize = max num of bytes being transferred
//				nb = num of bytes actually transferred
// Return:		0=No Data, 1=Good Data 2=Not Running, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_ReadData(int bindex, char *buff, int maxsize, int *nb) {
	char *rpnt;
	static int RdReady[FERSLIB_MAX_NBRD] = { 0 };
	static int Nbr[FERSLIB_MAX_NBRD] = { 0 };
	//static FILE *dd = NULL;

	*nb = 0;
	if (trylock(RxMutex[bindex]) != 0) return 0;
	if (ReadData_Init[bindex]) {
		RdReady[bindex] = 0;
		Nbr[bindex] = 0;
		ReadData_Init[bindex] = 0;
		unlock(RxMutex[bindex]);
		return 2;
	} 

	if (RxStatus[bindex] != RXSTATUS_RUNNING) {
		unlock(RxMutex[bindex]);
		return 2;
	}
	if (!RdReady[bindex]) {
		if (RxB_Nbytes[bindex][RxB_r[bindex]] == 0) {  // The buffer is empty => assert "WaitingForData" and return 0 bytes to the caller
			WaitingForData[bindex] = 1;
			unlock(RxMutex[bindex]);
			return 0;
		}
		RdReady[bindex] = 1;
		Nbr[bindex] = RxB_Nbytes[bindex][RxB_r[bindex]];  // Get the num of bytes available for reading in the buffer
		WaitingForData[bindex] = 0;
	}

	rpnt = RxBuff[bindex][RxB_r[bindex]] + RxBuff_rp[bindex];
	*nb = Nbr[bindex] - RxBuff_rp[bindex];  // num of bytes currently available for reading in RxBuff
	if (*nb > maxsize) *nb = maxsize;
	if (*nb > 0) {
		memcpy(buff, rpnt, *nb);
		RxBuff_rp[bindex] += *nb;
	}
	if (RxBuff_rp[bindex] == Nbr[bindex]) {  // end of current buff reached => switch to other buffer 
		RxB_Nbytes[bindex][RxB_r[bindex]] = 0;  
		RxB_r[bindex] ^= 1;
		RxBuff_rp[bindex] = 0;
		RdReady[bindex] = 0;
	}
	unlock(RxMutex[bindex]);
	return 1;
}

// *********************************************************************************************************
// Open/Close
// *********************************************************************************************************
// --------------------------------------------------------------------------------------------------------- 
// Description: Open the direct connection to the FERS board through the USB interface. 
//				After the connection the function allocates the memory buffers starts the thread  
//				that receives the data
// Inputs:		PID = board PID
//				bindex = board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_OpenDevice(int PID, int bindex) {
	int ret, started, i;
	f_thread_t threadID;
	static int OpenAllDevices = 1, Ndev = 0;

	USBDEV FERS_usb_temp;

	if (PID >= 10000) { // search for the board with the given PID between all the connected boards
		uint32_t d32;
		// 1st call => open all USB devices
		if (OpenAllDevices) {
			for(i=0; i<FERSLIB_MAX_NBRD; i++) { // If you use WDcfg.NumBrd ?
				ret = FERS_usb[i].open_connection(i);
				if (ret != 0) break;
			}
			Ndev = i;
			OpenAllDevices = 0;
		}

		// DNIN: here at least!
		for (i = 0; i < Ndev; i++) {
			if (!FERS_usb[i].IsOpen)
				return FERSLIB_ERR_COMMUNICATION;  // no further connected board is found
			FERS_usb[i].read_reg(a_pid, &d32);
			if (d32 == PID) break;
		} 
		if (i == Ndev) return FERSLIB_ERR_COMMUNICATION;  // no board found with the given PID
		// Swap indexes (i = board with wanted PID, bindex = wanted board index) 
		memcpy(&FERS_usb_temp, &FERS_usb[bindex], sizeof(USBDEV));
		memcpy(&FERS_usb[bindex], &FERS_usb[i], sizeof(USBDEV));
		memcpy(&FERS_usb[i], &FERS_usb_temp, sizeof(USBDEV));

		//if (!FERS_usb[bindex].IsOpen)
		//	return FERSLIB_ERR_COMMUNICATION;

	} else {  // open using consecutive index instead of PID
		ret = FERS_usb[bindex].open_connection(PID);
		if (ret != 0) return FERSLIB_ERR_COMMUNICATION;
	}

	for(int i=0; i<2; i++) {
		RxBuff[bindex][i] = (char *)malloc(RX_BUFF_SIZE);
		FERS_TotalAllocatedMem += RX_BUFF_SIZE;
	}
	initmutex(RxMutex[bindex]);
	QuitThread[bindex] = 0;
	if ((DebugLogs & DBLOG_LL_DATADUMP) || (DebugLogs & DBLOG_LL_MSGDUMP)) {
		char filename[200];
		sprintf(filename, "ll_log_%d.txt", bindex);
		Dump[bindex] = fopen(filename, "w");
	}
	FERS_usb[bindex].stream_enable(true);
	thread_create(usb_data_receiver, &bindex, &threadID);
	started = 0;
	while(!started) {
		lock(RxMutex[bindex]);
		if (RxStatus[bindex] != 0) started = 1;
		unlock(RxMutex[bindex]);
	}
	ret = LLusb_WriteRegister(bindex, a_commands, CMD_ACQ_STOP);	// stop acquisition (in case it was still running)
	ret |= LLusb_WriteRegister(bindex, a_commands, CMD_CLEAR);		// clear data in the FPGA FIFOs
	return 0;
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Close the connection to the FERS board and free buffers
// Inputs:		bindex: board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_CloseDevice(int bindex) {
	lock(RxMutex[bindex]);
	QuitThread[bindex] = 1;
	unlock(RxMutex[bindex]);
	FERS_usb[bindex].stream_enable(false);
#ifdef WIN32
	if (RxMutex[bindex] != NULL) destroymutex(RxMutex[bindex]);
#else
	FERS_usb[bindex].close_connection();
#endif
	for(int i=0; i<2; i++)
		if (RxBuff[bindex][i] != NULL) free(RxBuff[bindex][i]);
	if (Dump[bindex] != NULL) fclose(Dump[bindex]);
	return 0;
}


// --------------------------------------------------------------------------------------------------------- 
// Description: Enable/Disable data streaming
// Inputs:		bindex: board index
//				Enable: true/false
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_StreamEnable(int bindex, bool Enable) {
	return FERS_usb[bindex].stream_enable(Enable);
}

// --------------------------------------------------------------------------------------------------------- 
// Description: Reset IP address to default (192.168.50.3)
// Inputs:		bindex: board index
// Return:		0=OK, negative number = error code
// --------------------------------------------------------------------------------------------------------- 
int LLusb_Reset_IPaddress(int bindex) {
	return(FERS_usb[bindex].set_service_reg(1, 0));
}

