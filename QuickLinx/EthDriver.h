#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

/*
	File: EthDriver.h

	Description:
		Represents an Ethernet Driver configuration as stored in the Windows Registry under:
		
		HKLM\SOFTWARE\WOW6432Node\Rockwell Software\RSLinx\Drivers\AB_ETH\AB_ETH-x
*/

struct EthDriver
{
	// Unique identifier for a driver instance (e.g., "AB_ETH-1", "AB_ETH-2")
	std::wstring key_name;

	// Display name for the driver (from Registry "Name" value)
	std::wstring name;

	// Station number (from Registry "Station" DWORD)
	DWORD station;

	// Ping timeout in seconds (from Registry "Ping Timeout" DWORD)
	DWORD ping_timeout;

	// Inactivity timeout in seconds (from Registry "Inactivity Timeout" DWORD)
	DWORD inactivity_timeout;

	// Startup flag: 0 = disabled, 1 = enabled (from Registry "Startup" DWORD)
	DWORD startup;

	// List of nodes configured under a driver's Node Table
	std::vector<std::wstring> nodes;
};