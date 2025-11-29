#include "CSV.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <map>
#include <unordered_map>
#include <unordered_set>

//Helper Functions
namespace 
{
	// Small Trim helper functions to remove leading/trailing whitespace
	inline void LTrim(std::wstring& s)
	{
		s.erase(s.begin(),
			std::find_if(s.begin(), s.end(),
				[](wchar_t ch) { return !iswspace(ch); }));
	}

	inline void RTrim(std::wstring& s)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(),
			[](wchar_t ch) { return !iswspace(ch); }).base(),
			s.end());
	}

	inline void Trim(std::wstring& s)
	{
		LTrim(s);
		RTrim(s);
	}

	// join vector<wstring> with a separator
	std::wstring Join(	const std::vector<std::wstring>& parts,
						const std::wstring& sep)
	{
		if (parts.empty())
			return L"";

		std::wstringstream ss;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0) ss << sep;
			ss << parts[i];
		}
		return ss.str();
	}

	// Parse an IPv4 string into "base" (first three octets + dot) and "host" (last octet)
	bool ParseIPLastOctet(	const std::wstring& ip,
							std::wstring& base,
							int& host)
	{
		std::wstring s = ip;
		Trim(s);
		if (s.empty())
			return false;

		auto dotPos = s.find_last_of(L'.');
		if (dotPos == std::wstring::npos)
			return false;

		base = s.substr(0, dotPos + 1);       // "192.168.1."
		std::wstring tail = s.substr(dotPos + 1);
		Trim(tail);
		if (tail.empty())
			return false;

		try
		{
			host = std::stoi(tail);
		}
		catch (...)
		{
			return false;
		}

		if (host < 0 || host > 255)
			return false;

		return true;
	}

	// Convert node list to one or more ranges.
	// Each result is either "base.start-end" or "base.host".
	// Different subnets (different base) become separate ranges.
	std::vector<std::wstring> NodesToRanges(const std::vector<std::wstring>& nodes)
	{
		std::vector<std::wstring> results;
		if (nodes.empty())
			return results;

		// Group hosts by subnet base, e.g. "192.168.1."
		struct NodeInfo { int host; };
		std::map<std::wstring, std::vector<NodeInfo>> groups;

		for (auto ip : nodes)
		{
			std::wstring base;
			int host = 0;
			if (!ParseIPLastOctet(ip, base, host))
			{
				// If it doesn't parse as IPv4, treat it as its own "range"
				Trim(ip);
				if (!ip.empty())
					results.push_back(ip);
				continue;
			}

			groups[base].push_back({ host });
		}

		// For each subnet, build contiguous ranges
		for (auto& [base, hosts] : groups)
		{
			if (hosts.empty())
				continue;

			// Sort and dedupe
			std::sort(hosts.begin(), hosts.end(),
				[](const NodeInfo& a, const NodeInfo& b) {
					return a.host < b.host;
				});
			hosts.erase(std::unique(hosts.begin(), hosts.end(),
				[](const NodeInfo& a, const NodeInfo& b) {
					return a.host == b.host;
				}),
				hosts.end());

			// Walk and form ranges
			size_t i = 0;
			while (i < hosts.size())
			{
				int start = hosts[i].host;
				int end = start;

				while (i + 1 < hosts.size() && hosts[i + 1].host == end + 1)
				{
					++i;
					end = hosts[i].host;
				}

				// Build "base.start" or "base.start-end"
				std::wstringstream ss;
				if (start == end)
				{
					ss << base << start;
				}
				else
				{
					ss << base << start << L"-" << end;
				}
				results.push_back(ss.str());

				++i;
			}
		}
		return results;
	}

	std::vector<std::wstring> SplitCSVLine(const std::wstring& line)
	{
		std::vector<std::wstring> cols;
		std::wstring current;

		for (wchar_t ch : line)
		{
			if (ch == L',')
			{
				Trim(current);
				cols.push_back(current);
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}

		Trim(current);
		cols.push_back(current);
		return cols;
	}

	// Very simple IPv4 validator: "a.b.c.d" with octets 0–255
	bool IsValidIPv4(const std::wstring& ip)
	{
		std::wstring s = ip;
		Trim(s);
		if (s.empty())
			return false;

		std::vector<std::wstring> parts;
		std::wstring cur;

		for (wchar_t ch : s)
		{
			if (ch == L'.')
			{
				if (cur.empty())
					return false;
				parts.push_back(cur);
				cur.clear();
			}
			else
			{
				cur.push_back(ch);
			}
		}

		if (cur.empty())
			return false;
		parts.push_back(cur);

		if (parts.size() != 4)
			return false;

		try
		{
			for (const auto& part : parts)
			{
				if (part.empty())
					return false;

				int n = std::stoi(part);
				if (n < 0 || n > 255)
					return false;
			}
		}
		catch (...)
		{
			return false;
		}

		return true;
	}


	// Returns true if line_item looks like: "xxx.xxx.xxx.start-end"
		// where start/end are valid integers and end >= start.
	bool HasIPRange(const std::wstring& line_item)
	{
		return line_item.find(L'-') != std::wstring::npos;
	}

	// Expand "a.b.c.start-end" into individual IP addresses.
	// On any error (bad syntax, bad numbers, end < start, etc.) return an empty vector.
	std::vector<std::wstring> ExpandIPRange(const std::wstring& line_item)
	{
		std::wstring s = line_item;
		Trim(s);
		if (s.empty())
			return {};

		// Find last '.' to separate base from last octet + range
		std::size_t lastDot = s.find_last_of(L'.');
		if (lastDot == std::wstring::npos)
			return {}; // not even a.b.c.*

		std::wstring base = s.substr(0, lastDot + 1);   // includes trailing '.'
		std::wstring tail = s.substr(lastDot + 1);      // e.g. "10-20" or "10"

		Trim(tail);
		if (tail.empty())
			return {};

		// We only handle the "start-end" form here.
		std::size_t dashPos = tail.find(L'-');
		if (dashPos == std::wstring::npos)
		{
			// No dash -> this function is only for ranges, so treat as invalid.
			return {};
		}

		std::wstring startStr = tail.substr(0, dashPos);
		std::wstring endStr = tail.substr(dashPos + 1);

		Trim(startStr);
		Trim(endStr);

		if (startStr.empty() || endStr.empty())
		{
			// Covers "192.168.1.-5" and "192.168.1.10-"
			return {};
		}

		// Make sure both start and end are all digits
		auto isAllDigits = [](const std::wstring& t) {
			if (t.empty()) return false;
			for (wchar_t ch : t)
			{
				if (!iswdigit(ch))
					return false;
			}
			return true;
			};

		if (!isAllDigits(startStr) || !isAllDigits(endStr))
			return {};

		int start = 0;
		int end = 0;

		try
		{
			start = std::stoi(startStr);
			end = std::stoi(endStr);
		}
		catch (...)
		{
			return {};
		}

		// Octet range must be 0–255 and end >= start
		if (start < 0 || start > 255 || end < 0 || end > 255 || end < start)
		{
			// This catches the 192.168.1.20-10 case
			return {};
		}

		std::vector<std::wstring> ips;
		ips.reserve(static_cast<std::size_t>(end - start + 1));

		for (int i = start; i <= end; ++i)
		{
			std::wstringstream ws;
			ws << base << i;
			ips.push_back(ws.str());
		}

		return ips;
	}
	
}	// namespace

