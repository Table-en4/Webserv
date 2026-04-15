#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include "LocationConfig.hpp"

class ServerConfig {
	public:
		int							port;
		std::string					server_name;
		size_t						client_max_body;
		std::map<int, std::string>	error_pages;
		std::vector<LocationConfig>	locations;
	
		ServerConfig();
		~ServerConfig();

		void addLocation(const LocationConfig& loc);
};
