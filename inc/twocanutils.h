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

#ifndef TWOCAN_UTILS_H
#define TWOCAN_UTILS_H

// For error definitions
#include "twocanerror.h"

// For specific Windows functions & typedefs such as LPDWORD etc.
#ifdef __WXMSW__
#include <windows.h>
#endif

// For wxWidgets Pi
#include <wx/math.h>

// For strtoul
#include <stdlib.h>

// For memcpy
#include <string.h>

// Actisense Specific
// Name of the Actisense EBL Log Reader or NGT-1 Device for input
// Used in he settings dialog and in determinig what device to load
#define CONST_LOG_READER "EBL Log Reader"
#define CONST_NGT_READER "NGT-1 Device Reader"

// Some NMEA 2000 constants
#define CONST_HEADER_LENGTH 4
#define CONST_PAYLOAD_LENGTH 8
#define CONST_FRAME_LENGTH (CONST_HEADER_LENGTH + CONST_PAYLOAD_LENGTH)

// Note to self, I think this is correct, address 255 is the global address, 254 is the NULL address
#define CONST_GLOBAL_ADDRESS 255
#define CONST_MAX_DEVICES 253 
#define CONST_NULL_ADDRESS 254

// Maximum payload for NMEA multi-frame Fast Message
#define CONST_MAX_FAST_PACKET 223_LENGTH

// Maximum payload for ISO 11783-3 Multi Packet
#define CONST_MAX_ISO_MULTI_PACKET_LENGTH 1785 

// For this device's ISO Address Claim 
// BUG BUG Should be user configurable
#define CONST_MANUFACTURER_CODE 2019 // I assume proper numbers are issued by NMEA
#define CONST_DEVICE_FUNCTION 130 // BUG BUG Should have an enum for all of the NMEA 2000 device function codes
#define CONST_DEVICE_CLASS 120 // BUG BUG Should have an enum for all of the NMEA 2000 device class codes
#define CONST_MARINE_INDUSTRY 4

// For this device's NMEA Product Information
// BUG BUG Should be user configurable
#define CONST_DATABASE_VERSION 2100
#define CONST_PRODUCT_CODE 1770 // Trivia - Captain Cook discovers Australia !
#define CONST_CERTIFICATION_LEVEL 0 // We have not been certified, although I think we support the PGN's required for level 1
#define CONST_LOAD_EQUIVALENCY 1 // PC is self powered, so assume little or no drain on NMEA 2000 network
#define CONST_MODEL_ID "Actisense plugin"
#define CONST_SOFTWARE_VERSION  "1.0" // BUG BUG Should derive from PLUGIN_VERSION_MAJOR etc.

// Maximum number of multi-frame Fast Messages we can support in the Fast Message Buffer, just an arbitary number
#define CONST_MAX_MESSAGES 100

// Stale Fast Message expiration  (I think Fast Messages must be sent within 250 msec)
#define CONST_TIME_EXCEEDED 250

// Whether an existing Fast Message entry exists, in order to append a frame
#define NOT_FOUND -1

// Dropped or missing frames from a Fast Message
#define CONST_DROPPEDFRAME_THRESHOLD 200
#define CONST_DROPPEDFRAME_PERIOD 5

// Some time intervals 
#define CONST_TEN_MILLIS 10
#define CONST_ONE_SECOND 100 * CONST_TEN_MILLIS
#define CONST_ONE_MINUTE 60 * CONST_ONE_SECOND

// NMEA 2000 priorities - derived from observation. As priority is only 3 bits, values range from 0-7
#define CONST_PRIORITY_MEDIUM 6 // seen for 60928 ISO Address Claim, 59904 ISO Request
#define CONST_PRIORITY_LOW 7 // Seen for 126996 Product Information
#define CONST_PRIORITY_VERY_HIGH 2 // Seen for Navigation Data and Speed through Water
#define CONST_PRIORITY_HIGH 3 // Seen for Depth

// Some useful conversion functions

// Radians to degrees and vice versa
#define RADIANS_TO_DEGREES(x) (x * 180 / M_PI)
#define DEGREES_TO_RADIANS(x) (x * M_PI / 180)

// Celsius to Fahrenheit
#define CELSIUS_TO_FAHRENHEIT(x) ((x * 9 / 5) + 32)
#define FAHRENHEIT_TO_CELSIUS(x) ((x - 32) * 5 / 9)

// Pascal to Pounds Per Square Inch (PSI)
#define PASCAL_TO_PSI(x) (x * 0.00014504)
#define PSI_TO_PASCAL(x) (x * 6894.75729)

// Metres per second
#define CONVERT_MS_KNOTS 1.94384
#define CONVERT_MS_KMH 3.6
#define CONVERT_MS_MPH 2.23694

