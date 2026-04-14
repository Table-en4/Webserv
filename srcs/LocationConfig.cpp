#include "../incs/Webserv.hpp"

LocationConfig::LocationConfig() : path(""), root(""), autoindex(false), redirect_code(0), redirect_url(""), upload_store("") {}

LocationConfig::~LocationConfig() {}
