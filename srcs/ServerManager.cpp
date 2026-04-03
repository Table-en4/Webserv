#include "../incs/Webserv.hpp"

ServerManager::ServerManager() {}

ServerManager::~ServerManager() {
	for (size_t i = 0; i < _server_fds.size(); i++) {
		close(_server_fds[i]);
	}
}

const std::vector<int>& ServerManager::getServerFds() const {
    return _server_fds;
}

void ServerManager::initServers(const std::vector<ServerConfig>& configs) {
	for (size_t i = 0; i < configs.size(); i++) {
		int server_fd;
		struct sockaddr_in adress;

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
			throw std::runtime_error(std::string(RED) + "Error: setsockpot fqiled" + RESET);
		}

		adress.sin_family = AF_INET;
		adress.sin_addr.s_addr = INADDR_ANY;
		adress.sin_port = htons(configs[i].port);

		if (bind(server_fd, (struct sockaddr*)&adress, sizeof(adress)) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error Bind failed" + RESET);
		}

		// 10 = la taille de la fille d'attente des connexions
		if (listen(server_fd, 10) < 0) {
			close(server_fd);
			throw std::runtime_error(std::string(RED) + "Error: list failed" + RESET);
		}
		_server_fds.push_back((server_fd));
		std::cout << GREEN << "Server run and listening port " << configs[i].port << RESET << std::endl;
	}
}
