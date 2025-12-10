#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

/*
	File: EthDriver.h

	Description: 
		Represents an Ethernet Driver configuration as stored in the Windows Registry under: 
		
		HKLM\\SOFTWARE\\WOW6432Node\\Rockwell Software\\RSLinx\\Drivers\\AB_ETH\\AB_ETH-x
*/

struct EthDriver {

	std::wstring				key_name;				// "AB_ETH-1", "AB_ETH-2", ...
	std::wstring				name;					// The "Name" Registry value
	DWORD						station;				// The "Station" DWORD
	DWORD						ping_timeout;			// The "Ping Timeout" DWORD (seconds)
	DWORD						inactivity_timeout;		// The "Inactivity Timeout" DWORD (seconds)
	DWORD						startup;				// The "Startup" DWORD (0 or 1)
	
	std::vector<std::wstring>	nodes;					// Each entry under the Node Table

};