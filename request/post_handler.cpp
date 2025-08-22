#include "post_handler.hpp"

void PostHandler::parse_form_data(const std::string &body,
	const std::string &content_type, const LocationContext *loc,
	size_t expected_body_size)
{
	size_t	end_position;

	std::cout << "Parsing multipart data..." << std::endl;
	// Extract boundary once
	if (!boundary_found)
	{
		boundary = extract_boundary(content_type);
		boundary_found = true;
		std::cout << "Extracted boundary: " << boundary << std::endl;
	}
	// Extract filename once (usually in the headers of first chunk)
	if (!file_name_found)
	{
		file_name = extract_filename(body);
		file_name_found = true;
		std::cout << "Extracted filename: " << file_name << std::endl;
	}
	// Handle first chunk (headers + maybe start of file data)
	if (!data_start)
	{
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
			start_position += 2;
		}
		else
		{
			start_position += 4;
		}
		data_start = true;
		// handle case small data
		std::string closing_boundary = "--" + boundary;
		end_position = body.find(closing_boundary, start_position);
		if (end_position != std::string::npos)
		{
			// Cut CRLF before boundary
			if (end_position >= 2 && body.substr(end_position - 2, 2) == "\r\n")
				end_position -= 2;
			else if (end_position >= 1 && body[end_position - 1] == '\n')
				end_position -= 1;
			std::string file_data = body.substr(start_position, end_position
					- start_position);
			save_request_body(file_name, file_data, loc);
			return ;
		}
		// Write only after headers
		std::string chunk = body.substr(start_position);
		save_request_body(file_name, chunk, loc);
		return ;
	}
	// Handle last chunk (when total size matches expected)
	if (total_received_size == expected_body_size)
	{
		std::string boundary_marker = "--" + boundary;
		end_position = body.find(boundary_marker);
		if (end_position == std::string::npos)
		{
			std::cout << "ERROR: Cannot find data end!" << std::endl;
			save_request_body("debug_error.txt", body, loc);
			return ;
		}
		// Cut CRLF before boundary if exists
		if (end_position >= 2 && body.substr(end_position - 2, 2) == "\r\n")
			end_position -= 2;
		else if (end_position >= 1 && body[end_position - 1] == '\n')
			end_position -= 1;
		std::string final_chunk = body.substr(0, end_position);
		save_request_body(file_name, final_chunk, loc);
		return ;
	}
	// Handle middle chunks → write everything
	save_request_body(file_name, body, loc);
}

void PostHandler::parse_type_body(const std::string &body,
	const std::map<std::string, std::string> &http_headers,
	const LocationContext *loc, size_t expected_body_size)
{
	if (http_headers.find("content-type") != http_headers.end())
	{
		std::string content_type = http_headers.at("content-type");
		if (content_type.find("multipart/form-data") != std::string::npos)
		{
			std::cout << "Parsing as multipart/form-data" << std::endl;
			parse_form_data(body, content_type, loc, expected_body_size);
		}
		else
		{
			save_request_body("post_body_default.txt", body, loc);
		}
	}
	else
	{
		std::cout << "No Content-Type header found- saving to file" << std::endl;
		save_request_body("post_body_default.txt", body, loc);
	}
}

RequestStatus PostHandler::handle_post_request_with_chunked(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	const ServerContext *cfg, const LocationContext *loc)
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
				return (POSTED_SUCCESSFULLY);
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

RequestStatus PostHandler::handle_post_request(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	size_t expected_body_size, const ServerContext *cfg,
	const LocationContext *loc)
{
	size_t	start;
	size_t	end;

	if (http_headers.find("transfer-encoding") != http_headers.end())
	{
		std::string transfer_encoding = http_headers.at("transfer-encoding");
		start = transfer_encoding.find_first_not_of(" \t\r\n");
		end = transfer_encoding.find_last_not_of(" \t\r\n");
		if (start != std::string::npos && end != std::string::npos)
		{
			transfer_encoding = transfer_encoding.substr(start, end - start
					+ 1);
		}
		std::cout << "Found transfer-encoding: '" << transfer_encoding << "'" << std::endl;
		if (transfer_encoding == "chunked")
		{
			std::cout << "Using chunked transfer encoding!" << std::endl;
			return (handle_post_request_with_chunked(http_headers,
					incoming_data, cfg, loc));
		}
	}
	else if (!incoming_data.empty())
	{
		total_received_size += incoming_data.size();
		parse_type_body(incoming_data, http_headers, loc, expected_body_size);
		incoming_data.clear();
		if (total_received_size > parse_max_body_size(cfg->clientMaxBodySize))
		{
			std::cout << "ERROR: POST body size is too large!" << std::endl;
			return (PAYLOAD_TOO_LARGE);
		}
		if (total_received_size < expected_body_size)
		{
			std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
			return (BODY_BEING_READ);
		}
	}
	// print final size and expected size
	std::cout << "Total received size: " << total_received_size << std::endl;
	std::cout << "Expected body size: " << expected_body_size << std::endl;
	return (POSTED_SUCCESSFULLY);
}