namespace CSV 
{
	/*	-----------------------------------------------------------------
		Function: ReadDriversFromFile

		Desc: Reads ETH drivers from a CSV file into EthDriver structs

		Format: Type,Name,Range
		Example: AB_ETH,FL-IRVING, 192.168.2-90
		-----------------------------------------------------------------
	*/
	bool ReadDriversFromFile(const std::wstring& path,
		std::vector<EthDriver>& drivers_out,
		std::wstring& error_message)
	{
		drivers_out.clear();
		error_message.clear();

		// 1. Validate structure first
		if (!ValidateFormat(path, error_message))
		{
			return false;
		}

		std::wifstream file(path);
		if (!file.is_open())
		{
			error_message = L"Failed to open CSV file: " + path;
			return false;
		}

		std::wstring line;

		// Skip header
		if (!std::getline(file, line))
		{
			error_message = L"CSV file is empty after header.";
			return false;
		}

		std::size_t lineNum = 2;   // data starts at line 2

		// NEW: per-driver sets of IPs we've already seen
		std::unordered_map<std::wstring,
			std::unordered_set<std::wstring>> seen_nodes;

		while (std::getline(file, line))
		{
			std::wstring tmp = line;
			Trim(tmp);
			if (tmp.empty())
			{
				++lineNum;
				continue;
			}

			auto cols = SplitCSVLine(line);
			if (cols.size() < 3)
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": expected 3 columns (Type,Name,Range).";
				return false;
			}

			std::wstring type = cols[0];
			std::wstring name = cols[1];
			std::wstring range = cols[2];

			Trim(type);
			Trim(name);
			Trim(range);

			if (type != L"AB_ETH")
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": unsupported Type \"" + type + L"\".";
				return false;
			}

