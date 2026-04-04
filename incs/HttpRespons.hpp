#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

class HttpResponse {
	private:
		std::string buildError(int code, const ServerConfig& config);
		std::string serveFile(const std::string& filepath);
		std::string buildHeaders(int code, const std::string& content_type, size_t body_len);

		std::string resolveFilePath(const HttpRequest& req, const ServerConfig& config);
		std::string getStatusMessage(int code);
		std::string getMimeType(const std::string& path);
		bool		fileExists(const std::string& path);

	public:
		std::string build(const HttpRequest& req, const ServerConfig& config);
};
