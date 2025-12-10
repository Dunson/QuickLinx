#pragma once

#include <vector>
#include <string>

#include "EthDriver.h"

/*
	File: ImportEngine.h

	Description: 
		Provides functions to merge_drivers or overwrite_drivers EthDriver entries
		from a CSV import into the existing registry entries.

		Functions return an ImportResult struct containing
		details about the operation, including updated drivers,
		new drivers added, any errors encountered, and a success flag.

		Helper functions are included in ImportEngine.cpp under a private namespace
*/

namespace ImportEngine
{
	struct ImportResult
	{
		std::vector<EthDriver>		updated_drivers;		// Drivers that were modified	(Existing registry entries)
		std::vector<EthDriver>		new_drivers;			// Drivers that were added		(New AB_ETH-x entries)		
			
		std::vector<std::wstring>	errors;					// Any errors that occurred during the import process

		bool						success = true;			// Success flag
	};

	// Merges the imported drivers into the registry drivers.
	ImportResult merge_drivers(				const std::vector<EthDriver>& registry_drivers,
									const std::vector<EthDriver>& csv_drivers);

	// Overwrites the registry drivers with the imported drivers.
	ImportResult overwrite_drivers(			const std::vector<EthDriver>& registry_drivers,
									const std::vector<EthDriver>& csv_drivers);

} // namespace ImportEngine