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
// Unit: Actisene Device - Receives NMEA2000 PGN's and converts to NMEA 183 Sentences
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release
//

#include "actisense_device.h"

ActisenseDevice::ActisenseDevice(wxEvtHandler *handler) : wxThread(wxTHREAD_JOINABLE) {
	// Save a reference to our "parent", the plugin event handler so we can pass events to it
	eventHandlerAddress = handler;
	
	// initialise Message Queue to receive frames from either an Actisense EBL log file or Actisense NGT-1 device
	canQueue = new wxMessageQueue<std::vector<byte>>();
	
	// Initialize the statistics
	// BUG BUG Get around to actually doing something with these !!
	receivedFrames = 0;
	transmittedFrames = 0;
	droppedFrames = 0;
	fastFrames = 0;
	errorFrames = 0;
	standardFrames = 0;
	averageProcessingTime = 0;
	minimumProcessingTime = 0;
	maximumProcessingTime = 0;
	
	// Each AIS multi sentence message has a sequential Message ID
	AISsequentialMessageId = 0;

	// Until engineInstance > 0 then assume a single engined vessel
	IsMultiEngineVessel = FALSE;
		
	// BUG BUG - Need to finalize use case and reflect in the preferences dialog
	// BUG BUG - Logging not currently exposed in the Preferences dialog
	// BUG BUG - Perform logging in the NGT-1 driver ??
	if (logLevel > FLAGS_LOG_NONE) {
		wxDateTime tm = wxDateTime::Now();
		// construct a filename with the following format twocan-2018-12-31_210735.log
		wxString fileName = tm.Format("twocan-%Y-%m-%d_%H%M%S.log");
		if (rawLogFile.Open(wxString::Format("%s//%s", wxStandardPaths::Get().GetDocumentsDir(), fileName), wxFile::write)) {
			wxLogMessage(_T("Actisense Device, Created log file %s"), fileName);
			// If a CSV format initialize with a header row
			if (logLevel == FLAGS_LOG_CSV) {
				rawLogFile.Write("Source,Destination,PGN,Priority,D1,D2,D3,D4,D5,D6,D7,D8\r\n");
			}
		}
		else {
			wxLogError(_T("Actisense Device, Unable to create raw log file %s"), fileName);
		}
	}
}

ActisenseDevice::~ActisenseDevice(void) {
	// Anything to do in the destructor ??
}

// Init, Load the Actisense NGT-1 adapter driver or the Actisense EBL Log File Reader
int ActisenseDevice::Init(wxString driverPath) {
	int returnCode;

	driverName = driverPath;

	if (driverName.CmpNoCase(CONST_LOG_READER) == 0) {
		// Load the Actisense EBL log file reader
		deviceInterface = new ActisenseEBL(canQueue);
		returnCode = deviceInterface->Open(CONST_LOGFILE_NAME);
	}
	else if (driverName.CmpNoCase(CONST_NGT_READER) == 0) {
		// Load the  Actisense NGT-1 interface
		deviceInterface = new ActisenseNGT1(canQueue);
		// By default, adapterPortName is empty, therefore the NGT-1 interface
		// will automagically determine the correct port. If not empty, it overrides the automatic port selection
		returnCode = deviceInterface->Open(adapterPortName);
	}

	else {
		// BUG BUG Should not reach this condition
		returnCode = SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DEVICE, TWOCAN_ERROR_DRIVER_NOT_FOUND);
	}
	
	return returnCode;
		
}

// DeInit
// BUG BUG At present DeInit is not called by the Actisense Plugin
int ActisenseDevice::DeInit() {
	return TWOCAN_RESULT_SUCCESS;
}

// Entry, the method that is executed upon thread start
// Merely loops continuously waiting for frames to be received by the NGT-1 Adapter or EBL Log Reader
wxThread::ExitCode ActisenseDevice::Entry() {
	return (wxThread::ExitCode)ReadActisenseDriver();
}

// OnExit, called when the plugin invokes thread->delete and Entry returns
void ActisenseDevice::OnExit() {
	// Terminate the interface thread
	wxThread::ExitCode threadExitCode;
	wxThreadError threadError;
	
	wxMessageOutputDebug().Printf(_T("Actisense Device, Terminating interface thread id (0x%x)\n"), deviceInterface->GetId());
	threadError = deviceInterface->Delete(&threadExitCode,wxTHREAD_WAIT_BLOCK);
	if (threadError == wxTHREAD_NO_ERROR) {
		wxLogMessage(_T("Actisense Device, Terminated interface thread (%lu)"), threadExitCode);
		wxMessageOutputDebug().Printf(_T("Actisense Device, Terminated interface thread (%lu)\n"), threadExitCode);
	}
	else {
		wxLogMessage(_T("Actisense Device, Error terminating interface thread (%lu)"),threadError);
		wxMessageOutputDebug().Printf(_T("Actisense Device, Error terminating interface thread (%lu)\n"),threadError);		
	}
	// wait for the interface thread to exit
	deviceInterface->Wait(wxTHREAD_WAIT_BLOCK);
	
	// Can only invoke close if it is a joinable thread as detached threads delete themselves.
	int returnCode;
	returnCode = deviceInterface->Close();
	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogMessage(_T("Actisense Device, Error closing interface (%lu)"),returnCode);
		wxMessageOutputDebug().Printf(_T("Actisense Device, Error closing interface (%lu)\n"),returnCode);
	}
	
	// only delete the interface if it is a joinable thread.
	delete deviceInterface;

	eventHandlerAddress = NULL;

	// If logging, close log file
	if (logLevel > FLAGS_LOG_NONE) {
		if (rawLogFile.IsOpened()) {
			rawLogFile.Flush();
			rawLogFile.Close();
			wxLogMessage(_T("Actisense Device, Closed Log File"));
			wxMessageOutputDebug().Printf(_T("Actisense Device, Closed Log File\n"));
		}
	}
	
	// If logging to influxDb, close the queue & the subsequently the thread and connection
	//if (enableInfluxDB) {
	//	delete influxDbQueue;
	//	influxDBThread.Destroy()
	//}
	
	// if logging to Excel, close the spreadsheet file
	//if (enableExcel) {
	//	if (spreadSheetFile.IsOpened()) {
	//		spreadSheetFile.Flush();
	//		spreadSheetFile.Close();
	//	}
	//}
}

int ActisenseDevice::ReadActisenseDriver(void) {
	wxMessageQueueError queueError;
	std::vector<byte> receivedFrame;
			
	// Start the interface's Read thread
	deviceInterface->Run();
	wxLogMessage(_T("Actisense Device, Started interface thread"));
	wxMessageOutputDebug().Printf(_T("Actisense Device, Started interface thread\n"));
	
	while (!TestDestroy()) {
		
		// Wait for a CAN Frame
		queueError = canQueue->ReceiveTimeout(100,receivedFrame);
		
		switch (queueError) {
			case wxMSGQUEUE_NO_ERROR:
				ParseMessage(receivedFrame);
				break;
			case wxMSGQUEUE_TIMEOUT:
				break;
			case wxMSGQUEUE_MISC_ERROR:
				// BUG BUG, Should we log this ??
				break;
		}

	} // end while

	wxLogMessage(_T("Actisense Device, Read thread exiting"));
	wxMessageOutputDebug().Printf(_T("Actisense Device, Read thread exiting\n"));
	
	return TWOCAN_RESULT_SUCCESS;	
}


// Queue the SENTENCE_RECEIVED_EVENT to the plugin where it will push the NMEA 0183 sentence into OpenCPN
void ActisenseDevice::RaiseEvent(wxString sentence) {
	wxCommandEvent *event = new wxCommandEvent(wxEVT_SENTENCE_RECEIVED_EVENT, SENTENCE_RECEIVED_EVENT);
	event->SetString(sentence);
	wxQueueEvent(eventHandlerAddress, event);
}

// Big switch statement to parse received NMEA 2000 messages

// receivedFrame is the raw Actisense data with the following format:
// receivedFrame[0] - Actisense Command ID (Tx or Rx)
// receivedFrame[1] - Overall length
// receivedFrame[2] - Priority 
// receivedFrame[3..5] -Parameter Group Number
// receivedFrame[6] - Destination address
// receivedframe[7] - Source address
// receivedFrame[8..11] - Actisense timestamp
// receivedFrame[12] - NMEA 2000 data length
// receivedFrame[13..n-1] NMEA 2000 data
// receivedFrame[n] - Actisense checksum, ensures sum of all bytes modulo 256 == 0 

// Note that if actisenseChecksum = FALSE then
// No overall length value and no checksum. Seems as though when using Linux these are not used ?

