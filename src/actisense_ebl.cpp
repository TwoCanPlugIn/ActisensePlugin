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
// Unit: ActisenseEBL - Reads Actisense EBL Log Files
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include <actisense_ebl.h>

ActisenseEBL::ActisenseEBL(wxMessageQueue<std::vector<byte>> *messageQueue) : ActisenseInterface(messageQueue) {
}

ActisenseEBL::~ActisenseEBL() {
}

int ActisenseEBL::Open(const wxString& fileName) {
	// Open the log file
	
	// BUG BUG Could determine the path separator & code appropriately....
	
#ifdef __WXMSW__
	logFileName = wxString::Format("%s\\%s", wxStandardPaths::Get().GetDocumentsDir(),fileName.c_str());
#endif

	
#ifdef __LINUX__
	logFileName = wxString::Format("%s/%s", wxStandardPaths::Get().GetDocumentsDir(),fileName.c_str());	
#endif

#ifdef __WXOSX__
	logFileName = wxString::Format("%s/%s", wxStandardPaths::Get().GetDocumentsDir(),fileName.c_str());	
#endif

	wxLogMessage(_T("Actisense EBL, Attempting to open log file: %s"), logFileName.c_str());
	
	logFileStream.open(logFileName.c_str(), std::ifstream::in | std::ifstream::binary);	
	

	if (logFileStream.fail()) {
		wxLogMessage(_T("Actisense EBL, Failed to open file: %s"), logFileName.c_str());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER,TWOCAN_ERROR_FILE_NOT_FOUND);
	}
	else {
		wxLogMessage(_T("Actisense EBL, successfully opened file: %s"), logFileName.c_str());
		return TWOCAN_RESULT_SUCCESS;
	}
}

int ActisenseEBL::Close(void) {
	if (logFileStream.is_open()) {
		logFileStream.close();
	}
	return TWOCAN_RESULT_SUCCESS;
}

void ActisenseEBL::Read() {
	// used to construct valid Actisense messages
	std::vector<byte> assemblyBuffer;
	// read 1K at a time
	std::vector<byte> readBuffer(1024,0);
	// used to iterate through the readBuffer
	std::streamsize bytesRead;
	// if we've found an ASCII Control Char DLE or ESC
	bool isEscaped = false;
	// if we've found an ASCII Control Char STX (preceded by a DLE)
	// or a BEMSTART (preceded by an ESC)
	bool msgStart = false;
	// if we've found an ASCII Control Char ETX (also preceded by a DLE)
	// or a BEMEND (preceded by an ESC)
	bool msgComplete = false;
		
	while (!TestDestroy()) {
		
		logFileStream.read(reinterpret_cast<char *>(readBuffer.data()), readBuffer.size());
		
		bytesRead = logFileStream.gcount(); 
		
		if (bytesRead > 0) {
								
			for (int i=0; i < bytesRead; i++) {
				
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
						for (auto it: assemblyBuffer) {
							checksum += it;
						}
					}
					else {
						// Don't perform the checksum calculation, so just "dupe" it
						checksum = 256;
					}
					
					if ((checksum % 256) == 0) {
						if (assemblyBuffer.at(0) == N2K_RX_CMD) {
							// we have a valid received frame so send it
							deviceQueue->Post(assemblyBuffer);																					
						}
					}
					
					wxThread::Sleep(20);
					
					// Reset everything for next message
					assemblyBuffer.clear();
					msgStart = false;
					msgComplete = false;
					isEscaped = false;
							
				}	// end if msgComplete
			
			} // end for
			
			// If end of file, rewind to beginning
			if (logFileStream.eof()) {
				logFileStream.clear();
				logFileStream.seekg(0, std::ios::beg); 
				wxLogMessage(_T("Actisense EBL, Rewinding Log File"));
			}
			
		} // end if bytesread > 0
									
	} // end while !TestDestroy
		
	wxLogMessage(_T("Actisense EBL, Thread terminated"));
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode ActisenseEBL::Entry() {
	// Merely loops continuously reading the log file
	wxLogMessage(_T("Actisense EBL, Read Thread Starting"));
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void ActisenseEBL::OnExit() {
}

int ActisenseEBL::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	// Not implemented for log reader
	return TWOCAN_RESULT_SUCCESS;
}
