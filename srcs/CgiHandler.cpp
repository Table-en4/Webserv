#include "../incs/Webserv.hpp"

CgiHandler::CgiHandler(const HttpRequest& req, const ServerConfig& config, const std::string& script_path)
    : _script_path(script_path), _req(req), _config(config) {
    buildEnv();
}

CgiHandler::~CgiHandler() {}

std::string CgiHandler::getInterpreter(const std::string& ext) {
    if (ext == ".php")
        return "/usr/bin/php-cgi";
    if (ext == ".py")
        return "/usr/bin/python3";
    if (ext == ".pl")
        return "/usr/bin/perl";
    return "";
}

void CgiHandler::buildEnv() {
    std::string path = _req.path;
    std::string query_string = "";
    size_t q = path.find('?');
    if (q != std::string::npos) {
        query_string = path.substr(q + 1);
        path = path.substr(0, q);
    }

    std::ostringstream port_oss;
    port_oss << _config.port;
    _env["REQUEST_METHOD"]  = _req.method;
    _env["QUERY_STRING"]    = query_string;
    _env["SCRIPT_FILENAME"] = _script_path;
    _env["SCRIPT_NAME"]     = path;
    _env["PATH_INFO"]       = path;
    _env["SERVER_NAME"]     = _config.server_name;
    _env["SERVER_PORT"]     = port_oss.str();
    _env["REDIRECT_STATUS"] = "200";
    _env["SERVER_PROTOCOL"] = "HTTP/1.1";
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";

    if (_req.headers.count("Host"))
        _env["HTTP_HOST"] = _req.headers.at("Host");
    if (_req.headers.count("Content-Type"))
        _env["CONTENT_TYPE"] = _req.headers.at("Content-Type");
    if (_req.headers.count("Content-Length"))
        _env["CONTENT_LENGTH"] = _req.headers.at("Content-Length");

    if (_req.headers.count("Cookie"))
        _env["HTTP_COOKIE"] = _req.headers.at("Cookie");

    if (!_req.body.empty() && !_env.count("CONTENT_LENGTH")) {
        std::ostringstream oss;
        oss << _req.body.size();
        _env["CONTENT_LENGTH"] = oss.str();
    }
}

std::string CgiHandler::parseCgiOutput(const std::string& raw) {
    size_t sep = raw.find("\r\n\r\n");
    if (sep == std::string::npos)
        sep = raw.find("\n\n");
    if (sep == std::string::npos)
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";

    size_t header_block_len = (raw[sep] == '\r') ? 4 : 2;
    std::string cgi_headers = raw.substr(0, sep);
    std::string body = raw.substr(sep + header_block_len);

    int status_code = 200;
    std::string status_msg = "OK";

    size_t status_pos = cgi_headers.find("Status:");
    if (status_pos != std::string::npos) {
        size_t end = cgi_headers.find('\n', status_pos);
        std::string status_line = cgi_headers.substr(status_pos + 7, end - status_pos - 7);
        // trim \r et espaces
        while (!status_line.empty() && (status_line[0] == ' ' || status_line[0] == '\r'))
            status_line.erase(0, 1);
        while (!status_line.empty() && (status_line[status_line.size()-1] == ' ' || status_line[status_line.size()-1] == '\r'))
            status_line.erase(status_line.size()-1);

        status_code = std::atoi(status_line.c_str());

        //extraire le message si présent ex: "303 See Other"
        size_t space = status_line.find(' ');
        if (space != std::string::npos)
            status_msg = status_line.substr(space + 1);
        else
            status_msg = (status_code == 200 ? "OK" :
                         status_code == 303 ? "See Other" :
                         status_code == 403 ? "Forbidden" :
                         status_code == 404 ? "Not Found" : "Internal Server Error");
    }

    std::string cleaned_headers;
    std::istringstream hstream(cgi_headers);
    std::string hline;
    while (std::getline(hstream, hline)) {
        if (!hline.empty() && hline[hline.size()-1] == '\r')
            hline.erase(hline.size()-1);
        if (hline.substr(0, 7) == "Status:")
            continue;
        cleaned_headers += hline + "\r\n";
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
             << cleaned_headers
             << "Content-Length: " << body.size() << "\r\n"
             << "\r\n"
             << body;

    return response.str();
}

std::string CgiHandler::execute() {
    size_t dot = _script_path.rfind('.');
    if (dot == std::string::npos)
        throw std::runtime_error("500");

    std::string ext = _script_path.substr(dot);
    std::string interpreter = getInterpreter(ext);
    if (interpreter.empty())
        throw std::runtime_error("500");

    char resolved[4096];
    if (realpath(_script_path.c_str(), resolved) == NULL)
        throw std::runtime_error("500");
    std::string abs_script_path = resolved;

    //script_dir = dossier contenant le script
    std::string script_dir = abs_script_path;
    size_t last_slash = script_dir.rfind('/');
    if (last_slash != std::string::npos)
        script_dir = script_dir.substr(0, last_slash);

    _env["SCRIPT_FILENAME"] = abs_script_path;

    std::vector<std::string> env_strings;
    for (std::map<std::string, std::string>::iterator it = _env.begin(); it != _env.end(); ++it)
        env_strings.push_back(it->first + "=" + it->second);

    std::vector<char*> env_ptrs;
    for (size_t i = 0; i < env_strings.size(); i++)
        env_ptrs.push_back(const_cast<char*>(env_strings[i].c_str()));
    env_ptrs.push_back(NULL);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(interpreter.c_str()));
    argv.push_back(const_cast<char*>(abs_script_path.c_str()));
    argv.push_back(NULL);

    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0)
        throw std::runtime_error("500");

    pid_t pid = fork();
    if (pid < 0) {
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        throw std::runtime_error("500");
    }

    if (pid == 0) {
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        chdir(script_dir.c_str());

        execve(interpreter.c_str(), argv.data(), env_ptrs.data());
        exit(0);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    if (!_req.body.empty())
        write(stdin_pipe[1], _req.body.c_str(), _req.body.size());
    close(stdin_pipe[1]);

    // boucle while anti timeout
    int time_to_wait_us = 5000000; // 5000000us == 5 seconds
    int time_passed = 0;
    pid_t res;
    while (true)
    {
        res = waitpid(pid, NULL, WNOHANG);
        // std::cout << "res: " << res << " <> pid: " << pid << std::endl;
        if (res == pid) // pid finished
            break;
        else if (res == -1)
        {
            kill(pid, SIGTERM);
            usleep(100000);
            std::string body = "<html><body><h1>Internal server error 500</h1></body></html>";
            HttpResponse r;
            return r.buildHeaders(500, "text/html", body.size()) + body;
        }
        if (time_passed >= time_to_wait_us)
        {
            kill(pid, SIGTERM);
            std::string body = "<html><body><h1>Internal server error 504 Gateway timeout</h1></body></html>";
            HttpResponse r;
            return r.buildHeaders(504, "text/html", body.size()) + body;
        }
        usleep( 10000);
        time_passed += 10000;
    }

    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(stdout_pipe[0], buf, sizeof(buf) - 1)) > 0)
        output += std::string(buf, n);
    close(stdout_pipe[0]);

    if (output.empty())
        throw std::runtime_error("500");

    return parseCgiOutput(output);
}
