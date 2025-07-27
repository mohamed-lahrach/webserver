#include "request.hpp"

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
	(void)request_data;
	return (true);
}