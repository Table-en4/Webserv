#include "../incs/Webserv.hpp"

int main(int ac, char **av) {
    if (ac != 2) {
        std::cout << RED << "bad arguments correct way :" << RESET << std::endl;
        std::cout << GREEN << "./webserv <file.conf>" << RESET << std::endl;
        return 1;
    }

    std::string filename = av[1];
    
    if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".conf") {
        std::cout << RED << "Error: file must be <filename>.conf" << RESET << std::endl;
        return 1;
    }

    try {
        Parser p;
        
        p.loadFile(filename);
        p.parse();
        ServerManager manager;
        manager.initServers(p.getServers());
        std::cout << GREEN << "Server is runing Ctrl+c to cancel" << RESET << std::endl;
        
        //port 8080
        int test_server_fd = manager.getServerFds()[0];

        while (true) { 

            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(test_server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cerr << "Error: connection" << std::endl;
                continue;
            }

            char buffer[4096] = {0};
            int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read > 0) {
                std::cout << MAGENTA << "\n http request" << RESET << std::endl;
                std::cout << buffer << std::endl;
            }
            std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Content-Length: 53\r\n"
                                   "\r\n"
                                   "<h1>tasty crousty = degueu</h1>";

            send(client_fd, response.c_str(), response.size(), 0);
            std::cout << "msg send" << std::endl;

            close(client_fd);
        }
    }
    catch (const std::exception& e) {
        std::cerr << RED << e.what() << RESET << std::endl;
        return 1;
    }
    
    return 0;
}