void ActisenseDevice::ParseMessage(std::vector<byte> receivedFrame) {
	CanHeader header;
	std::vector<byte> payload;
	std::vector<wxString> nmeaSentences;
	bool result = FALSE;
	bool hasChecksum = TRUE;
	bool isValidFrame = FALSE;
	
	// From Hubert's dumps some message have and some do not have checksums, aarrgghhh!
	
	if (receivedFrame.at(0) == N2K_RX_CMD) {
		// overall length in byte 1 excludes command byte(0), length byte(1) and checksum byte(n)	
		if (receivedFrame.at(1) == receivedFrame.size() - 3) {
			// the checksum character at the end of the message
			// ensures that the sum of all characters modulo 256 equals 0
			int checksum = 0;
			for (auto it : receivedFrame) {
				checksum += it;
			}
			if ((checksum % 256) == 0) {
				hasChecksum = TRUE;
				isValidFrame = TRUE;
			}
			else {
				hasChecksum = FALSE;
				// but is also possibly an invalid frame ??
				// What's the probability that byte 1 matches the frame length when the frame doesn't include length & checksum ??
				isValidFrame = FALSE;
			}
			
		}
		else {
			hasChecksum = FALSE;
			isValidFrame = TRUE;
		}
						
		// debug hex dump of received message
		int j = 0;
		wxString debugString;
		debugMutex->Lock();
		wxMessageOutputDebug().Printf(_T("Received Frame\n"));
		for (size_t i = 0; i < receivedFrame.size(); i++) {
			debugString.Append(wxString::Format("%02X ",receivedFrame.at(i)));
			j++;
			if ((j % 8) == 0) {
				wxMessageOutputDebug().Printf(_T("%s\n"),debugString.c_str());
				j = 0;
				debugString.Clear();
			}
		}
		
		if (!debugString.IsEmpty()) {	
			wxMessageOutputDebug().Printf(_T("%s\n"),debugString.c_str());
			debugString.Clear();
		}
		wxMessageOutputDebug().Printf(_T("\n"));
		// unlock once we have prnted out the header debugMutex->Unlock();
		// end of debugging
	
		if ((hasChecksum == TRUE) && (isValidFrame == TRUE)) {
		
		// Construct the CAN Header
		header.pgn = receivedFrame.at(3) + (receivedFrame.at(4)<< 8) + (receivedFrame.at(5) << 16);
		header.destination = receivedFrame.at(6);
		header.source = receivedFrame.at(7);
		header.priority = receivedFrame.at(2);
		
			
		// Timestamp is encoded over bytes 8,9,10,11
		// BUG BUG if we are logging, use this as the time stamp ??
	
		// Data Length is stored in byte 12
		// Copy the CAN data
			for (int i = 0; i < receivedFrame[12]; i++) {
				payload.push_back(receivedFrame[13 + i]);
			}
		}
		else if ((hasChecksum == FALSE) && (isValidFrame == TRUE)) {
			// No checksum and no overall length. Alter indexes as appropriate.
			// Construct the CAN Header
			header.pgn = receivedFrame.at(2) + (receivedFrame.at(3) << 8) + (receivedFrame.at(4) << 16);
			header.destination = receivedFrame.at(5);
			header.source = receivedFrame.at(6);
			header.priority = receivedFrame.at(1);
	
			// Timestamp is encoded over bytes 7,8,9,10
			// BUG BUG if we are logging, use this as the time stamp ??
	
			// Data Length is stored in byte 11
			// Copy the CAN data
			for (int i = 0; i < receivedFrame[11]; i++) {
				payload.push_back(receivedFrame[12 + i]);
			}
		}
	
		// If we receive a frame from a device, then by definition it is still alive!
		networkMap[header.source].timestamp = wxDateTime::Now();
		
		// debugMutex->Lock();
		wxMessageOutputDebug().Printf(_T("Source: %lu\n"),header.source);
		wxMessageOutputDebug().Printf(_T("PGN: %lu\n"),header.pgn);
		wxMessageOutputDebug().Printf(_T("Destination: %lu\n"),header.destination);
		wxMessageOutputDebug().Printf(_T("Priority: %lu\n\n"),header.priority);
		debugMutex->Unlock();	
		
		switch (header.pgn) {
			
		case 59392: // ISO Ack
			// No need for us to do anything as we don't send any requests (yet)!
			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;
			
		case 59904: // ISO Request
			unsigned int requestedPGN;
			
			DecodePGN59904(payload, &requestedPGN);
			// What has been requested from us ?
			switch (requestedPGN) {
			
				case 60928: // Address Claim
					// BUG BUG The bastards are using an address claim as a heartbeat !!
					if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
						int returnCode;
						returnCode = SendAddressClaim(networkAddress);
						if (returnCode != TWOCAN_RESULT_SUCCESS) {
							wxLogMessage(_T("Actisense Device, Error Sending Address Claim (%lu)"), returnCode);
						}
					}
					break;
			
				case 126464: // Supported PGN
					if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
						int returnCode;
						returnCode = SendSupportedPGN();
						if (returnCode != TWOCAN_RESULT_SUCCESS) {
							wxLogMessage(_T("Actisense Device, Error Sending Supported PGN (%lu)"), returnCode);
						}
					}
					break;
			
				case 126993: // Heartbeat
					// BUG BUG I don't think an ISO Request is allowed to request a heartbeat ??
					break;
			
				case 126996: // Product Information 
					if ((header.destination == networkAddress) || (header.destination == CONST_GLOBAL_ADDRESS)) {
						int returnCode;
						returnCode = SendProductInformation();
						if (returnCode != TWOCAN_RESULT_SUCCESS) {
							wxLogMessage(_T("Actisense Device, Error Sending Product Information (%lu)"), returnCode);
						}
					}
					break;
			
				default:
					// BUG BUG For other requested PG's send a NACK/Not supported
					break;
			}
			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;
			
		case 60928: // ISO Address Claim
			DecodePGN60928(payload, &deviceInformation);
			// if another device is not claiming our address, just log it
			if (header.source != networkAddress) {
				
				// Add the source address so that we can  construct a "map" of the NMEA2000 network
				deviceInformation.networkAddress = header.source;
				
				// BUG BUG Extraneous Noise Remove for production
				
	#ifndef NDEBUG

				wxLogMessage(_T("Actisense Network, Address: %d"), deviceInformation.networkAddress);
				wxLogMessage(_T("Actisense Network, Manufacturer: %d"), deviceInformation.manufacturerId);
				wxLogMessage(_T("Actisense Network, Unique ID: %lu"), deviceInformation.uniqueId);
				wxLogMessage(_T("Actisense Network, Class: %d"), deviceInformation.deviceClass);
				wxLogMessage(_T("Actisense Network, Function: %d"), deviceInformation.deviceFunction);
				wxLogMessage(_T("Actisense Network, Industry %d"), deviceInformation.industryGroup);
				
	#endif
			
				// Maintain the map of the NMEA 2000 network.
				// either this is a newly discovered device, or it is resending its address claim
				if ((networkMap[header.source].uniqueId == deviceInformation.uniqueId) || (networkMap[header.source].uniqueId == 0)) {
					networkMap[header.source].manufacturerId = deviceInformation.manufacturerId;
					networkMap[header.source].uniqueId = deviceInformation.uniqueId;
					networkMap[header.source].timestamp = wxDateTime::Now();
				}
				else {
					// or another device is claiming the address that an existing device had used, so clear out any product info entries
					networkMap[header.source].manufacturerId = deviceInformation.manufacturerId;
					networkMap[header.source].uniqueId = deviceInformation.uniqueId;
					networkMap[header.source].timestamp = wxDateTime::Now();
					networkMap[header.source].productInformation = {}; // I think this should initialize the product information struct;
				}
			}
			else {
				// Another device is claiming our address
				// If our NAME is less than theirs, reclaim our current address 
				if (deviceName < deviceInformation.deviceName) {
					int returnCode;
					returnCode = SendAddressClaim(networkAddress);
					if (returnCode == TWOCAN_RESULT_SUCCESS) {
						wxLogMessage(_T("Actisense Device, Reclaimed network address %lu"), networkAddress);
					}
					else {
						wxLogMessage(_T("Actisense Device, Error reclaming network address %lu (%lu)"), networkAddress, returnCode);
					}
				}
				// Our uniqueId is larger (or equal), so increment our network address and see if we can claim the new address
				else {
					networkAddress += 1;
					if (networkAddress <= CONST_MAX_DEVICES) {
						int returnCode;
						returnCode = SendAddressClaim(networkAddress);
						if (returnCode == TWOCAN_RESULT_SUCCESS) {
							wxLogMessage(_T("Actisense Device, Claimed network address %lu"), networkAddress);
						}
						else {
							wxLogMessage(_T("Actisense Device, Error claiming network address %lu (%lu)"), networkAddress, returnCode);
						}
					}
					else {
						// BUG BUG More than 253 devices on the network, we send an unable to claim address frame (source address = 254)
						// Chuckles to self. What a nice DOS attack vector! Kick everyone else off the network!
						// I guess NMEA never thought anyone would hack a boat! What were they (not) thinking!
						wxLogError(_T("Actisense Device, Unable to claim address, more than %d devices"), CONST_MAX_DEVICES);
						networkAddress = 0;
						int returnCode;
						returnCode = SendAddressClaim(CONST_NULL_ADDRESS);
						if (returnCode == TWOCAN_RESULT_SUCCESS) {
							wxLogMessage(_T("Actisense Device, Claimed network address %lu"), networkAddress);
						}
						else {
							wxLogMessage(_T("Actisense Device, Error claiming network address %lu (%lu)"), networkAddress, returnCode);
						}
					}
				}
			}
			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;
			
		case 65240: // ISO Commanded address
			// A device is commanding another device to use a specific address
			DecodePGN65240(payload, &deviceInformation);
			// If we are being commanded to use a specific address
			// BUG BUG Not sure if an ISO Commanded Address frame is broadcast or if header.destination == networkAddress
			if (deviceInformation.uniqueId == uniqueId) {
				// Update our network address to the commanded address and send an address claim
				networkAddress = deviceInformation.networkAddress;
				int returnCode;
				returnCode = SendAddressClaim(networkAddress);
				if (returnCode == TWOCAN_RESULT_SUCCESS) {
					wxLogMessage(_T("Actisense Device, Claimed commanded network address: %lu"), networkAddress);
				}
				else {
					wxLogMessage("Actisense Device, Error claiming commanded network address %lu: %lu", networkAddress, returnCode);
				}
			}
			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;
			
		case 126992: // System Time
			if (supportedPGN & FLAGS_ZDA) {
				result = DecodePGN126992(payload, &nmeaSentences);
			}
			break;
			
		case 126993: // Heartbeat
			DecodePGN126993(header.source, payload);
			// Update the matching entry in the network map
			// BUG BUG what happens if we are yet to have populated this entry with the device details ?? Probably nothing...
			networkMap[header.source].timestamp = wxDateTime::Now();
			result = FALSE;
			break;
			
		case 126996: // Product Information
			DecodePGN126996(payload, &productInformation);
			
			// BUG BUG Extraneous Noise
			
	#ifndef NDEBUG
			wxLogMessage(_T("Actisense Node, Network Address %d"), header.source);
			wxLogMessage(_T("Actisense Node, DB Ver: %d"), productInformation.dataBaseVersion);
			wxLogMessage(_T("Actisense Node, Product Code: %d"), productInformation.productCode);
			wxLogMessage(_T("Actisense Node, Cert Level: %d"), productInformation.certificationLevel);
			wxLogMessage(_T("Actisense Node, Load Level: %d"), productInformation.loadEquivalency);
			wxLogMessage(_T("Actisense Node, Model ID: %s"), productInformation.modelId);
			wxLogMessage(_T("Actisense Node, Model Version: %s"), productInformation.modelVersion);
			wxLogMessage(_T("Actisense Node, Software Version: %s"), productInformation.softwareVersion);
			wxLogMessage(_T("Actisense Node, Serial Number: %s"), productInformation.serialNumber);
	#endif
			
			// Maintain the map of the NMEA 2000 network.
			networkMap[header.source].productInformation = productInformation;
			networkMap[header.source].timestamp = wxDateTime::Now();

			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;

		case 127245: // Rudder
			if (supportedPGN & FLAGS_RDR) {
				result = DecodePGN127245(payload, &nmeaSentences);
			}
			break;
			
		case 127250: // Heading
			if (supportedPGN & FLAGS_HDG) {
				result = DecodePGN127250(payload, &nmeaSentences);
			}
			break;
			
		case 127251: // Rate of Turn
			if (supportedPGN & FLAGS_ROT) {
				result = DecodePGN127251(payload, &nmeaSentences);
			}
			break;
			
		case 127257: // Attitude
			if (supportedPGN & FLAGS_XDR) {
				result = DecodePGN127257(payload, &nmeaSentences);
			}
			break;
			
		case 127258: // Magnetic Variation
			// BUG BUG needs flags 
			// BUG BUG Not actually used anywhere
			result = DecodePGN127258(payload, &nmeaSentences);
			break;

		case 127488: // Engine Parameters, Rapid Update
			if (supportedPGN & FLAGS_ENG) {
				result = DecodePGN127488(payload, &nmeaSentences);
			}
			break;

		case 127489: // Engine Parameters, Dynamic
			if (supportedPGN & FLAGS_ENG) {
				result = DecodePGN127489(payload, &nmeaSentences);
			}
			break;

		case 127505: // Fluid Levels
			if (supportedPGN & FLAGS_TNK) {
				result = DecodePGN127505(payload, &nmeaSentences);
			}
			break;
			
		case 128259: // Boat Speed
			if (supportedPGN & FLAGS_VHW) {
				result = DecodePGN128259(payload, &nmeaSentences);
			}
			break;
			
		case 128267: // Water Depth
			if (supportedPGN & FLAGS_DPT) {
				result = DecodePGN128267(payload, &nmeaSentences);
			}
			break;
			
		case 129025: // Position - Rapid Update
			if (supportedPGN & FLAGS_GLL) {
				result = DecodePGN129025(payload, &nmeaSentences);
			}
			break;
		
		case 129026: // COG, SOG - Rapid Update
			if (supportedPGN & FLAGS_VTG) {
				result = DecodePGN129026(payload, &nmeaSentences);
			}
			break;
		
		case 129029: // GNSS Position
			if (supportedPGN & FLAGS_GGA) {
				result = DecodePGN129029(payload, &nmeaSentences);
			}
			break;
		
		case 129033: // Time & Date
			if (supportedPGN & FLAGS_ZDA) {
				result = DecodePGN129033(payload, &nmeaSentences);
			}
			break;
			
		case 129038: // AIS Class A Position Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129038(payload, &nmeaSentences);
			}
			break;
		
		case 129039: // AIS Class B Position Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129039(payload, &nmeaSentences);
			}
			break;
		
		case 129040: // AIS Class B Extended Position Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129040(payload, &nmeaSentences);
			}
			break;
		
		case 129041: // AIS Aids To Navigation (AToN) Position Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129041(payload, &nmeaSentences);
			}
			break;
		
		case 129283: // Cross Track Error
			if (supportedPGN & FLAGS_XTE) {
				result = DecodePGN129283(payload, &nmeaSentences);
			}
			break;
			
		case 129284: // Navigation Information
			if (supportedPGN & FLAGS_NAV) {
				result = DecodePGN129284(payload, &nmeaSentences);
			}
			break;
			
		case 129285: // Route & Waypoint Information
			if (supportedPGN & FLAGS_RTE) {
				result = DecodePGN129285(payload, &nmeaSentences);
			}
			break;

		case 129793: // AIS Position and Date Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129793(payload, &nmeaSentences);
			}
			break;
		
		case 129794: // AIS Class A Static & Voyage Related Data
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129794(payload, &nmeaSentences);
			}
			break;
		
		case 129798: // AIS Search and Rescue (SAR) Position Report
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129798(payload, &nmeaSentences);
			}
			break;
		
		case 129808: // Digital Selective Calling (DSC)
			if (supportedPGN & FLAGS_DSC) {
				result = DecodePGN129808(payload, &nmeaSentences);
			}
			break;
		
		case 129809: // AIS Class B Static Data, Part A
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129809(payload, &nmeaSentences);
			}
			break;
		
		case 129810: // Class B Static Data, Part B
			if (supportedPGN & FLAGS_AIS) {
				result = DecodePGN129810(payload, &nmeaSentences);
			}
			break;
		
		case 130306: // Wind data
			if (supportedPGN & FLAGS_MWV) {
				result = DecodePGN130306(payload, &nmeaSentences);
			}
			break;
		
		case 130310: // Environmental Parameters
			if (supportedPGN & FLAGS_MWT) {
				result = DecodePGN130310(payload, &nmeaSentences);
			}
			break;
			
		case 130311: // Environmental Parameters (supercedes 130310)
			if (supportedPGN & FLAGS_MWT) {
				result = DecodePGN130311(payload, &nmeaSentences);
			}
			break;
		
		case 130312: // Temperature
			if (supportedPGN & FLAGS_MWT) {
				result = DecodePGN130312(payload, &nmeaSentences);
			}
			break;
			
		case 130316: // Temperature Extended Range
			if (supportedPGN & FLAGS_MWT) {
				result = DecodePGN130316(payload, &nmeaSentences);
			}
			break;
				
		default:
			// BUG BUG Should we log an unsupported PGN error ??
			// No NMEA 0183 sentences to pass onto OpenCPN
			result = FALSE;
			break;
		}
		// Send each NMEA 0183 Sentence to OpenCPN
		if (result == TRUE) {
			for (std::vector<wxString>::iterator it = nmeaSentences.begin(); it != nmeaSentences.end(); ++it) {
				SendNMEASentence(*it);
			}
		}
	}
}

