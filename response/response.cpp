#include "response.hpp"
#include <sstream>
#include <iostream>   
#include <cstring>    
#include <sys/socket.h>
#include <sys/epoll.h>  
#include <unistd.h>    
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>

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

void Response::set_error_response(RequestStatus status)
{
	switch (status)
	{
		case BAD_REQUEST:
			set_code(400);
			set_content("<html><body><h1>400 Bad Request</h1><p>The request could not be understood by the server.</p></body></html>");
			break;
		case FORBIDDEN:
			set_code(403);
			set_content("<html><body><h1>403 Forbidden</h1><p>Access to this resource is forbidden.</p></body></html>");
			break;
		case NOT_FOUND:
			set_code(404);
			set_content("<html><body><h1>404 Not Found</h1><p>The requested resource was not found.</p></body></html>");
			break;
		case METHOD_NOT_ALLOWED:
			set_code(405);
			set_content("<html><body><h1>405 Method Not Allowed</h1><p>The request method is not supported for this resource.</p></body></html>");
			break;
		case PAYLOAD_TOO_LARGE:
			set_code(413);
			set_content("<html><body><h1>413 Payload Too Large</h1><p>The request entity is too large.</p></body></html>");
			break;
		case INTERNAL_ERROR:
			set_code(500);
			set_content("<html><body><h1>500 Internal Server Error</h1><p>The server encountered an internal error.</p></body></html>");
			break;
		default:
			set_code(500);
			set_content("<html><body><h1>500 Internal Server Error</h1><p>An unexpected error occurred.</p></body></html>");
			break;
	}
	set_header("Content-Type", "text/html");
	set_header("Connection", "close");
}


std::string Response::what_reason(int code)
{
	switch (code)
	{
	case 200:
		return "OK";
	case 400:
		return "Bad Request";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 413:
		return "Payload Too Large";
	case 500:
		return "Internal Server Error";
	default:
		return "Unknown";
	}
}

std::string Response::list_dir(const std::string& path, const std::string& request_path)
{
	std::stringstream html;
	
	html << "<html><body><h1>Directory list</h1><ul>";
	
	DIR* dir = opendir(path.c_str());
	if (dir == NULL) {
		html << "<li>Error: Could not open directory</li>";
		set_code(403);
	} else {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
				continue;
			}

			std::string fullPath = path + "/" + entry->d_name;
			struct stat fileStat;

			if (stat(fullPath.c_str(), &fileStat) == 0) {

				std::string url = request_path + "/" + entry->d_name;
				if (S_ISREG(fileStat.st_mode)) {
					html << "<li><a href=\"" << url << "\">" << entry->d_name << "</a> (File)</li>";
				} else if (S_ISDIR(fileStat.st_mode)) {
					html << "<li><a href=\"" << url << "/\">" << entry->d_name << "/</a> (Directory)</li>";
				}
			}
		}
		closedir(dir);
		set_code(200);
	}
	
	html << "</ul></body></html>";
	return html.str();
}

void Response::check_upload_file(const std::string& file_path)
{
			std::ifstream file(file_path.c_str());
			if (file.is_open())
			{
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string file_content = buffer.str();
				file.close();
				
				std::string mime_type = mime_detector.get_mime_type(file_path);
				
				set_code(200);
				set_content(file_content);
				set_header("Content-Type", mime_type);
			}
			else
			{
				set_code(403);
				set_content("<html><body><h1>403 Forbidden</h1><p>Access to this file is forbidden.</p></body></html>");
				set_header("Content-Type", "text/html");
				std::cout << "File exists but cannot be opened (403 Forbidden)" << std::endl;
			}
}



void Response::analyze_request_and_set_response(const std::string& path)
{
	std::cout << "=== ANALYZING REQUEST PATH: " << path << " ===" << std::endl;
	
	std::string file_path = "www" + path;
	
	std::cout << "Trying to serve file: " << file_path << std::endl;
	
	struct stat s;
	if(stat(file_path.c_str(),&s)==0)
	{
		if(s.st_mode & S_IFDIR)
		{
			std::string index_path = file_path + "/index.html";
			std::ifstream index_file(index_path.c_str());
			
			if (index_file.is_open())
			{
				check_upload_file(index_path);
			}
			else
			{
				std::cout << "No index.html found, generating directory listing" << std::endl;
				std::string dir_listing = list_dir(file_path, path);
				
				
				set_content(dir_listing);
				set_header("Content-Type", "text/html");

			}
		}
		else
		{
			check_upload_file(file_path);
		}
	}
	else
	{
		set_code(404);
		set_content("<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>");
		set_header("Content-Type", "text/html");
		std::cout << "Path does not exist - returning 404 Not Found" << std::endl;
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
	send(client_fd, full_response.c_str(), full_response.length(), 0);

}