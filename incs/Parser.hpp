#pragma once

#include <string>
#include <vector>
#include "ServerConfig.hpp"


class Parser {
    //met ici toutes les privates variables
    private:
        std::vector<std::string>    tokens;
        std::vector<ServerConfig>   _servers;
        size_t                      pos;
        
    
    //et ici toutes les privates fonctions apparament c'est une
    //norme en cpp on peut le faire tqt ca change rien
    private:
        std::string peek();
        std::string get();
        void expect(const std::string& token);

        void parseServer();
        void parseLocation(ServerConfig& current_server);

    public:
        Parser();
        ~Parser();

        const std::vector<ServerConfig>& getServers() const;

        void loadFile(const std::string& path);
        void tokenize(const std::string& content);
        void parse();
};
