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
// Unit: Preferences Dialog for the Plugin
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//
// Note Device, & Logging pages are hidden as these functions
// are yet to be implemented in the orignal TwoCan plugin
// and also cannot be crrently tested with the NGT-1 adapter


#include "actisense_settings.h"

// Constructor and destructor implementation
// inherits froms ActisenseSettingsBase which was implemented using wxFormBuilder
ActisenseSettings::ActisenseSettings( wxWindow* parent) 
	: ActisenseSettingsBase(parent) {

	// BUG BUG  Set the dialog's 16x16 icon
	wxIcon icon;
	icon.CopyFromBitmap(*_img_actisense_logo_16);
	ActisenseSettings::SetIcon(icon);
	togglePGN = FALSE;
}

ActisenseSettings::~ActisenseSettings() {
// Just in case we are closed from the dialog's close button 
	
	// We are closing...
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
}

void ActisenseSettings::OnInit(wxInitDialogEvent& event) {
	this->settingsDirty = FALSE;
		
	// Settings Tab
	wxArrayString *pgn = new wxArrayString();
	
	// BUG BUG Localization
	// Note to self, order must match FLAGS
	pgn->Add(_T("127250 ") + _("Heading") + _T(" (HDG)"));
	pgn->Add(_T("128259 ") + _("Speed") + _T(" (VHW)"));
	pgn->Add(_T("128267 ") + _("Depth") + _T(" (DPT)"));
	pgn->Add(_T("129025 ") + _("Position") + _T(" (GLL)"));
	pgn->Add(_T("129026 ") + _("Course and Speed over Ground") + _(" (VTG)"));
	pgn->Add(_T("129029 ") + _("GNSS") + _T(" (GGA)"));
	pgn->Add(_T("129033 ") + _("Time") + _T(" (ZDA)"));
	pgn->Add(_T("130306 ") + _("Wind") + _T(" (MWV)"));
	pgn->Add(_T("130310 ") + _("Water Temperature") + _(" (MWT)"));
	pgn->Add(_T("129808 ") + _("Digital Selective Calling") + _T(" (DSC)"));
	pgn->Add(_T("129038..41 ") + _("AIS Class A & B messages") + _T(" (VDM)"));
	pgn->Add(_T("129285 ") + _("Route/Waypoint") + _T(" (WPL/RTE)"));
	pgn->Add(_T("127251 ") + _("Rate of Turn") + _T(" (ROT)"));
	pgn->Add(_T("129283 ") + _("Cross Track Error") + _T(" (XTE)"));
	pgn->Add(_T("127257 ") + _("Attitude") + _T(" (XDR)"));
	pgn->Add(_T("127488..49 ") + _("Engine Parameters") + _T(" (XDR)"));
	pgn->Add(_T("127505 ") + _("Fluid Levels") + _T(" (XDR) "));
	pgn->Add(_T("127245 ") + _("Rudder Angle") + _T(" (RSA)"));
	pgn->Add(_T("127508 ") + _("Battery Status") + _T(" (XDR)"));
	pgn->Add(_T("129284 ") + _("Navigation Data") + _T(" (BWC/BWR/BOD/WCV)"));
			
	// Populate the listbox and check/uncheck as appropriate
	for (size_t i = 0; i < pgn->Count(); i++) {
		chkListPGN->Append(pgn->Item(i));
		chkListPGN->Check(i, (supportedPGN & (int)pow(2, i) ? TRUE : FALSE));
	}

	// Search for the  drivers that are present
	EnumerateDrivers();
	
	// Populate the ComboBox and set the default driver selection
	for (AvailableAdapters::iterator it = this->adapters.begin(); it != this->adapters.end(); it++){
		// Both the first & second item of the hashmap is either "Log File Reader" or Actisense NGT-1"
		cmbInterfaces->Append(it->first);
		// Ensure that the driver being used is selected in the Combobox 
		if (canAdapter == it->second) {
			cmbInterfaces->SetStringSelection(it->first);
		}
	}
	
	// About Tab
	bmpAbout->SetBitmap(*_img_actisense_black);
  
	// BUG BUG Localization & version numbers
	txtAbout->SetLabel(_T("OpenCPN PlugIn for Actisense\xae NGT-1.\nEnables some NMEA 2000\xae data to be directly integrated with OpenCPN.\
	\n\nThis software is not supported by Actisense.\nSend bug reports to twocanplugin@hotmail.com.\
	\n\nActisense is a registered trademark of Active Research Limited.\nNMEA 2000 is a registered trademark of National Marine Electronics Association."));

	// Debug Tab
	// BUG BUG Localization
	btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));

	// Network Tab - Currently hidden
	wxSize gridSize;
	gridSize = this->GetClientSize();
	gridSize.SetHeight(gridSize.GetHeight() * 0.75f);
	gridSize.SetWidth(gridSize.GetWidth());
	dataGridNetwork->SetMinSize(gridSize);
	dataGridNetwork->SetMaxSize(gridSize);

	for (int i = 0; i < CONST_MAX_DEVICES; i++) {
		// Renumber row labels to match network address 0 - 253
		dataGridNetwork->SetRowLabelValue(i, std::to_string(i));
		// No need to iterate over non-existent entries
		if ((networkMap[i].uniqueId > 0) || (strlen(networkMap[i].productInformation.modelId) > 0) ) {
			dataGridNetwork->SetCellValue(i, 0, wxString::Format("%lu", networkMap[i].uniqueId));
			// Look up the manufacturer name
			std::unordered_map<int, std::string>::iterator it = deviceManufacturers.find(networkMap[i].manufacturerId);
			if (it != deviceManufacturers.end()) {
				dataGridNetwork->SetCellValue(i, 1, it->second);
			}
			else {
				dataGridNetwork->SetCellValue(i, 1, wxString::Format("%d", networkMap[i].manufacturerId));
			}
			dataGridNetwork->SetCellValue(i, 2, wxString::Format("%s", networkMap[i].productInformation.modelId));
			// We don't receive our own heartbeats so ignore our time stamp value
			if (networkMap[i].uniqueId != uniqueId) {
				wxGridCellAttr *attr;
				attr = new wxGridCellAttr;
				// Differentiate dead/alive devices 
				attr->SetTextColour((wxDateTime::Now() > (networkMap[i].timestamp + wxTimeSpan::Seconds(60))) ? *wxRED : *wxGREEN);
				dataGridNetwork->SetAttr(i, 0, attr);
			}
		}
	}
	
	// Device tab - currently hidden
	// Display the capabilities of ths device
	chkDeviceMode->SetValue(deviceMode);
	chkEnableHeartbeat->SetValue(enableHeartbeat);
	chkSignalK->SetValue(enableSignalK);
	chkGateway->SetValue(enableGateway);
	
	// Enable/Disable as appropriate
	chkEnableHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkSignalK->Enable(chkDeviceMode->IsChecked());
	
	// Display the curent NMEA 2000 product information of this device
	labelNetworkAddress->SetLabel(wxString::Format("Network Address: %u", networkAddress));
	labelUniqueId->SetLabel(wxString::Format("Unique ID: %lu", uniqueId));
	labelModelId->SetLabel(wxString::Format("Model ID: Actisense NGT-1"));
	labelManufacturer->SetLabel("Manufacturer: Actisense");
	labelSoftwareVersion->SetLabel(wxString::Format("Software Version: %s", CONST_SOFTWARE_VERSION));

	// Logging Options, both raw and analytical - currently hidden
	// Add raw logging options to the hashmap
	logging["None"] = FLAGS_LOG_NONE;
	logging["TwoCan"] = FLAGS_LOG_RAW;
	logging["Canboat"] = FLAGS_LOG_CANBOAT;
	logging["Candump"] = FLAGS_LOG_CANDUMP;
	logging["YachtDevices"] = FLAGS_LOG_YACHTDEVICES;
	logging["CSV"] = FLAGS_LOG_CSV;

	for (LoggingOptions::iterator it = this->logging.begin(); it != this->logging.end(); it++){
		cmbLogging->Append(it->first);
		if (logLevel == it->second) {
			cmbLogging->SetStringSelection(it->first);
		}
	}
	
	// Data Analysis logging options
	chkSpreadsheet->SetValue(enableExcel);
	chkInfluxDB->SetValue(enableInfluxDB);
	
	// Ensure the dialog is sized correctly	
	// And hide the pages that are not yet tested/implemented
	// Interesting.... once a page is removed, the indexes kind of shuffle down.
	notebookTabs->RemovePage(1);
	notebookTabs->RemovePage(1);
	notebookTabs->RemovePage(1);
	
	Fit();
	
}

