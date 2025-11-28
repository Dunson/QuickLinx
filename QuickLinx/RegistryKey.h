#pragma once

#include <windows.h>
#include <string>
#include <vector>


class RegistryKey {
public:
	RegistryKey() noexcept = default;
	~RegistryKey();

	// Non-copyable (prevents corruption of handle)
	RegistryKey(const RegistryKey&) = delete;
	RegistryKey& operator=(const RegistryKey&) = delete;

	// Movable (safe transfer of ownership)
	RegistryKey(RegistryKey&& other) noexcept
		: m_hKey(other.m_hKey)
	{
		other.m_hKey = nullptr;
	}

	// Open an existing Registry (Read only)
	LONG Open(			HKEY root, 
						const std::wstring& sub_key, 
						REGSAM access = KEY_READ | KEY_WOW64_32KEY) noexcept;

	// Create or open key (for writing)
	LONG Create(		HKEY root, 
						const std::wstring& sub_key, 
						DWORD options = REG_OPTION_NON_VOLATILE, 
						REGSAM access = KEY_READ | KEY_WRITE | KEY_WOW64_32KEY) noexcept;

	//Check if open
	bool IsOpen() const noexcept { return m_hKey != nullptr; }

	//Explicitly close if still open
	void Close() noexcept;

	//-----------------Enumeration-------------------

	// Enumerate subkey names
	LONG EnumSubkey(	DWORD index, 
						std::wstring& name_out) const noexcept;

	//Enumerate value entries (name, type, raw data)
	LONG EnumValue(		DWORD index, 
						std::wstring& name_out, 
						DWORD& type_out, 
						std::vector<BYTE>& data_out) const noexcept;

	//-----------------Query Helpers-----------------
	LONG QueryString(	const std::wstring& value_name, 
						std::wstring& value_out) const noexcept;

	LONG QueryDword(	const std::wstring& value_name, 
						DWORD& value_out) const noexcept;

	//-----------------Write Helpers-----------------
	LONG SetString(		const std::wstring& value_name, 
						const std::wstring& value) const noexcept;

	LONG SetDword(		const std::wstring& value_name, 
						DWORD value) const noexcept;

	//-----------------Deletion----------------------
	LONG DeleteValue(	const std::wstring& value_name) const noexcept;

	LONG DeleteSubkey(	const std::wstring& sub_key_name) const noexcept;

private:
	HKEY m_hKey = nullptr;
};
