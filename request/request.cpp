#include "request.hpp"
#include "get_handler.hpp"
#include "post_handler.hpp"
#include "delete_handler.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>

Request::Request() : http_method(""), requested_path(""), http_version(""), got_all_headers(false), expected_body_size(0), request_body(""), cfg_(0), loc_(0), get_handler(), post_handler(), delete_handler()
{
	std::cout << "Creating a new HTTP request parser with modular handlers" << std::endl;
}

Request::~Request()
{
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

void Request::set_config(const ServerContext &cfg)
{
	cfg_ = &cfg;
	if (!requested_path.empty())
		loc_ = match_location(requested_path);
}

const LocationContext *Request::match_location(const std::string &path) const
{
	if (!cfg_)
		return 0;
	const LocationContext *best = 0;
	size_t bestLen = 0;
	for (std::vector<LocationContext>::const_iterator it = cfg_->locations.begin(); it != cfg_->locations.end(); ++it)
	{
		const std::string &p = it->path;
		if (p.empty())
			continue;
		if (path.compare(0, p.size(), p) == 0)
		{
			if (p.size() > bestLen)
			{
				best = &(*it);
				bestLen = p.size();
			}
		}
	}
	return best;
}

RequestStatus Request::figure_out_http_method()
{
	if (!loc_->allowedMethods.empty())
	{
		bool ok = false;
		for (std::vector<std::string>::const_iterator it = loc_->allowedMethods.begin(); it != loc_->allowedMethods.end(); ++it)
		{
			if (*it == http_method)
			{
				ok = true;
				break;
			}
		}
		if (!ok)
			return METHOD_NOT_ALLOWED;
	}

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
		return METHOD_NOT_ALLOWED;
	}
}
