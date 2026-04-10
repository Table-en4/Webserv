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

    if (_req.headers.count("Host"))
        _env["HTTP_HOST"] = _req.headers.at("Host");
    if (_req.headers.count("Content-Type"))
        _env["CONTENT_TYPE"] = _req.headers.at("Content-Type");
    if (_req.headers.count("Content-Length"))
        _env["CONTENT_LENGTH"] = _req.headers.at("Content-Length");
    if (_req.headers.count("Cookie"))
        _env["COOKIE"] = _req.headers.at("Cookie");
    else if (!_req.body.empty()) {
        std::ostringstream oss;
        oss << _req.body.size();
        _env["CONTENT_LENGTH"] = oss.str();
    }
}

std::string CgiHandler::parseCgiOutput(const std::string& raw) {
    //Le CGI retourne ses propres headers + body
    //on doit construire la réponse HTTP complète
    size_t sep = raw.find("\r\n\r\n");
    if (sep == std::string::npos)
        sep = raw.find("\n\n");
    if (sep == std::string::npos)
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";

    std::string cgi_headers = raw.substr(0, sep);
    std::string body = raw.substr(sep + (raw[sep] == '\r' ? 4 : 2));

    //on cherche le status dans les headers CGI
    int status_code = 200;
    std::string status_msg = "OK";
    size_t status_pos = cgi_headers.find("Status:");
    if (status_pos != std::string::npos) {
        size_t end = cgi_headers.find('\n', status_pos);
        std::string status_line = cgi_headers.substr(status_pos + 7, end - status_pos - 7);
        while (!status_line.empty() && status_line[0] == ' ')
            status_line.erase(0, 1);
        status_code = std::atoi(status_line.c_str());
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
             << cgi_headers << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "\r\n"
             << body;

    return response.str();
}

std::string CgiHandler::execute() {
    //trouver l'interpréteur selon l'extension
    size_t dot = _script_path.rfind('.');
    if (dot == std::string::npos)
        throw std::runtime_error("500");

    std::string ext = _script_path.substr(dot);
    std::string interpreter = getInterpreter(ext);
    if (interpreter.empty())
        throw std::runtime_error("500");

    //construire l'environnement pour execve
    std::vector<std::string> env_strings;
    for (std::map<std::string, std::string>::iterator it = _env.begin(); it != _env.end(); ++it)
        env_strings.push_back(it->first + "=" + it->second);

    std::vector<char*> env_ptrs;
    for (size_t i = 0; i < env_strings.size(); i++)
        env_ptrs.push_back(const_cast<char*>(env_strings[i].c_str()));
    env_ptrs.push_back(NULL);

    //argv pour execve : [interpreter, script, NULL]
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(interpreter.c_str()));
    argv.push_back(const_cast<char*>(_script_path.c_str()));
    argv.push_back(NULL);

    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0)
        throw std::runtime_error("500");

    std::string script_dir = _script_path;
    size_t last_slash = script_dir.rfind('/');
    if (last_slash != std::string::npos)
        script_dir = script_dir.substr(0, last_slash);

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("500");

    if (pid == 0) {

        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        chdir(script_dir.c_str());

        execve(interpreter.c_str(), argv.data(), env_ptrs.data());
        exit(1);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    //Envoyer le body au CGI si POST
    if (!_req.body.empty())
        write(stdin_pipe[1], _req.body.c_str(), _req.body.size());
    close(stdin_pipe[1]);

    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(stdout_pipe[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        output += std::string(buf, n);
    }
    close(stdout_pipe[0]);
    int status;
    waitpid(pid, &status, 0);

    if (output.empty())
        throw std::runtime_error("500");

    return parseCgiOutput(output);
}