// Decode PGN 59904 ISO Request
int ActisenseDevice::DecodePGN59904(std::vector<byte> payload, unsigned int *requestedPGN) {
	if (payload.size() > 0) {
		*requestedPGN = payload[0] | (payload[1] << 8) | (payload[2] << 16);
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 60928 ISO Address Claim
int ActisenseDevice::DecodePGN60928(std::vector<byte> payload, DeviceInformation *deviceInformation) {
	if ((payload.size() > 0) && (deviceInformation != NULL)) {
		
		// Unique Identity Number 21 bits
		deviceInformation->uniqueId = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0x1FFFFF;
		
		// Manufacturer Code 11 bits
		deviceInformation->manufacturerId = ((payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0xFFE00000) >> 21;

		// Not really fussed about these
		// ISO ECU Instance 3 bits()
		//(payload[4] & 0xE0) >> 5;
		// ISO Function Instance 5 bits
		//payload[4] & 0x1F;

		// ISO Function Class 8 bits
		deviceInformation->deviceFunction = payload[5];

		// Reserved 1 bit
		//(payload[6] & 0x80) >> 7

		// Device Class 7 bits
		deviceInformation->deviceClass = payload[6] & 0x7F;

		// System Instance 4 bits
		deviceInformation->deviceInstance = payload[7] & 0x0F;

		// Industry Group 3 bits - Marine == 4
		deviceInformation->industryGroup = (payload[7] & 0x70) >> 4;

		// ISO Self Configurable 1 bit
		//payload[7] & 0x80) >> 7

		// NAME
		deviceInformation->deviceName = (unsigned long long)payload[0] | ((unsigned long long)payload[1] << 8) | ((unsigned long long)payload[2] << 16) | ((unsigned long long)payload[3] << 24) | ((unsigned long long)payload[4] << 32) | ((unsigned long long)payload[5] << 40) | ((unsigned long long)payload[6] << 48) | ((unsigned long long)payload[7] << 54);
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 65240 ISO Commanded Address
int ActisenseDevice::DecodePGN65240(std::vector<byte> payload, DeviceInformation *deviceInformation) {
	if ((payload.size() > 0) && (deviceInformation != NULL)) {
		
		// Unique Id 21 bits
		deviceInformation->uniqueId = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0x1FFFFF;
		
		// Manufacturer Code 11 bits
		deviceInformation->manufacturerId = ((payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24)) & 0xFFE00000) >> 21;

		// Not really fussed about these
		// ISO ECU Instance 3 bits()
		//(payload[4] & 0xE0) >> 5;
		// ISO Function Instance 5 bits
		//payload[4] & 0x1F;

		// ISO Function Class 8 bits
		deviceInformation->deviceFunction = payload[5];

		// Reserved 1 bit
		//(payload[6] & 0x80) >> 7

		// Device Class 7 bits
		deviceInformation->deviceClass = payload[6] & 0x7F;

		// System Instance 4 bits
		deviceInformation->deviceInstance = payload[7] & 0x0F;

		// Industry Group 3 bits - Marine == 4
		deviceInformation->industryGroup = (payload[7] & 0x70) >> 4;

		// ISO Self Configurable 1 bit
		//payload[7] & 0x80) >> 7
		
		// Commanded Network Address
		deviceInformation->networkAddress = payload[8];
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126992 NMEA System Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
bool ActisenseDevice::DecodePGN126992(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte timeSource;
		timeSource = (payload[1] & 0xF) >> 4;

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);
		
		if ((TwoCanUtils::IsDataValid(daysSinceEpoch)) && (TwoCanUtils::IsDataValid(secondsSinceMidnight))) {

			wxDateTime tm;
			tm.ParseDateTime("00:00:00 01-01-1970");
			tm += wxDateSpan::Days(daysSinceEpoch);
			tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight /10000);
			nmeaSentences->push_back(wxString::Format("$IIZDA,%s", tm.Format("%H%M%S.00,%d,%m,%Y,%z")));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126993 NMEA Heartbeat
bool ActisenseDevice::DecodePGN126993(const int source, std::vector<byte> payload) {
	if (payload.size() > 0) {

		unsigned short timeOffset;
		timeOffset = payload[0] | (payload[1] << 8);
		
		byte counter;
		counter = payload[2];
		
		// BUG BUG following are not correct

		byte class1CanState;
		class1CanState = payload[3] & 0x07;

		byte class2CanState;
		class2CanState = (payload[3] & 0x38) >> 3;
		
		byte equipmentState;
		equipmentState = payload[3] & 0x40 >> 6;

		// BUG BUG Remove for production once this has been tested
#ifndef NDEBUG
		wxLogMessage(_T("Actisense Heartbeat, Source: %d, Time: %d, Count: %d, CAN 1: %d, CAN 2: %d"), source, timeOffset, counter, class1CanState, class2CanState);
#endif
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 126996 NMEA Product Information
int ActisenseDevice::DecodePGN126996(std::vector<byte> payload, ProductInformation *productInformation) {
	if ((payload.size() > 0) && (productInformation != NULL)) {

		// Should divide by 100 to get the correct displayable version
		productInformation->dataBaseVersion = payload[0] | (payload[1] << 8);

		productInformation->productCode = payload[2] | (payload[3] << 8);

		// Each of the following strings are up to 32 bytes long, and NOT NULL terminated.

		// Model ID Bytes [4] - [35]
		memset(&productInformation->modelId[0], '\0' ,32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[4 + j])) {
				productInformation->modelId[j] = payload[4 + j];
			}
		}

		// Software Version Bytes [36] - [67]
		memset(&productInformation->softwareVersion[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[36 + j])) {
				productInformation->softwareVersion[j] = payload[36 + j];
			}
		}

		// Model Version Bytes [68] - [99]
		memset(&productInformation->modelVersion[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[68 + j])) {
				productInformation->modelVersion[j] = payload[68 + j];
			}
		}

		// Serial Number Bytes [100] - [131]
		memset(&productInformation->serialNumber[0], '\0', 32);
		for (int j = 0; j < 31; j++) {
			if (isprint(payload[100 + j])) {
				productInformation->serialNumber[j] = payload[100 + j];
			}
		}

		productInformation->certificationLevel = payload[132];
		productInformation->loadEquivalency = payload[133];

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127245 NMEA Rudder
// $--RSA, x.x, A, x.x, A*hh<CR><LF>
bool ActisenseDevice::DecodePGN127245(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte instance;
		instance = payload[0];

		byte directionOrder;
		directionOrder = payload[1] & 0x03;

		short angleOrder; // 0001 radians
		angleOrder = payload[3] | (payload[4] << 8);

		short position; // 0001 radians
		position = payload[5] | (payload[6] << 8);

		if (TwoCanUtils::IsDataValid(position)) {
			// Main (or Starboard Rudder
			if (instance == 0) { 
				nmeaSentences->push_back(wxString::Format("$IIRSA,%.2f,A,0.0,V", RADIANS_TO_DEGREES((float)position / 10000)));
				return TRUE;
			}
			// Port Rudder
			if (instance == 1) {
				nmeaSentences->push_back(wxString::Format("$IIRSA,0.0,V,%.2f,A", RADIANS_TO_DEGREES((float)position / 10000)));
				return TRUE;
			}
			return FALSE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127250 NMEA Vessel Heading
// $--HDG, x.x, x.x, a, x.x, a*hh<CR><LF>
// $--HDT,x.x,T*hh<CR><LF>
bool ActisenseDevice::DecodePGN127250(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short heading;
		heading = payload[1] | (payload[2] << 8);

		short deviation;
		deviation = payload[3] | (payload[4] << 8);

		short variation;
		variation = payload[5] | (payload[6] << 8);

		byte headingReference;
		headingReference = (payload[7] & 0x03);
		
		// Sign of variation and deviation corresponds to East (E) or West (W)
		
		if (headingReference == HEADING_MAGNETIC) {
		
			if (TwoCanUtils::IsDataValid(heading)) {
				
				nmeaSentences->push_back(wxString::Format("$IIHDM,%.2f", RADIANS_TO_DEGREES((float)heading / 10000)));
			
				if (TwoCanUtils::IsDataValid(deviation)) {
				
					if (TwoCanUtils::IsDataValid(variation)) {
						// heading, deviation and variation all valid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,%.2f,%c,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W', \
							RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
				
					else {
						// heading, deviation are valid, variation invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,%.2f,%c,,", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)deviation / 10000), deviation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
				}
				
				else {
					if (TwoCanUtils::IsDataValid(variation)) {
						// heading and variation valid, deviation invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,,,%.2f,%c", RADIANS_TO_DEGREES((float)heading / 10000), \
							RADIANS_TO_DEGREES((float)variation / 10000), variation >= 0 ? 'E' : 'W'));
						return TRUE;
					}
					else {
						// heading valid, deviation and variation both invalid
						nmeaSentences->push_back(wxString::Format("$IIHDG,%.2f,,,,", RADIANS_TO_DEGREES((float)heading / 10000)));
						return TRUE;
		
					}	
					
				}
			
			}
			else {
				return FALSE;
			}
		}
		else if (headingReference == HEADING_TRUE) {
			if (TwoCanUtils::IsDataValid(heading)) {
				nmeaSentences->push_back(wxString::Format("$IIHDT,%.2f", RADIANS_TO_DEGREES((float)heading / 10000)));
				return TRUE;
			}
			else {
				return FALSE;
			}
			
		}
		else {
			return FALSE;
		}
		
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127251 NMEA Rate of Turn (ROT)
// $--ROT,x.x,A*hh<CR><LF>
bool ActisenseDevice::DecodePGN127251(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		int rateOfTurn;
		rateOfTurn = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		// convert radians per second to degress per minute
		// -ve sign means turning to port
		
		if (TwoCanUtils::IsDataValid(rateOfTurn)) {
			nmeaSentences->push_back(wxString::Format("$IIROT,%.2f,A", RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-8)));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127257 NMEA Attitude
// $--XDR,a,x.x,a,c--c,...…………...a,x.x,a,c--c*hh<CR><LF>
//        |  |  |   |      |        |
//        |  |  |   |      |      Transducer 'n'1
//        |  |  |   |   Data for variable # of transducers
//        |  |  | Transducer #1 ID
//        |  | Units of measure, Transducer #12
//        | Measurement data, Transducer #1
//     Transducer type, Transducer #1
// Yaw, Pitch & Roll - Transducer type is A (Angular displacement), Units of measure is D (degrees)

bool ActisenseDevice::DecodePGN127257(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		short yaw;
		yaw = payload[1] | (payload[2] << 8);

		short pitch;
		pitch = payload[3] | (payload[4] << 8);

		short roll;
		roll = payload[5] | (payload[6] << 8);

		wxString xdrString;

		// BUG BUG Not sure if Dashboard supports yaw and whether roll should be ROLL or HEEL
		if (TwoCanUtils::IsDataValid(yaw)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,YAW,", RADIANS_TO_DEGREES((float)yaw / 10000)));
		}

		if (TwoCanUtils::IsDataValid(pitch)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,PTCH,", RADIANS_TO_DEGREES((float)pitch / 10000)));
		}

		if (TwoCanUtils::IsDataValid(roll)) {
			xdrString.Append(wxString::Format("A,%0.2f,D,HEEL,", RADIANS_TO_DEGREES((float)roll / 10000)));
		}

		if (xdrString.length() > 0) {
			xdrString.Prepend("IIXDR,");
			nmeaSentences->push_back(xdrString);
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}


// Decode PGN 127258 NMEA Magnetic Variation
bool ActisenseDevice::DecodePGN127258(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte variationSource; //4 bits
		variationSource = payload[1] & 0x0F;

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[2] | (payload[3] << 8);

		short variation;
		variation = payload[4] | (payload[5] << 8);

		// to calculate variation: RADIANS_TO_DEGREES((float)variation / 10000);

		// BUG BUG Needs to be added to other sentences such as HDG and RMC conversions
		// As there is no direct NMEA 0183 sentence just for variation
		return FALSE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127488 NMEA Engine Parameters, Rapid Update
bool ActisenseDevice::DecodePGN127488(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte engineInstance;
		engineInstance = payload[0];

		unsigned short engineSpeed;
		engineSpeed = payload[1] | (payload[2] << 8);

		unsigned short engineBoostPressure;
		engineBoostPressure = payload[3] | (payload[4] << 8);

		// BUG BUG Need to clarify units & resolution, although unlikely to use this anywhere
		short engineTrim;
		engineTrim = payload[5];

		// Note that until we receive data from engine instance 1, we will always assume it is a single engine vessel
		if (engineInstance > 0) {
			IsMultiEngineVessel = TRUE;
		}

		if (TwoCanUtils::IsDataValid(engineSpeed)) {

			switch (engineInstance) {
				// Note use of flag to identify whether single engine or dual engine as
				// engineInstance 0 in a dual engine configuration is the port engine
				// BUG BUG Should I use XDR or RPM sentence ?? Depends on how I code the Engine Dashboard !!
			case 0:
				if (IsMultiEngineVessel) {
					nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,PORT", engineSpeed * 0.25f));
					// nmeaSentences->push_back(wxString::Format("$IIRPM,E,2,%.2f,,A", engineSpeed * 0.25f));
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,MAIN", engineSpeed * 0.25f));
					// nmeaSentences->push_back(wxString::Format("$IIRPM,E,0,%.2f,,A", engineSpeed * 0.25f));
				}
				break;
			case 1:
				nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,STBD", engineSpeed * 0.25f));
				// nmeaSentences->push_back(wxString::Format("$IIRPM,E,1,%.2f,,A", engineSpeed * 0.25f));
				break;
			default:
				nmeaSentences->push_back(wxString::Format("$IIXDR,T,%.2f,R,MAIN", engineSpeed * 0.25f));
				// nmeaSentences->push_back(wxString::Format("$IIRPM,E,0,%.2f,,A", engineSpeed * 0.25f));
				break;
				
				// FYI Signal K Data Format
				//
				// {
				// "context": "vessels.urn:mrn:imo:mmsi:234567890",
				// "updates": [
				//{
				// "source": {
				// "label": "N2000-01",
				// "type": "NMEA2000",
				// "src": "017",
				// "pgn": 127488
				// },
				// "timestamp": "2010-01-07T07:18:44Z",
				// "values": [
				// {
				// "path": "propulsion.0.revolutions",
				// "value": 16.341667
				//},
				// {
				// "path": "propulsion.0.boostPressure",
				// "value": 45500
				// }
				// ]
				// }
				// ]
				// } 
				//

			}
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127489 NMEA Engine Parameters, Dynamic
bool ActisenseDevice::DecodePGN127489(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte engineInstance;
		engineInstance = payload[0];

		unsigned short oilPressure; // hPa (1hPa = 100Pa)
		oilPressure = payload[1] | (payload[2] << 8);

		unsigned short oilTemperature; // 0.01 degree resolution, in Kelvin
		oilTemperature = payload[3] | (payload[4] << 8);

		unsigned short engineTemperature; // 0.01 degree resolution, in Kelvin
		engineTemperature = payload[5] | (payload[6] << 8);

		unsigned short alternatorPotential; // 0.01 Volts
		alternatorPotential = payload[7] | (payload[8] << 8);

		unsigned short fuelRate; // 0.1 Litres/hour
		fuelRate = payload[9] | (payload[10] << 8);

		unsigned short totalEngineHours;  // seconds
		totalEngineHours = payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24);

		unsigned short coolantPressure; // hPA
		coolantPressure = payload[15] | (payload[16] << 8);

		unsigned short fuelPressure; // hPa
		fuelPressure = payload[17] | (payload[18] << 8);

		unsigned short reserved;
		reserved = payload[19];

		short statusOne;
		statusOne = payload[20] | (payload[21] << 8);
		// BUG BUG Think of using XDR switch status with meaningful naming
		// XDR parameters, "S", No units, "1" = On, "0" = Off
		// Eg. "$IIXDR,S,1,,S100,S,1,,S203" to indicate Status One - Check Engine, Status 2 - Maintenance Needed
		// BUG BUG Would need either icons or text messages to display the status in the dashboard
		// {"0": "Check Engine"},
		// { "1": "Over Temperature" },
		// { "2": "Low Oil Pressure" },
		// { "3": "Low Oil Level" },
		// { "4": "Low Fuel Pressure" },
		// { "5": "Low System Voltage" },
		// { "6": "Low Coolant Level" },
		// { "7": "Water Flow" },
		// { "8": "Water In Fuel" },
		// { "9": "Charge Indicator" },
		// { "10": "Preheat Indicator" },
		// { "11": "High Boost Pressure" },
		// { "12": "Rev Limit Exceeded" },
		// { "13": "EGR System" },
		// { "14": "Throttle Position Sensor" },
		// { "15": "Emergency Stop" }]

		short statusTwo;
		statusTwo = payload[22] | (payload[23] << 8);

		// {"0": "Warning Level 1"},
		// { "1": "Warning Level 2" },
		// { "2": "Power Reduction" },
		// { "3": "Maintenance Needed" },
		// { "4": "Engine Comm Error" },
		// { "5": "Sub or Secondary Throttle" },
		// { "6": "Neutral Start Protect" },
		// { "7": "Engine Shutting Down" }]

		byte engineLoad;  // percentage
		engineLoad = payload[24];

		byte engineTorque; // percentage
		engineTorque = payload[25];

		// As above, until data is received from engine instance 1 we always assume a single engine vessel
		if (engineInstance > 0) {
			IsMultiEngineVessel = TRUE;
		}

		// BUG BUG Instead of using logical and, separate into separate sentences so if invalid value for one or two sensors, we still send something
		if ((TwoCanUtils::IsDataValid(oilPressure)) && (TwoCanUtils::IsDataValid(engineTemperature)) && (TwoCanUtils::IsDataValid(alternatorPotential))) {

			switch (engineInstance) {
			case 0:
				if (IsMultiEngineVessel) {
					nmeaSentences->push_back(wxString::Format("$IIXDR,P,%.2f,P,PORT,C,%.2f,C,PORT,U,%.2f,V,PORT", (float)(oilPressure * 100.0f), (float)(engineTemperature * 0.01f) + CONST_KELVIN, (float)(alternatorPotential * 0.01f)));
					// Type G = Generic, I'm defining units as H to define hours
					nmeaSentences->push_back(wxString::Format("$IIXDR,G,%.2f,H,PORT", (float)totalEngineHours / 3600));
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIXDR,P,%.2f,P,MAIN,C,%.2f,C,MAIN,U,%.2f,V,MAIN", (float)(oilPressure * 100.0f), (float)(engineTemperature * 0.01f) + CONST_KELVIN, (float)(alternatorPotential * 0.01f)));
					nmeaSentences->push_back(wxString::Format("$IIXDR,G,%.2f,H,MAIN", (float)totalEngineHours / 3600));
				}
				break;
			case 1:
				nmeaSentences->push_back(wxString::Format("$IIXDR,P,%.2f,P,STBD,C,%.2f,C,STBD,U,%.2f,V,STBD", (float)(oilPressure * 100.0f), (float)(engineTemperature * 0.01f) + CONST_KELVIN, (float)(alternatorPotential * 0.01f)));
				nmeaSentences->push_back(wxString::Format("$IIXDR,G,%.2f,H,STBD", (float)totalEngineHours / 3600));
				break;
			default:
				nmeaSentences->push_back(wxString::Format("$IIXDR,P,%.2f,P,MAIN,C,%.2f,C,MAIN,U,%.2f,V,MAIN", (float)(oilPressure * 100.0f), (float)(engineTemperature * 0.01f) + CONST_KELVIN, (float)(alternatorPotential * 0.01f)));
				nmeaSentences->push_back(wxString::Format("$IIXDR,G,%.2f,H,MAIN", (float)totalEngineHours / 3600));
				break;
			}
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127505 NMEA Fluid Levels
bool ActisenseDevice::DecodePGN127505(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte instance;
		instance = payload[0] & 0xF;

		byte tankType;
		tankType = (payload[0] & 0xF0) >> 4;

		unsigned short tankLevel; // percentage in 0.025 increments
		tankLevel = payload[1] | (payload[2] << 8);

		unsigned int tankCapacity; // 0.1 L
		tankCapacity = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		if ((TwoCanUtils::IsDataValid(tankLevel)) && (TwoCanUtils::IsDataValid(tankCapacity))) {
			switch (tankType) {
				// Using Type = V (Volume) but units = P to indicate percentage rather than M (Cubic Meters)
			case 0:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,FUEL", (float)tankLevel * 0.025f));
				break;
			case 1:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,H20", (float)tankLevel * 0.025f));
				break;
			case 2:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,GREY", (float)tankLevel * 0.025f));
				break;
			case 3:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,LIVE", (float)tankLevel * 0.025f));
				break;
			case 4:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,OIL", (float)tankLevel * 0.025f));
				break;
			case 5:
				nmeaSentences->push_back(wxString::Format("$IIXDR,V,%.2f,P,BLK", (float)tankLevel * 0.025f));
				break;
			}
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 127508 NMEA Battery Status
bool ActisenseDevice::DecodePGN127508(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte batteryInstance;
		batteryInstance = payload[0] & 0xF;

		unsigned short batteryVoltage; // 0.01 volts
		batteryVoltage = payload[1] | (payload[2] << 8);

		unsigned short batteryCurrent; // 0.1 amps	
		batteryCurrent = payload[3] | (payload[4] << 8);
		
		unsigned short batteryTemperature; // 0.01 degree resolution, in Kelvin
		batteryTemperature = payload[5] | (payload[6] << 8);
		
		byte sid;
		sid = payload[7];
		
		// Using Transducer Type = U (Electrical), Units V (Voltage), A (Current in amps), C (Temperature in Celsius)
		// Assuming battery instance 0 = STRT (Start or Engine battery) , 1 = HOUS (House or Auxilliary battery)"
		
		if ((TwoCanUtils::IsDataValid(batteryVoltage)) && (TwoCanUtils::IsDataValid(batteryCurrent))) {
			if (batteryInstance == 0) { 
				nmeaSentences->push_back(wxString::Format("$IIXDR,U,%.2f,V,STRT,U,%.2f,A,STRT,C,%.2f,C,STRT", 
				(float)(batteryVoltage * 0.01f), (float)(batteryCurrent * 0.1f), (float)(batteryTemperature * 0.01f) + CONST_KELVIN));			
			}
			else { // Assume any instance other than 0 is a house or auxilliary battery
				nmeaSentences->push_back(wxString::Format("$IIXDR,U,%.2f,V,HOUS,U,%.2f,A,HOUS,C,%.2f,C,HOUS", 
				(float)(batteryVoltage * 0.01f), (float)(batteryCurrent * 0.1f), (float)(batteryTemperature * 0.01f) + CONST_KELVIN));			
			}
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
			return FALSE;
	}
}

// Decode PGN 128259 NMEA Speed & Heading
// $--VHW, x.x, T, x.x, M, x.x, N, x.x, K*hh<CR><LF>
bool ActisenseDevice::DecodePGN128259(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short speedWaterReferenced;
		speedWaterReferenced = payload[1] | (payload[2] << 8);

		unsigned short speedGroundReferenced;
		speedGroundReferenced = payload[3] | (payload[4] << 8);
		
		if (TwoCanUtils::IsDataValid(speedWaterReferenced)) {

			// BUG BUG Maintain heading globally from other sources to insert corresponding values into sentence	
			nmeaSentences->push_back(wxString::Format("$IIVHW,,T,,M,%.2f,N,%.2f,K", (float)speedWaterReferenced * CONVERT_MS_KNOTS / 100, \
				(float)speedWaterReferenced * CONVERT_MS_KMH / 100));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 128267 NMEA Depth
// $--DPT,x.x,x.x,x.x*hh<CR><LF>
// $--DBT,x.x,f,x.x,M,x.x,F*hh<CR><LF>
bool ActisenseDevice::DecodePGN128267(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short depth;
		depth = payload[1] | (payload[2] << 8);

		short offset;
		offset = payload[3] | (payload[4] << 8);

		unsigned short maxRange;
		maxRange = payload[5] | (payload[6] << 8);

		if (TwoCanUtils::IsDataValid(depth)) {
			
			// return wxString::Format("$IIDPT,%.2f,%.2f,%.2f", (float)depth / 100, (float)offset / 100, \
			//	((maxRange != 0xFFFF) && (maxRange > 0)) ? maxRange / 100 : (int)NULL);
		
			// OpenCPN Dashboard only accepts DBT sentence
			nmeaSentences->push_back(wxString::Format("$IIDBT,%.2f,f,%.2f,M,%.2f,F", CONVERT_METRES_FEET * (double)depth / 100, \
				(double)depth / 100, CONVERT_METRES_FATHOMS * (double)depth / 100));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 128275 NMEA Distance Log
// $--VLW, x.x, N, x.x, N, x.x, N, x.x, N*hh<CR><LF>
//          |       |       |       Total cumulative water distance, Nm
//          |       |       Water distance since reset, Nm
//          |      Total cumulative ground distance, Nm
//          Ground distance since reset, Nm

bool ActisenseDevice::DecodePGN128275(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		unsigned int cumulativeDistance;
		cumulativeDistance = payload[7] | (payload[8] << 8) | (payload[9] << 16) | (payload[10] << 24);

		unsigned int tripDistance;
		tripDistance = payload[11] | (payload[12] << 8) | (payload[13] << 16) | (payload[14] << 24);

		if (TwoCanUtils::IsDataValid(cumulativeDistance)) {
			if (TwoCanUtils::IsDataValid(tripDistance)) {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,%.2f,N,%.2f,N", CONVERT_METRES_NAUTICAL_MILES * tripDistance, CONVERT_METRES_NAUTICAL_MILES * cumulativeDistance));
				return TRUE;
			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,,N,%.2f,N", CONVERT_METRES_NAUTICAL_MILES * cumulativeDistance));
				return TRUE;
			}
		}
		else {
			if (TwoCanUtils::IsDataValid(tripDistance)) {
				nmeaSentences->push_back(wxString::Format("$IIVLW,,,,,%.2f,N,,N", CONVERT_METRES_NAUTICAL_MILES * tripDistance));
				return TRUE;
			}
			else {
				return FALSE;
			}
			
		}
					
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129025 NMEA Position Rapid Update
// $--GLL, llll.ll, a, yyyyy.yy, a, hhmmss.ss, A, a*hh<CR><LF>
//                                           Status A valid, V invalid
//                                               mode - note Status = A if Mode is A (autonomous) or D (differential)
bool ActisenseDevice::DecodePGN129025(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {
		
		int latitude;
		latitude = (int)payload[0] | ((int)payload[1] << 8) | ((int)payload[2] << 16) | ((int)payload[3] << 24);

		int longitude;
		longitude = (int)payload[4] | ((int)payload[5] << 8) | ((int)payload[6] << 16) | ((int)payload[7] << 24);

		if (TwoCanUtils::IsDataValid(latitude) && TwoCanUtils::IsDataValid(longitude)) {

			double latitudeDouble = ((double)latitude * 1e-7);
			double latitudeDegrees = trunc(latitudeDouble);
			double latitudeMinutes = (latitudeDouble - latitudeDegrees) * 60;

			double longitudeDouble = ((double)longitude * 1e-7);
			double longitudeDegrees = trunc(longitudeDouble);
			double longitudeMinutes = (longitudeDouble - longitudeDegrees) * 60;

			char gpsMode;
			gpsMode = 'A';

			// BUG BUG Verify S & W values are indeed negative
			// BUG BUG Mode & Status are not available in PGN 129025
			// BUG BUG UTC Time is not available in  PGN 129025

			wxDateTime tm;
			tm = wxDateTime::Now();

			nmeaSentences->push_back(wxString::Format("$IIGLL,%02.0f%07.4f,%c,%03.0f%07.4f,%c,%s,%c,%c", abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', \
				abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', tm.Format("%H%M%S.00").ToAscii(), gpsMode, ((gpsMode == 'A') || (gpsMode == 'D')) ? 'A' : 'V'));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129026 NMEA COG SOG Rapid Update
// $--VTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh<CR><LF>
bool ActisenseDevice::DecodePGN129026(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		// True = 0, Magnetic = 1
		byte headingReference;
		headingReference = (payload[1] & 0x03);

		unsigned short courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned short speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		// BUG BUG if Heading Ref = True (0), then ignore %.2f,M and vice versa if Heading Ref = Magnetic (1), ignore %.2f,T
		// BUG BUG GPS Mode should be obtained rather than assumed
		
		if (headingReference == HEADING_TRUE) {
			if (TwoCanUtils::IsDataValid(courseOverGround)) {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,,M,,N,,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
			}
			else {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,,M,%.2f,N,%.2f,K,%c", \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;
				}
				else {
					return FALSE;
				}
				
			}
			
		}
		
		else if (headingReference == HEADING_MAGNETIC) {
			if (TwoCanUtils::IsDataValid(courseOverGround)) {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
				else {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,%.2f,M,,N,,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), GPS_MODE_AUTONOMOUS));
					return TRUE;								
				}
			}
			else {
				if (TwoCanUtils::IsDataValid(speedOverGround)) {
					nmeaSentences->push_back(wxString::Format("$IIVTG,,T,,M,%.2f,N,%.2f,K,%c", \
					(float)speedOverGround * CONVERT_MS_KNOTS / 100, (float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
					return TRUE;
				}
				else {
					return FALSE;
				}
				
			}
			
			
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}	
}

// Decode PGN 129029 NMEA GNSS Position
// $--GGA, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x, xx, x.x, x.x, M, x.x, M, x.x, xxxx*hh<CR><LF>
//                                             |  |   hdop         geoidal  age refID 
//                                             |  |        Alt
//                                             | sats
//                                           fix Qualty

bool ActisenseDevice::DecodePGN129029(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[1] | (payload[2] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[3] | (payload[4] << 8) | (payload[5] << 16) | (payload[6] << 24);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		long long latitude;
		latitude = (((long long)payload[7] | ((long long)payload[8] << 8) | ((long long)payload[9] << 16) | ((long long)payload[10] << 24) \
			| ((long long)payload[11] << 32) | ((long long)payload[12] << 40) | ((long long)payload[13] << 48) | ((long long)payload[14] << 56)));


		long long longitude;
		longitude = (((long long)payload[15] | ((long long)payload[16] << 8) | ((long long)payload[17] << 16) | ((long long)payload[18] << 24) \
			| ((long long)payload[19] << 32) | ((long long)payload[20] << 40) | ((long long)payload[21] << 48) | ((long long)payload[22] << 56)));

		if (TwoCanUtils::IsDataValid(latitude) && TwoCanUtils::IsDataValid(longitude)) {

			double latitudeDouble = ((double)latitude * 1e-16);
			double latitudeDegrees = trunc(latitudeDouble);
			double latitudeMinutes = (latitudeDouble - latitudeDegrees) * 60;

			double longitudeDouble = ((double)longitude * 1e-16);
			double longitudeDegrees = trunc(longitudeDouble);
			double longitudeMinutes = (longitudeDouble - longitudeDegrees) * 60;

			double altitude;
			altitude = 1e-6 * (((long long)payload[23] | ((long long)payload[24] << 8) | ((long long)payload[25] << 16) | ((long long)payload[26] << 24) \
				| ((long long)payload[27] << 32) | ((long long)payload[28] << 40) | ((long long)payload[29] << 48) | ((long long)payload[30] << 56)));


			byte fixType;
			byte fixMethod;

			fixType = (payload[31] & 0xF0) >> 4;
			fixMethod = payload[31] & 0x0F;

			byte fixIntegrity;
			fixIntegrity = payload[32] & 0x03;

			byte numberOfSatellites;
			numberOfSatellites = payload[33];

			unsigned short hDOP;
			hDOP = payload[34] | (payload[35] << 8);

			unsigned short pDOP;
			pDOP = payload[36] | (payload[37] << 8);

			unsigned long geoidalSeparation; //0.01
			geoidalSeparation = payload[38] | (payload[39] << 8);

			byte referenceStations;
			referenceStations = payload[40];

			unsigned short referenceStationType;
			unsigned short referenceStationID;
			unsigned short referenceStationAge;

			// We only need one reference station for the GGA sentence
			if (referenceStations != 0xFF && referenceStations > 0) {
				// BUG BUG Check this, may have bit orders wrong
				referenceStationType = payload[43] & 0xF0 >> 4;
				referenceStationID = (payload[43] & 0xF) << 4 | payload[44];
				referenceStationAge = (payload[45] | (payload[46] << 8));
			}

			nmeaSentences->push_back(wxString::Format("$IIGGA,%s,%02.0f%07.4f,%c,%03.0f%07.4f,%c,%d,%d,%.2f,%.1f,M,%.1f,M,,", \
				tm.Format("%H%M%S").ToAscii(), fabs(latitudeDegrees), fabs(latitudeMinutes), latitudeDegrees >= 0 ? 'N' : 'S', \
				fabs(longitudeDegrees), fabs(longitudeMinutes), longitudeDegrees >= 0 ? 'E' : 'W', \
				fixType, numberOfSatellites, (double)hDOP * 0.01f, (double)altitude * 1e-6, \
				(double)geoidalSeparation * 0.01f));
			return TRUE;

			// BUG BUG for the time being ignore reference stations, too lazy to code this
			//, \
			//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationAge : "", \
			//	((referenceStations != 0xFF) && (referenceStations > 0)) ? referenceStationID : "");
			//
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129033 NMEA Date & Time
// $--ZDA, hhmmss.ss, xx, xx, xxxx, xx, xx*hh<CR><LF>
bool ActisenseDevice::DecodePGN129033(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {
		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[0] | (payload[1] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		short localOffset;
		localOffset = payload[6] | (payload[7] << 8);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);
		
		nmeaSentences->push_back(wxString::Format("$IIZDA,%s,%d,%d", tm.Format("%H%M%S,%d,%m,%Y"), (int)localOffset / 60, localOffset % 60));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Template for NMEA183 AIS VDM messages
// !--VDM,x,x,x,a,s--s,x*hh
//       | | | |   |  Number of fill bits
//       | | | |   Encoded Message
//       | | | AIS Channel
//       | | Sequential Message ID
//       | Sentence Number
//      Total Number of sentences


// Decode PGN 129038 NMEA AIS Class A Position Report
// AIS Message Types 1,2 or 3
bool ActisenseDevice::DecodePGN129038(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16) & 0x7FFFF);

		int transceiverInformation; // unused in NMEA183 conversion
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		int rateOfTurn;
		rateOfTurn = payload[23] | (payload[24] << 8);

		int navigationalStatus;
		navigationalStatus = payload[25] & 0x0F;
		
		int reserved;
		reserved = payload[25] & 0x30 >> 4;

		int manoeuverIndicator;
		manoeuverIndicator = (payload[25] & 0xC0) >> 6;

		int spare;
		spare = (payload[26] & 0x07);

		int reservedForRegionalApplications;
		reservedForRegionalApplications = (payload[26] & 0xF8) >> 3;

		int sequenceID;
		sequenceID = (payload[26] & 0xC0) >> 6;

		// Encode correct AIS rate of turn from sensor data as per ITU M.1371 standard
		// BUG BUG fix this up to remove multiple calculations. 
		int AISRateOfTurn;

		// Undefined/not available
		if (rateOfTurn == 0xFFFF) {
			AISRateOfTurn = -128;
		}
		// Greater or less than 708 degrees/min
		else if ((RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-8) * 60) > 708) {
			AISRateOfTurn = 127;
		}

		else if ((RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-8) * 60) < -708) {
			AISRateOfTurn = -127;
		}

		else {
			AISRateOfTurn = 4.733 * sqrt(RADIANS_TO_DEGREES((float)rateOfTurn * 3.125e-8) * 60);
		}


		// Encode VDM message using 6 bit ASCII 

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 4, navigationalStatus);
		AISInsertInteger(binaryData, 42, 8, AISRateOfTurn);
		AISInsertInteger(binaryData, 50, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 89, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 116, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 128, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 137, 6, timeStamp);
		AISInsertInteger(binaryData, 143, 2, manoeuverIndicator);
		AISInsertInteger(binaryData, 145, 3, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);

		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,0", AISEncodePayload(binaryData)));

		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129039 NMEA AIS Class B Position Report
// AIS Message Type 18
bool ActisenseDevice::DecodePGN129039(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);
						
		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;
		
		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);
		
		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround =payload[14] | (payload[15] << 8);

		int  speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x7) << 16);

		int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int trueHeading;
		trueHeading = RADIANS_TO_DEGREES((payload[21] | (payload[22] << 8)));

		int regionalReservedA;
		regionalReservedA = payload[23];

		int regionalReservedB;
		regionalReservedB = payload[24] & 0x03;

		int unitFlag;
		unitFlag = (payload[24] & 0x04) >> 2;

		int displayFlag;
		displayFlag = (payload[24] & 0x08) >> 3;

		int dscFlag;
		dscFlag = (payload[24] & 0x10) >> 4;

		int bandFlag;
		bandFlag = (payload[24] & 0x20) >> 5;

		int msg22Flag;
		msg22Flag = (payload[24] & 0x40) >> 6;

		int assignedModeFlag;
		assignedModeFlag = (payload[24] & 0x80) >> 7;
		
		int	sotdmaFlag;
		sotdmaFlag = payload[25] & 0x01;
		
		// Encode VDM Message using 6bit ASCII
				
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, 0xFF); // spare
		AISInsertInteger(binaryData, 46, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 85, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 112, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 124, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 133, 6, timeStamp);
		AISInsertInteger(binaryData, 139, 2, regionalReservedB);
		AISInsertInteger(binaryData, 141, 1, unitFlag);
		AISInsertInteger(binaryData, 142, 1, displayFlag);
		AISInsertInteger(binaryData, 143, 1, dscFlag);
		AISInsertInteger(binaryData, 144, 1, bandFlag);
		AISInsertInteger(binaryData, 145, 1, msg22Flag);
		AISInsertInteger(binaryData, 146, 1, assignedModeFlag); 
		AISInsertInteger(binaryData, 147, 1, raimFlag); 
		AISInsertInteger(binaryData, 148, 1, sotdmaFlag); 
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129040 AIS Class B Extended Position Report
// AIS Message Type 19
bool ActisenseDevice::DecodePGN129040(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(312);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int regionalReservedA;
		regionalReservedA = payload[18];

		int regionalReservedB;
		regionalReservedB = payload[19] & 0x0F;

		int reservedA;
		reservedA = (payload[19] & 0xF0) >> 4;

		int shipType;
		shipType = payload[20];

		int trueHeading;
		trueHeading = payload[21] | (payload[22] << 8);

		int reservedB;
		reservedB = payload[23] & 0x0F;

		int gnssType;
		gnssType = (payload[23] & 0xF0) >> 4;

		int shipLength;
		shipLength = payload[24] | (payload[25] << 8);
		
		int shipBeam;
		shipBeam = payload[26] | (payload[27] << 8);

		int refStarboard;
		refStarboard = payload[28] | (payload[29] << 8);

		int refBow;
		refBow = payload[30] | (payload[31] << 8);
		
		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName += static_cast<char>(payload[32 + i]);
		}
		
		int dteFlag;
		dteFlag = payload[52] & 0x01;

		int assignedModeFlag;
		assignedModeFlag = payload[52] & 0x02 >> 1;

		int spare;
		spare = (payload[52] & 0x0C) >> 2;

		int AisTransceiverInformation;
		AisTransceiverInformation = (payload[52] & 0xF0) >> 4;

		// Encode VDM Message using 6bit ASCII
			
		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 8, regionalReservedA);
		AISInsertInteger(binaryData, 46, 10, CONVERT_MS_KNOTS * speedOverGround * 0.1f);
		AISInsertInteger(binaryData, 56, 1, positionAccuracy);
		AISInsertInteger(binaryData, 57, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 85, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 112, 12, RADIANS_TO_DEGREES((float)courseOverGround) * 0.001f);
		AISInsertInteger(binaryData, 124, 9, RADIANS_TO_DEGREES((float)trueHeading) * 0.0001f);
		AISInsertInteger(binaryData, 133, 6, timeStamp);
		AISInsertInteger(binaryData, 139, 4, regionalReservedB);
		AISInsertString(binaryData, 143, 120, shipName);
		AISInsertInteger(binaryData, 263, 8, shipType);
		AISInsertInteger(binaryData, 271, 9, refBow / 10);
		AISInsertInteger(binaryData, 280, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 289, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 295, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 301, 4, gnssType);
		AISInsertInteger(binaryData, 305, 1, raimFlag);
		AISInsertInteger(binaryData, 306, 1, dteFlag);
		AISInsertInteger(binaryData, 307, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 308, 4, spare);

		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message, Note no fillbits
		// One day I'll remember why I chose 28 as the length of a multisentence VDM message
		// BUG BUG Or just send two messages, 26 bytes long
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);
		
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, 28)));
			}
		}
		
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
	}

	else {
		return FALSE;
	}
}

// Decode PGN 129041 AIS Aids To Navigation (AToN) Report
// AIS Message Type 21
bool ActisenseDevice::DecodePGN129041(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(358);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int shipLength;
		shipLength = payload[14] | (payload[15] << 8);

		int shipBeam;
		shipBeam = payload[16] | (payload[17] << 8);

		int refStarboard;
		refStarboard = payload[18] | (payload[19] << 8);

		int refBow;
		refBow = payload[20] | (payload[21] << 8);
		
		int AToNType;
		AToNType = (payload[22] & 0xF8) >> 3;

		int offPositionFlag;
		offPositionFlag = (payload[22]  & 0x04) >> 2;

		int virtualAToN;
		virtualAToN = (payload[22] & 0x02) >> 1;;

		int assignedModeFlag;
		assignedModeFlag = payload[22] & 0x01;

		int spare;
		spare = payload[23] & 0x01;

		int gnssType;
		gnssType = (payload[23] & 0x1E) >> 1;

		int reserved;
		reserved = payload[23] & 0xE0 >> 5;

		int AToNStatus;
		AToNStatus = payload[24];

		int transceiverInformation;
		transceiverInformation = (payload[25] & 0xF8) >> 3;

		int reservedB;
		reservedB = payload[25] & 0x07;

		// BUG BUG This is variable up to 20 + 14 (34) characters
		std::string AToNName;
		int AToNNameLength = payload[26];
		if (payload[27] == 1) { // First byte indicates encoding, 0 for Unicode, 1 for ASCII
			for (int i = 0; i < AToNNameLength - 1; i++) {
				AToNName += static_cast<char>(payload[28 + i]);
			}
		} 
								
		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 5, AToNType);
		AISInsertString(binaryData, 43, 120, AToNNameLength <= 20 ? AToNName : AToNName.substr(0,20));
		AISInsertInteger(binaryData, 163, 1, positionAccuracy);
		AISInsertInteger(binaryData, 164, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 192, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 219, 9, refBow / 10);
		AISInsertInteger(binaryData, 228, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 237, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 243, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 249, 4, gnssType);
		AISInsertInteger(binaryData, 253, 6, timeStamp);
		AISInsertInteger(binaryData, 259, 1, offPositionFlag);
		AISInsertInteger(binaryData, 260, 8, AToNStatus);
		AISInsertInteger(binaryData, 268, 1, raimFlag);
		AISInsertInteger(binaryData, 269, 1, virtualAToN);
		AISInsertInteger(binaryData, 270, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 271, 1, spare);
		// Why is this called a spare (not padding) when in actual fact 
		// it functions as padding, Refer to the ITU Standard ITU-R M.1371-4 for clarification
		int fillBits = 0;
		if (AToNName.length() > 20) {
			// Add the AToN's name extension characters if necessary
			// BUG BUG Should check that shipName.length is not greater than 34
			AISInsertString(binaryData, 272, (AToNName.length() - 20) * 6,AToNName.substr(20,AToNName.length() - 20));
			fillBits = (272 + ((AToNName.length() - 20) * 6)) % 6;
			// Add padding to align on 6 bit boundary
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272 + (AToNName.length() - 20) * 6, fillBits, 0);
			}
		}
		else {
			// Add padding to align on 6 bit boundary
			fillBits = 272 % 6;
			if (fillBits > 0) {
				AISInsertInteger(binaryData, 272, fillBits, 0);
			}
		}
		
		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send the VDM message
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);
		
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28))));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, 28)));
			}
		}
				
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129283 NMEA Cross Track Error
// $--XTE, A, A, x.x, a, N, a*hh<CR><LF>
bool ActisenseDevice::DecodePGN129283(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte xteMode;
		xteMode = payload[1] & 0x0F;

		byte navigationTerminated;
		navigationTerminated = payload[1] & 0xC0;

		int crossTrackError;
		crossTrackError = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);
		
		if (TwoCanUtils::IsDataValid(crossTrackError)) {

			nmeaSentences->push_back(wxString::Format("$IIXTE,A,A,%.2f,%c,N", fabsf(CONVERT_METRES_NAUTICAL_MILES * crossTrackError * 0.01f), crossTrackError < 0 ? 'L' : 'R'));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129284 Navigation Data
//$--BWC, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BWR, hhmmss.ss, llll.ll, a, yyyyy.yy, a, x.x, T, x.x, M, x.x, N, c--c, a*hh<CR><LF>
//$--BOD, x.x, T, x.x, M, c--c, c--c*hh<CR><LF>
//$--WCV, x.x, N, c--c, a*hh<CR><LF>

// Not sure of this use case, as it implies there is already a chartplotter on board
bool ActisenseDevice::DecodePGN129284(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {
		byte sid;
		sid = payload[0];
		
		int distance;
		distance = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);
		
		byte bearingRef; // Magnetic or True
		bearingRef = (payload[5] & 0xC0) >> 6;
		
		byte perpendicularCrossed; // Yes or No
		perpendicularCrossed = (payload[5] & 0x30) >> 4;
		
		byte circleEntered; // Yes or No
		circleEntered = (payload[5] & 0x0C) >> 2;

		byte calculationType; //Great Circle or Rhumb Line
		calculationType = payload[5] & 0x03;
		
		int secondsSinceMidnight;
		secondsSinceMidnight = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		unsigned short daysSinceEpoch;
		daysSinceEpoch = payload[10] | (payload[11] << 8);
		
		short bearingOrigin;
		bearingOrigin = payload[12] | (payload[13] << 8);
		
		short bearingPosition;
		bearingPosition = payload[14] | (payload[15] << 8);
		
		int originWaypointId;
		originWaypointId = payload[16] | (payload[17] << 8) | (payload[18] << 16) | (payload[19] << 24);
		
		int destinationWaypointId;
		destinationWaypointId = payload[20] | (payload[21] << 8) | (payload[22] << 16) | (payload[23] << 24);
		
		double latitude;
		latitude = ((payload[24] | (payload[25] << 8) | (payload[26] << 16) | (payload[27] << 24))) * 1e-7;
		
		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs(latitude - latitudeDegrees);
		
		double longitude;
		longitude = ((payload[28] | (payload[29] << 8) | (payload[30] << 16) | (payload[31] << 24))) * 1e-7;
		
		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs(longitude - longitudeDegrees);
		
		short waypointClosingVelocity;
		waypointClosingVelocity = payload[32] | (payload[33] << 8);

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		wxDateTime timeNow = wxDateTime::Now();

		if (calculationType == GREAT_CIRCLE) { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000.0f), 
					CONVERT_METRES_NAUTICAL_MILES * distance * 0.01f, destinationWaypointId));
			}
			else if (bearingRef == HEADING_MAGNETIC) {
				nmeaSentences->push_back(wxString::Format("$IIBWC,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"), 
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S', 
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W', 
					RADIANS_TO_DEGREES((float)bearingPosition / 10000.0f), \
					CONVERT_METRES_NAUTICAL_MILES * distance * 0.01f, destinationWaypointId));
			}
		
		}
		else if (calculationType == RHUMB_LINE) { 
			if (bearingRef == HEADING_TRUE) {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,%.2f,T,,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000.0f), \
					CONVERT_METRES_NAUTICAL_MILES * distance * 0.01f, destinationWaypointId));
			}
			else if (bearingRef == HEADING_MAGNETIC) {
				nmeaSentences->push_back(wxString::Format("$IIBWR,%s,%02d%05.2f,%c,%03d%05.2f,%c,,T,%.2f,M,%.2f,N,%d,A", 
					timeNow.Format("%H%M%S.00"),
					abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
					abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
					RADIANS_TO_DEGREES((float)bearingPosition / 10000.0f), \
					CONVERT_METRES_NAUTICAL_MILES * distance * 0.01f, destinationWaypointId));
			}
		}

	
		if (bearingRef == HEADING_TRUE) {
			nmeaSentences->push_back(wxString::Format("$IIBOD,%.2f,T,,M,%d,%d", 
				RADIANS_TO_DEGREES((float)bearingOrigin / 10000.0f),destinationWaypointId, originWaypointId));
		}
		else if (bearingRef == HEADING_MAGNETIC) {
			nmeaSentences->push_back(wxString::Format("$IIBOD,,T,%.2f,M,%d,%d", 
				RADIANS_TO_DEGREES((float)bearingOrigin / 10000.0f),  destinationWaypointId, originWaypointId));
		}

		nmeaSentences->push_back(wxString::Format("$IIWCV,%.2f,N,%d,A",CONVERT_MS_KNOTS * waypointClosingVelocity * 0.01f, destinationWaypointId));

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129285 Route and Waypoint Information
// $--RTE,x.x,x.x,a,c--c,c--c, ... c--c*hh<CR><LF>
//         |   |  |  |    |         |
//         |   |  |  |    |        Waypoint ID n
//         |   |  |  |    Waypoint ID 2
//         |   |  |  Waypoint ID 1
//         |   |  c - complete, w - previous waypoint, next waypoint, and the remainder
//         |  Message Number
//       Total number of messages being transmitted

