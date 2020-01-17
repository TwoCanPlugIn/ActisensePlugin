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


#ifndef ACTISENSE_NGT1_H
#define ACTISENSE_NGT1_H

#include "actisense_interface.h"

#ifdef __WXMSW__
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <winreg.h>
#endif

#ifdef __LINUX__
#include <termios.h>
#include <unistd.h>
#endif

extern bool actisenseChecksum;

// Implements the EBL NGT1 interface
class ActisenseNGT1 : public ActisenseInterface {

public:
	// Constructor and destructor
	ActisenseNGT1(wxMessageQueue<std::vector<byte>> *messageQueue);
	~ActisenseNGT1(void);
	
	// Open and Close the NGT-1 interface
	// Overridden functions
	int Open(const wxString& optionalPortName);
	int Close(void);
	void Read();
	int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);

protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	// Detect when thread is killed
	int threadIsAlive;
	WCHAR portName[6];
	int ConfigureAdapter(void);
	int ConfigurePort(void);
	
	// Serial port handle
#ifdef __LINUX__
	int serialPortHandle;
#endif

#ifdef __WXMSW__
	HANDLE serialPortHandle;
	int FindDeviceRegistryKey(WCHAR *actisenseRegistryKey, int *actisenseSerialNumber);
	int GetDevicePort(WCHAR *rootKey, WCHAR *friendlyName, WCHAR *portName);
	int GetPortSettings(WCHAR *portName,int *baudRate, int *dataBits, int *stopBits, int *parity);
#endif

};

#endif
