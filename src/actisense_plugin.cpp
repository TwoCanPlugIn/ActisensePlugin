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

//
// Project: Actisense Plugin
// Description: Actisense NGT-1 plugin for OpenCPN
// Unit: OpenCPN Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include "actisense_plugin.h"
#include "actisense_icons.h"

// Globally accessible variables used by the plugin, device and the settings dialog.
wxFileConfig *configSettings;
wxString canAdapter;
wxString adapterPortName;
int supportedPGN;
bool deviceMode;
bool debugWindowActive;
bool enableHeartbeat;
bool enableGateway;
bool enableSignalK;
bool enableExcel;
bool enableInfluxDB;
bool actisenseChecksum;
int logLevel;


unsigned long uniqueId;
int networkAddress;
NetworkInformation networkMap[CONST_MAX_DEVICES];

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
	return new Actisense(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
	delete p;
}

// Actisense plugin constructor. Note it inherits from wxEvtHandler so that we can receive events
// from the Actisense Device when a frame is received
Actisense::Actisense(void *ppimgr) : opencpn_plugin_18(ppimgr), wxEvtHandler() {
	// Wire up the event handler
	Connect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(Actisense::OnSentenceReceived));
	// Load the plugin bitmaps/icons 
	initialize_images();
}

Actisense::~Actisense(void) {
	// Disconnect the event handler
	Disconnect(wxEVT_SENTENCE_RECEIVED_EVENT, wxCommandEventHandler(Actisense::OnSentenceReceived));
}

int Actisense::Init(void) {
	// Perform any necessary initializations here
	
	// Maintain a reference to the OpenCPN window
	// Will use as the parent for the Autopilot Dialog
	parentWindow = GetOCPNCanvasWindow();

	// Maintain a reference to the OpenCPN configuration object 
	configSettings = GetOCPNConfigObject();

	// Actisense preferences dialog
	settingsDialog = nullptr;
	
	// Initialize Actisense to a nullptr to prevent crashes when trying to stop an unitialized device
	// which is what happens upon first startup. Bugger, there must be a better way.
	actisenseDevice = nullptr;

	// Toggles display of captured NMEA 2000 frames in the "debug" tab of the preferences dialog
	debugWindowActive = FALSE;

	// Load the configuration items
	if (LoadConfiguration()) {
		// Start the Actisense Device which will in turn load either the NGT-1 device or the EBL Log File device
		StartDevice();
	}
	
	// Notify OpenCPN what events we want to receive callbacks for
	// WANTS_NMEA_SENTENCES could be used for future versions where we might convert NMEA 0183 sentences to NMEA 2000
	return (WANTS_PREFERENCES | WANTS_CONFIG );
}

// OpenCPN is either closing down, or we have been disabled from the Preferences Dialog
bool Actisense::DeInit(void) {
	// Persist our network address to prevent address claim conflicts next time we start
	// This current version does not yet act as an ActiveDevice
	if (deviceMode == TRUE) {
		if (configSettings) {
			configSettings->SetPath(_T("/PlugIns/Actisense"));
			configSettings->Write(_T("Address"), networkAddress);
		}
	}
	
	// Terminate the Actisense Device Thread
	StopDevice();

	return TRUE;
}

// Indicate what version of the OpenCPN Plugin API we support
int Actisense::GetAPIVersionMajor() {
	return OPENCPN_API_VERSION_MAJOR;
}

int Actisense::GetAPIVersionMinor() {
	return OPENCPN_API_VERSION_MINOR;
}

// The actisense plugin version numbers. 
// BUG BUG anyway to automagically generate version numbers like Visual Studio & .NET assemblies ?
int Actisense::GetPlugInVersionMajor() {
	return PLUGIN_VERSION_MAJOR;
}

int Actisense::GetPlugInVersionMinor() {
	return PLUGIN_VERSION_MINOR;
}

// Return descriptions for the Actisense Plugin
wxString Actisense::GetCommonName() {
	return _T("Plugin for Actisense\xae NGT-1");
}

wxString Actisense::GetShortDescription() {
	//Trademark character ® code is \xae
	return _T("Plugin for Actisense\xae NGT-1, integrates OpenCPN with NMEA2000\xae networks.");
}

wxString Actisense::GetLongDescription() {
	// Localization ??
    return _T("PlugIn for Actisense\xae NGT-1, integrates OpenCPN with NMEA2000\xae networks.\nEnables some NMEA2000\xae data to be directly integrated with OpenCPN.\nNote this is not supported by Active Research Limited.");
}

// 32x32 pixel PNG file, use pgn2wx.pl perl script
wxBitmap* Actisense::GetPlugInBitmap() {
		return _img_actisense_logo_32;
}

// BUG BUG for future versions
void Actisense::SetNMEASentence(wxString &sentence) {
	// Maintain a local copy of the NMEA Sentence for conversion to a NMEA 2000 PGN
	wxString nmea183Sentence = sentence;    
	// BUG BUG To Do
	if ((deviceMode==TRUE) && (enableGateway == TRUE)) {
		// Decode NMEA 183 sentence
		// Encode NMEA 2000 frame
		// transmit
	}
}

// Frame received event handler. Events queued from Actisense Device.
// NMEA 0183 sentences are passed via the SetString()/GetString() functions
void Actisense::OnSentenceReceived(wxCommandEvent &event) {
	switch (event.GetId()) {
	case SENTENCE_RECEIVED_EVENT:
		PushNMEABuffer(event.GetString());
		// If the preference dialog is open and the debug tab is toggled, display the NMEA 183 sentences
		// Superfluous as they can be seen in the Connections tab.
		if ((debugWindowActive) && (settingsDialog != NULL)) {
			settingsDialog->txtDebug->AppendText(event.GetString());
		}
		break;
	default:
		event.Skip();
	}
}