// and 
// $--WPL,llll.ll,a,yyyyy.yy,a,c--c
bool ActisenseDevice::DecodePGN129285(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {
		// BUG BUG, Should calculate how many sentences to send based on the number of waypoints
		// rather than just assuming a single sentence.
		wxString routeSentence = "$IIRTE,1,1,c";
		
		unsigned short rps;
		rps = payload[0] | (payload[1] << 8);

		unsigned short nItems;
		nItems = payload[2] | (payload[3] << 8);

		unsigned short databaseVersion;
		databaseVersion = payload[4] | (payload[5] << 8);

		unsigned short routeID;
		routeID = payload[6] | (payload[7] << 8);

		// I presume forward/reverse
		byte direction; 
		direction = (payload[8] & 0xE0) >> 5;

		byte supplementaryInfo;
		supplementaryInfo = (payload[8] & 0x18) >> 3;

		// NMEA reserved
		byte reservedA = payload[8] & 0x07;

		// As we need to iterate repeated fields with variable length strings
		// can't use hardcoded indexes into the payload
		int index = 11;

		std::string routeName;
		int routeNameLength = payload[9];
		if (payload[10] == 1) {
			// first byte of Route name indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < routeNameLength - 2; i++) {
				routeName += static_cast<char>(payload[index + i]);
				index++;
			}
		}

		// NMEA reserved 
		byte reservedB = payload[index];
		index++;

		// repeated fields
		for (unsigned int i = 0; i < nItems; i++) {
			unsigned short waypointID;
			waypointID = payload[index] | (payload[index + 1] << 8);

			routeSentence.append(wxString::Format(",%d", waypointID));

			index += 2;

			std::string waypointName;
			int waypointNameLength = payload[index];
			index++;
			if (payload[index] == 1) {
				// first byte of Waypoint Name indicates encoding; 0 for Unicode, 1 for ASCII
				index++;
				for (int i = 0; i < waypointNameLength - 2; i++) {
					waypointName += (static_cast<char>(payload[index]));
					index++;
				}
			}

					
			double latitude = (payload[index] | (payload[index + 1] << 8) | (payload[index + 2] << 16) | (payload[index + 3] << 24)) * 1e-7;
			int latitudeDegrees = trunc(latitude);
			double latitudeMinutes = fabs(latitude - latitudeDegrees);

			double longitude = (payload[index + 4] | (payload[index + 5] << 8) | (payload[index + 6] << 16) | (payload[index + 7] << 24)) * 1e-7;
			int longitudeDegrees = trunc(longitude);
			double longitudeMinutes = fabs(longitude - longitudeDegrees);

			index += 8;

			// BUG BUG Do we use WaypointID or Waypoint Name ??
			nmeaSentences->push_back(wxString::Format("$IIWPL,%02d%05.2f,%c,%03d%05.2f,%c,%d",
				abs(latitudeDegrees), fabs(latitudeMinutes), latitude >= 0 ? 'N' : 'S',
				abs(longitudeDegrees), fabs(longitudeMinutes), longitude >= 0 ? 'E' : 'W',
				waypointID));
		}

		nmeaSentences->push_back(routeSentence);

		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129793 AIS Date and Time report
// AIS Message Type 4 and if date is present also Message Type 11
bool ActisenseDevice::DecodePGN129793(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(168);

		// Should really check whether this is 4 (Base Station) or 
		// 11 (mobile station, but only in response to a request using message 10)
		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);
		
		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int reservedA;
		reservedA = (payload[13] & 0xFC) >> 2;

		int secondsSinceMidnight;
		secondsSinceMidnight = payload[14] | (payload[15] << 8) | (payload[16] << 16) | (payload[17] << 24);

		int communicationState;
		communicationState = payload[18] | (payload[19] << 8) | ((payload[20] & 0x7) << 16);

		int transceiverInformation; // unused in NMEA183 conversion, BUG BUG Just guessing
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		int daysSinceEpoch;
		daysSinceEpoch = payload[21] | (payload[22] << 8);

		int reservedB;
		reservedB = payload[23] & 0x0F;

		int gnssType;
		gnssType = (payload[23] & 0xF0) >> 4;

		int spare;
		spare = payload[24];

		int longRangeFlag = 0;

		wxDateTime tm;
		tm.ParseDateTime("00:00:00 01-01-1970");
		tm += wxDateSpan::Days(daysSinceEpoch);
		tm += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);

		// Encode VDM message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 14, tm.GetYear());
		AISInsertInteger(binaryData, 52, 4, tm.GetMonth() + 1);
		AISInsertInteger(binaryData, 56, 5, tm.GetDay());
		AISInsertInteger(binaryData, 61, 5, tm.GetHour());
		AISInsertInteger(binaryData, 66, 6, tm.GetMinute());
		AISInsertInteger(binaryData, 72, 6, tm.GetSecond());
		AISInsertInteger(binaryData, 78, 1, positionAccuracy);
		AISInsertInteger(binaryData, 79, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 107, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 134, 4, gnssType);
		AISInsertInteger(binaryData, 138, 1, longRangeFlag); // Long Range flag doesn't appear to be set anywhere
		AISInsertInteger(binaryData, 139, 9, spare);
		AISInsertInteger(binaryData, 148, 1, raimFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		// BUG BUG DEBUG
		wxMessageOutputDebug().Printf(_T("!AIVDM,1,1,,B,%s,0"), AISEncodePayload(binaryData));

		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 129794 NMEA AIS Class A Static and Voyage Related Data
// AIS Message Type 5
bool ActisenseDevice::DecodePGN129794(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(426,0);
	
		unsigned int messageID;
		messageID = payload[0] & 0x3F;

		unsigned int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		unsigned int userID; // aka MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);
		
		unsigned int imoNumber;
		imoNumber = payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24);
		
		std::string callSign;
		for (int i = 0; i < 7; i++) {
			callSign += static_cast<char>(payload[9 + i]);
		}
		
		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName += static_cast<char>(payload[16 + i]);
		}
		
		unsigned int shipType;
		shipType = payload[36];

		unsigned int shipLength;
		shipLength = payload[37] | (payload[38] << 8);

		unsigned int shipBeam;
		shipBeam = payload[39] | (payload[40] << 8);

		unsigned int refStarboard;
		refStarboard = payload[41] | (payload[42] << 8);

		unsigned int refBow;
		refBow = payload[43] | (payload[44] << 8);

		unsigned int daysSinceEpoch;
		daysSinceEpoch = payload[45] | (payload[46] << 8);

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[47] | (payload[48] << 8) | (payload[49] << 16) | (payload[50] << 24);

		wxDateTime eta;
		eta.ParseDateTime("00:00:00 01-01-1970");
		eta += wxDateSpan::Days(daysSinceEpoch);
		eta += wxTimeSpan::Seconds((wxLongLong)secondsSinceMidnight / 10000);
		
		unsigned int draft;
		draft = payload[51] | (payload[52] << 8);

		std::string destination;
		for (int i = 0; i < 20; i++) {
			destination += static_cast<char>(payload[53 + i]);
		}
				
		int aisVersion;
		aisVersion = (payload[73] & 0x03);

		int gnssType;
		gnssType = (payload[73] & 0x3C) >> 2;

		int dteFlag;
		dteFlag = (payload[73] & 0x40) >> 6;

		int transceiverInformation;
		transceiverInformation = payload[74] & 0x1F;

		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);		
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, aisVersion );
		AISInsertInteger(binaryData, 40, 30, imoNumber);
		AISInsertString(binaryData, 70, 42, callSign);
		AISInsertString(binaryData, 112, 120, shipName);
		AISInsertInteger(binaryData, 232, 8, shipType);
		AISInsertInteger(binaryData, 240, 9, refBow / 10);
		AISInsertInteger(binaryData, 249, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 258, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 264, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 270, 4, gnssType);
		AISInsertInteger(binaryData, 274, 4, eta.GetMonth() + 1);
		AISInsertInteger(binaryData, 278, 5, eta.GetDay()); 
		AISInsertInteger(binaryData, 283, 5, eta.GetHour());
		AISInsertInteger(binaryData, 288, 6, eta.GetMinute());
		AISInsertInteger(binaryData, 294, 8, draft / 10);
		AISInsertString(binaryData, 302, 120, destination);
		AISInsertInteger(binaryData, 422, 1, dteFlag);
		AISInsertInteger(binaryData, 423, 1, 0xFF); //spare

		wxString encodedVDMMessage = AISEncodePayload(binaryData);
		
		// Send VDM message in two NMEA183 sentences
		
		nmeaSentences->push_back(wxString::Format("!AIVDM,2,1,%d,%c,%s,0", AISsequentialMessageId, transceiverInformation==0?'A':'B', encodedVDMMessage.Mid(0,35).c_str()));
		nmeaSentences->push_back(wxString::Format("!AIVDM,2,2,%d,%c,%s,2", AISsequentialMessageId, transceiverInformation == 0 ? 'A' : 'B', encodedVDMMessage.Mid(35,36).c_str()));
				
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

