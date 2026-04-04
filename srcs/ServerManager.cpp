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

	for (size_t i = 0; i < _server_fds.size(); i++) {
		if (_server_fds[i] == server_fd) {
			_client_to_config[client_fd] = i;
			break;
		}
	}
	addToEpoll(client_fd, EPOLLIN);
}

void ServerManager::handleClient(int client_fd) {
    char buffer[8192] = {0};
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        return;
    }

    std::string raw(buffer, bytes_read);
    HttpRequest req;
    HttpResponse res;

	//on récup la config du bon server (par port)
	//on prend la premiere juste pour le test
	//faudra que je fasse un HostHeader
    std::string response;
    try {
        req.parse(raw);
        int config_idx = 0;
        if (_client_to_config.count(client_fd))
            config_idx = _client_to_config[client_fd];
        response = res.build(req, _configs[config_idx]);
    }
    catch (...) {
        response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    }

    send(client_fd, response.c_str(), response.size(), 0);
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    _client_to_config.erase(client_fd);
    close(client_fd);
}

void ServerManager::run() {
	struct epoll_event events[64];

	while (true) {
		int n = epoll_wait(_epoll_fd, events, 64, -1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			throw std::runtime_error(std::string(RED) + "Error: epoll_wait failed" + RESET);
		}
		for (int i = 0; i < n; i++) {
			int fd = events[i].data.fd;

			if (events[i].events & (EPOLLERR | EPOLLHUP)) {
				close(fd);
				continue;
			}
			if (isServerFd(fd))
				handelNewConnection(fd);
			else if (events[i].events & EPOLLIN)
				handleClient(fd);
		}
	}
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

		// 10 = la taille de la fille d'attente des connexions
		if (listen(server_fd, 10) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error: listen failed" + RESET);
		}
		_server_fds.push_back((server_fd));
		_configs.push_back(configs[i]);
		addToEpoll(server_fd, EPOLLIN);
		std::cout << GREEN << "Server run and listening port " << configs[i].port << RESET << std::endl;
	}
}
