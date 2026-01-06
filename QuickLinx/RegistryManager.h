#pragma once

#include <string>
#include <vector>

#include "EthDriver.h"

/*
	File: RegistryManager.h

	Description:
		Provides static functions to load, save, and delete EthDriver entries from the
		Windows Registry. All operations are performed on EthDriver structures and
		interact with registry keys under:

		HKLM\SOFTWARE\WOW6432Node\Rockwell Software\RSLinx\Drivers\AB_ETH\
*/

class RegistryManager
{
public:
	// Loads all AB_ETH-x driver configurations from the registry.
	// Returns a vector of EthDriver structures; empty vector if no drivers found.
	static std::vector<EthDriver> LoadDrivers();

	// Saves (creates or overwrites) a single driver in the registry.
	// Returns true on success; false if the operation failed.
	static bool SaveDriver(const EthDriver& driver);

	// Deletes a driver from the registry by its key name (e.g., "AB_ETH-1").
	// Returns true on success; false if the driver was not found or deletion failed.
	static bool DeleteDriver(const std::wstring& keyName);
};