//	Decode PGN 129798 AIS SAR Aircraft Position Report
// AIS Message Type 9
bool ActisenseDevice::DecodePGN129798(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(168);
		
		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		double longitude;
		longitude = ((payload[5] | (payload[6] << 8) | (payload[7] << 16) | (payload[8] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = fabs((longitude - longitudeDegrees) * 60);

		double latitude;
		latitude = ((payload[9] | (payload[10] << 8) | (payload[11] << 16) | (payload[12] << 24))) * 1e-7;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = fabs((latitude - latitudeDegrees) * 60);

		int positionAccuracy;
		positionAccuracy = payload[13] & 0x01;

		int raimFlag;
		raimFlag = (payload[13] & 0x02) >> 1;

		int timeStamp;
		timeStamp = (payload[13] & 0xFC) >> 2;

		int courseOverGround;
		courseOverGround = payload[14] | (payload[15] << 8);

		int speedOverGround;
		speedOverGround = payload[16] | (payload[17] << 8);

		int communicationState;
		communicationState = (payload[18] | (payload[19] << 8) | (payload[20] << 16)) & 0x7FFFF;

		int transceiverInformation; 
		transceiverInformation = (payload[20] & 0xF8) >> 3;

		double altitude;
		altitude = 1e-6 * (((long long)payload[21] | ((long long)payload[22] << 8) | ((long long)payload[23] << 16) | ((long long)payload[24] << 24) \
			| ((long long)payload[25] << 32) | ((long long)payload[26] << 40) | ((long long)payload[27] << 48) | ((long long)payload[28] << 56)));
		
		int reservedForRegionalApplications;
		reservedForRegionalApplications = payload[29];

		int dteFlag; 
		dteFlag = payload[30] & 0x01;

		// BUG BUG Just guessing these to match NMEA2000 payload with ITU AIS fields

		int assignedModeFlag;
		assignedModeFlag = (payload[30] & 0x02) >> 1;

		int sotdmaFlag;
		sotdmaFlag = (payload[30] & 0x04) >> 2;

		int altitudeSensor;
		altitudeSensor = (payload[30] & 0x08) >> 3;

		int spare;
		spare = (payload[30] & 0xF0) >> 4;

		int reserved;
		reserved = payload[31];
		
		// Encode VDM Message using 6bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 12, altitude);
		AISInsertInteger(binaryData, 50, 10, speedOverGround);
		AISInsertInteger(binaryData, 60, 1, positionAccuracy);
		AISInsertInteger(binaryData, 61, 28, ((longitudeDegrees * 60) + longitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 89, 27, ((latitudeDegrees * 60) + latitudeMinutes) * 10000);
		AISInsertInteger(binaryData, 116, 12, courseOverGround);
		AISInsertInteger(binaryData, 128, 6, timeStamp);
		AISInsertInteger(binaryData, 134, 8, reservedForRegionalApplications); // 1 bit altitide sensor
		AISInsertInteger(binaryData, 142, 1, dteFlag);
		AISInsertInteger(binaryData, 143, 3, spare);
		// BUG BUG just guessing
		AISInsertInteger(binaryData, 146, 1, assignedModeFlag);
		AISInsertInteger(binaryData, 147, 1, raimFlag);
		AISInsertInteger(binaryData, 148, 1, sotdmaFlag);
		AISInsertInteger(binaryData, 149, 19, communicationState);
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}
	
//	Decode PGN 129801 AIS Addressed Safety Related Message
// AIS Message Type 12
bool ActisenseDevice::DecodePGN129801(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(1008);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		int reservedA;
		reservedA = payload[4] & 0x01;

		int transceiverInfo;
		transceiverInfo = (payload[5] & 0x3E) >> 1;

		int sequenceNumber;
		sequenceNumber = (payload[5] & 0xC0) >> 6;

		int destinationId;
		destinationId = payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24);

		int reservedB;
		reservedB = payload[10] & 0x3F;

		int retransmitFlag;
		retransmitFlag = (payload[10] & 0x40) >> 6;

		int reservedC;
		reservedC = (payload[10] & 0x80) >> 7;

		std::string safetyMessage;
		int safetyMessageLength = payload[11];
		if (payload[12] == 1) {
			// first byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < safetyMessageLength - 2; i++) {
				safetyMessage += (static_cast<char>(payload[13 + i]));
			}
		}

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, sourceID);
		AISInsertInteger(binaryData, 38, 2, sequenceNumber);
		AISInsertInteger(binaryData, 40, 30, destinationId);
		AISInsertInteger(binaryData, 70, 1, retransmitFlag);
		AISInsertInteger(binaryData, 71, 1, 0); // unused spare
		AISInsertString(binaryData, 72, 936, safetyMessage);

		// BUG BUG Calculate fill bits correcty as safetyMessage is variable in length

		int fillBits = 0;
		fillBits = 1008 % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 968, fillBits, 0);
		}

		wxString encodedVDMMessage = AISEncodePayload(binaryData);

		// Send the VDM message
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);
		
		for (int i = 0; i < numberOfVDMMessages; i++) {
			if (i == numberOfVDMMessages -1) { // This is the last message
			nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, encodedVDMMessage.size() - (i * 28)),fillBits));
			}
			else {
				nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,B,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, 28)));
			}
		}
		
		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
		
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129802 AIS Broadcast Safety Related Message 
// AIS Message Type 14
bool ActisenseDevice::DecodePGN129802(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(1008);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int sourceID;
		sourceID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | ((payload[4] & 0x3F) << 24);

		int reservedA;
		reservedA = (payload[4] & 0xC0) >> 6;

		int transceiverInfo;
		transceiverInfo = payload[5] & 0x1F;

		int reservedB;
		reservedB = (payload[5] & 0xE0) >> 5;

		std::string safetyMessage;
		int safetyMessageLength = payload[6];
		if (payload[7] == 1) { 
			// first byte of safety message indicates encoding; 0 for Unicode, 1 for ASCII
			for (int i = 0; i < safetyMessageLength - 2; i++) {
				safetyMessage += (static_cast<char>(payload[8 + i]));
			}
		}

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, sourceID);
		AISInsertInteger(binaryData, 38, 2, 0); //spare
		int l = safetyMessage.size();
		// Remember 6 bits per character
		AISInsertString(binaryData, 40, l * 6, safetyMessage.c_str());

		// Calculate fill bits as safetyMessage is variable in length
		// According to ITU, maximum length of safetyMessage is 966 6bit characters
		int fillBits = (40 + (l * 6)) % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 40 + (l * 6), fillBits, 0);
		}

		// BUG BUG Should check whether the binary message is smaller than 1008 btes otherwise
		// we just need a substring from the binaryData
		std::vector<bool>::const_iterator first = binaryData.begin();
		std::vector<bool>::const_iterator last = binaryData.begin() + 40 + (l * 6) + fillBits;
		std::vector<bool> newVec(first, last);

		// Encode the VDM Message using 6bit ASCII
		wxString encodedVDMMessage = AISEncodePayload(newVec);

		// Send the VDM message, use 28 characters as an arbitary number for multiple NMEA 183 sentences
		int numberOfVDMMessages = ((int)encodedVDMMessage.Length() / 28) + ((encodedVDMMessage.Length() % 28) >  0 ? 1 : 0);
		if (numberOfVDMMessages == 1) {
			nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,A,%s,%d", encodedVDMMessage, fillBits));
		}
		else {
			for (int i = 0; i < numberOfVDMMessages; i++) {
				if (i == numberOfVDMMessages - 1) { // Is this the last message, if so append number of fillbits as appropriate
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,A,%s,%d", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, 28), fillBits));
				}
				else {
					nmeaSentences->push_back(wxString::Format("!AIVDM,%d,%d,%d,A,%s,0", numberOfVDMMessages, i, AISsequentialMessageId, encodedVDMMessage.Mid(i * 28, 28)));
				}
			}
		}

		AISsequentialMessageId += 1;
		if (AISsequentialMessageId == 10) {
			AISsequentialMessageId = 0;
		}

		return TRUE;
	}
	else {
		return FALSE;
	}
}


