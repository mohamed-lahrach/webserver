#include "response.hpp"
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

Response::Response() : status_code(200), content("Welcome to My Web Server!"), file_stream(NULL), is_streaming_file(false)
{

	set_header("Content-Type", "text/html");
	set_header("Connection", "close");
}

Response::~Response()
{
	if (file_stream)
	{
		file_stream->close();
		delete file_stream;
		file_stream = NULL;
	}
}

void Response::set_code(int code)
{
	status_code = code;
}

void Response::set_content(const std::string &body_content)
{
	content = body_content;
}

void Response::set_header(const std::string &key, const std::string &value)
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
	case LENGTH_REQUIRED:
		set_code(411);
		set_content("<html><body><h1>411 Length Required</h1><p>The request did not specify the Content-Length header.</p></body></html>");
		break;
	case PAYLOAD_TOO_LARGE:
		set_code(413);
		set_content("<html><body><h1>413 Payload Too Large</h1><p>The request entity is too large.</p></body></html>");
		break;
	default:
		set_code(500);
		set_content("<html><body><h1>500 Internal Server Error</h1><p>An unexpected error occurred.</p></body></html>");
		break;
	}
	set_header("Content-Type", "text/html");
	set_header("Connection", "close");
}


bool Response::handle_return_directive(const std::string &return_dir)
{
	if (return_dir.empty())
		return false;
	std::string url = return_dir;
	set_code(302);
	set_header("Location", url);
	set_header("Content-Type", "text/html");

	return true;
}

std::string Response::what_reason(int code)
{
	switch (code)
	{
	case 200:
		return "OK";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 303:
		return "See Other";
	case 307:
		return "Temporary Redirect";
	case 308:
		return "Permanent Redirect";
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
		return "Unknown Status Code";
	}
}


void Response::check_file(const std::string &file_path)
{
	std::ifstream file(file_path.c_str());
	if (file.is_open())
	{
		file.close();
		current_file_path = file_path;
		set_code(200);
	}
	else
	{
		set_code(403);
		set_content("<html><body><h1>403 Forbidden</h1><p>Access to this file is forbidden.</p></body></html>");
		set_header("Content-Type", "text/html");
		std::cout << "File exists but cannot be opened (403 Forbidden)" << std::endl;
	}
}

void Response::start_file_streaming(int client_fd)
{
	struct stat file_stat;
	if (stat(current_file_path.c_str(), &file_stat) != 0)
	{
		std::cout << "Error: Cannot stat file" << std::endl;
		set_code(500);
		return;
	}

	std::cout << "File size: " << file_stat.st_size << " bytes" << std::endl;

	std::stringstream response;
	response << "HTTP/1.0 200 OK\r\n";
	response << "Content-Type: " << mine_type.get_mime_type(current_file_path) << "\r\n";
	response << "Content-Length: " << file_stat.st_size << "\r\n";
	response << "Connection: close\r\n\r\n";

	std::string headers = response.str();
	std::cout << "Sending headers " << std::endl;
	send(client_fd, headers.c_str(), headers.length(), 0);

	file_stream = new std::ifstream(current_file_path.c_str(), std::ios::binary);
	if (!file_stream->is_open())
	{
		std::cout << "ERROR: Cannot open file for streaming: " << current_file_path << std::endl;
		delete file_stream;
		file_stream = NULL;
		set_code(500);
		current_file_path.clear(); 
		return;
	}

	std::cout << "File opened successfully for streaming. Stream good: " << file_stream->good() << std::endl;
	is_streaming_file = true;
}

void Response::continue_file_streaming(int client_fd)
{
	if (!file_stream || !file_stream->is_open())
	{
		std::cout << "ERROR: File stream is not open - finishing streaming" << std::endl;
		finish_file_streaming();
		return;
	}

	std::cout << "Reading from file stream" << std::endl;
	file_stream->read(file_buffer, sizeof(file_buffer));
	std::streamsize bytes_read = file_stream->gcount();

	std::cout << "Read " << bytes_read << " bytes from file" << std::endl;

	if (bytes_read > 0)
	{
		ssize_t bytes_sent = send(client_fd, file_buffer, bytes_read, 0);
		std::cout << "Sent " << bytes_sent << " bytes to client" << std::endl;

		if (bytes_sent == -1)
		{
			std::cout << "Client disconnected  " << std::endl;
			finish_file_streaming();
			return;
		}

		if (file_stream->eof())
		{
			std::cout << "File streaming completed " << std::endl;
			finish_file_streaming();
		}
	}
	else
	{
		finish_file_streaming();
	}
}
void Response::finish_file_streaming()
{
	if (file_stream)
	{
		file_stream->close();
		delete file_stream;
		file_stream = NULL;
	}
	is_streaming_file = false;
	current_file_path.clear();
}

