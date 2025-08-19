#include "post_handler.hpp"

void PostHandler::parse_form_data(const std::string &body,
	const std::string &content_type, const LocationContext *loc)
{
	size_t	start_position;
	size_t	end_position;
	size_t	data_end;

	std::cout << "Parsing multipart data..." << std::endl;
	std::string boundary = extract_boundary(content_type);
	std::string file_name = extract_filename(body);
	std::cout << "Extracted boundary: " << boundary << std::endl;
	std::cout << "Extracted filename: " << file_name << std::endl;
	// Find where file data starts (after headers)
	start_position = body.find("\r\n\r\n");
	if (start_position == std::string::npos)
	{
		start_position = body.find("\n\n");
		if (start_position == std::string::npos)
		{
			std::cout << "ERROR: Cannot find data start!" << std::endl;
			save_request_body("debug_error.txt", body, loc);
			return ;
		}
		start_position += 2; // skip \n\n
	}
	else
	{
		start_position += 4; // skip \r\n\r\n
	}
	std::string boundary_marker = "--" + boundary;
	end_position = body.find(boundary_marker, start_position);
	if (end_position == std::string::npos)
	{
		std::cout << "ERROR: Cannot find data end!" << std::endl;
		save_request_body("debug_error.txt", body, loc);
		return ;
	}
	data_end = end_position;
	// Check for \r\n before boundary and remove it
	if (data_end >= 2 && body.substr(data_end - 2, 2) == "\r\n")
	{
		data_end -= 2;
	}
	// Check for just \n before boundary and remove it
	else if (data_end >= 1 && body[data_end - 1] == '\n')
	{
		data_end -= 1;
	}
	std::string clean_data = body.substr(start_position, data_end
			- start_position);
	save_request_body(file_name, clean_data, loc);
}

void PostHandler::parse_type_body(const std::string &body,
	const std::map<std::string, std::string> &http_headers , const LocationContext *loc)
{
	if (http_headers.find("content-type") != http_headers.end())
	{
		std::string content_type = http_headers.at("content-type");
		if (content_type.find("multipart/form-data") != std::string::npos)
		{
			std::cout << "Parsing as multipart/form-data" << std::endl;
			parse_form_data(body, content_type , loc);
		}
		else
		{
			save_request_body("post_body_default.txt", body,loc);
		}
	}
	else
	{
		std::cout << "No Content-Type header found- saving to file" << std::endl;
		save_request_body("post_body_default.txt", body,loc);
	}
}

RequestStatus PostHandler::handle_post_request_with_chunked(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	const ServerContext *cfg,
	const LocationContext *loc)
{
	size_t	processed_pos;
	size_t	crlf_pos;

	std::cout << "=== CHUNKED HANDLER CALLED ===" << std::endl;
	std::cout << "Incoming data size: " << incoming_data.size() << std::endl;
	
	buffer_not_parser += incoming_data;
	incoming_data.clear();
	processed_pos = 0;
	while (true)
	{
		if (chunk_size == 0)
		{
			crlf_pos = buffer_not_parser.find("\r\n", processed_pos);
			if (crlf_pos == std::string::npos)
				break ; // need more data
			std::string size_str = buffer_not_parser.substr(processed_pos,
					crlf_pos - processed_pos);
			chunk_size = std::strtoul(size_str.c_str(), NULL, 16);
			processed_pos = crlf_pos + 2;
			if (chunk_size == 0)
			{
				if (parse_size(cfg, chunk_body_parser) == 0)
				{
					std::cout << "ERROR: POST body size is too large!" << std::endl;
					return (PAYLOAD_TOO_LARGE);
				}
				parse_type_body(chunk_body_parser, http_headers, loc);
				buffer_not_parser.clear();
				chunk_body_parser.clear();
				return (EVERYTHING_IS_OK);
			}
		}
		// Check if we have enough chunk data + CRLF
		if (buffer_not_parser.size() - processed_pos < chunk_size + 2)
			break ; // not enough data yet
		chunk_body_parser.append(buffer_not_parser, processed_pos, chunk_size);
		processed_pos += chunk_size + 2;
		chunk_size = 0;
	}
	// Remove processed part from parse buffer
	if (processed_pos > 0)
		buffer_not_parser.erase(0, processed_pos);
	return (BODY_BEING_READ);
}

RequestStatus PostHandler::handle_post_request(const std::string &requested_path,
	const std::map<std::string, std::string> &http_headers,
	std::string &incoming_data, size_t expected_body_size,
	const ServerContext *cfg, const LocationContext *loc)
{
	(void)requested_path;
	if (http_headers.find("transfer-encoding") != http_headers.end())
	{
		std::string transfer_encoding = http_headers.at("transfer-encoding");
		size_t start = transfer_encoding.find_first_not_of(" \t\r\n");
		size_t end = transfer_encoding.find_last_not_of(" \t\r\n");
		if (start != std::string::npos && end != std::string::npos)
		{
			transfer_encoding = transfer_encoding.substr(start, end - start + 1);
		}
		std::cout << "Found transfer-encoding: '" << transfer_encoding << "'" << std::endl;
		if (transfer_encoding == "chunked")
		{
			std::cout << "Using chunked transfer encoding!" << std::endl;
			return (handle_post_request_with_chunked(http_headers, incoming_data,
					cfg, loc));
		}
	}
	else if (incoming_data.size() < expected_body_size)
	{
		std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
		return (BODY_BEING_READ);
	}
	std::cout << "Received full POST body data!" << std::endl;
	if (parse_size(cfg, incoming_data) == 0)
	{
		std::cout << "ERROR: POST body size is too large!" << std::endl;
		return (PAYLOAD_TOO_LARGE);
	}
	parse_type_body(incoming_data, http_headers, loc);
	return (EVERYTHING_IS_OK);
}
