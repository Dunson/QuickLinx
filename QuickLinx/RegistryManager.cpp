#include "RegistryManager.h"
#include "RegistryKey.h"

#include <windows.h>
#include <string>

// Base registry path for AB_ETH drivers (32-bit RSLinx on 64-bit Windows)
namespace
{
	const wchar_t* RSLINX_AB_ETH_BASE =
		L"SOFTWARE\\WOW6432Node\\Rockwell Software\\RSLinx\\Drivers\\AB_ETH";

	const wchar_t* VAL_NAME_NAME = L"Name";
	const wchar_t* VAL_NAME_STATION = L"Station";
	const wchar_t* VAL_NAME_PING_TIMEOUT = L"Ping Timeout";
	const wchar_t* VAL_NAME_INACTIVITY = L"Inactivity Timeout";
	const wchar_t* VAL_NAME_STARTUP = L"Startup";

	const wchar_t* SUBKEY_NODE_TABLE = L"Node Table";

	// Small helper to join base path + subkey
	std::wstring JoinPath(const std::wstring& base, const std::wstring& sub)
	{
		if (base.empty())
			return sub;
		if (sub.empty())
			return base;
		return base + L"\\" + sub;
	}

	// Helper to convert raw REG_SZ data (UTF-16) into std::wstring
	std::wstring BytesToWString(const std::vector<BYTE>& data)
	{
		if (data.empty())
			return std::wstring();

		const wchar_t* wptr = reinterpret_cast<const wchar_t*>(data.data());
		std::size_t len = data.size() / sizeof(wchar_t);

		// Trim any trailing nulls
		while (len > 0 && wptr[len - 1] == L'\0')
		{
			--len;
		}

		return std::wstring(wptr, len);
	}
}	// namespace


//	---------------------------------------------------------------------
//	Load all AB_ETH-x drivers from Registry
//	---------------------------------------------------------------------
std::vector<EthDriver> RegistryManager::LoadDrivers()
{
	std::vector<EthDriver> drivers;

	RegistryKey baseKey;
	LONG result = baseKey.Open(HKEY_LOCAL_MACHINE, RSLINX_AB_ETH_BASE);
	if (result != ERROR_SUCCESS)
	{
		// Could not open base key – return empty list.
		return drivers;
	}

	DWORD index = 0;
	std::wstring subKeyName;

	while (true)
	{
		subKeyName.clear();
		result = baseKey.EnumSubkey(index, subKeyName);

		if (result == ERROR_NO_MORE_ITEMS)
		{
			break;	// finished enumeration
		}
		else if (result != ERROR_SUCCESS)
		{
			// Some other error – stop enumerating
			break;
		}

		++index;

		// Open this specific driver key, e.g. AB_ETH-1
		const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, subKeyName);
		RegistryKey driverKey;
		if (driverKey.Open(HKEY_LOCAL_MACHINE, fullPath) != ERROR_SUCCESS)
		{
			// Skip this driver if it cannot be opened
			continue;
		}

		EthDriver driver;
		driver.key_name = subKeyName;	// "AB_ETH-1"

		// Read basic values; if any required one fails, skip this driver
		if (driverKey.QueryString(VAL_NAME_NAME, driver.name) != ERROR_SUCCESS)
			continue;
		if (driverKey.QueryDword(VAL_NAME_STATION, driver.station) != ERROR_SUCCESS)
			continue;

		// Optional values – ignore errors
		(void)driverKey.QueryDword(VAL_NAME_PING_TIMEOUT, driver.ping_timeout);
		(void)driverKey.QueryDword(VAL_NAME_INACTIVITY, driver.inactivity_timeout);
		(void)driverKey.QueryDword(VAL_NAME_STARTUP, driver.startup);

		// ----------------- Load Node Table -----------------
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
					break;
				}
				else if (valResult != ERROR_SUCCESS)
				{
					// Stop on error
					break;
				}

				++nodeIndex;

				// Skip the (Default) value which appears as an empty name
				if (valueName.empty())
					continue;

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


//	---------------------------------------------------------------------
//	Save (create or overwrite) one driver
//	---------------------------------------------------------------------
bool RegistryManager::SaveDriver(const EthDriver& driver)
{
	if (driver.key_name.empty())
		return false;

	const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, driver.key_name);

	RegistryKey driverKey;
	LONG result = driverKey.Create(
		HKEY_LOCAL_MACHINE,
		fullPath,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);

	if (result != ERROR_SUCCESS)
		return false;

	bool ok = true;

	ok = ok && (driverKey.SetString(VAL_NAME_NAME, driver.name) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_STATION, driver.station) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_PING_TIMEOUT, driver.ping_timeout) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_INACTIVITY, driver.inactivity_timeout) == ERROR_SUCCESS);
	ok = ok && (driverKey.SetDword(VAL_NAME_STARTUP, driver.startup) == ERROR_SUCCESS);

	// If writing any of these failed, bail before touching the node table.
	if (!ok)
		return false;

	// ----------------- Write Node Table -----------------
	const std::wstring nodePath = JoinPath(fullPath, SUBKEY_NODE_TABLE);

	// Delete any existing Node Table subtree to avoid stale entries
	::RegDeleteTreeW(HKEY_LOCAL_MACHINE, nodePath.c_str());

	RegistryKey nodeKey;
	result = nodeKey.Create(
		HKEY_LOCAL_MACHINE,
		nodePath,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);

	if (result != ERROR_SUCCESS)
		return false;

	int node_num = 0;

	// Write each node as a sequential value: "0", "1", "2", ... skipping 63
	for (size_t i = 0; i < driver.nodes.size(); ++i)
	{
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


//	---------------------------------------------------------------------
//	Delete a driver (entire key tree)
//	---------------------------------------------------------------------
bool RegistryManager::DeleteDriver(const std::wstring& keyName)
{
	if (keyName.empty())
		return false;

	const std::wstring fullPath = JoinPath(RSLINX_AB_ETH_BASE, keyName);

	// Use RegDeleteTreeW so that any subkeys (e.g., Node Table) are removed too.
	const LONG result = ::RegDeleteTreeW(
		HKEY_LOCAL_MACHINE,
		fullPath.c_str());

	return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
}
