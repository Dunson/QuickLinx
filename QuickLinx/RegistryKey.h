#pragma once

	#include <string>
#include <vector>
#include <windows.h>

/*
	File: RegistryKey.h

	Description:
		A RAII (Resource Acquisition Is Initialization) wrapper around Windows Registry keys.
		Provides a clean interface for opening, creating, querying, setting, enumerating,
		and deleting registry keys and values.

		Automatically closes registry key handles on destruction, preventing resource leaks.
		Move-only semantics ensure safe ownership transfer without handle duplication.

		Wraps Windows API functions:
			* RegOpenKeyEx, RegCreateKeyEx
			* RegQueryValueEx, RegSetValueEx
			* RegEnumKeyEx, RegEnumValue
			* RegDeleteValue, RegDeleteKey
*/

class RegistryKey
{
public:
	// Constructor and destructor
	RegistryKey() noexcept = default;
	~RegistryKey();

	// Non-copyable to prevent handle duplication and resource corruption
	RegistryKey(const RegistryKey&) = delete;
	RegistryKey& operator=(const RegistryKey&) = delete;

	// Move constructor transfers handle ownership safely
	RegistryKey(RegistryKey&& other) noexcept
		: m_hKey(other.m_hKey)
	{
		other.m_hKey = nullptr;
	}

	// Key Operations

	// Opens an existing registry key in read-only mode.
	// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
	LONG Open(
		HKEY root,
		const std::wstring& sub_key,
		REGSAM access = KEY_READ | KEY_WOW64_32KEY
	) noexcept;

	// Creates or opens a registry key in read-write mode.
	// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
	LONG Create(
		HKEY root,
		const std::wstring& sub_key,
		DWORD options = REG_OPTION_NON_VOLATILE,
		REGSAM access = KEY_READ | KEY_WRITE | KEY_WOW64_32KEY
	) noexcept;

	// Returns true if a registry key is currently open
	bool IsOpen() const noexcept { return m_hKey != nullptr; }

	// Explicitly closes the registry key handle if open
	void Close() noexcept;

	// Enumeration

	// Enumerates subkey names at the given index.
	// Returns ERROR_SUCCESS on success; ERROR_NO_MORE_ITEMS when index exceeds available subkeys.
	LONG EnumSubkey(
		DWORD index,
		std::wstring& name_out
	) const noexcept;

	// Enumerates value entries (name, type, raw data) at the given index.
	// Returns ERROR_SUCCESS on success; ERROR_NO_MORE_ITEMS when index exceeds available values.
	LONG EnumValue(
		DWORD index,
		std::wstring& name_out,
		DWORD& type_out,
		std::vector<BYTE>& data_out
	) const noexcept;

	// Query Helpers

	// Queries a string value from the registry.
	// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if value does not exist.
	LONG QueryString(
		const std::wstring& value_name,
		std::wstring& value_out
	) const noexcept;

	// Queries a DWORD value from the registry.
	// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if value does not exist.
	LONG QueryDword(
		const std::wstring& value_name,
		DWORD& value_out
	) const noexcept;

	// Write Helpers

	// Sets a string value in the registry.
	// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
	LONG SetString(
		const std::wstring& value_name,
		const std::wstring& value
	) const noexcept;

	// Sets a DWORD value in the registry.
	// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
	LONG SetDword(
		const std::wstring& value_name,
		DWORD value
	) const noexcept;

	// Deletion

	// Deletes a value entry from the registry key.
	// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if value does not exist.
	LONG DeleteValue(
		const std::wstring& value_name
	) const noexcept;

	// Deletes a subkey from the registry.
	// Subkey must be empty (no child keys or values).
	// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if subkey does not exist.
	LONG DeleteSubkey(
		const std::wstring& sub_key_name
	) const noexcept;

private:
	// Windows registry key handle; nullptr when not open
	HKEY m_hKey = nullptr;
};
