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

#ifndef ACTISENSE_EBL_H
#define ACTISENSE_EBL_H

#include "actisense_interface.h"

// Whether to perform checksum calculation
extern bool actisenseChecksum;

// Implements the Actisense EBL Log File Format Reader
class ActisenseEBL : public ActisenseInterface {

public:
	// Constructor and destructor
	ActisenseEBL(wxMessageQueue<std::vector<byte>> *messageQueue);
	~ActisenseEBL(void);

	// Open and Close the log file
	int Open(const wxString& fileName);
	int Close(void);
	void Read();
	int Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload);
	
protected:
	// wxThread overridden functions
	virtual wxThread::ExitCode Entry();
	virtual void OnExit();

private:
	std::string logFileName;
	std::ifstream logFileStream;
};

#endif
