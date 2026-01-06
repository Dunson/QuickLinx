#include "RegistryManager.h"
#include "RegistryKey.h"

#include <string>
#include <windows.h>

// Registry Configuration (Anonymous Namespace)
namespace
{
	// Base registry path for AB_ETH drivers under 32-bit RSLinx on 64-bit Windows
	const wchar_t* RSLINX_AB_ETH_BASE =
		L"SOFTWARE\\WOW6432Node\\Rockwell Software\\RSLinx\\Drivers\\AB_ETH";

	// Registry value names for driver properties
	const wchar_t* VAL_NAME_NAME = L"Name";
	const wchar_t* VAL_NAME_STATION = L"Station";
	const wchar_t* VAL_NAME_PING_TIMEOUT = L"Ping Timeout";
	const wchar_t* VAL_NAME_INACTIVITY = L"Inactivity Timeout";
	const wchar_t* VAL_NAME_STARTUP = L"Startup";

	// Registry subkey name for node IP table
	const wchar_t* SUBKEY_NODE_TABLE = L"Node Table";

	// Joins a base registry path with a subkey using backslash delimiter.
	// Example: JoinPath("HKLM\\Software", "MyKey") returns "HKLM\\Software\\MyKey"
	std::wstring JoinPath(const std::wstring& base, const std::wstring& sub)
	{
		if (base.empty())
			return sub;
		if (sub.empty())
			return base;
		return base + L"\\" + sub;
	}

	// Converts raw REG_SZ registry data (UTF-16 bytes) into a std::wstring.
	// Trims trailing null terminators.
	std::wstring BytesToWString(const std::vector<BYTE>& data)
	{
		if (data.empty())
			return std::wstring();

		const wchar_t* wptr = reinterpret_cast<const wchar_t*>(data.data());
		std::size_t len = data.size() / sizeof(wchar_t);

		// Trim trailing null terminators
		while (len > 0 && wptr[len - 1] == L'\0')
		{
			--len;
		}

		return std::wstring(wptr, len);
	}

} // namespace

// Loads all AB_ETH-x driver configurations from the Windows Registry.
// Returns a vector of EthDriver structures; empty vector if no drivers are found
// or the registry key cannot be accessed.
std::vector<EthDriver> RegistryManager::LoadDrivers()
{
	std::vector<EthDriver> drivers;

	// Open the base AB_ETH drivers key
	RegistryKey baseKey;
	LONG result = baseKey.Open(HKEY_LOCAL_MACHINE, RSLINX_AB_ETH_BASE);
	if (result != ERROR_SUCCESS)
	{
		// Base key does not exist or cannot be opened; return empty list
		return drivers;
	}

	// Enumerate all subkeys under the base path (AB_ETH-1, AB_ETH-2, etc.)
	DWORD index = 0;
	std::wstring subKeyName;

	while (true)
	{
		subKeyName.clear();
		result = baseKey.EnumSubkey(index, subKeyName);

		if (result == ERROR_NO_MORE_ITEMS)
		{
			break; // Enumeration complete
		}
		else if (result != ERROR_SUCCESS)
		{
			break; // Unexpected error; stop enumeration
		}

		++index;

		// Open this specific driver key (e.g., AB_ETH-1)
		const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, subKeyName);
		RegistryKey driverKey;
		if (driverKey.Open(HKEY_LOCAL_MACHINE, fullPath) != ERROR_SUCCESS)
		{
			// Skip this driver if it cannot be opened
			continue;
		}

		// Populate EthDriver structure
		EthDriver driver;
		driver.key_name = subKeyName; // e.g., "AB_ETH-1"

		// Read required values; skip driver if any required value is missing
		if (driverKey.QueryString(VAL_NAME_NAME, driver.name) != ERROR_SUCCESS)
			continue;
		if (driverKey.QueryDword(VAL_NAME_STATION, driver.station) != ERROR_SUCCESS)
			continue;

		// Read optional timeout and startup values; ignore errors
		(void)driverKey.QueryDword(VAL_NAME_PING_TIMEOUT, driver.ping_timeout);
		(void)driverKey.QueryDword(VAL_NAME_INACTIVITY, driver.inactivity_timeout);
		(void)driverKey.QueryDword(VAL_NAME_STARTUP, driver.startup);

		// Load Node Table (list of IP addresses)
		const std::wstring nodePath = JoinPath(fullPath, SUBKEY_NODE_TABLE);
		RegistryKey nodeKey;
		if (nodeKey.Open(HKEY_LOCAL_MACHINE, nodePath) == ERROR_SUCCESS)
		{
			DWORD nodeIndex = 0;
			std::wstring valueName;
			DWORD type = 0;
			std::vector<BYTE> data;

			while (true)
			{
				valueName.clear();
				data.clear();
				type = 0;

				LONG valResult = nodeKey.EnumValue(nodeIndex, valueName, type, data);
				if (valResult == ERROR_NO_MORE_ITEMS)
				{
					break; // No more node entries
				}
				else if (valResult != ERROR_SUCCESS)
				{
					break; // Stop on error
				}

				++nodeIndex;

				// Skip the (Default) value which appears as an empty name
				if (valueName.empty())
					continue;

				// Only process string-type values
				if (type != REG_SZ && type != REG_EXPAND_SZ)
					continue;

				std::wstring ip = BytesToWString(data);
				if (!ip.empty())
				{
					driver.nodes.push_back(ip);
				}
			}
		}

		drivers.push_back(std::move(driver));
	}

	return drivers;
}

