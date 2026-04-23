#include "../incs/Webserv.hpp"

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
    for (size_t i = 0; i < _server_fds.size(); i++) {
        close(_server_fds[i]);
    }

    for (std::map<int, int>::iterator it = _client_to_config.begin(); it != _client_to_config.end(); ++it) {
        close(it->first);
    }

    for (std::map<int, CgiJob>::iterator it = _cgi_jobs.begin(); it != _cgi_jobs.end(); ++it) {
        kill(it->second.pid, SIGKILL);
        waitpid(it->second.pid, NULL, 0);
        close(it->first);
    }
    
    if (_epoll_fd >= 0) {
        close(_epoll_fd);
    }
}

const std::vector<int>& ServerManager::getServerFds() const {
    return _server_fds;
}

const ServerConfig& ServerManager::getServerConfig(int client_fd, const HttpRequest& req) {
    int default_cfg_idx = _client_to_config[client_fd];
    int client_port     = _configs[default_cfg_idx].port;

    std::string host = "";
    if (req.headers.count("Host")) {
        host = req.headers.at("Host");
        size_t colon_pos = host.find(':');
        if (colon_pos != std::string::npos)
            host = host.substr(0, colon_pos);
    }

    for (size_t i = 0; i < _configs.size(); i++) {
        if (_configs[i].port == client_port && _configs[i].server_name == host)
            return _configs[i];
    }

    if (host.empty() || host == "localhost" || host == "127.0.0.1")
        return _configs[default_cfg_idx];

    throw std::runtime_error("400");
}

bool ServerManager::isServerFd(int fd) const {
    for (size_t i = 0; i < _server_fds.size(); i++) {
        if (_server_fds[i] == fd)
            return true;
    }
    return false;
}

bool ServerManager::isCgiFd(int fd) const {
    return _cgi_jobs.count(fd) > 0;
}

void ServerManager::addToEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events  = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        throw std::runtime_error(std::string(RED) + "Error: epoll_ctl failed" + RESET);
}

void ServerManager::handelNewConnection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t          client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return ;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        close(client_fd);
        return;
    }

    for (size_t i = 0; i < _server_fds.size(); i++) {
        if (_server_fds[i] == server_fd) {
            _client_to_config[client_fd] = i;
            break;
        }
    }
    addToEpoll(client_fd, EPOLLIN);
}

void ServerManager::closeClient(int client_fd) {
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    _client_to_config.erase(client_fd);
    _client_buffers.erase(client_fd);
    _write_buffers.erase(client_fd);
    close(client_fd);
}

void ServerManager::setEpollOut(int fd) {
    struct epoll_event ev;
    ev.events  = EPOLLOUT;
    ev.data.fd = fd;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void ServerManager::sendResponse(int client_fd) {
    std::string& response = _write_buffers[client_fd];

    if (response.empty()) {
        closeClient(client_fd);
        return;
    }

    ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);

    if (sent < 0) {
        closeClient(client_fd);
        return;
    }
    if (sent == 0) {
        closeClient(client_fd);
        return;
    }

    response.erase(0, sent);

    if (response.empty())
        closeClient(client_fd);
}

// Appelé quand epoll signale EPOLLIN sur un pipe CGI
void ServerManager::handleCgiOutput(int pipe_fd) {
    CgiJob& job = _cgi_jobs[pipe_fd];
    char    buf[4096];
    ssize_t n;

    // lire tout ce qui est dispo (non-bloquant)
    while ((n = read(pipe_fd, buf, sizeof(buf))) > 0)
        job.output += std::string(buf, n);

    // n == 0 ou EAGAIN : vérifier si le process est terminé
    int   status;
    pid_t res = waitpid(job.pid, &status, WNOHANG);

    if (res == 0) {
        // process toujours en vie, pas encore de EOF → attendre prochain EPOLLIN
        // (le timeout sera géré dans run())
        return;
    }

    // process terminé (res == pid) ou erreur waitpid (res == -1)
    // vider le reste du pipe au cas où
    while ((n = read(pipe_fd, buf, sizeof(buf))) > 0)
        job.output += std::string(buf, n);

    std::string response;
    if (job.output.empty() || res == -1) {
        HttpResponse r;
        response = r.buildError(500, job.config);
    } else {
        response = CgiHandler::parseCgiOutput(job.output);
    }

    _write_buffers[job.client_fd] = response;
    setEpollOut(job.client_fd);

    // cleanup
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, pipe_fd, NULL);
    close(pipe_fd);
    _cgi_jobs.erase(pipe_fd);
}

