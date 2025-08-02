#include "response.hpp"
#include <sstream>
#include <iostream>   
#include <cstring>    
#include <sys/socket.h>
#include <sys/epoll.h>  
#include <unistd.h>    
#include <map>

Response::Response() : status_code(200), content("Welcome to My Web Server!")
{

	set_header("Content-Type", "text/html");
	set_header("Connection", "close");
}


Response::~Response()
{

}

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

void Response::analyze_request_and_set_response(const std::string& path)
{
	std::cout << "=== ANALYZING REQUEST PATH: " << path << " ===" << std::endl;
	
	if (path == "/" || path == "/index.html" || path == "/home")
	{
		set_code(200);
		set_content("Welcome to My Web Server!");
		std::cout << "✓ Valid path - returning 200 OK" << std::endl;
	}
	else
	{
		set_code(404);
		set_content("404 Not Found\n");
		set_header("Content-Type", "text");
		std::cout << "✗ Unknown path - returning 404 Not Found" << std::endl;
	}
}

std::string Response::what_reason(int code)
{
	switch (code)
	{
	case 200:
		return "OK";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 500:
		return "Internal Server Error";
	default:
		return "Unknown";
	}
}

void Response::handle_response(int client_fd)
{
	std::cout << "=== RESPONSE: Building and sending response ===" << std::endl;
	
	std::ostringstream status_stream;
	status_stream << status_code;
	std::string code_str = status_stream.str();
	std::string reason_phrase = what_reason(status_code);  
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