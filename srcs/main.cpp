#include "../incs/Webserv.hpp"

// ======= a bouger ces deux functions dans un utils ou jsp

// temporary libft split
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    size_t start = 0;
    size_t end = s.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find(delimiter, start);
    }
    
    // Add the last remaining part
    tokens.push_back(s.substr(start));
    
    return tokens;
}

// function temporaire
std::string extractPathFromBuffer(char buffer[])
{
    std::string strBuffer(buffer); // convert

    std::vector<std::string> lines = split(strBuffer, '\n');
    std::vector<std::string> firstLine = split(lines[0], ' ');
    const std::string requestedPath = firstLine[1];
    return requestedPath;
}

// =========

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

        // jle fais un peu a la zeb mais trql
        Routes routes;

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
                // std::cout << buffer << std::endl;
            }

            std::string requestedPath = extractPathFromBuffer(buffer);
            routes.displayRoute(requestedPath, client_fd);

            // std::string response = "HTTP/1.1 200 OK\r\n"
            //                        "Content-Type: text/html\r\n"
            //                        "Content-Length: 53\r\n"
            //                        "\r\n"
            //                        "<h1>tasty crousty = degueu</h1>";

            // send(client_fd, response.c_str(), response.size(), 0);
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
