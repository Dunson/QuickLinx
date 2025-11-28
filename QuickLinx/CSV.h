#pragma once

#include <string>
#include <vector>
#include "EthDriver.h"

namespace CSV
{
	//Parse CSV file into EthDriver Struct
	bool ReadDrivesFromFile(	const std::wstring& path, 
								std::vector<EthDriver>& drivers_out, 
								std::wstring& error_message);

	//Write drivers back to CSV file
	bool WriteDrivesToFile(		const std::wstring& path,
								const std::vector<EthDriver>& drivers_in,
								std::wstring& error_message);

	//Validate CSV file format
	bool ValidateFormat(		const std::wstring& path,
								std::wstring& error_message);

	//Helper functions for IP range expansion -- WILL BE MOVED TO PRIVATE NAMESPACE LATER
	namespace Line {
		bool IsIPRange(const std::wstring& line_item);
		std::vector<std::wstring> ExpandIPRange(const std::wstring& line_item);
	}
}