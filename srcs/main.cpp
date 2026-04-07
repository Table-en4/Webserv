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
        std::cout << GREEN << "Server is running - Ctrl+C to stop" << RESET << std::endl;
        manager.run();
    }
    catch (const std::exception& e) {
        std::cerr << RED << e.what() << RESET << std::endl;
        return 1;
    }
    
    return 0;
}
