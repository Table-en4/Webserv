#pragma once

#include <string>
#include <map>
#include <vector>
#include <sys/wait.h>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

class CgiHandler {
public:
    CgiHandler(const HttpRequest& req, const ServerConfig& config, const std::string& script_path);
    ~CgiHandler();
 
    int         launch(pid_t& out_pid);
    static std::string parseCgiOutput(const std::string& raw);
 
private:
    std::string _script_path;
    const HttpRequest&   _req;
    const ServerConfig&  _config;
    std::map<std::string, std::string> _env;
 
    void        buildEnv();
    std::string getInterpreter(const std::string& ext);
};
