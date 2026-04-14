#include "../incs/Webserv.hpp"
#include <sys/stat.h>

HttpResponse::HttpResponse() : _matched_location(NULL) {}

std::string HttpResponse::getStatusMessage(int code) {
    if (code == 200)
      return "Ok";
    if (code == 201)
      return "Created";
    if (code == 301)
      return "Moved Permanently";
    if (code == 302)
      return "Found";
    if (code == 400)
      return "Bad Request";
    if (code == 403)
      return "Forbidden";
    if (code == 404)
      return "Not Found";
    if (code == 405)
      return "Method Not Allowed";
    if (code == 413)
      return "Payload Too Large";
    if (code == 500)
      return "Internal Server Error";
    if (code == 504)
      return "Gateway Timeout";
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
    
    html << "<a href=\"" << url_path;
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

// a move dans un file utils TODO:
std::string getCurrentWorkingDir() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != NULL) {
        return std::string(buffer);
    }
    return "";
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

    if (!root.empty() && root[0] != '/') {
      char buffer[4096];
      if (getcwd(buffer, sizeof(buffer)))
        root = std::string(buffer) + "/" + root;
    }

    if (suffix.empty() || suffix == "/")
        suffix = "/";
    else if (root[root.length() - 1] != '/' && suffix[0] != '/')
        root += "/";

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

// Extrait le nom de fichier depuis Content-Disposition: form-data; name="file"; filename="foo.txt"
static std::string extractFilename(const std::string& disposition) {
    size_t pos = disposition.find("filename=\"");
    if (pos == std::string::npos)
        return "";
    pos += 10;
    size_t end = disposition.find("\"", pos);
    if (end == std::string::npos)
        return "";
    return disposition.substr(pos, end - pos);
}

std::string HttpResponse::handlePost(const HttpRequest& req, const std::string& path, const ServerConfig& config) {
    std::string content_type = "";
    if (req.headers.count("Content-Type"))
        content_type = req.headers.at("Content-Type");

    size_t mp_pos = content_type.find("multipart/form-data");
    if (mp_pos != std::string::npos) {
        size_t b_pos = content_type.find("boundary=");
        if (b_pos == std::string::npos)
            return buildError(400, config);
        std::string boundary = "--" + content_type.substr(b_pos + 9);
        while (!boundary.empty() && (boundary[boundary.size()-1] == '\r' || boundary[boundary.size()-1] == ' '))
            boundary.erase(boundary.size()-1);

        std::string store_dir = "";
        if (_matched_location && !_matched_location->upload_store.empty())
            store_dir = _matched_location->upload_store;
        else if (_matched_location && !_matched_location->root.empty())
            store_dir = _matched_location->root;
        else
            store_dir = path;

        if (store_dir.empty() || isDirectory(store_dir) == false) {
            mkdir(store_dir.c_str(), 0755);
        }
        if (store_dir[store_dir.size()-1] != '/')
            store_dir += "/";

        const std::string& body = req.body;
        size_t pos_body = 0;
        bool any_saved = false;

        while (true) {
            size_t bstart = body.find(boundary, pos_body);
            if (bstart == std::string::npos)
                break;
            pos_body = bstart + boundary.size();

            if (pos_body + 2 <= body.size() && body.substr(pos_body, 2) == "--")
                break;

            if (pos_body < body.size() && body[pos_body] == '\r')
              pos_body++;
            if (pos_body < body.size() && body[pos_body] == '\n')
              pos_body++;

            std::string filename = "";
            while (pos_body < body.size()) {
                size_t eol = body.find("\r\n", pos_body);

                if (eol == std::string::npos)
                  eol = body.find("\n", pos_body);

                if (eol == std::string::npos)
                  break;

                std::string hline = body.substr(pos_body, eol - pos_body);
                pos_body = eol + (body[eol] == '\r' ? 2 : 1);
                if (hline.empty())
                  break;
                if (hline.find("Content-Disposition:") != std::string::npos ||
                    hline.find("content-disposition:") != std::string::npos) {
                    filename = extractFilename(hline);
                }
            }

            size_t part_end = body.find("\r\n" + boundary, pos_body);
            if (part_end == std::string::npos)
                part_end = body.find("\n" + boundary, pos_body);
            if (part_end == std::string::npos)
                break;

            if (filename.empty()) {
                pos_body = part_end;
                continue;
            }

            std::string part_body = body.substr(pos_body, part_end - pos_body);
            std::string dest = store_dir + filename;

            std::ofstream outfile(dest.c_str(), std::ios::out | std::ios::binary);
            if (!outfile.is_open())
                return buildError(403, config);
            outfile.write(part_body.c_str(), part_body.size());
            outfile.close();
            any_saved = true;
            pos_body = part_end;
        }

        if (!any_saved)
            return buildError(400, config);

        std::string body_resp = "<html><body><h1>201 Created: File(s) uploaded successfully.</h1></body></html>";
        return buildHeaders(201, "text/html", body_resp.size()) + body_resp;
    }

    // POST simple (non-multipart) : écrire le body dans le fichier cible
    if (isDirectory(path))
        return buildError(403, config);

    std::ofstream outfile(path.c_str(), std::ios::out | std::ios::binary);
    if (!outfile.is_open())
        return buildError(403, config);

    outfile.write(req.body.c_str(), req.body.size());
    outfile.close();

    std::string body_resp = "<html><body><h1>201 Created: File saved successfully.</h1></body></html>";
    return buildHeaders(201, "text/html", body_resp.size()) + body_resp;
}

std::string HttpResponse::handleDirectory(const std::string& path, const HttpRequest& req, const ServerConfig& config) {
  std::string index_file = path;
  if (index_file.empty() || index_file[index_file.size() - 1] != '/')
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
			std::string body = config._routes_collections._routes[i].func(config._routes_collections._routes[i]);
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

        // redirect check — doit être après resolveFilePath qui set _matched_location
        if (_matched_location && _matched_location->redirect_code != 0) {
            std::ostringstream oss;
            oss << "HTTP/1.1 " << _matched_location->redirect_code << " "
                << getStatusMessage(_matched_location->redirect_code) << "\r\n"
                << "Location: " << _matched_location->redirect_url << "\r\n"
                << "Content-Length: 0\r\n"
                << "Connection: close\r\n"
                << "\r\n";
            return oss.str();
        }

        if (path.empty())
            return buildError(404, config);

        std::cout << "resolveFilePath => " << path << std::endl;

        size_t dot = path.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = path.substr(dot);
            if (ext == ".php" || ext == ".py" || ext == ".pl") {
                if (!fileExists(path))
                    return buildError(404, config);
                CgiHandler cgi(req, config, path);
                return cgi.execute();
            }
        }


        if (isDirectory(path) && _matched_location && !_matched_location->index.empty()) {
            std::string index_ext = _matched_location->index;
            size_t idot = index_ext.rfind('.');
            if (idot != std::string::npos) {
                std::string ext = index_ext.substr(idot);
                if (ext == ".php" || ext == ".py" || ext == ".pl") {
                    std::string script_path = path;
                    if (script_path[script_path.size() - 1] != '/')
                        script_path += "/";
                    script_path += _matched_location->index;
                    if (!fileExists(script_path))
                        return buildError(404, config);
                    CgiHandler cgi(req, config, script_path);
                    return cgi.execute();
                }
            }
        }

        if (req.method == "DELETE")
            return handleDelete(path, config);
        if (req.method == "POST")
            return handlePost(req, path, config);
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
