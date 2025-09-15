#include "request.hpp"
#include "get_handler.hpp"
#include "post_handler.hpp"
#include "delete_handler.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include "../utils/utils.hpp"

Request::Request() : http_method(""), requested_path(""), http_version(""), got_all_headers(false), expected_body_size(0), request_body(""), config(0), location(0), get_handler(), post_handler(), delete_handler()
{
	std::cout << "Creating a new HTTP request parser with modular handlers" << std::endl;
}

Request::~Request()
{
}

std::string remove_spaces_and_lower(const std::string &str)
{

	size_t start = 0;
	size_t end = str.size();
	while (start < end && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r'))
		++start;
	while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
		--end;

	std::string lower_str = str.substr(start, end - start);
	for (size_t i = 0; i < lower_str.length(); ++i)
		lower_str[i] = tolower(lower_str[i]);
	return lower_str;
}

void Request::set_config(ServerContext &cfg)
{
	config = &cfg;
	std::cout << " config is up \n";
	if (!requested_path.empty())
	{
		location = match_location(requested_path);
		if (location)
			std::cout << " location found: " << location->path << "\n";
		else
			std::cout << " location not found for path: " << requested_path << "\n";
	}
}

LocationContext *Request::match_location(const std::string &resquested_path)
{
	if (!config)
		return 0;
	LocationContext *matched_location = 0;
	size_t longest_len = 0;

	std::cout << "Matching path '" << resquested_path << "' against locations:" << std::endl;
	std::vector<LocationContext>::iterator it_location = config->locations.begin();
	while (it_location != config->locations.end())
	{
		if (it_location->path[it_location->path.length() - 1] == '/' && it_location->path.length() > 1)
			it_location->path = it_location->path.substr(0, it_location->path.length() - 1);

	
		if (resquested_path.compare(0, it_location->path.length(), it_location->path) == 0)
		{
			if (it_location->path == "/" ||
				resquested_path.length() == it_location->path.length() ||
				(it_location->path[it_location->path.length() - 1] == '/') ||
				resquested_path[it_location->path.length()] == '/')
			{
				std::cout << "matched location: " << it_location->path << std::endl;
				if (it_location->path.size() > longest_len)
				{
					matched_location = &(*it_location);
					longest_len = it_location->path.size();
				}
			}
		}

		++it_location;
	}
	return matched_location;
}

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
			if (!check_for_valid_http_start())
			{
				std::cout << "Invalid HTTP request format detected" << std::endl;
				return BAD_REQUEST;
			}
			std::cout << "Headers are not complete yet - waiting for more data" << std::endl;
			return NEED_MORE_DATA;
		}

		std::cout << "Found all headers! Now reading them..." << std::endl;

		std::string just_the_headers = incoming_data.substr(0, headers_end_position);

		if (!parse_http_headers(just_the_headers))
		{
			std::cout << "Something went wrong reading the headers!" << std::endl;
			return BAD_REQUEST;
		}

		got_all_headers = true;

		incoming_data = incoming_data.substr(headers_end_position + 4);

		std::cout << "Successfully read all headers!" << std::endl;
		std::cout << "HTTP Method: " << http_method << ", Requested Path: "
				  << requested_path << ", Version: " << http_version << std::endl;

		if (http_method == "POST")
		{
			std::map<std::string, std::string>::iterator it_content_len = http_headers.find("content-length");
			std::map<std::string, std::string>::iterator it_transfer_enc = http_headers.find("transfer-encoding");
			if (it_content_len != http_headers.end())
			{
				expected_body_size = std::atoi(it_content_len->second.c_str());
				std::cout << "This request should have a body with " << expected_body_size << " bytes" << std::endl;
			}
			else if (it_transfer_enc != http_headers.end() &&
					 it_transfer_enc->second.find("chunked") != std::string::npos)
			{
				expected_body_size = 0;
				std::cout << "Using chunked transfer encoding - size unknown" << std::endl;
			}
			else
			{
				std::cout << "Missing Content-Length header and no chunked encoding" << std::endl;
				return LENGTH_REQUIRED;
			}
		}

		return HEADERS_ARE_READY;
	}

	return HEADERS_ARE_READY;
}
bool Request::check_for_valid_http_start()
{
	size_t first_line_end = incoming_data.find("\r\n");
	if (first_line_end == std::string::npos)
		return false;

	std::string first_line = incoming_data.substr(0, first_line_end);
	std::stringstream line_stream(first_line);
	std::string method, path, version;

	if (!(line_stream >> method >> path >> version))
	{
		std::cout << "error from stream \n";
		return false;
	}
	if (version != "HTTP/1.1" && version != "HTTP/1.0")
		return false;
	if (path[0] != '/')
		return false;
	if (method != "GET" && method != "POST" && method != "DELETE")
		return false;

	return true;
}

