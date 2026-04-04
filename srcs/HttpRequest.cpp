#include "../incs/Webserv.hpp"

HttpRequest::HttpRequest() : valid(false) {}

void HttpRequest::parseRequestLine(const std::string& line) {
	std::istringstream ss(line);
	if (!(ss >> method >> path >> version))
		throw std::runtime_error("400");
	if (method != "GET" && method != "POST" && method != "DELETE")
		throw std::runtime_error("405");
	if (version != "HTTP/1.1" && version != "HTTP/1.0")
		throw std::runtime_error("505");
}

void HttpRequest::parseHeaders(const std::string& raw, size_t& pos) {
	std::string line;

	while (pos < raw.size()) {
		size_t end = raw.find("\r\n", pos);
		if (end == std::string::npos)
			end = raw.find("\n", pos);
		if (end == std::string::npos)
			break;
		
		line = raw.substr(pos, end - pos);
		pos = end + (raw[end] == '\r' ? 2 : 1);

		if (line.empty())
			break;
		
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		
		std::string key = line.substr(0, colon);
		std::string val = line.substr(colon + 1);

		while (!val.empty() && val[0] == ' ')
			val.erase(0, 1);
		while (!key.empty() && key[key.size() - 1] == ' ')
			key.erase(key.size() - 1);
		
		headers[key] = val;
	}
}

void HttpRequest::parse(const std::string& raw) {
    size_t pos = 0;

    size_t end = raw.find("\r\n");
    if (end == std::string::npos)
		end = raw.find("\n");
    if (end == std::string::npos)
		throw std::runtime_error("400");

    parseRequestLine(raw.substr(0, end));
    pos = end + (raw[end] == '\r' ? 2 : 1);
    parseHeaders(raw, pos);

    if (headers.count("Content-Length")) {
        size_t len = std::atoi(headers["Content-Length"].c_str());
        if (pos + len <= raw.size())
            body = raw.substr(pos, len);
    }

    valid = true;
}
