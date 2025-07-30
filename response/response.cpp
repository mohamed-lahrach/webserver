#include "response.hpp"
#include <sstream>
#include <iostream>     // For std::cout
#include <cstring>      // For strlen()
#include <sys/socket.h> // For send()
#include <sys/epoll.h>  // For epoll_ctl(), EPOLL_CTL_DEL
#include <unistd.h>     // For close()
#include <map>
// Constructor
Response::Response() : status_code(200), content("Welcome to My Web Server!")
{
	// Set default headers
	set_header("Content-Type", "text/html");
	set_header("Connection", "close");
}

// Destructor
Response::~Response()
{

}

// Setter methods
void Response::set_code(int code)
{
	status_code = code;
}

void Response::set_content(const std::string& body_content)
{
	content = body_content;
}

void Response::set_header(const std::string& key, const std::string& value)
{
	headers[key] = value;
}

void Response::handle_response(int client_fd)
{
	std::cout << "=== RESPONSE: Building and sending response ===" << std::endl;
	
	std::ostringstream status_stream;
	status_stream << status_code;
	std::string code_str = status_stream.str();
	std::string reason_phrase = "OK";  
	std::string status_line = "HTTP/1.0 " + code_str + " " + reason_phrase + "\r\n";
	std::string headers_line;
	std::map<std::string ,std::string>::iterator ite=headers.begin();

	while (ite!=headers.end())
	{
		headers_line+=ite->first + ": " + ite->second + "\r\n";
		ite++;
	}
	
	std::ostringstream content_length_stream;
	content_length_stream << content.length();
	headers_line += "Content-Length: " + content_length_stream.str() + "\r\n";
	headers_line += "\r\n";  

	std::string full_response = status_line + headers_line + content;

	std::cout << "status line: " << status_line;
	std::cout << "headers: " << headers_line;
	std::cout << "content: " << content << std::endl;
	std::cout << "=== Complete Response ===\n" << full_response << "========================" << std::endl;
	
	std::cout << "Sending response to client " << client_fd << std::endl;
	ssize_t bytes_sent = send(client_fd, full_response.c_str(), full_response.length(), 0);

	if (bytes_sent > 0)
	{
		std::cout << "✓ Response sent successfully (" << bytes_sent << " bytes)" << std::endl;
	}
	else
	{
		std::cout << "✗ Failed to send response" << std::endl;
	}
}