// Appelé dans run() pour killer les CGI qui ont dépassé le timeout
void ServerManager::checkCgiTimeouts() {
    const int CGI_TIMEOUT = 5; // secondes

    std::vector<int> to_kill;
    for (std::map<int, CgiJob>::iterator it = _cgi_jobs.begin(); it != _cgi_jobs.end(); ++it) {
        if (time(NULL) - it->second.start_time >= CGI_TIMEOUT)
            to_kill.push_back(it->first);
    }

    for (size_t i = 0; i < to_kill.size(); i++) {
        int      pipe_fd = to_kill[i];
        CgiJob&  job     = _cgi_jobs[pipe_fd];

        kill(job.pid, SIGKILL);
        waitpid(job.pid, NULL, 0);

        HttpResponse r;
        std::string  response = r.buildError(504, job.config);
        _write_buffers[job.client_fd] = response;
        setEpollOut(job.client_fd);

        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, pipe_fd, NULL);
        close(pipe_fd);
        _cgi_jobs.erase(pipe_fd);
    }
}

void ServerManager::handleClient(int client_fd) {
    char    tmp[8192];
    ssize_t bytes_read = recv(client_fd, tmp, sizeof(tmp) - 1, 0);

    if (bytes_read < 0) {
        closeClient(client_fd);
        return;
    }
    if (bytes_read == 0) {
        closeClient(client_fd);
        return;
    }

    _client_buffers[client_fd] += std::string(tmp, bytes_read);
    std::string& buf = _client_buffers[client_fd];

    size_t header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return;

    size_t body_start     = header_end + 4;
    size_t content_length = 0;
    size_t cl_pos         = buf.find("Content-Length:");

    if (cl_pos != std::string::npos && cl_pos < header_end) {
        content_length = std::atoi(buf.c_str() + cl_pos + 15);

        int cfg_idx = _client_to_config.count(client_fd) ? _client_to_config[client_fd] : 0;
        if (content_length > _configs[cfg_idx].client_max_body) {
            _write_buffers[client_fd] = "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 29\r\n\r\n<h1>Payload is too large</h1>";
            setEpollOut(client_fd);
            return;
        }

        if (buf.size() - body_start < content_length)
            return;
    }

    HttpRequest  req;
    HttpResponse res;
    std::string  response;

    try {
        req.parse(buf);
        const ServerConfig& target_config = getServerConfig(client_fd, req);

        // Vérifier si c'est une requête CGI
        std::string path = res.resolveFilePath(req, target_config);

        // redirect check
        const LocationConfig* matched = res.getMatchedLocation();
        if (matched && matched->redirect_code != 0) {
            std::ostringstream oss;
            oss << "HTTP/1.1 " << matched->redirect_code << " "
                << res.getStatusMessage(matched->redirect_code) << "\r\n"
                << "Location: " << matched->redirect_url << "\r\n"
                << "Content-Length: 0\r\n"
                << "Connection: close\r\n\r\n";
            _write_buffers[client_fd] = oss.str();
            setEpollOut(client_fd);
            return;
        }

        bool is_cgi = false;
        std::string script_path = path;

        if (!path.empty()) {
            size_t dot = path.rfind('.');
            if (dot != std::string::npos) {
                std::string ext = path.substr(dot);
                if (ext == ".php" || ext == ".py" || ext == ".pl")
                    is_cgi = true;
            }
            // CGI via index d'un répertoire
            if (!is_cgi && res.isDirectory(path) && matched && !matched->index.empty()) {
                size_t idot = matched->index.rfind('.');
                if (idot != std::string::npos) {
                    std::string ext = matched->index.substr(idot);
                    if (ext == ".php" || ext == ".py" || ext == ".pl") {
                        script_path = path;
                        if (script_path[script_path.size()-1] != '/')
                            script_path += "/";
                        script_path += matched->index;
                        is_cgi = true;
                    }
                }
            }
        }

        if (is_cgi) {
            if (!res.fileExists(script_path)) {
                _write_buffers[client_fd] = res.buildError(404, target_config);
                setEpollOut(client_fd);
                return;
            }

            // Lancement non-bloquant du CGI
            CgiHandler handler(req, target_config, script_path);
            pid_t      pid;
            int        pipe_fd = handler.launch(pid);

            // enregistrer le job
            CgiJob job;
            job.pid        = pid;
            job.pipe_fd    = pipe_fd;
            job.client_fd  = client_fd;
            job.start_time = time(NULL);
            job.config     = target_config;
            job.req        = req;
            _cgi_jobs[pipe_fd] = job;

            // surveiller le pipe via epoll
            addToEpoll(pipe_fd, EPOLLIN);
            return; // réponse sera envoyée dans handleCgiOutput()
        }

        // requête non-CGI : traitement synchrone habituel
        response = res.build(req, target_config);
    }
    catch (std::exception &err) {
        std::string err_code_str = err.what();
        int err_code = -1;
        std::stringstream ss(err_code_str);
        ss >> err_code;

        std::ostringstream oss;
        oss << "HTTP/1.1 " << err_code << " "
            << res.getStatusMessage(err_code) << "\r\n"
            << "Content-Length: "
            << res.getStatusMessage(err_code).length()
            << "\r\n"
            << "Connection: close\r\n\r\n"
            << res.getStatusMessage(err_code);
        response = oss.str();
    }

    _write_buffers[client_fd] = response;
    setEpollOut(client_fd);
}

