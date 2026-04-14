#pragma once
#include <string>
#include <vector>

class LocationConfig {
	public:
		std::string					path;		// "/assets"
		std::string					root;		// "/var/www/assets"
		std::string					index;		// "index.html"
		bool						autoindex;
		std::vector<std::string>	methods; 	// GET, POST, DELETE etc...
		int							redirect_code;	// 301, 302, 0 = no redirect
		std::string					redirect_url;	// destination URL
		std::string					upload_store;	// directory for uploaded files

		LocationConfig();
		~LocationConfig();
};