// Decode PGN 129808 NMEA DSC Call
// A mega confusing combination !!
// $--DSC, xx,xxxxxxxxxx,xx,xx,xx,x.x,x.x,xxxxxxxxxx,xx,a,a
//          |     |       |  |  |  |   |  MMSI        | | Expansion Specifier
//          |   MMSI     Category  Position           | Acknowledgement        
//          Format Specifer  |  |      |Time          Nature of Distress
//                           |  Type of Communication or Second telecommand
//                           Nature of Distress or First Telecommand

// and
// $--DSE

bool ActisenseDevice::DecodePGN129808(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte formatSpecifier;
		formatSpecifier = payload[0];

		byte dscCategory;
		dscCategory = payload[1];

		char mmsiAddress[5];
		sprintf(mmsiAddress, "%02d%02d%02d%02d%02d", payload[2], payload[3], payload[4], payload[5], payload[6]);

		byte firstTeleCommand; // or Nature of Distress
		firstTeleCommand = payload[7];

		byte secondTeleCommand; // or Communication Mode
		secondTeleCommand = payload[8];

		char receiveFrequency;
		receiveFrequency = payload[9]; // Encoded of 9, 10, 11, 12, 13, 14

		char transmitFrequency;
		transmitFrequency = payload[15]; // Encoded of 15, 16, 17, 18, 19, 20

		char telephoneNumber;
		telephoneNumber = payload[21]; // encoded over 8 or 16 bytes

		int index = 0;

		double latitude;
		latitude = ((payload[index + 1] | (payload[index + 2] << 8) | (payload[index + 3] << 16) | (payload[index + 4] << 24))) * 1e-7;

		index += 4;

		int latitudeDegrees = trunc(latitude);
		double latitudeMinutes = (latitude - latitudeDegrees) * 60;

		double longitude;
		longitude = ((payload[index + 1] | (payload[index + 2] << 8) | (payload[index + 3] << 16) | (payload[index + 4] << 24))) * 1e-7;

		int longitudeDegrees = trunc(longitude);
		double longitudeMinutes = (longitude - longitudeDegrees) * 60;

		unsigned int secondsSinceMidnight;
		secondsSinceMidnight = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24);

		// note payload index.....
		char vesselInDistress[5];
		sprintf(vesselInDistress, "%02d%02d%02d%02d%02d", payload[2], payload[3], payload[4], payload[5], payload[6]);

		byte endOfSequence;
		endOfSequence = payload[101]; // 1 byte

		byte dscExpansionEnabled; // Encoded over two bits
		dscExpansionEnabled = (payload[102] & 0xC0) >> 6;

		byte reserved; // 6 bits
		reserved = payload[102] & 0x3F;

		byte callingRx; // 6 bytes
		callingRx = payload[103];

		byte callingTx; // 6 bytes
		callingTx = payload[104];

		unsigned int timeOfTransmission;
		timeOfTransmission = payload[105] | (payload[106] << 8) | (payload[107] << 16) | (payload[108] << 24);

		unsigned int dayOfTransmission;
		dayOfTransmission = payload[109] | (payload[110] << 8);

		unsigned int messageId;
		messageId = payload[111] | (payload[112] << 8);

		// The following pairs are repeated

		byte dscExpansionSymbol;
		dscExpansionSymbol = payload[113];

		// Now iterate through the DSE Expansion data

		for (size_t i = 120; i < sizeof(payload);) {
			switch (payload[i]) {
				// refer to ITU-R M.821 Table 1.
			case 100: // enhanced position
				// 4 characters (8 digits)
				i += 4;
				break;
			case 101: // Source and datum of position
				i += 9;
				break;
			case 102: // Current speed of the vessel - 4 bytes
				i += 4;
				break;
			case 103: // Current course of the vessel - 4 bytes
				i += 4;
				break;
			case 104: // Additional Station information - 10
				i += 10;
				break;
			case 105: // Enhanced Geographic Area - 12
				i += 12;
				break;
			case 106: // Numbr of persons onboard - 2 characters
				i += 2;
				break;

			}
		}
		
		return FALSE;
	}
	else {
		return FALSE;
	}

}

