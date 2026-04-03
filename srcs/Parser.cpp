#include "../incs/Webserv.hpp"

Parser::Parser() : pos(0) {}

Parser::~Parser() {}

std::string Parser::peek() {
    if (pos >= tokens.size())
        throw std::runtime_error(std::string(RED) + "Error: unexpected end of file" + RESET);
    return (tokens[pos]);
}

std::string Parser::get() {
    if (pos >= tokens.size())
        throw std::runtime_error(std::string(RED) + "Error: unexpected end of file" + RESET);
    return tokens[pos++];
}

void Parser::expect(const std::string& token) {
    std::string getting = get();
    if (getting != token)
        throw std::runtime_error(std::string(RED) + "Error: expected '" + token + "' but got '" + getting + "'" + RESET);
}

void Parser::tokenize(const std::string& content) {
    std::string current;
    bool in_comment = false;

    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];

        if (in_comment) {
            if (c == '\n')
                in_comment = false;
            continue ;
        }

        if (c == '#') {
            in_comment = true;
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue ;
        }

        if (std::isspace(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        }
        else if (c == '{' || c == '}' || c == ';') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        else {
            current += c;
        }
    }
    if (!current.empty())
        tokens.push_back(current);
    /*for (size_t i = 0; i < tokens.size(); i++)
        std::cout << tokens[i] << std::endl;*/
}

void Parser::loadFile(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error(std::string(RED) + "Error: cannot open config file" + RESET);
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    tokenize(content);
}

void Parser::parseLocation(ServerConfig& current_server) {
    LocationConfig current_location;
    current_location.path = get();
    std::cout <<  MAGENTA << " [Location] Path: " << BLUE << current_location.path << RESET << std::endl;
    expect("{");

    while (peek() != "}") {
        std::string directive = get();

        if (directive == "root") {
            current_location.root = get();
            std::cout << MAGENTA << "   Root: " << BLUE << current_location.root << RESET << std::endl;
            expect(";");
        }
        else if (directive == "index") {
            current_location.index = get();
            std::cout << MAGENTA << "   Index: " << BLUE << current_location.index << RESET << std::endl;
            expect(";");
        }
        else if (directive == "autoindex") {
            std::string autoindex_value = get();
            current_location.autoindex = (autoindex_value == "on" || autoindex_value == "true");
            std::cout << MAGENTA << "   Autoindex: " << BLUE << autoindex_value << RESET << std::endl;
            expect(";");
        }
        else if (directive == "allow_methods") {
            std::cout << MAGENTA << "    Methods: ";
            
            while (peek() != ";") {
                current_location.methods.push_back(get());
                std::cout << BLUE << current_location.methods.back() << " " << RESET;
            }
            std::cout << std::endl;
            
            expect(";");
        }
        else
            throw std::runtime_error(std::string(RED) + "Error: unknown directive '" + directive + "'in location block" + RESET);
    }
    current_server.addLocation(current_location);
    expect("}");
    
}

void Parser::parseServer() {
    ServerConfig current_server;
    get();
    expect("{");

    while (peek() != "}") {
        std::string directive = get();

        if (directive == "listen") {
            current_server.port = std::atoi(get().c_str());
            std::cout << YELLOW << "Port: " << RESET << current_server.port << std::endl;
            expect(";");
        }
        else if (directive == "server_name") {
            current_server.server_name = get();
            std::cout << YELLOW << "Server Name: " << RESET << current_server.server_name << std::endl;
            expect(";");
        }
        else if (directive == "error_page") {
            int code = std::atoi(get().c_str());
            std::string page = get();
            current_server.error_pages[code] = page;
            std::cout << YELLOW << "Error Page: " << RESET << code << " -> " << page << std::endl;
            expect(";");
        }
        else if (directive == "client_max_body_size") {
            current_server.client_max_body = std::atoi(get().c_str());
            std::cout << YELLOW << "Max Body Size: " << RESET << current_server.client_max_body << std::endl;
            expect(";");
        }
        else if (directive == "location") {
            parseLocation(current_server);
        }
        else {
            throw std::runtime_error(std::string(RED) + "Error: unknown directive '" + directive + "' in server block" + RESET);
        }
    }
    expect("}");
    this->_servers.push_back(current_server);
}

void Parser::parse() {
    while (pos < tokens.size()) {
        if (peek() == "server")
            parseServer();
        else
            throw std::runtime_error("Error: unknow token '" + peek() + "' (expected 'server')");
    }
}
