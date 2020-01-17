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
// Unit: TwoCanError - Get/Format error codes and Debug Print functions
// Owner: twocanplugin@hotmail.com
// Date: 6/1/2020
// Version History: 
// 1.0 Initial Release (based on TwoCan Plugin version 1.6)
//


#include "twocanerror.h"

#ifdef __LINUX__
#include <cerrno>
#endif

char *GetErrorMessage(int errorCode) {
	
#ifdef  __WXMSW__ 

	// Retrieve the Windows system error message for the last-error code

	LPVOID lpMsgBuf;
	
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	return (char *)lpMsgBuf;
	
#endif

#ifdef __LINUX__

	// Retrieve the Linux system error message for the last-error code
	
	return (char *)strerror(errorCode);
	
#endif

}

