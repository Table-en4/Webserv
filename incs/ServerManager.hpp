#pragma once

#include "ServerConfig.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class  ServerManager {
	private:
		std::vector<int>	_server_fds;

	public:
		ServerManager();
		~ServerManager();

		void initServers(const std::vector<ServerConfig>& configs);
		const std::vector<int>& getServerFds() const;
};
