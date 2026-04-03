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
struct Route
{
	std::string path;
	Methods::Type methods; // 
	std::string html_file_path;
	RouteHandler func; // function to be called when client want to access any routes
};

class Routes
{
	std::vector<struct Route> _routes;

	public:
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