// Decode PGN 129809 AIS Class B Static Data Report, Part A 
// AIS Message Type 24, Part A
bool ActisenseDevice::DecodePGN129809(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {
		
		std::vector<bool> binaryData(164);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		std::string shipName;
		for (int i = 0; i < 20; i++) {
			shipName += static_cast<char>(payload[5 + i]);
		}

		// Encode VDM Message using 6 bit ASCII

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, 0x0); // Part A = 0
		AISInsertString(binaryData, 40, 120, shipName);
		
		// Add padding to align on 6 bit boundary
		int fillBits = 0;
		fillBits = 160 % 6;
		if (fillBits > 0) {
			AISInsertInteger(binaryData, 160, fillBits, 0);
		}
		
		// Send a single VDM sentence, note no sequential message Id		
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,%d", AISEncodePayload(binaryData), fillBits));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 129810 AIS Class B Static Data Report, Part B 
// AIS Message Type 24, Part B
bool ActisenseDevice::DecodePGN129810(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		std::vector<bool> binaryData(168);

		int messageID;
		messageID = payload[0] & 0x3F;

		int repeatIndicator;
		repeatIndicator = (payload[0] & 0xC0) >> 6;

		int userID; // aka sender's MMSI
		userID = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);

		int shipType;
		shipType = payload[5];

		std::string vendorId;
		for (int i = 0; i < 7; i++) {
			vendorId += static_cast<char>(payload[6 + i]);
		}

		std::string callSign;
		for (int i = 0; i < 7; i++) {
			callSign += static_cast<char>(payload[13 + i]);
		}
		
		unsigned int shipLength;
		shipLength = payload[20] | (payload[21] << 8);



		unsigned int shipBeam;
		shipBeam = payload[22] | (payload[23] << 8);

		unsigned int refStarboard;
		refStarboard = payload[24] | (payload[25] << 8);

		unsigned int refBow;
		refBow = payload[26] | (payload[27] << 8);

		unsigned int motherShipID; // aka mother ship MMSI
		motherShipID = payload[28] | (payload[29] << 8) | (payload[30] << 16) | (payload[31] << 24);

		int reserved;
		reserved = (payload[32] & 0x03);

		int spare;
		spare = (payload[32] & 0xFC) >> 2;

		AISInsertInteger(binaryData, 0, 6, messageID);
		AISInsertInteger(binaryData, 6, 2, repeatIndicator);
		AISInsertInteger(binaryData, 8, 30, userID);
		AISInsertInteger(binaryData, 38, 2, 0x01); // Part B = 1
		AISInsertInteger(binaryData, 40, 8, shipType);
		AISInsertString(binaryData, 48, 42, vendorId);
		AISInsertString(binaryData, 90, 42, callSign);
		AISInsertInteger(binaryData, 132, 9, refBow / 10);
		AISInsertInteger(binaryData, 141, 9, (shipLength / 10) - (refBow / 10));
		AISInsertInteger(binaryData, 150, 6, (shipBeam / 10) - (refStarboard / 10));
		AISInsertInteger(binaryData, 156, 6, refStarboard / 10);
		AISInsertInteger(binaryData, 162 ,6 , 0); //spare
		
		// Send a single VDM sentence, note no fillbits nor a sequential message Id
		nmeaSentences->push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
		
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130306 NMEA Wind
// $--MWV,x.x,a,x.x,a,A*hh<CR><LF>
bool ActisenseDevice::DecodePGN130306(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short windSpeed;
		windSpeed = payload[1] | (payload[2] << 8);

		unsigned short windAngle;
		windAngle = payload[3] | (payload[4] << 8);

		byte windReference;
		windReference = (payload[5] & 0x07);

		if (TwoCanUtils::IsDataValid(windSpeed)) {
			if (TwoCanUtils::IsDataValid(windAngle)) {
				nmeaSentences->push_back(wxString::Format("$IIMWV,%.2f,%c,%.2f,N,A", RADIANS_TO_DEGREES((float)windAngle/10000), \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100));
				return TRUE;

			}
			else {
				nmeaSentences->push_back(wxString::Format("$IIMWV,,%c,%.2f,N,A", \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T', (double)windSpeed * CONVERT_MS_KNOTS / 100));
				return TRUE;	
			}
		}
		else {
			if (TwoCanUtils::IsDataValid(windAngle)) {
				nmeaSentences->push_back(wxString::Format("$IIMWV,%.2f,%c,,N,A", RADIANS_TO_DEGREES((float)windAngle/10000), \
				(windReference == WIND_REFERENCE_APPARENT) ? 'R' : 'T'));
				return TRUE;
			}
			else {
				return FALSE;
			}
		} 
		
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130310 NMEA Water & Air Temperature and Pressure
// $--MTW,x.x,C*hh<CR><LF>
bool ActisenseDevice::DecodePGN130310(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		unsigned short waterTemperature;
		waterTemperature = payload[1] | (payload[2] << 8);

		unsigned short airTemperature;
		airTemperature = payload[3] | (payload[4] << 8);

		unsigned short airPressure;
		airPressure = payload[5] | (payload[6] << 8);
		
		if (TwoCanUtils::IsDataValid(waterTemperature)) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)waterTemperature * 0.01f) + CONST_KELVIN));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130311 NMEA Environment  (supercedes 130311)
// $--MTW,x.x,C*hh<CR><LF>
bool ActisenseDevice::DecodePGN130311(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte temperatureSource;
		temperatureSource = payload[1] & 0x3F;
		
		byte humiditySource;
		humiditySource = (payload[1] & 0xC0) >> 6;
		
		unsigned short temperature;
		temperature = payload[2] | (payload[3] << 8);
			
		unsigned short humidity;
		humidity = payload[4] | (payload[5] << 8);
		//	Resolution 0.004
			
		unsigned short pressure;
		pressure = payload[6] | (payload[7] << 8);
		
		if ((temperatureSource == TEMPERATURE_SEA) && (TwoCanUtils::IsDataValid(temperature))) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)temperature * 0.01f) + CONST_KELVIN));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}


// Decode PGN 130312 NMEA Temperature
// $--MTW,x.x,C*hh<CR><LF>
bool ActisenseDevice::DecodePGN130312(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte instance;
		instance = payload[1];

		byte source;
		source = payload[2];

		unsigned short actualTemperature;
		actualTemperature = payload[3] | (payload[4] << 8);

		unsigned short setTemperature;
		setTemperature = payload[5] | (payload[6] << 8);

		if ((source == TEMPERATURE_SEA) && (TwoCanUtils::IsDataValid(actualTemperature))) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)actualTemperature * 0.01f) + CONST_KELVIN));
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

