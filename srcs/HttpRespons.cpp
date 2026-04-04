#include "../incs/Webserv.hpp"

std::string HttpResponse::getStatusMessage(int code) {
    if (code == 200)
		return "Ok";
    if (code == 400)
		return "Bad Request";
    if (code == 403)
		return "Forbidden";
    if (code == 404)
		return "Not Found";
    if (code == 405)
		return "Method Not Allowed";
    if (code == 505)
		return "HTTP Version Not Supported";
    return "Internal Server Error";
}

std::string HttpResponse::getMimeType(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos)
		return "application/octet-stream";

    std::string ext = path.substr(dot);

    if (ext == ".html" || ext == ".htm")
		return "text/html";
    if (ext == ".css")
		return "text/css";
    if (ext == ".js")
		return "application/javascript";
    if (ext == ".png")
		return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
    if (ext == ".txt")
		return "text/plain";
    return "application/octet-stream";
}

bool HttpResponse::fileExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

std::string HttpResponse::buildHeaders(int code, const std::string& contentType, size_t bodyLen) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << getStatusMessage(code) << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << bodyLen << "\r\n"
        << "Connection: close\r\n"
        << "\r\n";
    return oss.str();
}

std::string HttpResponse::serveFile(const std::string& filepath) {
    std::ifstream file(filepath.c_str(), std::ios::binary);

    if (!file.is_open())
		return "";

    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string HttpResponse::resolveFilePath(const HttpRequest& req, const ServerConfig& config) {
    const LocationConfig* best = NULL;
    size_t best_len = 0;

    for (size_t i = 0; i < config.locations.size(); i++) {
        const std::string& loc_path = config.locations[i].path;

        if (req.path.substr(0, loc_path.size()) == loc_path) {
            if (loc_path.size() > best_len) {
                best_len = loc_path.size();
                best = &config.locations[i];
            }
        }
    }

    if (!best) return "";

    bool method_ok = false;
    for (size_t i = 0; i < best->methods.size(); i++) {
        if (best->methods[i] == req.method) {
			method_ok = true;
			break;
		}
    }
    if (!method_ok)
		throw std::runtime_error("405");

    //c'est ici qu'on construit le path
    std::string root = best->root;
    std::string suffix = req.path.substr(best_len);
    if (suffix.empty() || suffix == "/")
        suffix = "/" + best->index;

    return root + suffix;
}

std::string HttpResponse::buildError(int code, const ServerConfig& config) {
    std::map<int, std::string>::const_iterator it = config.error_pages.find(code);
    std::string body;
    if (it != config.error_pages.end() && fileExists(it->second))
        body = serveFile(it->second);
    else {
        std::ostringstream oss;
        oss << "<html><body><h1>" << code << " " << getStatusMessage(code) << "</h1></body></html>";
        body = oss.str();
    }
    return buildHeaders(code, "text/html", body.size()) + body;
}

std::string HttpResponse::build(const HttpRequest& req, const ServerConfig& config) {
    try {
        std::string filepath = resolveFilePath(req, config);
        if (filepath.empty() || !fileExists(filepath))
            return buildError(404, config);

        std::string body = serveFile(filepath);
        if (body.empty())
            return buildError(403, config);

        return buildHeaders(200, getMimeType(filepath), body.size()) + body;
    }
    catch (const std::runtime_error& e) {
        int code = std::atoi(e.what());
        if (code == 0) code = 500;
        return buildError(code, config);
    }
}
