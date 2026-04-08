#pragma once

#include <string>
#include <map>
#include <vector>
#include <sys/wait.h>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

class CgiHandler {
    private:
        std::string                         _script_path;
        std::string                         _cgi_bin;
        std::map<std::string, std::string>  _env;
        const HttpRequest&                  _req;
        const ServerConfig&                 _config;
    
    private:
        //fonction tah minishell prime
        void        buildEnv();
        std::string parseCgiOutput(const std::string& raw);
        std::string getInterpreter(const std::string& ext);
        
    public:
        CgiHandler(const HttpRequest& req, const ServerConfig& config, const std::string& script_path);
        ~CgiHandler();

        std::string execute();
};
