#include "../incs/Webserv.hpp"

ServerConfig::ServerConfig() : port(8080), client_max_body(1000000) {}

ServerConfig::~ServerConfig() {}

void ServerConfig::addLocation(const LocationConfig& loc) {
	locations.push_back(loc);
}
