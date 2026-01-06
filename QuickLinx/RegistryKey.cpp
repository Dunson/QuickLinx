#include "RegistryKey.h"

#include <cassert>

RegistryKey::~RegistryKey()
{
	Close();
}

// Opens an existing registry key in read-only mode (by default).
// Closes any previously held handle before opening a new one.
// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
LONG RegistryKey::Open(HKEY root,
	const std::wstring& sub_key,
	REGSAM access) noexcept
{
	// Close any existing handle first
	Close();

	HKEY hKey = nullptr;
	const LONG result = ::RegOpenKeyExW(
		root,
		sub_key.c_str(),
		0,                // Reserved
		access,
		&hKey);

	if (result == ERROR_SUCCESS)
	{
		m_hKey = hKey;
	}

	return result;
}

// Creates or opens a registry key with the specified options and access rights.
// Closes any previously held handle before operating on a new one.
// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
LONG RegistryKey::Create(HKEY root,
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
		0,                // Reserved
		nullptr,          // Class string (unused)
		options,
		access,
		nullptr,          // Security attributes (uses default)
		&hKey,
		&disposition);

	if (result == ERROR_SUCCESS)
	{
		m_hKey = hKey;
	}

	return result;
}

// Explicitly closes the registry key handle if currently open.
// Safe to call even if the key is not open.
void RegistryKey::Close() noexcept
{
	if (m_hKey != nullptr)
	{
		::RegCloseKey(m_hKey);
		m_hKey = nullptr;
	}
}

// Enumerates subkey names at the specified index.
// Returns ERROR_SUCCESS on success; ERROR_NO_MORE_ITEMS if index exceeds available subkeys.
// Automatically resizes the buffer if the initial capacity is insufficient.
LONG RegistryKey::EnumSubkey(DWORD index,
	std::wstring& name_out) const noexcept
{
	name_out.clear();

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	// Start with a reasonable buffer; will grow if needed
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
		// name_len now contains the required size (in characters)
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
		// name_len does not include the terminating null character
		name_out.assign(buffer.data(), name_len);
	}

	return result;
}

// Enumerates value entries (name, type, raw data) at the specified index.
// Returns ERROR_SUCCESS on success; ERROR_NO_MORE_ITEMS if index exceeds available values.
// Automatically resizes buffers if initial capacity is insufficient.
LONG RegistryKey::EnumValue(DWORD index,
	std::wstring& name_out,
	DWORD& type_out,
	std::vector<BYTE>& data_out) const noexcept
{
	name_out.clear();
	data_out.clear();
	type_out = 0;

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD name_len = 256;  // In characters
	DWORD data_size = 256; // In bytes

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
		// Resize buffers to the required sizes and retry
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
		// Trim name and data to actual sizes
		name_out.assign(name_buffer.data(), name_len);
		data_out.resize(data_size);
	}

	return result;
}

// Queries a REG_SZ or REG_EXPAND_SZ string value from the registry.
// Automatically resizes the buffer if the initial capacity is insufficient.
// Returns ERROR_SUCCESS on success; ERROR_DATATYPE_MISMATCH if the value is not a string type.
LONG RegistryKey::QueryString(const std::wstring& value_name,
	std::wstring& value_out) const noexcept
{
	value_out.clear();

	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	DWORD type = 0;
	DWORD data_size = 0;

	// First call: query the required buffer size
	LONG result = ::RegQueryValueExW(
		m_hKey,
		value_name.c_str(),
		nullptr,
		&type,
		nullptr,
		&data_size);

	if (result != ERROR_SUCCESS)
		return result;

	// Verify the value is a string type
	if (type != REG_SZ && type != REG_EXPAND_SZ)
		return ERROR_DATATYPE_MISMATCH;

	std::vector<wchar_t> buffer(data_size / sizeof(wchar_t));

	// Second call: retrieve the actual value
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
			// Some APIs may return without a trailing null; be defensive
			if (buffer.back() != L'\0')
				buffer.push_back(L'\0');
			value_out.assign(buffer.data());
		}
	}

	return result;
}

// Queries a REG_DWORD value from the registry.
// Returns ERROR_SUCCESS on success; ERROR_DATATYPE_MISMATCH if the value is not a DWORD.
LONG RegistryKey::QueryDword(const std::wstring& value_name,
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

	// Verify type and size
	if (type != REG_DWORD || data_size != sizeof(DWORD))
		return ERROR_DATATYPE_MISMATCH;

	return ERROR_SUCCESS;
}

// Sets a REG_SZ string value in the registry.
// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
LONG RegistryKey::SetString(const std::wstring& value_name,
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

// Sets a REG_DWORD value in the registry.
// Returns ERROR_SUCCESS on success; other LONG values indicate Windows API errors.
LONG RegistryKey::SetDword(const std::wstring& value_name,
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

// Deletes a single value from the registry key.
// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if the value does not exist.
LONG RegistryKey::DeleteValue(const std::wstring& value_name) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	return ::RegDeleteValueW(
		m_hKey,
		value_name.c_str());
}

// Deletes a subkey from the registry.
// The subkey must be empty (contain no child keys or values).
// Returns ERROR_SUCCESS on success; ERROR_FILE_NOT_FOUND if the subkey does not exist;
// ERROR_ACCESS_DENIED if the subkey is not empty or contains protected entries.
LONG RegistryKey::DeleteSubkey(const std::wstring& sub_key_name) const noexcept
{
	if (m_hKey == nullptr)
		return ERROR_INVALID_HANDLE;

	return ::RegDeleteKeyW(
		m_hKey,
		sub_key_name.c_str());
}
