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
	std::string header;
	while (std::getline(iss, header) && header != "")
	{
		std::string key, value;
		size_t pos = header.find(":");
		if (pos != std::string::npos)
		{
			key = header.substr(0, pos);
			value = header.substr(pos + 1);
			headers[key] = value;
		}
	}

	std::cout << "Method: " << method << std::endl;
	std::cout << "Path: " << path << std::endl;
	std::cout << "Version: " << version << std::endl;
	std::cout << "Headers:" << std::endl;
	std::map<std::string, std::string>::iterator it = headers.begin();
	while (it != headers.end())
	{
		std::cout << it->first << ": " << it->second << std::endl;
		++it;
	}

	return (true);
}
