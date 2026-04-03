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
    } 
    catch (const std::exception& e) {
        std::cerr << RED << e.what() << RESET << std::endl;
        return 1;
    }
    
    return 0;
}
