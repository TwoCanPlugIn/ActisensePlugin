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
// Unit: Actisense SignalK Server - Manages http & web socket servers, subscription and json encoding
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include "actisense_signalkserver.h"

SignalKServer::SignalKServer(void) : wxThread(wxTHREAD_DETACHED) {
	// ToDo
}
	
SignalKServer::~SignalKServer(void) {
		// ToDo
}

// Init - Start the servers
int SignalKServer::Init(void) {
	return TWOCAN_RESULT_SUCCESS;
}

// DeInit - Stop all of the servers
int SignalKServer::DeInit() {
	return TWOCAN_RESULT_SUCCESS;
}

// Entry, the method that is executed upon thread start
wxThread::ExitCode SignalKServer::Entry() {
	return (wxThread::ExitCode)TWOCAN_RESULT_SUCCESS;
}

// OnExit, called when thread->delete is invoked, and entry returns
void SignalKServer::OnExit() {
	
}
