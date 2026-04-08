#pragma once

#include "Webserv.hpp"

struct Route;
typedef std::string (*RouteHandler)(Route route);


// a bouger cet enum et g mis des nombres random on pourra changer ca oklm
struct Methods {
	enum Type {
		GET = 900,
		POST = 901,
		DELETE = 902
	};
};

// structure that represent a route
class HttpRequest;
struct Route
{
	std::string path;
	Methods::Type methods; // 
	std::string html_file_path;
	RouteHandler func; // function to be called when client want to access any routes

	HttpRequest *req; // pointeur initialisé à nul

};

class Routes
{
	public:
		std::vector<struct Route> _routes;
		Routes();
		~Routes();

		bool addRoute(
			std::string path,
			Methods::Type methods,
			std::string html_file_path,
			RouteHandler func
		);

		void displayRoute(
			std::string path,
			int client_fd
		);
};