#include "../incs/Webserv.hpp"

HttpResponse::HttpResponse() : _matched_location(NULL) {}

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

bool HttpResponse::isDirectory(const std::string& path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

bool HttpResponse::fileExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

std::string HttpResponse::generateAutoindex(const std::string& dir_path, const std::string& url_path) {
  DIR* dir = opendir(dir_path.c_str());
  if (!dir)
    throw std::runtime_error("403");
  
  std::ostringstream html;
  html << "<html><head><title>Index of " << url_path << "</title></head><body>"
       << "<h1>Index of " << url_path << "</h1><hr><pre>";
  
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string name = entry->d_name;
    if (name == ".")
      continue;
    std::string full_path = dir_path + "/" + name;
    struct stat st;
    std::string display = name;

    if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
      display += "/";
    
    html << "a herf=\"" << url_path;
    if (url_path.empty() || url_path[url_path.size() - 1] != '/')
      html << "/";
    html << name << "\"" << display << "</a>\n";
  }
  closedir(dir);

  html << "</pre><hr></body></html>";
  return html.str();
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
        if (best->methods[i] == req.method) { method_ok = true; break; }
    }
    if (!method_ok)
        throw std::runtime_error("405");

    _matched_location = best;

    std::string root = best->root;
    std::string suffix = req.path.substr(best_len);
    if (suffix.empty() || suffix == "/")
        suffix = "/";

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

/*
peut-etre créer une classe ou des fichiers à part pour gérer ca je sais pas
faudra que tu me donnes ton avis pour la lisibilité du code
*/

std::string HttpResponse::handleDelete(const std::string& path, const ServerConfig& config) {
  if (!fileExists(path))
    return buildError(404, config);
  
  if (std::remove(path.c_str()) == 0) {
    std::string body = "<html><body><h1>200 Ok: File deleted successfully.</h1></body></html>";
    return buildHeaders(200, "text/html", body.size()) + body;
  }
  else
    return buildError(403, config);
}

std::string HttpResponse::handlePost(const HttpRequest& req, const std::string& path, const ServerConfig& config) {
  if (isDirectory(path))
    return buildError(403, config);
  
  std::ofstream outfile(path.c_str(), std::ios::out | std::ios::binary);
  if (!outfile.is_open())
    return buildError(403, config);
  
  outfile.write(req.body.c_str(), req.body.size());
  outfile.close();

  std::string body = "<html><body><h1>201 Created: File saved successfully.</h1></body></html>";
  return buildHeaders(201, "text/html", body.size()) + body;
}

std::string HttpResponse::handleDirectory(const std::string& path, const HttpRequest& req, const ServerConfig& config) {
  std::string index_file = path;
  if (index_file.back() != '/')
    index_file += "/";

  if (_matched_location && !_matched_location->index.empty())
    index_file += _matched_location->index;
  else
    index_file += "index.html";

  if (fileExists(index_file)) {
    std::string body = serveFile(index_file);
    return buildHeaders(200, getMimeType(index_file), body.size()) + body;
  }

  if (_matched_location && _matched_location->autoindex) {
    std::string body = generateAutoindex(path, req.path);
    return buildHeaders(200, "text/html", body.size()) + body;
  }

  return buildError(403, config);
}

std::string HttpResponse::handleFile(const std::string& path, const ServerConfig& config) {
  if (!fileExists(path))
    return buildError(404, config);

  std::string body = serveFile(path);
  if (body.empty())
    return buildError(403, config);

  return buildHeaders(200, getMimeType(path), body.size()) + body;
}

std::string HttpResponse::handleGet(HttpRequest& req, ServerConfig& config)
{

  for (size_t i = 0; i < config._routes_collections._routes.size(); i++)
	{
		// it check if the path correspond to any of the routes
		if (config._routes_collections._routes[i].path == req.path)
		{
			std::string body = config._routes_collections._routes[i].func(config._routes_collections._routes[i]); // if yes we simply call the func

			// jsp pq faut faire cet merde pr avoir la longueur du body mais bon
			std::stringstream ss;
			ss << body.length();
			std::string htmlPageContentLengthStr = ss.str();

      config._routes_collections._routes[i].req = &req;
      return buildHeaders(200, "text/html", body.size()) + body;
		}
	}
  return buildError(404, config);
}

std::string HttpResponse::build(const HttpRequest& req, const ServerConfig& config) {
    try {
        std::string path = resolveFilePath(req, config);
        if (path.empty())
            return buildError(404, config);

        if (req.method == "DELETE")
            return handleDelete(path, config);
        if (req.method == "POST")
            return handlePost(req, path, config);
        if (req.method == "GET" && req.path.rfind("/cgi", 0) != 0 )
        {
          std::cout << "GET HANDLE " << std::endl;
          HttpRequest tmp_req = req;
          ServerConfig tmp_config = config;
          return handleGet(tmp_req, tmp_config);
        }
        if (isDirectory(path))
            return handleDirectory(path, req, config);

        return handleFile(path, config);
    }
    catch (const std::runtime_error& e) {
        int code = std::atoi(e.what());
        if (code == 0) code = 500;
        return buildError(code, config);
    }
}
