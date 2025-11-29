#include "RegistryKey.h"

#include <cassert>

RegistryKey::~RegistryKey()
{
	Close();
}

//------------------------------------------------------
// Open an existing key (read-only by default)
//------------------------------------------------------
LONG RegistryKey::Open(		HKEY root,
							const std::wstring& sub_key,
							REGSAM access) noexcept
{
	// Close any existing handle first
	Close();

	HKEY hKey = nullptr;
	const LONG result = ::RegOpenKeyExW(
		root,
		sub_key.c_str(),
		0,          // reserved
		access,
		&hKey);

	if (result == ERROR_SUCCESS)
	{
		m_hKey = hKey;
	}

	return result;
}

//------------------------------------------------------
// Create (or open) a key
//------------------------------------------------------
LONG RegistryKey::Create(	HKEY root,
							const std::wstring& sub_key,
							DWORD options,
							REGSAM access) noexcept
{
	// Close any existing handle first
	Close();

	HKEY hKey = nullptr;
	DWORD disposition = 0;

	const LONG result = ::RegCreateKeyExW(
		root,
		sub_key.c_str(),
		0,                  // reserved
		nullptr,            // class string
		options,
		access,
		nullptr,            // security attributes
		&hKey,
		&disposition);

	if (result == ERROR_SUCCESS)
	{
		m_hKey = hKey;
	}

	return result;
}

//------------------------------------------------------
// Explicit close
//------------------------------------------------------
void RegistryKey::Close() noexcept
{
	if (m_hKey != nullptr)
	{
		::RegCloseKey(m_hKey);
		m_hKey = nullptr;
	}
}

//------------------------------------------------------
// Enumerate subkeys by index
//------------------------------------------------------
LONG RegistryKey::EnumSubkey(	DWORD index,
								std::wstring& name_out) const noexcept
{
	name_out.clear();

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	// Start with a reasonable buffer; will grow if needed.
	DWORD name_len = 256;
	std::wstring buffer(name_len, L'\0');
	FILETIME ft = {};

	LONG result = ::RegEnumKeyExW(
		m_hKey,
		index,
		&buffer[0],
		&name_len,
		nullptr,
		nullptr,
		nullptr,
		&ft);

	if (result == ERROR_MORE_DATA)
	{
		// name_len now contains the required size (in chars)
		buffer.assign(name_len + 1, L'\0');
		result = ::RegEnumKeyExW(
			m_hKey,
			index,
			&buffer[0],
			&name_len,
			nullptr,
			nullptr,
			nullptr,
			&ft);
	}

	if (result == ERROR_SUCCESS)
	{
		// name_len does not include terminating null
		name_out.assign(buffer.data(), name_len);
	}

	return result;
}

//------------------------------------------------------
// Enumerate values by index (name, type, raw data)
//------------------------------------------------------
LONG RegistryKey::EnumValue(	DWORD index,
								std::wstring& name_out,
								DWORD& type_out,
								std::vector<BYTE>& data_out) const noexcept
{
	name_out.clear();
	data_out.clear();
	type_out = 0;

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD name_len = 256;	// in characters
	DWORD data_size = 256;	// in bytes

	std::wstring name_buffer(name_len, L'\0');
	data_out.resize(data_size);

	LONG result = ::RegEnumValueW(
		m_hKey,
		index,
		&name_buffer[0],
		&name_len,
		nullptr,
		&type_out,
		data_out.data(),
		&data_size);

	if (result == ERROR_MORE_DATA)
	{
		// Resize buffers according to required sizes and retry once.
		name_buffer.assign(name_len + 1, L'\0');
		data_out.resize(data_size);

		result = ::RegEnumValueW(
			m_hKey,
			index,
			&name_buffer[0],
			&name_len,
			nullptr,
			&type_out,
			data_out.data(),
			&data_size);
	}

	if (result == ERROR_SUCCESS)
	{
		name_out.assign(name_buffer.data(), name_len);
		data_out.resize(data_size);	// shrink to actual
	}

	return result;
}

//------------------------------------------------------
// Query a REG_SZ / REG_EXPAND_SZ string value
//------------------------------------------------------
LONG RegistryKey::QueryString(	const std::wstring& value_name,
								std::wstring& value_out) const noexcept
{
	value_out.clear();

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD type = 0;
	DWORD data_size = 0;

	// First call: get size
	LONG result = ::RegQueryValueExW(
		m_hKey,
		value_name.c_str(),
		nullptr,
		&type,
		nullptr,
		&data_size);

	if (result != ERROR_SUCCESS)
		return result;

	if (type != REG_SZ && type != REG_EXPAND_SZ)
		return ERROR_DATATYPE_MISMATCH;

	std::vector<wchar_t> buffer(data_size / sizeof(wchar_t));

	result = ::RegQueryValueExW(
		m_hKey,
		value_name.c_str(),
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(buffer.data()),
		&data_size);

	if (result == ERROR_SUCCESS)
	{
		// Ensure null-terminated and assign
		if (!buffer.empty())
		{
			// Some APIs may return without trailing null; be defensive.
			if (buffer.back() != L'\0')
				buffer.push_back(L'\0');
			value_out.assign(buffer.data());
		}
	}

	return result;
}

//------------------------------------------------------
// Query a REG_DWORD value
//------------------------------------------------------
LONG RegistryKey::QueryDword(	const std::wstring& value_name,
								DWORD& value_out) const noexcept
{
	value_out = 0;

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD type = 0;
	DWORD data_size = sizeof(DWORD);

	LONG result = ::RegQueryValueExW(
		m_hKey,
		value_name.c_str(),
		nullptr,
		&type,
		reinterpret_cast<LPBYTE>(&value_out),
		&data_size);

	if (result != ERROR_SUCCESS)
		return result;

	if (type != REG_DWORD || data_size != sizeof(DWORD))
		return ERROR_DATATYPE_MISMATCH;

	return ERROR_SUCCESS;
}

//------------------------------------------------------
// Set a REG_SZ value
//------------------------------------------------------
LONG RegistryKey::SetString(	const std::wstring& value_name,
								const std::wstring& value) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	const DWORD byte_count =
		static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));

	return ::RegSetValueExW(
		m_hKey,
		value_name.c_str(),
		0,
		REG_SZ,
		reinterpret_cast<const BYTE*>(value.c_str()),
		byte_count);
}

//------------------------------------------------------
// Set a REG_DWORD value
//------------------------------------------------------
LONG RegistryKey::SetDword(		const std::wstring& value_name,
								DWORD value) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	return ::RegSetValueExW(
		m_hKey,
		value_name.c_str(),
		0,
		REG_DWORD,
		reinterpret_cast<const BYTE*>(&value),
		static_cast<DWORD>(sizeof(DWORD)));
}

//------------------------------------------------------
// Delete a single value
//------------------------------------------------------
LONG RegistryKey::DeleteValue(	const std::wstring& value_name) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	return ::RegDeleteValueW(
		m_hKey,
		value_name.c_str());
}

//------------------------------------------------------
// Delete a subkey (must be empty)
//------------------------------------------------------
LONG RegistryKey::DeleteSubkey(	const std::wstring& sub_key_name) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	// Note: this will fail with ERROR_ACCESS_DENIED if the subkey is not empty.
	return ::RegDeleteKeyW(
		m_hKey,
		sub_key_name.c_str());
}
