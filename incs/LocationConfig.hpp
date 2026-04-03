#pragma once
#include <string>
#include <vector>

class LocationConfig {
	public:
		std::string					path;		// "/assets"
		std::string					root;		// "/var/www/assets"
		std::string					index;		// "index.html"
		bool						autoindex;
		std::vector<std::string>	methods; 	// GET, POST, DELET ect...

		LocationConfig();
		~LocationConfig();
};
