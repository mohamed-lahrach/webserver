#include "request.hpp"
#include <sstream>
#include <iostream>


// Constructor
Request::Request()
{
}

// Destructor
Request::~Request()
{
}
bool Request::handle_request(int client_fd, const char *request_data)
{
	(void)client_fd;
	
	std::string request_str = request_data;
	std::istringstream iss(request_str);
	iss >> method >> path >> version;
	std::cout << "Method: " << method << std::endl;
	std::cout << "Path: " << path << std::endl;
	std::cout << "Version: " << version << std::endl;
	

	return (true);
}