			if (name.empty())
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": Name field is empty.";
				return false;
			}

			// Find or create driver
			EthDriver* driverPtr = nullptr;

			auto it = std::find_if(
				drivers_out.begin(), drivers_out.end(),
				[&](const EthDriver& d) { return d.name == name; });

			if (it == drivers_out.end())
			{
				EthDriver d{};
				d.key_name = L"";
				d.name = name;
				d.station = 63;
				d.ping_timeout = 0;
				d.inactivity_timeout = 0;
				d.startup = 0;
				d.nodes.clear();

				drivers_out.push_back(std::move(d));
				driverPtr = &drivers_out.back();
			}
			else
			{
				driverPtr = &(*it);
			}

			// Ensure we have a set for this driver name
			auto& setRef = seen_nodes[name]; // creates empty set if not present

			// Expand range → IPs
			std::vector<std::wstring> ips;
			if (HasIPRange(range))
			{
				ips = ExpandIPRange(range);
				if (ips.empty())
				{
					error_message = L"Line " + std::to_wstring(lineNum) +
						L": range \"" + range +
						L"\" did not yield any addresses.";
					return false;
				}
			}
			else
			{
				ips.push_back(range);
			}

			// Append IPs, enforcing 254-node limit and duplicate check
			for (const auto& ip_raw : ips)
			{
				std::wstring ip = ip_raw;
				Trim(ip);

				// Duplicate check (per driver)
				if (setRef.find(ip) != setRef.end())
				{
					error_message = L"Line " + std::to_wstring(lineNum) +
						L": duplicate node IP \"" + ip +
						L"\" for driver \"" + name + L"\".";
					return false;
				}

				if (driverPtr->nodes.size() >= 254)
				{
					error_message =
						L"Driver \"" + name +
						L"\" exceeds maximum of 254 nodes. "
						L"Limit reached while processing line " +
						std::to_wstring(lineNum) + L".";
					return false;
				}

				driverPtr->nodes.push_back(ip);
				setRef.insert(ip);
			}

			++lineNum;
		}

		return true;
	}

	/*	-----------------------------------------------------------------
		Function: WriteDriversToFile

		Desc: Writes existing ETH drivers to a CSV file
		-----------------------------------------------------------------
	*/
	bool WriteDriversToFile(	const std::wstring& path,
								const std::vector<EthDriver>& drivers_in,
								std::wstring& error_message)
	{
		
		error_message.clear();

		std::wofstream file(path);
		if (!file.is_open())
		{
			error_message = L"Failed to open file for writing: " + path;
			return false;
		}

		//CSV Header
		file << L"Type,Name,Range\n";
		for (const auto& driver : drivers_in)
		{
			auto ranges = NodesToRanges(driver.nodes);

			if (ranges.empty())
			{
				file << L"AB_ETH," << driver.name << L"," << L"" << L"\n";
				continue;
			}

			for (const auto& r : ranges)
			{
				file << L"AB_ETH," << driver.name << L"," << r << L"\n";
			}
			
		}

		if (!file.good())
		{
			error_message = L"Error occurred while writing to file: " + path;
			return false;
		}

		return true;
		
	}

	/*	-----------------------------------------------------------------
		Function: ValidateFormat

		Desc: Validation to check the CSV file format is correct
		-----------------------------------------------------------------
	*/
	bool ValidateFormat(const std::wstring& path,
		std::wstring& error_message)
	{
		error_message.clear();

		std::wifstream file(path);
		if (!file.is_open())
		{
			error_message = L"Failed to open CSV file: " + path;
			return false;
		}

		std::wstring line;

		// ---- Check header ----
		if (!std::getline(file, line))
		{
			error_message = L"CSV file is empty.";
			return false;
		}

		auto headerCols = SplitCSVLine(line);
		if (headerCols.size() < 3)
		{
			error_message = L"CSV header is invalid. Expected: Type,Name,Range";
			return false;
		}

		if (headerCols[0] != L"Type" ||
			headerCols[1] != L"Name" ||
			headerCols[2] != L"Range")
		{
			error_message = L"CSV header must be: Type,Name,Range";
			return false;
		}

		// ---- Validate each data line ----
		std::size_t lineNum = 2;   // data starts at line 2

		while (std::getline(file, line))
		{
			// Allow completely blank lines
			std::wstring tmp = line;
			Trim(tmp);
			if (tmp.empty())
			{
				++lineNum;
				continue;
			}

			auto cols = SplitCSVLine(line);
			if (cols.size() < 3)
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": expected 3 columns (Type,Name,Range).";
				return false;
			}

			std::wstring type = cols[0];
			std::wstring name = cols[1];
			std::wstring range = cols[2];

			Trim(type);
			Trim(name);
			Trim(range);

			// Type must be AB_ETH (for now)
			if (type != L"AB_ETH")
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": unsupported Type \"" + type + L"\". "
					L"Expected \"AB_ETH\".";
				return false;
			}

			// Name: non-empty, <= 15 chars (RSLinx limit)
			if (name.empty())
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": Name field is empty.";
				return false;
			}

			if (name.length() > 15)
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": driver name \"" + name +
					L"\" exceeds 15-character limit.";
				return false;
			}

			// Range must be either a single IP or an IP range
			if (range.empty())
			{
				error_message = L"Line " + std::to_wstring(lineNum) +
					L": Range field is empty.";
				return false;
			}

			if (HasIPRange(range))
			{
				// Expand and validate each IP
				auto ips = ExpandIPRange(range);
				if (ips.empty())
				{
					error_message = L"Line " + std::to_wstring(lineNum) +
						L": IP range \"" + range +
						L"\" did not produce any addresses.";
					return false;
				}

				for (const auto& ip : ips)
				{
					if (!IsValidIPv4(ip))
					{
						error_message = L"Line " + std::to_wstring(lineNum) +
							L": \"" + ip +
							L"\" is not a valid IPv4 address.";
						return false;
					}
				}
			}
			else
			{
				// Treat as a single IP
				if (!IsValidIPv4(range))
				{
					error_message = L"Line " + std::to_wstring(lineNum) +
						L": Range \"" + range +
						L"\" is neither a valid IPv4 address "
						L"nor a valid range.";
					return false;
				}
			}

			++lineNum;
		}

		return true;
	}
}	// namespace CSV
