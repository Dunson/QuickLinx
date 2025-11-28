#pragma once

#include <vector>
#include <string>
#include "EthDriver.h"

class RegistryManager {

public:

	//	Load all AB_ETH-x drivers from Registry
	static std::vector<EthDriver> LoadDrivers();

	//	Save (create or overwrite) one driver (Returns true on success)
	static bool SaveDriver(const EthDriver& driver);

	//	Delete a driver
	static bool DeleteDriver(const std::wstring& keyName);

};