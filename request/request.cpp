#include "request.hpp"
#include "get_handler.hpp"
#include "post_handler.hpp" 
#include "delete_handler.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>


Request::Request() : http_method(""), requested_path(""), http_version(""), got_all_headers(false), expected_body_size(0), request_body(""), get_handler(), post_handler(), delete_handler()
{
	std::cout << "Creating a new HTTP request parser with modular handlers" << std::endl;
}


Request::~Request()
{
}

// bool Request::handle_request(int client_fd, const char *request_data)
// {
// 	(void)client_fd;

// 	std::string request_str = request_data;
// 	std::istringstream iss(request_str);
// 	iss >> http_method >> requested_path >> http_version;
// 	std::string header;
// 	while (std::getline(iss, header) && header != "")
// 	{
// 		std::string key, value;
// 		size_t pos = header.find(":");
// 		if (pos != std::string::npos)
// 		{
// 			key = header.substr(0, pos);
// 			value = header.substr(pos + 1);
// 			http_headers[key] = value;
// 		}
// 	}

// 	std::cout << "Method: " << http_method << std::endl;
// 	std::cout << "Path: " << requested_path << std::endl;
// 	std::cout << "Version: " << http_version << std::endl;
// 	std::cout << "Headers:" << std::endl;
// 	std::map<std::string, std::string>::iterator it = http_headers.begin();
// 	while (it != http_headers.end())
// 	{
// 		std::cout << it->first << ": " << it->second << std::endl;
// 		++it;
// 	}

// 	return (true);
// }

RequestStatus Request::add_new_data(const char *new_data, size_t data_size)
{
	std::cout << "=== GOT " << data_size << " NEW BYTES FROM CLIENT ===" << std::endl;


	incoming_data.append(new_data, data_size);
	std::cout << "Total data we have now: " << incoming_data.size() << " bytes" << std::endl;

	if (!got_all_headers)
	{
		size_t headers_end_position = incoming_data.find("\r\n\r\n");
		if (headers_end_position == std::string::npos)
		{
			std::cout << "Headers are not complete yet - waiting for more data" << std::endl;
			return NEED_MORE_DATA;
		}

		std::cout << "Found all headers! Now reading them..." << std::endl;

		std::string just_the_headers = incoming_data.substr(0, headers_end_position);

		if (!parse_http_headers(just_the_headers))
		{
			std::cout << "Something went wrong reading the headers!" << std::endl;
			return SOMETHING_WENT_WRONG;
		}

		got_all_headers = true;

		incoming_data = incoming_data.substr(headers_end_position + 4); 

		std::cout << "Successfully read all headers!" << std::endl;
		std::cout << "HTTP Method: " << http_method << ", Requested Path: " 
		<< requested_path << ", Version: " << http_version << std::endl;

		if (http_method == "POST" || http_method == "PUT")
		{
			if (http_headers.find("Content-Length") != http_headers.end())
			{
				expected_body_size = std::atoi(http_headers["Content-Length"].c_str());
				std::cout << "This request should have a body with " << expected_body_size << " bytes" << std::endl;
			}
		}

		return HEADERS_ARE_READY;
	}

	return HEADERS_ARE_READY;
}

bool Request::parse_http_headers(const std::string &header_text)
{
	std::istringstream header_stream(header_text);
	std::string current_line;

	if (!std::getline(header_stream, current_line))
		return false;

	std::istringstream first_line_parts(current_line);
	if (!(first_line_parts >> http_method >> requested_path >> http_version))
	{
		return false;
	}
	while (std::getline(header_stream, current_line))
	{

		if (current_line.empty())
			continue;
		size_t colon_position = current_line.find(':');
		std::string header_name = current_line.substr(0, colon_position);
		std::string header_value = current_line.substr(colon_position + 1);

		while (!header_value.empty() && (header_value[0] == ' ' || header_value[0] == '\t'))
		{
			header_value = header_value.substr(1);
		}
		while (!header_value.empty() && (header_value[header_value.length() - 1] == ' ' || header_value[header_value.length() - 1] == '\t'))
		{
			header_value = header_value.substr(0, header_value.length() - 1);
		}

		http_headers[header_name] = header_value;
		std::cout << "Found header: " << header_name << " = " << header_value << std::endl;
	}

	return true;
}

RequestStatus Request::figure_out_http_method()
{

	if (http_method == "GET")
	{
		return get_handler.handle_get_request(requested_path);
	}
	else if (http_method == "POST")
	{
		return post_handler.handle_post_request(requested_path, http_headers, incoming_data, expected_body_size);
	}
	else if (http_method == "DELETE")
	{
		return delete_handler.handle_delete_request(requested_path, http_headers);
	}
	else
	{
		std::cout << "Don't know how to handle method: " << http_method << std::endl;
		return SOMETHING_WENT_WRONG; 
	}
}
