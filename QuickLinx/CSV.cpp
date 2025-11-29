#include "CSV.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>
#include <map>
#include <unordered_map>
#include <unordered_set>

//Helper Functions in anonymous namespace
namespace 
{
	// Small Trim helper functions to remove leading/trailing whitespace
	inline void trim_left(std::wstring& s)
	{
		s.erase(s.begin(),
			std::find_if(s.begin(), s.end(),
				[](wchar_t ch) { return !iswspace(ch); }));
	}

	inline void trim_right(std::wstring& s)
	{
		s.erase(std::find_if(s.rbegin(), s.rend(),
			[](wchar_t ch) { return !iswspace(ch); }).base(),
			s.end());
	}

	inline void trim(std::wstring& s)
	{
		trim_left(s);
		trim_right(s);
	}

	// join vector<wstring> with a separator
	// e.g. join({"a","b","c"}, ",") -> "a,b,c"
	std::wstring join(	const std::vector<std::wstring>& parts,
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
	// e.g. "a.b.c.d" -> base="a.b.c.", host=d
	bool parse_ip_last_octet(	const std::wstring& ip,
								std::wstring& base,
								int& host)
	{
		std::wstring s = ip;
		trim(s);
		if (s.empty())
			return false;

		auto dot_pos = s.find_last_of(L'.');
		if (dot_pos == std::wstring::npos)
			return false;

		base = s.substr(0, dot_pos + 1);       // "a.b.c."
		std::wstring tail = s.substr(dot_pos + 1);
		trim(tail);
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
	std::vector<std::wstring> nodes_to_ranges(const std::vector<std::wstring>& nodes)
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
			if (!parse_ip_last_octet(ip, base, host))
			{
				// If it doesn't parse as IPv4, treat it as its own "range"
				trim(ip);
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
	
	// Split a CSV line into columns (simple, no quotes handling)
	// e.g. "a,b,c" -> {"a","b","c"}
	std::vector<std::wstring> split_csv_line(const std::wstring& line)
	{
		std::vector<std::wstring> cols;
		std::wstring current;

		for (wchar_t ch : line)
		{
			if (ch == L',')
			{
				trim(current);
				cols.push_back(current);
				current.clear();
			}
			else
			{
				current.push_back(ch);
			}
		}

		trim(current);
		cols.push_back(current);
		return cols;
	}

	// Very simple IPv4 validator: "a.b.c.d" with octets 0–255
	bool is_valid_ipv4(const std::wstring& ip_addr)
	{
		std::wstring ip_str = ip_addr;
		trim(ip_str);
		
		// Empty string check
		if (ip_str.empty())
			return false;	// Empty string is not valid

		std::vector<std::wstring> ip_octets;
		std::wstring curr_octet;

		// Split by '.' and collect octets
		for (wchar_t ch : ip_str)
		{
			if (ch == L'.')
			{
				if (curr_octet.empty())
					return false;
				ip_octets.push_back(curr_octet);
				curr_octet.clear();
			}
			else
			{
				curr_octet.push_back(ch);
			}
		}

		if (curr_octet.empty())
			return false;

		ip_octets.push_back(curr_octet);

		if (ip_octets.size() != 4)
			return false;

		try
		{
			for (const auto& part : ip_octets)
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


	// Returns true if line_item looks like: "a.b.c.start-end"
	bool has_ip_range(const std::wstring& line_item)
	{
		return line_item.find(L'-') != std::wstring::npos;
	}

	// Expand "a.b.c.start-end" into individual IP addresses.
	// On any error (bad syntax, bad numbers, end < start, etc.) return an empty vector.
	std::vector<std::wstring> expand_ip_range(const std::wstring& line_item)
	{
		std::wstring s = line_item;
		trim(s);
		if (s.empty())
			return {};

		// Find last '.' to separate base from last octet + range
		std::size_t last_dot = s.find_last_of(L'.');
		if (last_dot == std::wstring::npos)
			return {}; // not a valid IP format

		std::wstring base = s.substr(0, last_dot + 1);   // includes trailing '.'
		std::wstring tail = s.substr(last_dot + 1);      // e.g. "10-20" or "10"

		trim(tail);
		if (tail.empty())
			return {}; // no last octet

		// We only handle the "start-end" from here on 
		std::size_t dash_pos = tail.find(L'-');
		if (dash_pos == std::wstring::npos)
			return {}; // No dash -> not a range

		std::wstring range_start = tail.substr(0, dash_pos);
		std::wstring range_end = tail.substr(dash_pos + 1);

		trim(range_start);
		trim(range_end);

		if (range_start.empty() || range_end.empty())
			return {}; // Missing start or end range. ex: "192.168.1.-5" and "192.168.1.10-"

		// Make sure both start and end are all digits
		auto is_all_digits = [](const std::wstring& t) {
			if (t.empty()) return false;
			for (wchar_t ch : t)
			{
				if (!iswdigit(ch))
					return false;
			}
			return true;
			};

		if (!is_all_digits(range_start) || !is_all_digits(range_end))
			return {};	// Non-digit characters in range

		int start = 0;
		int end = 0;

		try
		{
			start = std::stoi(range_start);
			end = std::stoi(range_end);
		}
		catch (...)
		{
			return {};
		}

		// Octet range must be 0–255 and end >= start
		if (start < 0 || start > 255 || end < 0 || end > 255 || end < start)
			return{}; // This catches the a.b.c.d-e case where d > e and where d or e > 255

		// Finally, build the list of IPs
		std::vector<std::wstring> ips;
		ips.reserve(static_cast<std::size_t>(end - start + 1));	// Using reserve for memory allocation efficiency

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
		Function: read_drivers_from_file

		Desc: Reads ETH drivers from a CSV file into EthDriver structs

		Format: Type,Name,Range
		Example: AB_ETH,FL-IRVING, 192.168.2-90
		-----------------------------------------------------------------
	*/
	bool read_drivers_from_file(	const std::wstring& path,
									std::vector<EthDriver>& drivers_out,
									std::wstring& error_message)
	{
		drivers_out.clear();
		error_message.clear();

		// 1. Validate structure first
		if (!validate_csv_format(path, error_message))
			return false;

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

		std::size_t line_num = 2;   // data starts at line 2

		// Per-driver sets of IPs we've already seen. Used for duplicate detection.
		std::unordered_map<std::wstring, std::unordered_set<std::wstring>> seen_nodes;

		while (std::getline(file, line))
		{
			std::wstring tmp = line;
			trim(tmp);
			if (tmp.empty())
			{
				++line_num;
				continue;
			}

			auto cols = split_csv_line(line);
			if (cols.size() < 3)
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": expected 3 columns (Type,Name,Range).";
				return false;
			}

			std::wstring type = cols[0];
			std::wstring name = cols[1];
			std::wstring range = cols[2];

			trim(type);
			trim(name);
			trim(range);

			if (type != L"AB_ETH")
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": unsupported Type \"" + type + L"\".";
				return false;
			}

			if (name.empty())
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": Name field is empty.";
				return false;
			}

			// Find or create driver
			EthDriver* driver_ptr = nullptr;

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
				driver_ptr = &drivers_out.back();
			}
			else
			{
				driver_ptr = &(*it);
			}

			// Ensure we have a set for this driver name
			auto& set_ref = seen_nodes[name]; // creates empty set if not present

			// Expand range -> IPs
			std::vector<std::wstring> ips;
			if (has_ip_range(range))
			{
				ips = expand_ip_range(range);
				if (ips.empty())
				{
					error_message = L"Line " + std::to_wstring(line_num) +
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
				trim(ip);

				// Duplicate check (per driver)
				if (set_ref.find(ip) != set_ref.end())
				{
					error_message = L"Line " + std::to_wstring(line_num) +
									L": duplicate node IP \"" + ip +
									L"\" for driver \"" + name + L"\".";
					return false;
				}

				if (driver_ptr->nodes.size() >= 254)
				{
					error_message =
						L"Driver \"" + name +
						L"\" exceeds maximum of 254 nodes. "
						L"Limit reached while processing line " +
						std::to_wstring(line_num) + L".";
					return false;
				}

				driver_ptr->nodes.push_back(ip);
				set_ref.insert(ip);
			}

			++line_num;
		}

		return true;
	}

	/*	-----------------------------------------------------------------
		Function: write_drivers_to_file

		Desc: Writes existing ETH drivers to a CSV file
		-----------------------------------------------------------------
	*/
	bool write_drivers_to_file(		const std::wstring& path,
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
			auto ranges = nodes_to_ranges(driver.nodes);

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
		Function: validate_csv_format

		Desc: Validation to check the CSV file format is correct
		-----------------------------------------------------------------
	*/
	bool validate_csv_format(		const std::wstring& path,
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

		auto header_cols = split_csv_line(line);
		if (header_cols.size() < 3)
		{
			error_message = L"CSV header is invalid. Expected: Type,Name,Range";
			return false;
		}

		if (header_cols[0] != L"Type" ||
			header_cols[1] != L"Name" ||
			header_cols[2] != L"Range")
		{
			error_message = L"CSV header must be: Type,Name,Range";
			return false;
		}

		// ---- Validate each data line ----
		std::size_t line_num = 2;   // data starts at line 2

		while (std::getline(file, line))
		{
			// Allow completely blank lines
			std::wstring tmp = line;
			trim(tmp);
			if (tmp.empty())
			{
				++line_num;
				continue;
			}

			auto cols = split_csv_line(line);
			if (cols.size() < 3)
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": expected 3 columns (Type,Name,Range).";
				return false;
			}

			std::wstring type = cols[0];
			std::wstring name = cols[1];
			std::wstring range = cols[2];

			trim(type);
			trim(name);
			trim(range);

			// Type must be AB_ETH (for now)
			if (type != L"AB_ETH")
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": unsupported Type \"" + type + L"\". "
								L"Expected \"AB_ETH\".";
				return false;
			}

			// Name: non-empty, <= 15 chars (RSLinx limit)
			if (name.empty())
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": Name field is empty.";
				return false;
			}

			if (name.length() > 15)
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": driver name \"" + name +
								L"\" exceeds 15-character limit.";
				return false;
			}

			// Range must be either a single IP or an IP range
			if (range.empty())
			{
				error_message = L"Line " + std::to_wstring(line_num) +
								L": Range field is empty.";
				return false;
			}

			if (has_ip_range(range))
			{
				// Expand and validate each IP
				auto ips = expand_ip_range(range);
				if (ips.empty())
				{
					error_message = L"Line " + std::to_wstring(line_num) +
									L": IP range \"" + range +
									L"\" did not produce any addresses.";
					return false;
				}

				for (const auto& ip : ips)
				{
					if (!is_valid_ipv4(ip))
					{
						error_message = L"Line " + std::to_wstring(line_num) +
										L": \"" + ip +
										L"\" is not a valid IPv4 address.";
						return false;
					}
				}
			}
			else
			{
				// Treat as a single IP
				if (!is_valid_ipv4(range))
				{
					error_message = L"Line " + std::to_wstring(line_num) +
									L": Range \"" + range +
									L"\" is neither a valid IPv4 address "
									L"nor a valid range.";
					return false;
				}
			}

			++line_num;
		}

		return true;
	}
}	// namespace CSV
