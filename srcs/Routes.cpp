#include "../incs/Webserv.hpp"

// callback function that is called whenever a request to '/' is made
std::string RootHandler(struct Route route)
{
	std::ifstream file(route.html_file_path.c_str());

	std::stringstream htmlPageContent;
	htmlPageContent << file.rdbuf();
	file.close();

	return htmlPageContent.str();
}

/* We define all the routes here
jsp si c'est les + opti mais on verra + tard
*/
Routes::Routes()
{
	std::cout << "Init routes ..." << std::endl;
	
	// root route
	this->addRoute(
		"/",
		Methods::GET,
		"root.html",
		RootHandler
	);

	// homepage route 
	this->addRoute(
		"/home",
		Methods::GET,
		"home.html",
		RootHandler
	);

}

Routes::~Routes() {}

/*
This method allow to add a route 
*/
bool Routes::addRoute(
	std::string path,
	Methods::Type methods,
	std::string html_file_path,
	RouteHandler func
) {
	// TODO: check if route already exist later 

	struct Route route;
	route.path = path;
	route.methods = methods;
	route.html_file_path = "./routes/" + html_file_path;
	route.func = func;
	this->_routes.push_back(route);
	
	return true;
}

void Routes::displayRoute(std::string path, int client_fd)
{
	// iterate over _routes 
	for (size_t i = 0; i < this->_routes.size(); i++)
	{
		// it check if the path correspond to any of the routes
		if (this->_routes[i].path == path)
		{
			std::string htmlPageContent = this->_routes[i].func(this->_routes[i]); // if yes we simply call the func
			
			std::cout << "PROUUUUUUT" << std::endl; 
			std::cout << "htmlPageContent => " <<  htmlPageContent << std::endl;

			// jsp pq faut faire cet merde pr avoir la longueur du body mais bon
			std::stringstream ss;
			ss << htmlPageContent.length();
			std::string htmlPageContentLengthStr = ss.str();

			std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Content-Length:" + htmlPageContentLengthStr + "\r\n"
                                   "\r\n"
                                   + htmlPageContent;
			
			send(client_fd, response.c_str(), response.size(), 0);

		}
	}

	// TODO: if not found display error 404 pages ...
}