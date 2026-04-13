#include "../incs/Webserv.hpp"

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _server_fds.size(); i++)
		close(_server_fds[i]);
	close(_epoll_fd);
}

const std::vector<int>& ServerManager::getServerFds() const {
    return _server_fds;
}

const ServerConfig& ServerManager::getServerConfig(int client_fd, const HttpRequest& req) {
    int default_cfg_idx = _client_to_config[client_fd];
    int client_port = _configs[default_cfg_idx].port;

    std::string host = "";
    if (req.headers.count("Host")) {
        host = req.headers.at("Host");
        size_t colon_pos = host.find(':');
        if (colon_pos != std::string::npos)
            host = host.substr(0, colon_pos);
    }

    for (size_t i = 0; i < _configs.size(); i++) {
        if (_configs[i].port == client_port && _configs[i].server_name == host) {
            return _configs[i];
        }
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

void ServerManager::addToEpoll(int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		throw std::runtime_error(std::string(RED) + "Error: epoll_ctl failed" + RESET);
	}
}

void ServerManager::handelNewConnection(int server_fd) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd < 0)
		return ;

    //gestion du multi plexing
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        close(client_fd);
        return ;
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
    ev.events = EPOLLOUT;
    ev.data.fd = fd;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    //EPOLL_CTL_MOD modifie un fd deja utilisé
    //EPOLL_CTL_ADD ajoute un doublon pour éviter la réecriture sur un fd
}

void ServerManager::sendResponse(int client_fd) {
    std::string& response = _write_buffers[client_fd];

    if (response.empty()) {
        closeClient(client_fd);
        return;
    }

    //IMPORTANT: send n'evoie qu'une partie car on boucle sur epoll
    ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ;
        closeClient(client_fd);
        return;
    }

    response.erase(0, sent);

    if (response.empty()) {
        closeClient(client_fd);
    }
}

/*
recv() -> append dans _client_buffers[fd]
chercher "\r\n\r\n" dans le buffer -> headers complets ?
    non -> return (on attend le prochain EPOLLIN)
    oui -> extraire Content-Length
          vérifier contre client_max_body -> 413 si dépassé
          body complet ? -> traiter
          non -> return (on attend encore)
traiter -> envoyer réponse -> cleanup
*/
void ServerManager::handleClient(int client_fd) {
    char tmp[8192];
    int bytes_read = recv(client_fd, tmp, sizeof(tmp) - 1, 0);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ;
        closeClient(client_fd);
        return;
    }
    if (bytes_read == 0) {
        closeClient(client_fd);
        return ;
    }

    tmp[bytes_read] = '\0';
    _client_buffers[client_fd] += std::string(tmp, bytes_read);

    std::string& buf = _client_buffers[client_fd];

    size_t header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return;

    size_t body_start = header_end + 4;
    size_t content_length = 0;
    size_t cl_pos = buf.find("Content-Length:");
    if (cl_pos != std::string::npos && cl_pos < header_end) {
        content_length = std::atoi(buf.c_str() + cl_pos + 15);

        int cfg_idx = _client_to_config.count(client_fd) ? _client_to_config[client_fd] : 0;
        if (content_length > _configs[cfg_idx].client_max_body) {
            _write_buffers[client_fd] = "HTTP/1.1 413 Payload Too Large\r\n\r\n<h1>Payload is too large</h1>";
            setEpollOut(client_fd);
            return;
        }

        if (buf.size() - body_start < content_length)
            return;
    }

    //c'est ici qu'on construire la réponse
    HttpRequest req;
    HttpResponse res;
    std::string response;
    try {
        req.parse(buf);
        const ServerConfig& target_config = getServerConfig(client_fd, req);
        response = res.build(req, target_config);
    }
    catch (...) {
        response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    //on stocke et on demande EPOLLOUT
    _write_buffers[client_fd] = response;
    setEpollOut(client_fd);
}

void ServerManager::initServers(const std::vector<ServerConfig>& configs) {
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd < 0)
		throw std::runtime_error(std::string(RED) + "Error: epoll_create1 failed" + RESET);
	for (size_t i = 0; i < configs.size(); i++) {
		int server_fd;
		struct sockaddr_in address;

		//je cré mon socket ici
		//AF_INET = IPv4
		//SOCK_STREAM = TCP
		//0 = protocole par défaut
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server_fd < 0)
			throw std::runtime_error(std::string(RED) + "Error: Impossible to create socket" + RESET);
		
		//config du socket pour éviter l'erreur : adress already in use
		int opt = 1;
		if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error: setsockopt failed" + RESET);
		}

		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(configs[i].port);

		if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error: Bind failed" + RESET);
		}

		//10 = la taille de la fille d'attente des connexions
		if (listen(server_fd, 10) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error: listen failed" + RESET);
		}

        if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
            close(server_fd);
            throw std::runtime_error(std::string(RED) + "Error: fcntl failed" + RESET);
        }

		_server_fds.push_back((server_fd));
		_configs.push_back(configs[i]);
		addToEpoll(server_fd, EPOLLIN);
		std::cout << GREEN << "Server run and listening port " << configs[i].port << RESET << std::endl;
	}
}

void ServerManager::run() {
    struct epoll_event events[64];

    while (true) {
        int n = epoll_wait(_epoll_fd, events, 64, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Error: epoll_wait failed");
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                closeClient(fd);
                continue;
            }

            if (isServerFd(fd))
                handelNewConnection(fd);
            else if (events[i].events & EPOLLOUT)
                sendResponse(fd);      // ← NOUVEAU
            else if (events[i].events & EPOLLIN)
                handleClient(fd);
        }
    }
}
