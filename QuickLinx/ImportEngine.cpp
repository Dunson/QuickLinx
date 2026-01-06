#include "ImportEngine.h"

#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <sstream>

// Private Helper Functions (Anonymous Namespace)
namespace
{
	// Maximum number of nodes per driver (0-255 except 63 reserved by RSLinx)
	constexpr std::size_t MAX_NODES_PER_DRIVER = 254;

	// Extracts the numeric index from an AB_ETH-x key name.
	// Example: "AB_ETH-5" returns 5; "AB_ETH-abc" returns -1.
	// Returns -1 if the format is invalid or conversion fails.
	int extract_index(const std::wstring& key_name)
	{
		const std::wstring prefix = L"AB_ETH-";

		if (key_name.find(prefix, 0) != 0)
			return -1; // Not a valid AB_ETH entry

		try
		{
			return std::stoi(key_name.substr(prefix.size()));
		}
		catch (...)
		{
			return -1; // Conversion failed
		}
	}

	// Finds the maximum numeric index among all AB_ETH-x entries in registry drivers.
	// Returns 0 if no valid AB_ETH entries are found.
	int find_max_index(const std::vector<EthDriver>& registry_drivers)
	{
		int max_index = 0;
		for (const auto& d : registry_drivers)
		{
			int idx = extract_index(d.key_name);
			if (idx > max_index)
				max_index = idx;
		}
		return max_index;
	}

	// Collects node IPs into a sorted set to prevent duplicates and enable range operations.
	std::set<std::wstring> collect_nodes(const std::vector<std::wstring>& driver_nodes)
	{
		std::set<std::wstring> node_set;
		for (const auto& node : driver_nodes)
		{
			node_set.insert(node);
		}
		return node_set;
	}

	// Creates a new EthDriver with default RSLinx configuration values.
	// Parameters:
	//   key_name: Registry key identifier (e.g., "AB_ETH-1")
	//   display_name: User-facing driver name
	EthDriver create_driver(const std::wstring& key_name,
		const std::wstring& display_name)
	{
		EthDriver driver;

		driver.key_name = key_name;
		driver.name = display_name;

		// RSLinx default configuration values
		driver.station = 63;
		driver.ping_timeout = 6;
		driver.inactivity_timeout = 30;
		driver.startup = 0;

		return driver;
	}

} // namespace

namespace ImportEngine
{
	// Merges CSV-imported drivers with existing registry drivers.
	// Matching drivers (by name) have their node lists combined and deduplicated.
	// Non-matching CSV drivers are added as new drivers with generated key names.
	//
	// Returns an ImportResult containing:
	//   - updated_drivers: existing drivers with merged node lists
	//   - new_drivers: drivers created from CSV entries without existing matches
	//   - errors: any warnings or non-critical issues encountered
	//   - success: false if no drivers were imported; true otherwise
	ImportResult merge_drivers(const std::vector<EthDriver>& registry_drivers,
		const std::vector<EthDriver>& csv_drivers)
	{
		ImportResult result;

		int max_AB_ETH_index = find_max_index(registry_drivers);

		// Safeguard: no CSV drivers to process
		if (csv_drivers.empty() && !registry_drivers.empty())
		{
			result.errors.push_back(L"No drivers found in CSV import.");
			result.success = false;
			return result;
		}

		// Build a map of existing registry drivers indexed by name for O(log n) lookup
		std::map<std::wstring, EthDriver> registry_map;
		for (const auto& reg_driver : registry_drivers)
		{
			registry_map[reg_driver.name] = reg_driver;
		}

		// Process each CSV driver
		for (const auto& csv_driver : csv_drivers)
		{
			auto found_driver = registry_map.find(csv_driver.name);

			if (found_driver != registry_map.end())
			{
				// Matching driver found: merge node lists
				EthDriver& reg_driver = found_driver->second;

				// Collect existing nodes into a set to prevent duplicates
				std::set<std::wstring> node_set = collect_nodes(reg_driver.nodes);

				// Attempt to add CSV nodes
				for (const auto& node : csv_driver.nodes)
				{
					if (node_set.size() >= MAX_NODES_PER_DRIVER)
					{
						// Max node limit reached (CSV parser also enforces this as safeguard)
						result.errors.push_back(
							L"Driver '" + reg_driver.name + L"' (" + reg_driver.key_name +
							L") has reached maximum node limit. Extra nodes were skipped.");
						break;
					}
					node_set.insert(node);
				}

				// Update driver nodes from the deduplicated set
				reg_driver.nodes.assign(node_set.begin(), node_set.end());
				result.updated_drivers.push_back(reg_driver);
			}
			else
			{
				// No match found: create new driver
				EthDriver new_driver = csv_driver;

				// Generate new key name if the imported one is invalid
				if (extract_index(new_driver.key_name) == -1)
				{
					max_AB_ETH_index++;
					new_driver.key_name = L"AB_ETH-" + std::to_wstring(max_AB_ETH_index);
				}

				result.new_drivers.push_back(new_driver);
			}
		}

		return result;
	}

	// Overwrites existing registry drivers with CSV-imported drivers.
	// Matching drivers (by name) have their node lists completely replaced.
	// Non-matching CSV drivers are added as new drivers with generated key names.
	//
	// Returns an ImportResult containing:
	//   - updated_drivers: existing drivers with overwritten node lists
	//   - new_drivers: drivers created from CSV entries without existing matches
	//   - errors: any warnings or non-critical issues encountered
	//   - success: false if no drivers were imported; true otherwise
	ImportResult overwrite_drivers(const std::vector<EthDriver>& registry_drivers,
		const std::vector<EthDriver>& csv_drivers)
	{
		ImportResult result;

		int max_AB_ETH_index = find_max_index(registry_drivers);

		// Safeguard: no CSV drivers to process
		if (csv_drivers.empty() && !registry_drivers.empty())
		{
			result.errors.push_back(L"Overwrite failed. No drivers found in CSV import.");
			result.success = false;
			return result;
		}

		// Process each CSV driver
		for (const auto& csv_driver : csv_drivers)
		{
			// Search for existing driver with matching name
			auto found_driver = std::find_if(
				registry_drivers.begin(), registry_drivers.end(),
				[&csv_driver](const EthDriver& reg_driver)
				{
					return reg_driver.name == csv_driver.name;
				});

			if (found_driver != registry_drivers.end())
			{
				// Matching driver found: completely replace node list
				EthDriver updated_driver = *found_driver;
				updated_driver.nodes = csv_driver.nodes;
				result.updated_drivers.push_back(updated_driver);
			}
			else
			{
				// No match found: create new driver
				EthDriver new_driver = csv_driver;

				// Generate new key name if the imported one is invalid
				if (extract_index(new_driver.key_name) == -1)
				{
					max_AB_ETH_index++;
					new_driver.key_name = L"AB_ETH-" + std::to_wstring(max_AB_ETH_index);
				}

				result.new_drivers.push_back(new_driver);
			}
		}

		return result;
	}

} // namespace ImportEngine