// Display Actisense preferences dialog
void Actisense::ShowPreferencesDialog(wxWindow* parent) {
	settingsDialog = new ActisenseSettings(parent);

	if (settingsDialog->ShowModal() == wxID_OK) {

		// Save the settings
		if (SaveConfiguration()) {
			wxLogMessage(_T("Actisense Plugin, Settings Saved"));
		}
		else {
			wxLogMessage(_T("Actisense Plugin, Error Saving Settings"));
		}

		// Terminate the Actisense Device Thread, but only if we have a valid driver selected !
		StopDevice();
		
		if (LoadConfiguration()) {
			// Restart the Actisense  Device
			StartDevice();
		}
		else {
			wxLogError(_T("Actisense Plugin, Error loading settings. Device not restarted "));
		}
	}
	delete settingsDialog;
	settingsDialog = NULL;
}

// Loads a previously saved configuration
bool Actisense::LoadConfiguration(void) {
	if (configSettings) {
		configSettings->SetPath(_T("/PlugIns/Actisense"));
		configSettings->Read(_T("Adapter"), &canAdapter, _T(""));
		configSettings->Read(_T("AlternativePort"), &adapterPortName, _T(""));
		configSettings->Read(_T("PGN"), &supportedPGN, 0);
		configSettings->Read(_T("Mode"), &deviceMode, FALSE);
		configSettings->Read(_T("Log"), &logLevel, 0);
		configSettings->Read(_T("Address"), &networkAddress, 0);
		configSettings->Read(_T("Heartbeat"), &enableHeartbeat, FALSE);
		configSettings->Read(_T("Gateway"), &enableGateway, FALSE);
		configSettings->Read(_T("Checksum"), &actisenseChecksum, TRUE);
		return TRUE;
	}
	else {
		// Default Settings
		canAdapter = _T("");
		adapterPortName = _T("");
		supportedPGN = 0;
		deviceMode = FALSE;
		logLevel = 0;
		networkAddress = 0;
		enableHeartbeat = FALSE;
		enableGateway = FALSE;
		actisenseChecksum = TRUE;
		return TRUE;
	}
}

bool Actisense::SaveConfiguration(void) {
	if (configSettings) {
		configSettings->SetPath(_T("/PlugIns/Actisense"));
		// No UI for setting the adapterPortName (AlternativePort). It is set manually to override 
		// the default automatic detection of the serial port or tty device. 
		// Similarly no UI for setting the value of actisenseChecksum (Checksum)
		configSettings->Write(_T("Adapter"), canAdapter);
		configSettings->Write(_T("PGN"), supportedPGN);
		configSettings->Write(_T("Log"), logLevel);
		configSettings->Write(_T("Mode"), deviceMode);
		configSettings->Write(_T("Address"), networkAddress);
		configSettings->Write(_T("Heartbeat"), enableHeartbeat);
		configSettings->Write(_T("Gateway"), enableGateway);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void Actisense::StopDevice(void) {
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError;
	if (actisenseDevice != nullptr) {
		if (actisenseDevice->IsRunning()) {
			wxMessageOutputDebug().Printf(_T("Actisense Plugin, Terminating device thread id (0x%x)\n"), actisenseDevice->GetId());
			threadError = actisenseDevice->Delete(&threadExitCode, wxTHREAD_WAIT_BLOCK);
			if (threadError == wxTHREAD_NO_ERROR) {
				wxLogMessage(_T("Actisense Plugin, Terminated device thread (%lu)"), threadExitCode);
				wxMessageOutputDebug().Printf(_T("Actisense Plugin, Terminated device thread (%lu)\n"), threadExitCode);
			}
			else {
				wxLogMessage(_T("Actisense Plugin, Error terminating device thread (%lu)"), threadError);
				wxMessageOutputDebug().Printf(_T("Actisense Plugin, Error terminating device thread (%lu)\n"), threadError);
			}
			// wait for the interface thread to exit
			actisenseDevice->Wait(wxTHREAD_WAIT_BLOCK);
			
			// can only delete the interface if it is a joinable thread.
			delete actisenseDevice;
		}
	}
}

void Actisense::StartDevice(void) {
	actisenseDevice = new ActisenseDevice(this);
	if (!canAdapter.empty()) {
		int returnCode = actisenseDevice->Init(canAdapter);
		if ((returnCode & TWOCAN_RESULT_FATAL) == TWOCAN_RESULT_FATAL) {
			wxLogError(_T("Actisense Plugin,  Error initializing device (%lu)"), returnCode);
			wxMessageOutputDebug().Printf(_T("Actisense Plugin,  Error initializing device (%lu)\n"), returnCode);

		}
		else {
			wxLogMessage(_T("Actisense Plugin, Device initialized"));
			wxMessageOutputDebug().Printf(_T("Actisense Plugin, Device initialized"));
			int threadResult = actisenseDevice->Run();
			if (threadResult == wxTHREAD_NO_ERROR) {
				wxLogMessage(_T("Actisense Plugin, Successfully created device thread"));
				wxMessageOutputDebug().Printf(_T("Actisense Plugin, Successfully created device thread\n"));
			}
			else {
				wxLogError(_T("Actisense Plugin, Error creating device thread (%lu)"), threadResult);
				wxMessageOutputDebug().Printf(_T("Actisense Plugin, Error creating device thread (%lu)\n"), threadResult);
			}
		}
	}
	else {
		wxLogError(_T("Actisense Plugin, No device has been configured"));
	}
}
