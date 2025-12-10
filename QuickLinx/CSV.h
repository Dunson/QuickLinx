#pragma once
#include "EthDriver.h"

#include <string>
#include <vector>

/*
	File: CSV.h

	Desc: Provides functions to read and write EthDriver entries
		  to and from CSV files.

		  Helper functions are included in CSV.cpp under a private namespace
*/


namespace CSV
{
	//Parse CSV file into EthDriver Struct
	bool read_drivers_from_file(	const std::wstring& path, 
									std::vector<EthDriver>& drivers_out, 
									std::wstring& error_message);

	//Write drivers back to CSV file
	bool write_drivers_to_file(		const std::wstring& path,
									const std::vector<EthDriver>& drivers_in,
									std::wstring& error_message);

	//Validate CSV file format
	bool validate_csv_format(		const std::wstring& path,
									std::wstring& error_message);
}