bool Response::is_still_streaming() const
{
	return is_streaming_file;
}

std::string Response::list_dir(const std::string &path, const std::string &request_path)
{
	std::stringstream html;

	html << "<html><body><h1>Directory list</h1><ul>";

	DIR *dir = opendir(path.c_str());
	if (dir == NULL)
	{
		html << "<li>Error: Could not open directory</li>";
		set_code(403);
	}
	else
	{
		struct dirent *item;
		while ((item = readdir(dir)) != NULL)
		{
			if (std::string(item->d_name) == "." || std::string(item->d_name) == "..")
				continue;
			std::string url;
			if (request_path[request_path.length() - 1] == '/')
				url = request_path + item->d_name;
			else
				url = request_path + "/" + item->d_name;

			if (item->d_type == DT_REG)
				html << "<li><a href=\"" << url << "\">" << item->d_name << "</a> File</li>";
			else if (item->d_type == DT_DIR)
				html << "<li><a href=\"" << url << "/\">" << item->d_name << "/</a> Directory</li>";
		}
		closedir(dir);
		set_code(200);
	}

	html << "</ul></body></html>";
	return html.str();
}

void Response::handle_directory_listing(const std::string &file_path, const std::string &path, LocationContext *location_config)
{
	if (location_config && location_config->autoindex == "on")
	{
		std::cout << "Generating directory listing" << std::endl;
		std::string dir_listing = list_dir(file_path, path);
		set_content(dir_listing);
		set_header("Content-Type", "text/html");
	}
	else
	{
		std::cout << "Directory access forbidden" << std::endl;
		set_code(403);
		set_content("<html><body><h1>403 Forbidden</h1><p>Directory access is forbidden.</p></body></html>");
		set_header("Content-Type", "text/html");
	}
}

void Response::analyze_request_and_set_response(const std::string &path, LocationContext *location_config)
{
	if (!location_config->returnDirective.empty())
	{
		if (handle_return_directive(location_config->returnDirective))
			return;
	}

	std::string file_path = location_config->root + path;
	std::cout << "=== ANALYZING REQUEST PATH: " << file_path << " ===" << std::endl;
	struct stat s;
	if (stat(file_path.c_str(), &s) == 0)
	{
		if (s.st_mode & S_IFDIR)
		{
			std::vector<std::string> index_files;
			if (location_config && !location_config->indexes.empty())
			{
				index_files = location_config->indexes;

				bool index_found = false;
				for (std::vector<std::string>::iterator index_it = index_files.begin(); index_it != index_files.end(); ++index_it)
				{
					std::string index_path = file_path;
					if (index_path[index_path.length() - 1] != '/')
						index_path += "/";
					index_path += *index_it;

					std::ifstream index_file(index_path.c_str());

					if (index_file.is_open())
					{
						std::cout << "Found index file: " << *index_it << std::endl;
						check_file(index_path);
						index_found = true;
						break;
					}
				}

				if (!index_found)
				{
					handle_directory_listing(file_path, path, location_config);
				}
			}
			else
			{
				handle_directory_listing(file_path, path, location_config);
			}
		}
		else
		{
			check_file(file_path);
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
	std::cout << "-----------------RESPONSE---------------------" << std::endl;
	if (status_code == 200 && !current_file_path.empty())
	{
		if (!is_streaming_file)
		{
			std::cout << "Starting file streaming for: " << current_file_path << std::endl;
			start_file_streaming(client_fd);
		}
		else
		{
			std::cout << "Continuing file streaming..." << std::endl;
			continue_file_streaming(client_fd);
		}
	}
	else
	{
		std::stringstream status_stream;
		status_stream << status_code;
		std::string code_str = status_stream.str();
		std::string reason_phrase = what_reason(status_code);
		std::string status_line = "HTTP/1.0 " + code_str + " " + reason_phrase + "\r\n";
		std::string headers_line;

		std::map<std::string, std::string>::iterator ite = headers.begin();
		while (ite != headers.end())
		{
			headers_line += ite->first + ": " + ite->second + "\r\n";
			ite++;
		}

		std::stringstream content_length;
		content_length << content.length();
		headers_line += "Content-Length: " + content_length.str() + "\r\n";
		headers_line += "\r\n";

		std::string full_response = status_line + headers_line + content;

		std::cout << "Sending response to client " << client_fd << std::endl;
		send(client_fd, full_response.c_str(), full_response.length(), 0);
	}
}