#pragma once

#include <vector>
#include <string>
#include "EthDriver.h"

/*
	File: RegistryManager.h

	Description:
		Provides functions to load, save, and delete EthDriver entries in the Windows Registry.
		All functions are static and operate on EthDriver structures defined in EthDriver.h.

*/


class RegistryManager {

public:

	//	Load all AB_ETH-x drivers from Registry
	static std::vector<EthDriver> LoadDrivers();

	//	Save (create or overwrite_drivers) one driver (Returns true on success)
	static bool SaveDriver(const EthDriver& driver);

	//	Delete a driver
	static bool DeleteDriver(const std::wstring& keyName);

};