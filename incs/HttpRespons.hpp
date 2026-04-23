#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <cstdio>
#include <limits.h>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

class HttpResponse {
	private:
		const LocationConfig* _matched_location;

		std::string serveFile(const std::string& filepath);
		std::string getMimeType(const std::string& path);
		std::string generateAutoindex(const std::string& dir_path, const std::string& utl_path);
		std::string handleDelete(const std::string& path, const ServerConfig& config);
		std::string handlePost(const HttpRequest& req, const std::string& path, const ServerConfig& config);
		std::string handleGet(HttpRequest& req, ServerConfig& config);
		std::string handleDirectory(const std::string& path, const HttpRequest& req, const ServerConfig& config);
		std::string handleFile(const std::string& path, const ServerConfig& config);

	public:
		HttpResponse();

		std::string build(const HttpRequest& req, const ServerConfig& config);
		std::string buildHeaders(int code, const std::string& content_type, size_t body_len);
		std::string buildError(int code, const ServerConfig& config);
		std::string resolveFilePath(const HttpRequest& req, const ServerConfig& config);
		std::string getStatusMessage(int code);
		bool		fileExists(const std::string& path);
		bool		isDirectory(const std::string& path);
		bool		isRealPath(const std::string& path);

		const LocationConfig* getMatchedLocation() const;
};