bool Request::parse_http_headers(const std::string &header_text)
{
	std::stringstream header_stream(header_text);
	std::string current_line;

	if (!std::getline(header_stream, current_line))
		return false;
	std::stringstream first_line_stream(current_line);
	if (!(first_line_stream >> http_method >> requested_path >> http_version))
		return false;

	bool host_found = false;

	size_t query_pos = requested_path.find('?');
	if (query_pos != std::string::npos)
	{
		query_string = requested_path.substr(query_pos + 1);
		requested_path = requested_path.substr(0, query_pos);
		query_params = parse_query_string(query_string);
	}
	else
	{
		query_string = "";
		query_params.clear();
	}

	requested_path = url_decode(requested_path);
	size_t pos = 0;
	while ((pos = requested_path.find("//", pos)) != std::string::npos)
		requested_path.erase(pos, 1);
	
	if (requested_path.empty())
		requested_path = "/";
	else if (requested_path[0] != '/')
		requested_path = "/" + requested_path;
	while (std::getline(header_stream, current_line))
	{

		size_t colon_position = current_line.find(':');
		if (colon_position == std::string::npos)
		{
			std::cout << "now key value in header: '" << current_line << "'" << std::endl;
			return false;
		}

		if (colon_position > 0 && (current_line[colon_position - 1] == ' ' || current_line[colon_position - 1] == '\t'))
		{
			std::cout << "Invalid header (whitespace before colon): '" << std::endl;
			return false;
		}
		std::string header_name = current_line.substr(0, colon_position);
		std::string header_value = current_line.substr(colon_position + 1);

		std::string lower_name = remove_spaces_and_lower(header_name);
		http_headers[lower_name] = header_value;
		if (lower_name == "host")
			host_found = true;
	}
	if (!host_found)
		return false;

	return true;
}

RequestStatus Request::figure_out_http_method()
{
	if (location && !location->returnDirective.empty())
	{
		std::cout << "Found return directive: " << location->returnDirective << std::endl;
		return EVERYTHING_IS_OK;
	}

	if (!location || location->root.empty())
	{
		std::cout << " location not found \n";
		return NOT_FOUND;
	}

	if (!location->allowedMethods.empty())
	{
		bool ok = false;
		for (std::vector<std::string>::iterator it_method = location->allowedMethods.begin(); it_method != location->allowedMethods.end(); ++it_method)
		{
			if (*it_method == http_method)
			{
				ok = true;
				break;
			}
		}
		if (!ok)
			return METHOD_NOT_ALLOWED;
	}
	if (!location->cgiExtensions.empty() && !location->cgiPaths.empty())
	{
	
		for (size_t i = 0; i < location->cgiExtensions.size(); ++i)
		{
			const std::string &ext = location->cgiExtensions[i];
			if (requested_path.size() >= ext.size())
			{
				std::string file_ext = requested_path.substr(requested_path.size() - ext.size());
				if (file_ext == ext)
				{
					std::cout<<"its a cgi request..\n";
					if (http_method == "POST")
					{
						return post_handler.handle_post_request(http_headers, incoming_data, expected_body_size, config, location, requested_path);
					}
					return EVERYTHING_IS_OK;
				}
			}
		}
	}
	
	std::string full_path = resolve_file_path(requested_path, location);
	if (http_method == "GET")
		return get_handler.handle_get_request(full_path);
	else if (http_method == "POST")
	{
		return post_handler.handle_post_request(http_headers, incoming_data, expected_body_size, config, location, requested_path);
	}
	else if (http_method == "DELETE")
		return delete_handler.handle_delete_request(full_path);
	else
		return METHOD_NOT_ALLOWED;
}

bool Request::is_cgi_request() const
{
	if (!location || location->cgiExtensions.empty() || location->cgiPaths.empty())
		return false;

	std::string path = requested_path;
	size_t query_pos = path.find('?');
	if (query_pos != std::string::npos)
		path = path.substr(0, query_pos);

	for (size_t i = 0; i < location->cgiExtensions.size(); ++i)
	{
		const std::string &ext = location->cgiExtensions[i];
		if (path.size() >= ext.size())
		{
			std::string file_ext = path.substr(path.size() - ext.size());
			if (file_ext == ext)
				return true;
		}
	}

	return false;
}

std::string Request::get_cgi_post_body() const
{
	return post_handler.get_cgi_body();
}