// Metres to feet, fathoms
#define CONVERT_FATHOMS_FEET 6
#define CONVERT_METRES_FEET 3.28084
#define CONVERT_METRES_FATHOMS (CONVERT_METRES_FEET / CONVERT_FATHOMS_FEET)
#define CONVERT_METRES_NAUTICAL_MILES 0.000539957

// Kelvin to celsius
#define CONST_KELVIN -273.15
#define CONVERT_KELVIN(x) (x + CONST_KELVIN )

// NMEA 183 GPS Fix Modes
#define GPS_MODE_AUTONOMOUS 'A' 
#define GPS_MODE_DIFFERENTIAL 'D' 
#define GPS_MODE_ESTIMATED 'E' 
#define GPS_MODE_MANUAL  'M'
#define GPS_MODE_SIMULATED 'S'
#define GPS_MODE_INVALID 'N' 

// Some defintions used in NMEA 2000 PGN's

#define HEADING_TRUE 0
#define HEADING_MAGNETIC 1

#define GREAT_CIRCLE 0
#define RHUMB_LINE 1

#define	GNSS_FIX_NONE 0
#define	GNSS_FIX_GNSS 1
#define	GNSS_FIX_DGNSS 2
#define	GNSS_FIX_PRECISE_GNSS 3
#define	GNSS_FIX_REAL_TIME_KINEMATIC_INT 4
#define	GNSS_FIX_REAL_TIME_KINEMATIC_FLOAT 5
#define	GNSS_FIX_ESTIMATED 6
#define	GNSS_FIX_MANUAL 7
#define	GNSS_FIX_SIMULATED 8

#define	GNSS_INTEGRITY_NONE 0
#define	GNSS_INTEGRITY_SAFE 1
#define	GNSS_INTEGRITY_CAUTION 2

#define	WIND_REFERENCE_TRUE 0
#define	WIND_REFERENCE_MAGNETIC 1
#define	WIND_REFERENCE_APPARENT 2
#define	WIND_REFERENCE_BOAT_TRUE 3
#define	WIND_REFERENCE_BOAT_MAGNETIC 4

#define	TEMPERATURE_SEA 0
#define	TEMPERATURE_EXTERNAL 1
#define	TEMPERATURE_INTERNAL 2
#define	TEMPERATURE_ENGINEROOM 3
#define	TEMPERATURE_MAINCABIN 4
#define	TEMPERATURE_LIVEWELL 5
#define	TEMPERATURE_BAITWELL 6
#define	TEMPERATURE_REFRIGERATOR 7
#define	TEMPERATURE_HEATING 8
#define	TEMPERATURE_DEWPOINT 9
#define	TEMPERATURE_APPARENTWINDCHILL 10
#define	TEMPERATURE_THEORETICALWINDCHILL 11
#define	TEMPERATURE_HEATINDEX 12
#define	TEMPERATURE_FREEZER 13
#define	TEMPERATURE_EXHAUST 14


// Bit values to determine what NMEA 2000 PGN's are converted to their NMEA 0183 equivalent
// Warning must match order of items in Preferences Dialog !!
#define FLAGS_HDG 1
#define FLAGS_VHW 2 
#define FLAGS_DPT 4
#define FLAGS_GLL 8
#define FLAGS_VTG 16
#define FLAGS_GGA 32
#define FLAGS_ZDA 64
#define FLAGS_MWV 128
#define FLAGS_MWT 256
#define FLAGS_DSC 512
#define FLAGS_AIS 1024
#define FLAGS_RTE 2048
#define FLAGS_ROT 4096
#define FLAGS_XTE 8192
#define FLAGS_XDR 16384
#define FLAGS_ENG 32768
#define FLAGS_TNK 65536
#define FLAGS_RDR 131072
#define FLAGS_BAT 262144

// Bit values to determine in which format the log file is written
#define FLAGS_LOG_NONE 0
#define FLAGS_LOG_RAW 1 // My format 12 pairs of hex digits, 4 the CAN 2.0 Id, 8 the payload 
#define FLAGS_LOG_CANBOAT 2 // I recall seeing this format used in Canboat
#define FLAGS_LOG_CANDUMP 3 // Candump, a Linux utility
#define FLAGS_LOG_YACHTDEVICES 4 // Found some samples from their Voyage Data Recorder
#define FLAGS_LOG_CSV 5 // Comma Separted Variables

// All the NMEA 2000 data is transmitted as an unsigned char which for convenience sake, I call a byte
typedef unsigned char byte;

// ASCII Control Chracters
const byte DLE {0x10};
const byte STX {0x02};
const byte ETX {0x03};
const byte ESC {0x1B};
const byte BEMSTART {0x01};
const byte BEMEND {0x0A};

