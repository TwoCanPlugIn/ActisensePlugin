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
// Unit: ActisenseInterface - Abstract class for interfaces
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include <actisense_interface.h>

ActisenseInterface::ActisenseInterface(wxMessageQueue<std::vector<byte>> *messageQueue) : wxThread(wxTHREAD_JOINABLE) {
	// Save the Actisense Device message queue
	// NMEA 2000 messages are 'posted' to the Actisense device for subsequent parsing
	deviceQueue = messageQueue;
}

ActisenseInterface::~ActisenseInterface() {
}

int ActisenseInterface::Open(const wxString& fileName) {
	return TWOCAN_RESULT_SUCCESS;
}

int ActisenseInterface::Close(void) {
	return TWOCAN_RESULT_SUCCESS;
}

void ActisenseInterface::Read() {
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode ActisenseInterface::Entry() {
	Read();
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread is being destroyed
void ActisenseInterface::OnExit() {
}

int ActisenseInterface::Write(const unsigned int canId, const unsigned char payloadLength, const unsigned char *payload) {
	return TWOCAN_RESULT_SUCCESS;
}
