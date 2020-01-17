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

#ifndef ACTISENSE_SIGNALKDB_H
#define ACTISENSE_SIGNALKDB_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

// Error constants and macros
 #include "twocanerror.h"

// Constants, typedefs and utility functions for bit twiddling and array manipulation for NMEA 2000 messages
#include "twocanutils.h"


#ifdef __LINUX__
// For logging to get time values
#include <sys/time.h>
#endif


// STL
// used for AIS stuff
#include <vector>
#include <algorithm>
#include <bitset>
#include <iostream>

// wxWidgets
// BUG BUG work out which ones we really need
#include <wx/defs.h>

// String Format, Comparisons etc.
#include <wx/string.h>

// For converting NMEA 2000 date & time data
#include <wx/datetime.h>

// Raise events to the plugin
#include <wx/event.h>

// Perform read operations in their own thread
#include <wx/thread.h>

// Logging (Info & Errors)
#include <wx/log.h>

// Logging (raw NMEA 2000 messages)
#include <wx/file.h>

// User's paths/documents folder
#include <wx/stdpaths.h>


// The top level SignalK service
class SignalKDB {

public:
	// Constructor and destructor
	SignalKDB(void);
	~SignalKDB(void);

	// Initialize & DeInitialize the device.
	// As we don't throw errors in the constructor, invoke functions that may fail from these functions
	int Init(void);
	int DeInit(void);

protected:
	
private:
	
};

#endif