// BUG BUG Should prevent the user from shooting themselves in the foot if they select a driver that is not present
void ActisenseSettings::OnChoiceInterfaces(wxCommandEvent &event) {
	// BUG BUG should only set the dirty flag if we've actually selected a different driver
	this->settingsDirty = TRUE;
}

// Select NMEA 2000 parameter group numbers to be converted to their respective NMEA 0183 sentences
void ActisenseSettings::OnCheckPGN(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
} 

// Enable Logging of Raw NMEA 2000 frames
void ActisenseSettings::OnChoiceLogging(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Toggle real time display of received NMEA 2000 frames
void ActisenseSettings::OnPause(wxCommandEvent &event) {
	debugWindowActive = !debugWindowActive;
	// BUG BUG Localization
	ActisenseSettings::btnPause->SetLabel((debugWindowActive) ? _("Stop") : _("Start"));
}

// Copy the text box contents to the clipboard
void ActisenseSettings::OnCopy(wxCommandEvent &event) {
	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData(new wxTextDataObject(txtDebug->GetValue()));
		wxTheClipboard->Close();
	}
}

// Set whether the device is an actve or passive node on the NMEA 2000 network
void ActisenseSettings::OnCheckMode(wxCommandEvent &event) {
	chkEnableHeartbeat->Enable(chkDeviceMode->IsChecked());
	chkGateway->Enable(chkDeviceMode->IsChecked());
	chkSignalK->Enable(chkDeviceMode->IsChecked());
	this->settingsDirty = TRUE;
}

