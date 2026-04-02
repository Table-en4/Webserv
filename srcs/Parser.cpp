#include "../incs/Webserv.hpp"

Parser::Parser() : pos(0) {}

Parser::~Parser() {}

std::string Parser::peek() {
    if (pos >= tokens.size())
        throw std::runtime_error("Error: unexpected end of file");
    return (tokens[pos]);
}

std::string Parser::get() {
    if (pos >= tokens.size())
        throw std::runtime_error("Error: unexpected end of file");
    return tokens[pos++];
}

void Parser::expect(const std::string& token) {
    std::string getting = get();
    if (getting != token)
        throw std::runtime_error("Error: expected '" + token + "' but got '" + getting + "'");
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
        throw std::runtime_error("Error: cannot open config file");
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    tokenize(content);
}

void Parser::parseLocation() {
    std::string path = get();
    std::cout << " [Location] Path: " << path << std::endl;
    expect("{");

    while (peek() != "}") {
        std::string directive = get();

        if (directive == "root") {
            std::cout << "   Root: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "index") {
            std::cout << "   Index: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "autoindex") {
            std::cout << "   Autoindex: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "allow_methods") {
            std::cout << "    Methods: ";
            
            while (peek() != ";") {
                std::cout << get() << " ";
            }
            std::cout << std::endl;
            
            expect(";");
        }
        else
            throw std::runtime_error("Error: unknown directive '" + directive + "'in location block");
    }
    expect("}");
}

void Parser::parseServer() {
    get();
    expect("{");

    while (peek() != "}") {
        std::string directive = get();

        if (directive == "listen") {
            std::cout << "Port: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "server_name") {
            std::cout << "Server Name: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "error_page") {
            std::cout << "Error Page: " << get() << " -> " << get() << std::endl;
            expect(";");
        }
        else if (directive == "client_max_body_size") {
            std::cout << "Max Body Size: " << get() << std::endl;
            expect(";");
        }
        else if (directive == "location") {
            parseLocation();
        }
        else {
            throw std::runtime_error("Error: unknown directive '" + directive + "' in server block");
        }
    }
    expect("}");
}

void Parser::parse() {
    while (pos < tokens.size()) {
        if (peek() == "server")
            parseServer();
        else
            throw std::runtime_error("Error: unknow token '" + peek() + "' (expected 'server')");
    }
}