// Decode PGN 130316 NMEA Temperature Extended Range
// $--MTW,x.x,C*hh<CR><LF>
bool ActisenseDevice::DecodePGN130316(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		byte sid;
		sid = payload[0];

		byte instance;
		instance = payload[1];

		byte source;
		source = payload[2];

		unsigned int actualTemperature; // A three byte value, guess I need to special case the validity check. Bloody NMEA!!
		actualTemperature = payload[3] | (payload[4] << 8) | (payload[5] << 16);

		unsigned short setTemperature;
		setTemperature = payload[6] | (payload[7] << 8);

		if ((source == TEMPERATURE_SEA) && (actualTemperature < 0xFFFFFD)) {
			nmeaSentences->push_back(wxString::Format("$IIMTW,%.2f,C", ((float)actualTemperature * 0.001f) + CONST_KELVIN));
			return TRUE; 
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}


// Decode PGN 130577 NMEA Direction Data
// BUG BUG Work out what to convert this to
bool ActisenseDevice::DecodePGN130577(std::vector<byte> payload, std::vector<wxString> *nmeaSentences) {
	if (payload.size() > 0) {

		// 0 - Autonomous, 1 - Differential enhanced, 2 - Estimated, 3 - Simulated, 4 - Manual
		byte dataMode;
		dataMode = payload[0] & 0x0F;

		// True = 0, Magnetic = 1
		byte cogReference;
		cogReference = (payload[0] & 0x30);

		byte sid;
		sid = payload[1];

		unsigned short courseOverGround;
		courseOverGround = (payload[2] | (payload[3] << 8));

		unsigned short speedOverGround;
		speedOverGround = (payload[4] | (payload[5] << 8));

		unsigned short heading;
		heading = (payload[6] | (payload[7] << 8));

		unsigned short speedThroughWater;
		speedThroughWater = (payload[8] | (payload[9] << 8));

		unsigned short set;
		set = (payload[10] | (payload[11] << 8));

		unsigned short drift;
		drift = (payload[12] | (payload[13] << 8));


		nmeaSentences->push_back(wxString::Format("$IIVTG,%.2f,T,%.2f,M,%.2f,N,%.2f,K,%c", RADIANS_TO_DEGREES((float)courseOverGround / 10000), \
			RADIANS_TO_DEGREES((float)courseOverGround / 10000), (float)speedOverGround * CONVERT_MS_KNOTS / 100, \
			(float)speedOverGround * CONVERT_MS_KMH / 100, GPS_MODE_AUTONOMOUS));
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// Send an ISO Request
int ActisenseDevice::SendISORequest(const byte destination, const unsigned int pgn) {
	CanHeader header;
	header.pgn = 59904;
	header.destination = destination;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
		
	byte payload[3];
	payload[0] = pgn & 0xFF;
	payload[1] = (pgn >> 8) & 0xFF;
	payload[2] = (pgn >> 16) & 0xFF;
	
#ifdef __WXMSW__
	return (deviceInterface->Write(id, 3, payload));
#endif
	
#ifdef __LINUX__
	return (deviceInterface->Write(id,3,payload));
#endif
}

// Claim an Address on the NMEA 2000 Network
int ActisenseDevice::SendAddressClaim(const unsigned int sourceAddress) {
	CanHeader header;
	header.pgn = 60928;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = sourceAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
	
	byte payload[8];
	int manufacturerCode = CONST_MANUFACTURER_CODE;
	int deviceFunction = CONST_DEVICE_FUNCTION;
	int deviceClass = CONST_DEVICE_CLASS;
	int deviceInstance = 0;
	int systemInstance = 0;

	payload[0] = uniqueId & 0xFF;
	payload[1] = (uniqueId >> 8) & 0xFF;
	payload[2] = (uniqueId >> 16) & 0x1F;
	payload[2] |= (manufacturerCode << 5) & 0xE0;
	payload[3] = manufacturerCode >> 3;
	payload[4] = deviceInstance;
	payload[5] = deviceFunction;
	payload[6] = deviceClass << 1;
	payload[7] = 0x80 | (CONST_MARINE_INDUSTRY << 4) | systemInstance;

	// Add my entry to the network map
	networkMap[header.source].manufacturerId = manufacturerCode;
	networkMap[header.source].uniqueId = uniqueId;
	// BUG BUG What to do for my time stamp ??

	// And while we're at it, calculate my deviceName (aka NMEA 'NAME')
	deviceName = (unsigned long long)payload[0] | ((unsigned long long)payload[1] << 8) | ((unsigned long long)payload[2] << 16) | ((unsigned long long)payload[3] << 24) | ((unsigned long long)payload[4] << 32) | ((unsigned long long)payload[5] << 40) | ((unsigned long long)payload[6] << 48) | ((unsigned long long)payload[7] << 54);
	
#ifdef __WXMSW__
	return (deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif
	
#ifdef __LINUX__
	return (deviceInterface->Write(id,CONST_PAYLOAD_LENGTH,&payload[0]));
#endif
}

// Transmit NMEA 2000 Heartbeat
int ActisenseDevice::SendHeartbeat() {
	CanHeader header;
	header.pgn = 126993;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;

	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id, &header);

	byte payload[8];
	memset(payload, 0xFF, CONST_PAYLOAD_LENGTH);

	//BUG BUG 60000 milliseconds equals one minute. Should match this to the heartbeat timer interval
	payload[0] = 0x60; // 60000 & 0xFF;
	payload[1]  = 0xEA; // (60000 >> 8) & 0xFF;
	payload[2] = heartbeatCounter;
	payload[3] = 0xFF; // Class 1 CAN State, From observation of B&G devices indicates 255 ? undefined ??
	payload[4] = 0xFF; // Class 2 CAN State, From observation of B&G devices indicates 255 ? undefined ??
	payload[5] = 0xFF; // No idea, leave as 255 undefined
	payload[6] = 0xFF; //            "
	payload[7] = 0xFF; //            "

	heartbeatCounter++;
	if (!TwoCanUtils::IsDataValid(heartbeatCounter)) {  
		// From observation of B&G devices, appears to rollover after 252 ??
		heartbeatCounter = 0;
	}
	
#ifdef __WXMSW__
	return (deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif

#ifdef __LINUX__
	return (deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif

}

// Transmit NMEA 2000 Product Information
int ActisenseDevice::SendProductInformation() {
	CanHeader header;
	header.pgn = 126996;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	byte payload[134];
		
	unsigned short dbver = CONST_DATABASE_VERSION;
	memcpy(&payload[0],&dbver,sizeof(unsigned short)); 

	unsigned short pcode = CONST_PRODUCT_CODE;
	memcpy(&payload[2],&pcode,sizeof(unsigned short));
	
	// Note all of the string values are stored without terminating NULL character
	// Model ID Bytes [4] - [35]
	memset(&payload[4],0,32);
	char *hwVersion = CONST_MODEL_ID;
	memcpy(&payload[4], hwVersion,strlen(hwVersion));
	
	// Software Version Bytes [36] - [67]
	// BUG BUG Should derive from PLUGIN_VERSION_MAJOR and PLUGIN_VERSION_MINOR
	memset(&payload[36],0,32);
	char *swVersion = CONST_SOFTWARE_VERSION;  
	memcpy(&payload[36], swVersion,strlen(swVersion));
		
	// Model Version Bytes [68] - [99]
	memset(&payload[68],0,32);
	memcpy(&payload[68], hwVersion,strlen(hwVersion));
	
	// Serial Number Bytes [100] - [131] - Let's reuse our uniqueId as the serial number
	memset(&payload[100],0,32);
	std::string tmp;
	tmp = std::to_string(uniqueId);
	memcpy(&payload[100], tmp.c_str(),tmp.length());
	
	payload[132] = CONST_CERTIFICATION_LEVEL;
	
	payload[133] = CONST_LOAD_EQUIVALENCY;

	wxString *mid;
	mid = new wxString(CONST_MODEL_ID);
		
	strcpy(networkMap[header.source].productInformation.modelId,mid->c_str());
	return FragmentFastMessage(&header,sizeof(payload),&payload[0]);

}

// Transmit Supported Parameter Group Numbers
int ActisenseDevice::SendSupportedPGN() {
	CanHeader header;
	header.pgn = 126464;
	header.destination = CONST_GLOBAL_ADDRESS;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	// BUG BUG Should define our supported Parameter Group Numbers somewhere else, not compiled in the code ??
	unsigned int receivedPGN[] = {59904, 59392, 60928, 65240, 126464, 126992, 126993, 126996, 
		127250,	127251, 127258, 128259, 128267, 128275, 129025, 129026, 129029, 129033, 
		129028, 129039, 129040, 129041, 129283, 129793, 129794, 129798, 129801, 129802, 
		129808,	129809, 129810, 130306, 130310, 130312, 130577 };
	unsigned int transmittedPGN[] = {59392, 59904, 60928, 126208, 126464, 126993, 126996 };
	
	// Payload is a one byte function code (receive or transmit) and 3 bytes for each PGN 
	int arraySize;
	arraySize = sizeof(receivedPGN)/sizeof(unsigned int);
	
	byte *receivedPGNPayload;
	receivedPGNPayload = (byte *)malloc((arraySize * 3) + 1);
	receivedPGNPayload[0] = 0; // I think receive function code is zero

	for (int i = 0; i < arraySize; i++) {
		receivedPGNPayload[(i*3) + 1] = receivedPGN[i] & 0xFF;
		receivedPGNPayload[(i*3) + 2] = (receivedPGN[i] >> 8) & 0xFF;
		receivedPGNPayload[(i*3) + 3] = (receivedPGN[i] >> 16 ) & 0xFF;
	}
	
	FragmentFastMessage(&header,sizeof(receivedPGNPayload),receivedPGNPayload);
	
	free(receivedPGNPayload);

	arraySize = sizeof(transmittedPGN)/sizeof(unsigned int);
	byte *transmittedPGNPayload;
	transmittedPGNPayload = (byte *)malloc((arraySize * 3) + 1);
	transmittedPGNPayload[0] = 1; // I think transmit function code is 1
	for (int i = 0; i < arraySize; i++) {
		transmittedPGNPayload[(i*3) + 1] = transmittedPGN[i] & 0xFF;
		transmittedPGNPayload[(i*3) + 2] = (transmittedPGN[i] >> 8) & 0xFF;
		transmittedPGNPayload[(i*3) + 3] = (transmittedPGN[i] >> 16 ) & 0xFF;
	}
	
	FragmentFastMessage(&header,sizeof(transmittedPGNPayload),transmittedPGNPayload);

	free(transmittedPGNPayload);
	
	return TWOCAN_RESULT_SUCCESS;
}

// Respond to an ISO Rqst
// BUG BUG at present just NACK everything
int ActisenseDevice::SendISOResponse(unsigned int sender, unsigned int pgn) {
	CanHeader header;
	header.pgn = 59392;
	header.destination = sender;
	header.source = networkAddress;
	header.priority = CONST_PRIORITY_MEDIUM;
	
	unsigned int id;
	TwoCanUtils::EncodeCanHeader(&id,&header);
		
	byte payload[8];
	payload[0] = 1; // 0 = Ack, 1 = Nack, 2 = Access Denied, 3 = Network Busy
	payload[1] = 0; // No idea, the field is called Group Function
	payload[2] = 0; // No idea
	payload[3] = 0;
	payload[4] = 0;
	payload[5] = pgn & 0xFF;
	payload[6] = (pgn >> 8) & 0xFF;
	payload[7] = (pgn >> 16) & 0xFF;
	
#ifdef __WXMSW__
	return (deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &payload[0]));
#endif
	
#ifdef __LINUX__
	return (deviceInterface->Write(id,CONST_PAYLOAD_LENGTH,&payload[0]));
#endif
 
}

// Shamelessly copied from somewhere, another plugin ?
void ActisenseDevice::SendNMEASentence(wxString sentence) {
	sentence.Trim();
	wxString checksum = ComputeChecksum(sentence);
	sentence = sentence.Append(wxT("*"));
	sentence = sentence.Append(checksum);
	sentence = sentence.Append(wxT("\r\n"));
	RaiseEvent(sentence);
}

// Shamelessly copied from somewhere, another plugin ?
wxString ActisenseDevice::ComputeChecksum(wxString sentence) {
	unsigned char calculatedChecksum = 0;
	for (wxString::const_iterator it = sentence.begin() + 1; it != sentence.end(); ++it) {
		calculatedChecksum ^= static_cast<unsigned char> (*it);
	}
	return(wxString::Format(wxT("%02X"), calculatedChecksum));
}

// Fragment a Fast Packet Message into 8 byte payload chunks
int ActisenseDevice::FragmentFastMessage(CanHeader *header, unsigned int payloadLength, byte *payload) {
	unsigned int id;
	int returnCode;
	byte data[8];
	
	TwoCanUtils::EncodeCanHeader(&id,header);
		
	// Send the first frame
	// BUG BUG should maintain a map of sequential ID's for each PGN
	byte sid = 0;
	data[0] = sid;
	data[1] = payloadLength;
	memcpy(&data[2], &payload[0], 6);
	
#ifdef __WXMSW__
		returnCode = deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif
	
#ifdef __LINUX__
	returnCode = deviceInterface->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif

	if (returnCode != TWOCAN_RESULT_SUCCESS) {
		wxLogError(_T("Actisense Device, Error sending fast message frame"));
		// BUG BUG Should we log the frame ??
		return returnCode;
	}
	
	sid += 1;
	wxThread::Sleep(CONST_TEN_MILLIS);
	
	// Now the intermediate frames
	int iterations;
	iterations = (int)((payloadLength - 6) / 7);
		
	for (int i = 0; i < iterations; i++) {
		data[0] = sid;
		memcpy(&data[1],&payload[6 + (i * 7)],7);
		
#ifdef __WXMSW__
		returnCode = deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif

		
#ifdef __LINUX__
		returnCode = deviceInterface->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif

		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("Actisense Device, Error sending fast message frame"));
			// BUG BUG Should we log the frame ??
			return returnCode;
		}
		
		sid += 1;
		wxThread::Sleep(CONST_TEN_MILLIS);
	}
	
	// Is there a remaining frame ?
	int remainingBytes;
	remainingBytes = (payloadLength - 6) % 7;
	if (remainingBytes > 0) {
		data[0] = sid;
		memset(&data[1],0xFF,7);
		memcpy(&data[1], &payload[payloadLength - remainingBytes], remainingBytes );
		
#ifdef __WXMSW__
		returnCode = deviceInterface->Write(id, CONST_PAYLOAD_LENGTH, &data[0]);
#endif

		
#ifdef __LINUX__
		returnCode = deviceInterface->Write(id,CONST_PAYLOAD_LENGTH,&data[0]);
#endif
		if (returnCode != TWOCAN_RESULT_SUCCESS) {
			wxLogError(_T("Actisense Device, Error sending fast message frame"));
			// BUG BUG Should we log the frame ??
			return returnCode;
		}
	}
	
	return TWOCAN_RESULT_SUCCESS;
}

	
// Encode an 8 bit ASCII character using NMEA 0183 6 bit encoding
char ActisenseDevice::AISEncodeCharacter(char value)  {
		char result = value < 40 ? value + 48 : value + 56;
		return result;
}

// Decode a NMEA 0183 6 bit encoded character to an 8 bit ASCII character
char ActisenseDevice::AISDecodeCharacter(char value) {
	char result = value - 48;
	result = result > 40 ? result - 8 : result;
	return result;
}

// Create the NMEA 0183 AIS VDM/VDO payload from the 6 bit encoded binary data
wxString ActisenseDevice::AISEncodePayload(std::vector<bool>& binaryData) {
	wxString result;
	int j = 6;
	char temp = 0;
	// BUG BUG should probably use std::vector<bool>::size_type
	for (std::vector<bool>::size_type i = 0; i < binaryData.size(); i++) {
		temp += (binaryData[i] << (j - 1));
		j--;
		if (j == 0) { // "gnaw" through each 6 bits
			result.append(AISEncodeCharacter(temp));
			temp = 0;
			j = 6;
		}
	}
	return result;
}

// Decode the NMEA 0183 ASCII values, derived from 6 bit encoded data to an array of bits
// so that we can gnaw through the bits to retrieve each AIS data field 
std::vector<bool> ActisenseDevice::AISDecodePayload(wxString SixBitData) {
	std::vector<bool> decodedData(168);
	for (wxString::size_type i = 0; i < SixBitData.length(); i++) {
		char testByte = AISDecodeCharacter((char)SixBitData[i]);
		// Perform in reverse order so that we store in LSB order
		for (int j = 5; j >= 0; j--) {
			// BUG BUG generates compiler warning, could use ....!=0 but could be confusing ??
			decodedData.push_back((testByte & (1 << j))); // sets each bit value in the array
		}
	}
	return decodedData;
}

// Assemble AIS VDM message, fragmenting if necessary
	std::vector<wxString> ActisenseDevice::AssembleAISMessage(std::vector<bool> binaryData, const int messageType) {
	std::vector<wxString> result;
	result.push_back(wxString::Format("!AIVDM,1,1,,B,%s,0", AISEncodePayload(binaryData)));
	return result;
}

// Insert an integer value into AIS binary data, prior to AIS encoding
void ActisenseDevice::AISInsertInteger(std::vector<bool>& binaryData, int start, int length, int value) {
	for (int i = 0; i < length; i++) {
		// set the bit values, storing as MSB
		binaryData[start + length - i - 1] = (value & (1 << i));
	}
	return;
}

// Insert a date value, DDMMhhmm into AIS binary data, prior to AIS encoding
void ActisenseDevice::AISInsertDate(std::vector<bool>& binaryData, int start, int length, int day, int month, int hour, int minute) {
	AISInsertInteger(binaryData, start, 4, day);
	AISInsertInteger(binaryData, start + 4, 5, month);
	AISInsertInteger(binaryData, start + 9, 5, hour);
	AISInsertInteger(binaryData, start + 14, 6, minute);
	return;
}

// Insert a string value into AIS binary data, prior to AIS encoding
void ActisenseDevice::AISInsertString(std::vector<bool> &binaryData, int start, int length, std::string value) {

	// Should check that value.length is a multiple of 6 (6 bit ASCII encoded characters) and
	// that value.length * 6 is less than length.

	// convert to uppercase;
	std::transform(value.begin(), value.end(), value.begin(), ::toupper);

	// pad string with @ 
	// BUG BUG Not sure if this is correct. 
	value.append((length / 6) - value.length(), '@');

	// Encode each ASCII character to 6 bit ASCII according to ITU-R M.1371-4
	// BUG BUG Is this faster or slower than using a lookup table ??
	std::bitset<6> bitValue;
	for (int i = 0; i < static_cast<int>(value.length()); i++) {
		bitValue = value[i] >= 64 ? value[i] - 64 : value[i];
		for (int j = 0, k = 5; j < 6; j++, k--) {
			// set the bit values, storing as MSB
			binaryData.at((i * 6) + start + k) = bitValue.test(j);
		}
	}
}