// Actisense NGT-1 commands
const byte N2K_TX_CMD {0x92};
const byte N2K_RX_CMD {0x93};
const byte NGT_TX_CMD {0xA1};
const byte NGT_RX_CMD {0xA3};


// CAN v2.0 29 bit header as used by NMEA 2000
typedef struct CanHeader {
	byte priority;
	byte source;
	byte destination;
	unsigned int pgn;
} CanHeader;

// NMEA 2000 Product Information, transmitted in PGN 126996 NMEA Product Information
typedef struct ProductInformation {
	unsigned int dataBaseVersion;
	unsigned int productCode;
	// Note these are transmitted as unterminated 32 bit strings, allow for the additional terminating NULL
	char modelId[33];
	char softwareVersion[33];
	char modelVersion[33];
	char serialNumber[33];
	byte certificationLevel;
	byte loadEquivalency;
} ProductInformation;

// NMEA 2000 Device Information, transmitted in PGN 60928 ISO Address Claim
typedef struct DeviceInformation {
	unsigned long uniqueId;
	unsigned int deviceClass;
	unsigned int deviceFunction;
	byte deviceInstance;
	byte industryGroup;
	unsigned int manufacturerId;
	// Network Address is not part of the 60928 address claim, as the source network address is in the header
	// however this field is part of PGN 65420 Commanded Address
	byte networkAddress;
	// NAME is the value of the 8 bytes that make up this PGN. The NAME is used for resolving addess claim conflicts
	unsigned long deviceName;
} DeviceInformation;

// Used  to store the data for the Network Map, combines elements from address claim & product information
typedef struct NetworkInformation {
	unsigned long uniqueId;
	unsigned int manufacturerId;
	ProductInformation productInformation;
	wxDateTime timestamp; // Updated upon reception of heartbeat or address claim. Used to determine stale entries
} NetworkInformation;

// Utility functions used by both the ActisenseDevice

class TwoCanUtils {
	
public:
	
	// Convert four bytes to an integer so that some NMEA 2000 values can be 
	// derived from odd length, non byte aliged bitmasks
	static int ConvertByteArrayToInteger(const byte *buf, unsigned int *value);
	// Convert an integer to a byte array
	static int ConvertIntegerToByteArray(const int value, byte *buf);
	// Decodes a 29 bit CAN header from a byte array
	static int DecodeCanHeader(const byte *buf, CanHeader *header);
	// And its companion, encode a 29 bit CAN Header to a byte array
	static int EncodeCanHeader(unsigned int *id, const CanHeader *header);
	// Convert a string of hex characters to the corresponding byte array
	static int ConvertHexStringToByteArray(const byte *hexstr, const unsigned int len, byte *buf);
	// BUG BUG Any other conversion functions required ??

	
	// Data validation
	// Found somewhere, either Canboat or OpenSkipper
	// For each data type, the following is defined
	// MAX VALUE indicates Data not available
	// MAX VALUE -1 indicates Data out of range
	// MAX VALUE - 2 indicates NMEA reserved (not sure exactly what it is reserved for)

	// BUG BUG from a performance perspective would checking (value > MAX_VALUE - 2) be better ?
	template<typename T>
	static bool IsDataValid(T value);

	static bool IsDataValid(byte value) {
		if ((value += UCHAR_MAX) || (value == UCHAR_MAX - 1) || (value == UCHAR_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(char value) {
		if ((value == CHAR_MAX) || (value == CHAR_MAX - 1) || (value == CHAR_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(unsigned short value) {
		if ((value == USHRT_MAX) || (value == USHRT_MAX - 1) || (value == USHRT_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(short value) {
		if ((value == SHRT_MAX) || (value == SHRT_MAX - 1) || (value == SHRT_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(unsigned int value) {
		if ((value == UINT_MAX) || (value == UINT_MAX - 1) || (value == UINT_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(int value) {
		if ((value == INT_MAX) || (value == INT_MAX - 1) || (value == INT_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(unsigned long value) {
		if ((value == ULONG_MAX) || (value == ULONG_MAX - 1) || (value == ULONG_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(long value) {
		if ((value == LONG_MAX) || (value == LONG_MAX - 1) || (value == LONG_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
	
	static bool IsDataValid(unsigned long long value) {
		if ((value == ULLONG_MAX) || (value == ULLONG_MAX - 1) || (value == ULLONG_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}

	static bool IsDataValid(long long value) {
		if ((value == LLONG_MAX) || (value == LLONG_MAX - 1) || (value == LLONG_MAX - 2)) {
			return FALSE;
		}
		else {
			return TRUE;
		}
	}	
};

#endif