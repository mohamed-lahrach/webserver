#include "response.hpp"
#include <iostream>     // For std::cout
#include <cstring>      // For strlen()
#include <sys/socket.h> // For send()
#include <sys/epoll.h>  // For epoll_ctl(), EPOLL_CTL_DEL
#include <unistd.h>     // For close()
#include <map>
// Constructor
Response::Response()
{
    
}

// Destructor
Response::~Response()
{

}
void Response::handle_response(int client_fd)
{
	std::cout << "=== RESPONSE: Building and sending response ===" << std::endl;

	// Simple HTTP response
	const char *response = "HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html\r\n"
							"Content-Length: 25\r\n"
							"Connection: close\r\n"
							"\r\n"
							"Welcome to My Web Server!";

	// Send response to client
	std::cout << "Sending response to client " << client_fd << std::endl;
	ssize_t bytes_sent = send(client_fd, response, strlen(response), 0);

	if (bytes_sent > 0)
	{
		std::cout << "✓ Response sent successfully (" << bytes_sent << " bytes)" << std::endl;
	}
	else
	{
		std::cout << "✗ Failed to send response" << std::endl;
	}
}