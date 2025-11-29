#pragma once

#include <string>
#include <vector>
#include "EthDriver.h"

namespace CSV
{
	//Parse CSV file into EthDriver Struct
	bool ReadDriversFromFile(	const std::wstring& path, 
								std::vector<EthDriver>& drivers_out, 
								std::wstring& error_message);

	//Write drivers back to CSV file
	bool WriteDriversToFile(	const std::wstring& path,
								const std::vector<EthDriver>& drivers_in,
								std::wstring& error_message);

	//Validate CSV file format
	bool ValidateFormat(		const std::wstring& path,
								std::wstring& error_message);
}