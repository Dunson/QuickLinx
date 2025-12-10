#include "ImportEngine.h"

#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <cwctype>

// Private helper functions for ImportEngine
namespace 
{
	constexpr std::size_t MAX_NODES_PER_DRIVER = 254;		// 0..255 except 63 reserved

	// Extracts the index from an AB_ETH-x key name.
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

	// Finds the maximum index among AB_ETH-x entries in the registry drivers.
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

	// Prevent duplicate nodes by collecting all nodes into a std::set
	std::set<std::wstring> collect_nodes(const std::vector<std::wstring>& driver_nodes)
	{
		std::set<std::wstring> node_set;
		for (const auto& d : driver_nodes) 
		{
			node_set.insert(d);
		}
		return node_set;
	}

	//For creating new Ethernet Driver struct
	EthDriver create_driver(	const std::wstring& key_name,
								const std::wstring& display_name)
	{
		EthDriver driver;
		
		driver.key_name = key_name;
		driver.name = display_name;

		// Default Values
		driver.station =				63;
		driver.ping_timeout =			6;
		driver.inactivity_timeout =		30;
		driver.startup =				0;

		return driver;
	}
	
}	// anonymous namespace


namespace ImportEngine
{
	
	ImportResult merge_drivers(		const std::vector<EthDriver>& registry_drivers,
									const std::vector<EthDriver>& csv_drivers)
	{
		ImportResult result;

		int max_AB_ETH_index = find_max_index(registry_drivers);

		// No drivers in CSV to import
		if (csv_drivers.empty() && !registry_drivers.empty())
		{
			result.errors.push_back(L"No drivers found in CSV import.");
			result.success = false;
			return result;
		}

		// Map existing registry drivers by key_name for quick lookup
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
				// Existing driver
				EthDriver& reg_driver = found_driver->second;
				// Merge nodes, avoiding duplicates
				std::set<std::wstring> node_set = collect_nodes(reg_driver.nodes);
				for (const auto& node : csv_driver.nodes)
				{
					if (node_set.size() >= MAX_NODES_PER_DRIVER)
					{
						// Reached max nodes (CSV parser prevents adding more than 254 nodes. This condition is just a safeguard)
						result.errors.push_back(
							L"Driver '" + reg_driver.name + L"' (" + reg_driver.key_name +
							L") has reached maximum node limit. Extra nodes were skipped.");
						break;
					}
					node_set.insert(node);
				}
				// Update the driver's nodes from the set
				reg_driver.nodes.assign(node_set.begin(), node_set.end());
				result.updated_drivers.push_back(reg_driver);
			}
			else
			{
				// New driver - assign new key_name if necessary
				EthDriver new_driver = csv_driver;
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

	ImportResult overwrite_drivers(	const std::vector<EthDriver>& registry_drivers, 
									const std::vector<EthDriver>& csv_drivers)
	{
		ImportResult result;

		int max_AB_ETH_index = find_max_index(registry_drivers);

		// No drivers in CSV to import - (safeguard though this should be handled before calling this function)
		if (csv_drivers.empty() && !registry_drivers.empty())
		{
			result.errors.push_back(L"Overwrite failed. No drivers found in CSV import.");
			result.success = false;
			return result;
		}

		// Compare CSV drivers against registry drivers
		// If match is found (by name), update registry driver nodes
		// If no match, create new driver entry
		for (const auto& csv_driver : csv_drivers)
		{
			auto found_driver = std::find_if(registry_drivers.begin(), registry_drivers.end(),
				[&csv_driver](const EthDriver& reg_driver)
				{
					return reg_driver.name == csv_driver.name;
				});
			if (found_driver != registry_drivers.end())
			{
				// Existing driver found - overwrite nodes
				EthDriver updated_driver = *found_driver;
				updated_driver.nodes = csv_driver.nodes;
				result.updated_drivers.push_back(updated_driver);
			}
			else
			{
				// New driver - assign new key_name if necessary
				EthDriver new_driver = csv_driver;
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

}