void ServerManager::initServers(const std::vector<ServerConfig>& configs) {
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd < 0)
        throw std::runtime_error(std::string(RED) + "Error: epoll_create1 failed" + RESET);

    for (size_t i = 0; i < configs.size(); i++) {
        int                server_fd;
        struct sockaddr_in address;

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
            throw std::runtime_error(std::string(RED) + "Error: Impossible to create socket" + RESET);

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            close(server_fd);
            throw std::runtime_error(std::string(RED) + "Error: setsockopt failed" + RESET);
        }

        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port        = htons(configs[i].port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(server_fd);
            throw std::runtime_error(std::string(RED) + "Error: Bind failed" + RESET);
        }

        if (listen(server_fd, 10) < 0) {
            close(server_fd);
            throw std::runtime_error(std::string(RED) + "Error: listen failed" + RESET);
        }

        if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
            close(server_fd);
            throw std::runtime_error(std::string(RED) + "Error: fcntl failed" + RESET);
        }

        _server_fds.push_back(server_fd);
        _configs.push_back(configs[i]);
        addToEpoll(server_fd, EPOLLIN);
        std::cout << GREEN << "Server run and listening port " << configs[i].port << RESET << std::endl;
    }
}

volatile sig_atomic_t global_var_server_running = true;

void sigint_handler(int sig)
{
    if (sig == SIGINT)
    {
        std::cout << "Server shutting down ..." << std::endl;
        global_var_server_running = false;
    }

}

void ServerManager::run() {
    struct epoll_event events[64];

    signal(SIGINT, sigint_handler);

    while (global_var_server_running) {
        // timeout de 1s pour checkCgiTimeouts même sans événements
        int n = epoll_wait(_epoll_fd, events, 64, 1000);

        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Error: epoll_wait failed");
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                if (isCgiFd(fd))
                    handleCgiOutput(fd);
                else
                    closeClient(fd);
                continue;
            }

            if (isServerFd(fd))
                handelNewConnection(fd);
            else if (isCgiFd(fd))
                handleCgiOutput(fd);
            else if (events[i].events & EPOLLOUT)
                sendResponse(fd);
            else if (events[i].events & EPOLLIN)
                handleClient(fd);
        }

        checkCgiTimeouts();
    }
}
