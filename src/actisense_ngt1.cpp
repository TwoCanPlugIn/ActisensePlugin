// Copyright(C) 2018-2020 by Steven Adler
//
// This file is part of Actisense plugin for OpenCPN.
//
// Actisense plugin for OpenCPN is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Actisense plugin for OpenCPN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Actisense plugin for OpenCPN. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered trademark of the National Marine Electronics Association
// Actisense® is a registered trademark of Active Research Limited

// Project: Actisense Plugin
// Description: Actisense NGT-1 plugin for OpenCPN
// Unit: Actisense NGT-1 - Reads data from Actisense NGT-1 Adapter
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include <actisense_ngt1.h>

#ifdef __WXMSW__

// The NGT-1 device uses the FTDI Serial Controller
// therefore a registry entry with the matching Vendor & Product ID will be found under
const WCHAR *CONST_FTDIBUS_KEY = L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS";

// From the Actisense NGT-1 device .inf file
// Actisense NGT-1 Product & Vendor ID's and USB Serial Device Class GUID
const WCHAR *CONST_VENDOR_ID = L"VID_0403";
const WCHAR *CONST_PRODUCT_ID = L"PID_D9AA";
const WCHAR *CONST_CLASS_GUID = L"{4d36e978-e325-11ce-bfc1-08002be10318}";
// Under a subkey, will be another SubKey 'Device Parameters' which contains 
// the device's 'friendly name' and the serial port name


// Serial port configuration can be found in the following registry key.
// Each port (appended with ':') will have a string value 
// defining baud,parity,data,stop
const WCHAR *CONST_SERIAL_PORT_CONFIG = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

#endif

ActisenseNGT1::ActisenseNGT1(wxMessageQueue<std::vector<byte>> *messageQueue) : ActisenseInterface(messageQueue) {
}

ActisenseNGT1::~ActisenseNGT1() {
}

int ActisenseNGT1::Open(const wxString& optionalPortName) {
	int result;
	
	// If no optionalPortName is provided, search the registry to automagically detect the port
	if (optionalPortName.empty()) {
		result = ConfigurePort();
	}
	else {
		portName = optionalPortName;
		result = TWOCAN_RESULT_SUCCESS;
	}
	
	if (result != TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("Actisense NGT-1, Error detecting port (%lu)"), result);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error detecting port (%lu)\n"), result);
		return result;
	}
	
	wxLogMessage(_T("Actisense NGT-1, Attempting to open %s"), portName);
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Attempting to open %s\n"), portName);
			