// Saves (creates or overwrites) a single driver in the Windows Registry.
// Creates the driver key, writes all driver properties, and populates the Node Table
// with sequential value entries (skipping index 63 per RSLinx convention).
// Returns true on success; false if any operation fails.
bool RegistryManager::SaveDriver(const EthDriver& driver)
{
	if (driver.key_name.empty())
		return false;

	// Construct the full registry path for this driver
	const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, driver.key_name);

	// Create or open the driver key
	RegistryKey driverKey;
	LONG result = driverKey.Create(
		HKEY_LOCAL_MACHINE,
		fullPath,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);

	if (result != ERROR_SUCCESS)
		return false;

	// Write driver properties
	bool ok = true;
	ok = ok && (driverKey.SetString(VAL_NAME_NAME, driver.name) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_STATION, driver.station) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_PING_TIMEOUT, driver.ping_timeout) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_INACTIVITY, driver.inactivity_timeout) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_STARTUP, driver.startup) == ERROR_SUCCESS);

	// Bail if any required property write failed
	if (!ok)
		return false;

	// Write Node Table (list of IP addresses)
	const std::wstring nodePath = JoinPath(fullPath, SUBKEY_NODE_TABLE);

	// Delete any existing Node Table subtree to prevent stale entries
	::RegDeleteTreeW(HKEY_LOCAL_MACHINE, nodePath.c_str());

	// Create the Node Table key
	RegistryKey nodeKey;
	result = nodeKey.Create(
		HKEY_LOCAL_MACHINE,
		nodePath,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);

	if (result != ERROR_SUCCESS)
		return false;

	// Write each node as a sequential value entry: "0", "1", "2", etc., skipping 63
	int node_num = 0;
	for (size_t i = 0; i < driver.nodes.size(); ++i)
	{
		// Skip index 63 (reserved by RSLinx)
		if (node_num == 63)
			++node_num;

		const std::wstring valueName = std::to_wstring(node_num);
		const std::wstring& ip = driver.nodes[i];

		if (nodeKey.SetString(valueName, ip) != ERROR_SUCCESS)
		{
			ok = false;
			break;
		}
		++node_num;
	}

	return ok;
}

// Deletes a driver from the Windows Registry.
// Removes the entire driver key tree, including all subkeys (e.g., Node Table).
// Returns true on success; also returns true if the driver key does not exist
// (idempotent behavior).
bool RegistryManager::DeleteDriver(const std::wstring& keyName)
{
	if (keyName.empty())
		return false;

	const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, keyName);

	// Use RegDeleteTreeW to recursively delete the driver key and all subkeys
	const LONG result = ::RegDeleteTreeW(
		HKEY_LOCAL_MACHINE,
		fullPath.c_str());

	// Success if deleted or key did not exist
	return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}
