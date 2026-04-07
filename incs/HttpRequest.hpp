#pragma once

#include <string>
#include <map>
#include <sstream>
#include <stdexcept>

class HttpRequest {
	private:
		void parseRequestLine(const std::string& line);
		void parseHeaders(const std::string& raw, size_t& pos);

	public:
		std::string							method;
		std::string							path;
		std::string							version;
		std::string							body;
		std::map<std::string, std::string>	headers;
		bool								valid;

		HttpRequest();
		void parse(const std::string& raw);
};