#ifdef __LINUX__
	// Open the serial device
	serialPortHandle = open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
	
	if (serialPortHandle == -1) {
		wxLogMessage(_T("Actisense NGT-1, Error Opening %s"),portName.c_str());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error opening %s\n"),portName.c_str());
		return 	SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_SERIALPORT);
	}
    
    // Ensure it is a tty device
    if (!isatty(serialPortHandle)) { 
		wxLogMessage(_T("Actisense NGT-1, %s is not a TTY device\n"),portName.c_str());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, %s is not a TTY device\n"),portName.c_str());
		return SET_ERROR(TWOCAN_RESULT_FATAL , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
    }
    
    // Configure the serial port settings
	struct termios serialPortSettings;
	
	// Get the current configuration of the serial interface
	if (tcgetattr(serialPortHandle, &serialPortSettings) == -1) { 
		wxLogMessage(_T("Actisense NGT-1, Error getting serial port configuration"));
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error getting serial port configuration\n"));
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// Input flags, ignore parity
    serialPortSettings.c_iflag |= IGNPAR;

	// Output flags
	serialPortSettings.c_oflag = 0;

	// Local Mode flags
	//serialPortSettings.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	serialPortSettings.c_lflag = 0;

	// Control Mode flags
	// 8 bit character size, ignore modem control lines, enable receiver
	serialPortSettings.c_cflag |= CS8 | CLOCAL | CREAD;

	// Special character flags
	// BUG BUG Determine the optimum number of characters, inter-character timer off    
	serialPortSettings.c_cc[VMIN]  = 64;
	serialPortSettings.c_cc[VTIME] = 0;

	// Set baud rate
	if ((cfsetispeed(&serialPortSettings, B115200) == -1) || (cfsetospeed(&serialPortSettings, B115200) == -1)) {
		wxLogMessage(_T("Actisense NGT-1, Error setting baud  rate"));
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error setting baud rate\n"));
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// Apply the configuration
	if (tcsetattr(serialPortHandle, TCSAFLUSH, &serialPortSettings) == -1) { 
		wxLogMessage(_T("Actisense NGT-1, Error applying tty device settings"));
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error applying tty device settings\n"));
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
	
    
#endif

#ifdef __WXMSW__

	serialPortHandle = CreateFile(portName.wc_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	
	if (serialPortHandle == INVALID_HANDLE_VALUE) {
		wxLogMessage(_T("Actisense NGT-1, Error opening port %s (%lu)"), portName, GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error opening port %s (%lu)\n"), portName, GetLastError());
		return 	SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_SERIALPORT);
	}
	
	// Serial Port Settings
	DCB serialPortSettings = { 0 };
	serialPortSettings.DCBlength = sizeof(serialPortSettings);

	if (!GetCommState(serialPortHandle, &serialPortSettings)) {
		wxLogMessage(_T("Actisense NGT-1, Error GetCommState (%lu)"),GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error GetCommState (%lu)\n"),GetLastError());
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

	// BUG BUG Could use the settings from the registry
	// However 115200 is the factory default baud rate
	serialPortSettings.BaudRate = 115200; // baudRate;
	serialPortSettings.ByteSize = 8; // dataBits;
	serialPortSettings.StopBits = 0; // 0 is one stop bit;
	serialPortSettings.Parity = 0; //0 is no parity;

	if (!SetCommState(serialPortHandle, &serialPortSettings)) {
		wxLogMessage(_T("Actisense NGT-1, Error SetCommState (%lu)"),GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error SetCommState (%lu)\n"),GetLastError());
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
	
	COMMTIMEOUTS serialPortTimeouts = { 0 };
	serialPortTimeouts.ReadIntervalTimeout = MAXDWORD; //set to MAXDWORD for non-blocking
	serialPortTimeouts.ReadTotalTimeoutConstant = 0;
	serialPortTimeouts.ReadTotalTimeoutMultiplier = 0;
	serialPortTimeouts.WriteTotalTimeoutConstant = 10;
	serialPortTimeouts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(serialPortHandle, &serialPortTimeouts)) {
		wxLogMessage(_T("Actisense NGT-1, Error SetCommTimeOuts (%lu)"),GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error SetCommTimeOuts (%lu)\n"),GetLastError());
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
	
#endif

#ifdef __WXOSX__
	// ToDo
#endif

	wxLogMessage(_T("Actisense NGT-1, Successfully opened %s"), portName);
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Successfully opened %s\n"), portName);

	// Send the NGT-1 Initialization Sequence
	return ConfigureAdapter();
	
}

int ActisenseNGT1::Close(void) {

#ifdef __LINUX__
	close(serialPortHandle);
	// == 0 indicates success
#endif	
	
#ifdef __WXMSW__
	CloseHandle(serialPortHandle);
	// == 0 indicates error
#endif

#ifdef __WXOSX__
	// ToDo
#endif
	wxLogMessage(_T("Actisense NGT-1, Closed serial port"));
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Closed serial port\n"));
	
	return TWOCAN_RESULT_SUCCESS;
}

// Reads data from the port and assembles into Actisense messages
void ActisenseNGT1::Read() {
	// used to construct valid Actisense messages
	std::vector<byte> assemblyBuffer;
	// read 128 at a time ??
	std::vector<byte> readBuffer(128,0);
	// used to iterate through the readBuffer
#ifdef __LINUX__
	int bytesRead = 0;
#endif
#ifdef __WXMSW__
	DWORD bytesRead = 0;
#endif
	// if we've found an ASCII Control Char DLE or ESC
	bool isEscaped = false;
	// if we've found an ASCII Control Char STX (preceded by a DLE)
	// or a BEMSTART (preceded by an ESC)
	bool msgStart = false;
	// if we've found an ASCII Control Char ETX (also preceded by a DLE)
	// or a BEMEND (preceded by an ESC)
	bool msgComplete = false;
	
	while (!TestDestroy()) {
	
#ifdef __LINUX__
		bytesRead = read(serialPortHandle, (char *) &readBuffer[0], readBuffer.size());
	#endif
	
#ifdef __WXMSW__
		if (ReadFile(serialPortHandle, readBuffer.data(), readBuffer.size(), &bytesRead, NULL)) {
#endif

#ifdef __WXOSX__
			// ToDo
#endif

			if (bytesRead > 0) {
				
				wxMessageOutputDebug().Printf(_T("Bytes read (%lu)\n"),bytesRead);

				for (int i = 0; i < (int)bytesRead; i++) {

					unsigned char ch = readBuffer.at(i);

					// if last character was DLE or ESC
					if (isEscaped) {
						isEscaped = false;

						// Message Start
						if ((ch == STX) && (!msgStart)) {
							msgStart = true;
							msgComplete = false;
							assemblyBuffer.clear();
						}

						// Message End
						else if ((ch == ETX) && (msgStart)) {
							msgComplete = true;
							msgStart = false;
						}

						// Actisense Binary Encoded Message Start
						else if ((ch == BEMSTART) && (!msgStart)) {
							msgStart = true;
							msgComplete = false;
							assemblyBuffer.clear();
						}

						// Actisense Binary Encoded Message End
						else if ((ch == BEMEND) && (msgStart)) {
							msgComplete = true;
							msgStart = false;
						}

						// Escaped DLE
						else if ((ch == DLE) && (msgStart)) {
							assemblyBuffer.push_back(ch);
						}

						// Escaped ESC
						else if ((ch == ESC) && (msgStart)) {
							assemblyBuffer.push_back(ch);
						}

						else {
							// Can't have an escaped normal char
							msgComplete = false;
							msgStart = false;
							assemblyBuffer.clear();
						}
					}
					// Previous character was not a DLE or ESC
					else {
						if ((ch == DLE) || (ch == ESC)) {
							isEscaped = true;
						}
						else if (msgStart) {
							// a normal character
							assemblyBuffer.push_back(ch);
						}
					}

					if (msgComplete) {
						// we have a complete frame, process it

						// the checksum character at the end of the message
						// ensures that the sum of all characters modulo 256 equals 0
						int checksum = 0;
						if (actisenseChecksum) {
							for (auto it : assemblyBuffer) {
								checksum += it;
							}
						}
						else {
							// Don't perform the checksum calculation, so just "dupe" it
							checksum = 256;
						}

						if ((checksum % 256) == 0) {
							if (assemblyBuffer.at(0) == N2K_RX_CMD) {
								
								// debug hex dump of received message
								int j = 0;
								wxString debugString;
								for (int i = 0; i < assemblyBuffer.size(); i++) {
									debugString.Append(wxString::Format("%02X ",assemblyBuffer.at(i)));
									j++;
									if ((j % 8) == 0) {
										wxMessageOutputDebug().Printf("%s\n",debugString.c_str());
										j = 0;
										debugString.Clear();
									}
								}
								
								// we have received a valid frame so send it
								deviceQueue->Post(assemblyBuffer);
								
							}
						}


						// Reset everything for next message
						assemblyBuffer.clear();
						msgStart = false;
						msgComplete = false;
						isEscaped = false;

					}	// end if msgComplete

				} // end for

			} // end if bytes read > 0

#ifdef __WXMSW__
		} // end if ReadFile
#endif
							
	} // end while 
		
}

// BUG BUG Write not yet implemented
int ActisenseNGT1::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	return TWOCAN_RESULT_SUCCESS;
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode ActisenseNGT1::Entry() {
	// Merely loops continuously waiting for frames to be received by the Actisense adapter
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void ActisenseNGT1::OnExit() {
	wxLogMessage(_T("Actisense NGT-1, Read thread exiting."));
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Read thread exiting.\n"));
	// Nothing to do ??
}


// Send init sequence to NGT-1
int ActisenseNGT1::ConfigureAdapter(void) {
	// Specific NGT-1 Initialization Sequence - refer to Canboat
	// Note command, length, checksum and control chars have been pre-calculated
	std::vector<byte>writeBuffer {DLE,STX,NGT_TX_CMD,0x03,0x11,0x02,0x00,0x49, DLE,ETX};
		
#ifdef __WXMSW__
	DWORD bytesWritten = 0;
	
	WriteFile(serialPortHandle, writeBuffer.data(), writeBuffer.size(), &bytesWritten, NULL);
	
	if (bytesWritten == 0) {
		int err = GetLastError();
		wxLogMessage(_T("Actisense NGT-1, Error sending NGT-1 Reset Sequence: %d"),err);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error sending NGT-1 Reset Sequence: %d\n"),err);
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}

#endif

#ifdef __LINUX__

	int bytesWritten = 0;

	bytesWritten = write(serialPortHandle, writeBuffer.data(),writeBuffer.size());
	
	if (bytesWritten == -1) {
		return SET_ERROR(TWOCAN_RESULT_ERROR , TWOCAN_SOURCE_DRIVER , TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
#endif

#ifdef __WXOSX__
	// ToDo 
#endif

	wxLogMessage(_T("Actisense NGT-1, Sent NGT-1 Reset Sequence"));
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Sent NGT-1 Reset Sequence\n"));
	return TWOCAN_RESULT_SUCCESS;
}


int ActisenseNGT1::ConfigurePort(void) {
int result;
int serialNumber;

#ifdef __LINUX__	
// BUG BUG Need to find device
 portName = "/dev/ttyUSB0";
 result = TWOCAN_RESULT_SUCCESS;
#endif

#ifdef __WXOSX__
	// ToDo
	portName = "/dev/ttyUSB0";
	result = TWOCAN_RESULT_SUCCESS;
#endif

#ifdef __WXMSW__
	
	WCHAR registryPathName[MAX_PATH];
	WCHAR friendlyName[MAX_PATH];
	
	result = FindDeviceRegistryKey(registryPathName, &serialNumber);
	
	if (result != TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("Actisense NGT-1, Error searching registry: %d"),result);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error searching registry: %d\n"),result);
		return result;
	}
	
	result = GetDevicePort(registryPathName, friendlyName, portName);
	
	if (result != TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("Actisense NGT-1, Error retrieving port name: %d"),result);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Error retrieving port name: %d\n"),result);
		return result;
	}
#endif
	
	wxLogMessage(_T("Actisense NGT-1, Connected to: %s"),portName);
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Connected to: %s\n"),portName);
	return result;

}


#ifdef __WXMSW__
// Iterate through the registry until we find a key with matching vendor id & product id
int ActisenseNGT1::FindDeviceRegistryKey(WCHAR *actisenseRegistryKey, int *actisenseSerialNumber) {
	HKEY registryKey;
	DWORD result;

	BOOL foundKey = FALSE;
	
	// BUG BUG DEBUG
	wxLogMessage(_T("Actisense NGT - 1, Opening registry key %s"), CONST_FTDIBUS_KEY);
	wxMessageOutputDebug().Printf(_T("Actisense NGT - 1, Opening registry key %s\n"), CONST_FTDIBUS_KEY);

	// Get a handle to the registry
	// Require KEY_READ and ENUMERATE_KEYS rights 
	// to read the number of subkeys and to iterate through each one
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_FTDIBUS_KEY, 0, KEY_ENUMERATE_SUB_KEYS | KEY_READ, &registryKey);

	// if the key isn't found, assume the Actisense NGT-1 device has never been installed
	if (result != ERROR_SUCCESS) {
		
		// BUG BUG DEBUG
		wxLogMessage(_T("Actisense NGT-1, RegOpenKey Error: %lu (%d)"), result, GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, RegOpenKey Error: %lu (%d)\n"), result, GetLastError());
		
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}

	// BUG BUG DEBUG
	wxLogMessage(_T("Found registry key %s"), CONST_FTDIBUS_KEY);
	wxMessageOutputDebug().Printf(_T("Found registry key %s\n"), CONST_FTDIBUS_KEY);

	DWORD countOfKeys;
	DWORD maxKeyLength;
	DWORD countOfValues;
	DWORD maxValueLength;

	// Determine how many FTDI bus registry entries are present
	result = RegQueryInfoKey(registryKey, NULL, NULL, NULL, &countOfKeys, &maxKeyLength, NULL, &countOfValues, &maxValueLength, NULL, NULL, NULL);

	if (result != ERROR_SUCCESS) {
		
		// BUG BUG DEBUG
		wxLogMessage(_T("Actisense NGT-1, RegQueryKey Error: %lu (%d)"), result, GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, RegQueryKey Error: %lu (%d)\n"), result, GetLastError());
		
		RegCloseKey(registryKey);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}
	
	// BUG BUG DEBUG
	wxLogMessage(_T("Actisense NGT-1, Number of Sub Keys: %d"), countOfKeys);
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Number of Sub Keys: %d\n"), countOfKeys);
	
	// Iterate through each key until we find one that matches the Vendor & Product ID's
	for (int i = 0; i < (int)countOfKeys; i++) {
		WCHAR *keyName = (WCHAR *)malloc(1024);
		DWORD keyLength = 1024; 

		result = RegEnumKeyEx(registryKey, i, keyName, &keyLength, NULL, NULL, NULL, NULL);

		if (result != ERROR_SUCCESS) {
			// BUG BUG DEBUG
			wxLogMessage(_T("Actisense NGT-1, RegEnumKey Error: %lu (%d)"), result, GetLastError());
			wxMessageOutputDebug().Printf(_T("Actisense NGT-1, RegEnumKey Error: %lu (%d)\n"), result, GetLastError());
			break;
		}

		// Compare key name with vendor id, product id
		
		// BUG BUG DEBUG
		// wprintf(L"Sub Key %d: Name: %s Length:%d\n", i, keyName, keyLength);

		// A valid entry looks like VID_0403+PID_D9AA+251A8A
		// where the NGT-1 device serial number is appended to the Vendor ID & Product Id
		// Note the serial number itself is stored as Hex and appended with the character 'A'

		// Make a copy of the key name as wcstok is destructive, null terminate it
		WCHAR *actisenseKeyName = (WCHAR *)malloc(keyLength + 1);
		wcscpy(actisenseKeyName,keyName);
		
		WCHAR *buffer;
		WCHAR *vendorID = wcstok_s(keyName, L"+", &buffer);
		WCHAR *productID = wcstok_s(NULL, L"+", &buffer);
		WCHAR *rawSerialNumber = wcstok_s(NULL, L"+", &buffer);

		// BUG BUG DEBUG
		wxLogMessage(_T("Actisense NGT-1, Vendor ID: %s"), vendorID);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Vendor ID: %s\n"), vendorID);
		wxLogMessage(_T("Actisense NGT-1, Product ID: %s"), productID);
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Product ID: %s\n"), productID);

			
		// Check if we have a matching Vendor ID & Product ID
		if (((wcscmp(CONST_VENDOR_ID, vendorID)) == 0) && ((wcscmp(CONST_PRODUCT_ID, productID)) == 0)) {
			
			// Determine if there is a NGT-1 Device Serial Number
			if (rawSerialNumber != NULL) {
				WCHAR serialNumber[32] = { L"\0" }; // initialize as null terminated
				
				// BUG BUG DEBUG
				// wprintf(L"%s (%lu)\n", rawSerialNumber, wcslen(rawSerialNumber));
				
				// Remove the trailing 'A' character
				wcsncpy(serialNumber, rawSerialNumber, wcslen(rawSerialNumber) - 1);
				
				// BUG BUG DEBUG
				wxLogMessage(_T("Actisense NGT-1, Serial No: %lu"), wcstol(serialNumber, NULL, 16));
				wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Serial No: %lu\n"), wcstol(serialNumber, NULL, 16));
				// Convert serial number from hex to decimal and save it
				*actisenseSerialNumber = wcstol(serialNumber, NULL, 16);
			}
			
			// BUG BUG Does not handle case of multiple adapters
			foundKey = TRUE;
			
			// BUG BUG DEBUG
			wxLogMessage(_T("Actisense NGT-1, Found Actisense NGT-1 key: %s"), actisenseKeyName);
			wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Found Actisense NGT-1 key: %s\n"), actisenseKeyName);
			
			// Save the full key path to return to the caller
			
			// BUG BUG DEBUG
			wxLogMessage(_T("Actisense NGT-1, Saving: %s Length: %d"), actisenseKeyName, wcslen(actisenseKeyName));
			wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Saving: %s Length: %d\n"), actisenseKeyName, wcslen(actisenseKeyName));
			
			wsprintf(actisenseRegistryKey,L"%s\\%s",CONST_FTDIBUS_KEY, actisenseKeyName);
			break;

		} // end if matching key
		free(actisenseKeyName);
		free(keyName);
	} // end for each key
	
	
	RegCloseKey(registryKey);
	
	if (foundKey) {
		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}
	
}


// Get the associated COM port name
int ActisenseNGT1::GetDevicePort(WCHAR *rootKey, WCHAR *friendlyName, wxString portName) {
	HKEY registryKey;
	DWORD result;

	WCHAR *subKeyName = (WCHAR *)malloc(1024);
	DWORD subKeyLength = 1024 * (sizeof(subKeyName) / sizeof(*subKeyName));

	WCHAR *keyValue = (WCHAR *)malloc(1024);
	DWORD keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));
	DWORD keyType;

	BOOL foundKey = FALSE;

	// BUG BUG Should check keyName and portName are not NULL
	// BUG BUG DEBUG
	wxLogMessage(_T("Actisense NGT-1, Opening registry key: %s"), rootKey);
	wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Opening registry key: %s\n"), rootKey);

	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, rootKey, 0, KEY_ENUMERATE_SUB_KEYS | KEY_READ, &registryKey);

	if (result != ERROR_SUCCESS) {
		// BUG BUG DEBUG
		wxLogMessage(_T("Actisense NGT-1, RegOpenKey Error: %lu (%d)"), result, GetLastError());
		wxMessageOutputDebug().Printf(_T("Actisense NGT-1, RegOpenKey Error: %lu (%d)\n"), result, GetLastError());
		
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
	// iterate the sub keys until we find a sub key that contains the matching classGUID
	for (int i = 0;; i++) {

		result = RegEnumKeyEx(registryKey, i, subKeyName, &subKeyLength, NULL, NULL, NULL, NULL);

		if (result != ERROR_SUCCESS) {
			break;
		}

		// BUG BUG DEBUG
		wxMessageOutputDebug().Printf(_T("Registry Sub Key: %s (%d)\n", subKeyName, subKeyLength);

		// Note Service matches the SubKey 'FTSER2K' for detecting if device is present
		
		// Check if we have the correct registry key by comparing with the GUID from the .inf file
		result = RegGetValue(registryKey, subKeyName, L"ClassGUID", RRF_RT_ANY, &keyType, keyValue, &keyLength);

		if (result == ERROR_SUCCESS) {
		
			// BUG BUG DEBUG
			wxLogMessage(_T("Actisense NGT-1, ClassGUID Value: %s  Length: %d Type: %d"), keyValue, keyLength, keyType);
			wxMessageOutputDebug().Printf(_T("Actisense NGT-1, ClassGUID Value: %s  Length: %d Type: %d\n"), keyValue, keyLength, keyType);
			
			// BUG BUG Do I need to handle ANSI variants of the GUID
			// WCHAR unicodeValue[1024];
			// MultiByteToWideChar(CP_OEMCP, 0, (LPCCH)keyValue, -1, (LPWSTR)unicodeValue, (int)keyLength);
			if (wcscmp(keyValue, CONST_CLASS_GUID) == 0) {

				// BUG BUG Debug
				wxLogMessage(_T("Actisense NGT-1, Found Matching ClassGUID"));
				wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Found Matching ClassGUID\n"));

				// Get the friendly name for the NGT-1 device
				keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));
				result = RegGetValue(registryKey, subKeyName, L"FriendlyName", RRF_RT_ANY, &keyType, keyValue, &keyLength);

				if (result == ERROR_SUCCESS) {
					
					// BUG BUG DEBUG
					wxLogMessage(_T("Actisense NGT-1, FriendlyName Value: %s  Length: %d Type: %d"), keyValue, keyLength, keyType);
					wxMessageOutputDebug().Printf(_T("Actisense NGT-1, FriendlyName Value: %s  Length: %d Type: %d\n"), keyValue, keyLength, keyType);
					
					// Save the friendly name
					wcsncpy(friendlyName, keyValue, keyLength);
				} // end friendly name

				// Get the Serial Port Name associated with this device
				wcscat(subKeyName, L"\\Device Parameters");
				
				keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));;
				result = RegGetValue(registryKey, subKeyName, L"PortName", RRF_RT_ANY, &keyType, keyValue, &keyLength);
				
				if (result == ERROR_SUCCESS) {
					// BUG BUG DEBUG
					wxLogMessage(_T("Actisense NGT-1, Port Name: %s  Length: %d Type: %d"), keyValue , keyLength, keyType);
					wxMessageOutputDebug().Printf(_T("Actisense NGT-1, Port Name: %s  Length: %d Type: %d\n"), keyValue , keyLength, keyType);
					
					// Save the serial port name
					// Append the ':'
					portName = wxString::Format(_T("%s:"),keyValue);
					wxLogMessage(_T("Actisense NGT-1, COM Port: %s"), portName);
					wxMessageOutputDebug().Printf(_T("Actisense NGT-1, COM Port: %s"), portName);
					
					foundKey = TRUE;
				} // end port name

			} // end found matching ClassGUID

		} // end get ClassGUID

	} // end for each sub key
	free(keyValue);
	free(subKeyName);
	RegCloseKey(registryKey);
	
	if (foundKey) {
		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
}

// Retrieve the serial port settings from the registry
// Unused as we use the factory default NGT-1 settings, 115200 baud, 8 data, 1 stop, no parity
int ActisenseNGT1::GetPortSettings(wxString portName,int *baudRate, int *dataBits, int *stopBits, int *parity) {
	HKEY registryKey;
	DWORD result;

	WCHAR *keyValue = (WCHAR *)malloc(1024);
	DWORD keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));
	DWORD keyType;
	
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_SERIAL_PORT_CONFIG, 0, KEY_READ, &registryKey);

	if (result != ERROR_SUCCESS) {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_CONFIGURE_PORT);
	}

	result = RegGetValue(registryKey, L"Ports", portName.wc_str(), RRF_RT_ANY, &keyType, keyValue, &keyLength);

	if (result != ERROR_SUCCESS) {
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_CONFIGURE_PORT);
	}

	// BUG BUG DEBUG
	wxMessageOutputDebug().Printf(_T("Port Name: %s Value: %s  Length: %d Type: %d\n"), portName, keyValue, keyLength, keyType);

	WCHAR *buffer;

	*baudRate = wcstol(wcstok_s(keyValue, L",", &buffer), NULL, 10);
	*parity = wcstok_s(NULL, L",", &buffer)[0]; //Just want the first character
	*dataBits = _wtoi(wcstok_s(NULL, L",", &buffer));
	*stopBits = _wtoi(wcstok_s(NULL, L",", &buffer));

	free(keyValue);
	RegCloseKey(registryKey);
	return TWOCAN_RESULT_SUCCESS;
}
#endif