// Set whether the device sends heartbeats onto the network
void ActisenseSettings::OnCheckHeartbeat(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device acts as a bi-directional gateway (NMEA 0183 <--> NMEA 2000)
void ActisenseSettings::OnCheckGateway(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}
// Set whether the device acts as a SignalK Server
void ActisenseSettings::OnCheckSignalK(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device logs analytical data to a CSV file suitable for Excel
void ActisenseSettings::OnCheckExcel(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Set whether the device logs analytical data to InfluxDB (the database dejour...)
void ActisenseSettings::OnCheckInfluxDb(wxCommandEvent &event) {
	this->settingsDirty = TRUE;
}

// Right mouse click to check/uncheck all parameter group numbers
void ActisenseSettings::OnRightCick( wxMouseEvent& event) {
	togglePGN = !togglePGN;
	for (unsigned int i = 0; i < chkListPGN->GetCount(); i++) {
		chkListPGN->Check(i, togglePGN);
	}
	this->settingsDirty = TRUE;
}

void ActisenseSettings::OnOK(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}
	
	// Return OK
	EndModal(wxID_OK);
}

void ActisenseSettings::OnApply(wxCommandEvent &event) {
	// Save the settings
	if (this->settingsDirty) {
		SaveSettings();
		this->settingsDirty = FALSE;
	}
}

void ActisenseSettings::OnCancel(wxCommandEvent &event) {
	// Disable receiving of NMEA 2000 frames in the debug window, as we'll be closing
	debugWindowActive = FALSE;

	// Clear the clipboard
	if (wxTheClipboard->Open()) {
		wxTheClipboard->Clear();
		wxTheClipboard->Close();
	}

	// Ignore any changed settings and return CANCEL
	EndModal(wxID_CANCEL);
}

void ActisenseSettings::SaveSettings(void) {
	// Enumerate the check list box to determine checked items
	wxArrayInt checkedItems;
	chkListPGN->GetCheckedItems(checkedItems);

	supportedPGN = 0;
	// Save the bitflags representing the checked items
	for (wxArrayInt::const_iterator it = checkedItems.begin(); it < checkedItems.end(); it++) {
		supportedPGN |= 1 << (int)*it;
	}
	
	// BUG BUG Perhaps more elegant ?
	// enableHeartbeat = chkEnableHeartbeat->IsChecked();
	enableHeartbeat = FALSE;
	if (chkEnableHeartbeat->IsChecked()) {
		enableHeartbeat = TRUE;
	}
	
	enableGateway = FALSE;
	if (chkGateway->IsChecked()) {
		enableGateway = TRUE;
	}
	
	enableSignalK = FALSE;
	if (chkSignalK->IsChecked()) {
		enableSignalK = TRUE;
	}
	
	enableExcel = FALSE;
	if (chkSpreadsheet->IsChecked()) {
		enableExcel = TRUE;
	}
	
	enableInfluxDB = FALSE;
	if (chkInfluxDB->IsChecked()) {
		enableInfluxDB = TRUE;
	}
		
	deviceMode = FALSE;
	if (chkDeviceMode->IsChecked()) {
		deviceMode = TRUE;
	}

	if (cmbInterfaces->GetSelection() != wxNOT_FOUND) {
		canAdapter = adapters[cmbInterfaces->GetStringSelection()];
	} 
	else {
		canAdapter = _T("None");
	}

	logLevel = FLAGS_LOG_NONE;
	if (cmbLogging->GetSelection() != wxNOT_FOUND) {
		logLevel = logging[cmbLogging->GetStringSelection()];
	}
	else {
		logLevel = FLAGS_LOG_NONE;
	}
}

// Lists the available NGT-1 adapters or Logfile interfaces 
bool ActisenseSettings::EnumerateDrivers(void) {
	
	// Add the built-in Log File Reader to the Adapter hashmap
	adapters[CONST_LOG_READER] = CONST_LOG_READER;
	// BUG BUG Should really check if the Actisense NGT-1 device is present
	adapters[CONST_NGT_READER] = CONST_NGT_READER;
	
	return TRUE;
}
