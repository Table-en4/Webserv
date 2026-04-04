#pragma once

#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpRespons.hpp"
#include <vector>
#include <map>
#include <iostream>
#include <stdexcept>
#include <cerrno>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>

class ServerManager {
	private:
		std::vector<int>          _server_fds;
		std::vector<ServerConfig> _configs;
		std::map<int, int>        _client_to_config;
		int                       _epoll_fd;

		void addToEpoll(int fd, uint32_t events);
		void handelNewConnection(int server_fd);
		void handleClient(int client_fd);
		bool isServerFd(int fd) const;

	public:
		ServerManager();
		~ServerManager();

		void initServers(const std::vector<ServerConfig>& configs);
		void run();
		const std::vector<int>& getServerFds() const;
};
