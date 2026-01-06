#pragma once

#include <string>
#include <vector>

#include "EthDriver.h"

/*
	File: ImportEngine.h

	Description:
		Provides functions to merge or overwrite EthDriver entries from a CSV import
		into the existing registry entries.

		Functions return an ImportResult struct containing details about the operation,
		including updated drivers, newly added drivers, any errors encountered,
		and a success flag.

		Helper functions are defined in ImportEngine.cpp under a private namespace.
*/

namespace ImportEngine
{
	struct ImportResult
	{
		// Drivers that are modified (existing registry entries with updated values)
		std::vector<EthDriver> updated_drivers;

		// Drivers that are added (new AB_ETH-x entries created)
		std::vector<EthDriver> new_drivers;

		// Collection of error messages that occurred during import processing
		std::vector<std::wstring> errors;

		// Indicates whether the operation completed successfully
		bool success = true;
	};

	// Merges imported drivers into registry drivers, keeping existing entries and updating matches.
	// Returns an ImportResult with updated/new drivers and any errors encountered.
	ImportResult merge_drivers(
		const std::vector<EthDriver>& registry_drivers,
		const std::vector<EthDriver>& csv_drivers
	);

	// Overwrites registry drivers with imported drivers, replacing all matching entries.
	// Returns an ImportResult with updated/new drivers and any errors encountered.
	ImportResult overwrite_drivers(
		const std::vector<EthDriver>& registry_drivers,
		const std::vector<EthDriver>& csv_drivers
	);

} // namespace ImportEngine