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


#ifndef ACTISENSE_PLUGIN_H
#define ACTISENSE_PLUGIN_H

// Pre compiled headers 
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
      #include <wx/wx.h>
#endif
// Defines version numbers for this plugin
#include "version.h"

// What version of the OpenCPN plugin API does this plugin support
#define     OPENCPN_API_VERSION_MAJOR    1
#define     OPENCPN_API_VERSION_MINOR    8

// Defines all of the OpenCPN plugin virtual methods we need to override
#include "ocpn_plugin.h"

// Preferences Dialog
#include "actisense_settings.h"

// Actisense device, which is our implementation of a NMEA 2000 device
#include "actisense_device.h"

// BUG BUG check which wxWidget includes we really need
#include <wx/arrstr.h> 

// Configuration
#include <wx/fileconf.h>

// For passing references to wxCharacter strings
#include <wx/wxchar.h>

// Frame received events
#include <wx/event.h>

// Plugin receives FrameReceived events from the TwoCan device
const wxEventType wxEVT_SENTENCE_RECEIVED_EVENT = wxNewEventType();

// The Actisense plugin
class Actisense : public opencpn_plugin_18, public wxEvtHandler {

public:
	// The constructor
	Actisense(void *ppimgr);
	
	// and destructor
	~Actisense(void);

	// Overridden OpenCPN plugin methods
	int Init(void);
	bool DeInit(void);
	int GetAPIVersionMajor();
	int GetAPIVersionMinor();
	int GetPlugInVersionMajor();
	int GetPlugInVersionMinor();
	void ShowPreferencesDialog(wxWindow* parent);
	wxString GetCommonName();
	wxString GetShortDescription();
	wxString GetLongDescription();
	void SetNMEASentence(wxString &sentence); // Not used yet...
	wxBitmap *GetPlugInBitmap();
	
private: 
	// NMEA 2000 device
	ActisenseDevice *actisenseDevice;

	// Load & Save settings
	bool LoadConfiguration(void);
	bool SaveConfiguration(void);

	// Preferences dialog
	ActisenseSettings *settingsDialog;

	// Plugin bitmap
	wxBitmap pluginBitmap;

	// Reference to the OpenCPN window handle
	wxWindow *parentWindow;

	// NMEA 0183 sentence received events
	void OnSentenceReceived(wxCommandEvent &event);

	// Actisense Device, either EBL Log reader or NGT-1 device
	void StartDevice(void);
	void StopDevice(void);

};

#endif 

