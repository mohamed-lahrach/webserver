#include "post_handler.hpp"

RequestStatus PostHandler::parse_form_data(const std::string &body,
	const std::string &content_type, const LocationContext *loc,
	size_t expected_body_size)
{
	size_t	end_position;
	RequestStatus status;
	std::cout << "Parsing multipart data..." << std::endl;
	// Extract boundary once
	if (!boundary_found)
	{
		boundary = extract_boundary(content_type);
		if (boundary.empty())
		{
			std::cout << "ERROR: Failed to extract boundary" << std::endl;
			return (BAD_REQUEST);
		}
		boundary_found = true;
		std::cout << "Extracted boundary: " << boundary << std::endl;
	}
	// Extract filename once (usually in the headers of first chunk)
	if (!file_name_found)
	{
		file_name = extract_filename(body);
		if (file_name.empty())
		{
			std::cout << "ERROR: Failed to extract filename" << std::endl;
			return (BAD_REQUEST);
		}
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
				status = save_request_body("debug_error.txt", body, loc);
				if(status != POSTED_SUCCESSFULLY)
					return status;
				return BAD_REQUEST;
			}
			start_position += 2;
		}
		else
		{
			start_position += 4;
		}
		data_start = true;
		// handle case small data
		std::string closing_boundary = "--" + boundary + "--";  // Try final boundary first
		end_position = body.find(closing_boundary, start_position);
		if (end_position == std::string::npos)
		{
			closing_boundary = "--" + boundary;  // Try regular boundary
			end_position = body.find(closing_boundary, start_position);
		}
		std::cout << "Looking for boundary: '" << closing_boundary << "'" << std::endl;
		if (end_position != std::string::npos)
		{
			// Cut CRLF before boundary
			if (end_position >= 2 && body.substr(end_position - 2, 2) == "\r\n")
				end_position -= 2;
			else if (end_position >= 1 && body[end_position - 1] == '\n')
				end_position -= 1;
			std::string file_data = body.substr(start_position, end_position
					- start_position);
			status = save_request_body(file_name, file_data, loc);
			if (status != POSTED_SUCCESSFULLY)
				return status;
			return POSTED_SUCCESSFULLY;
		}
		// Write only after headers
		std::string chunk = body.substr(start_position);
		status = save_request_body(file_name, chunk, loc);
		if (status != POSTED_SUCCESSFULLY)
			return status;
		return POSTED_SUCCESSFULLY;
	}
	// Handle last chunk (when total size matches expected)
	if (total_received_size == expected_body_size)
	{
		std::string boundary_marker = "--" + boundary + "--";  // Final boundary has -- at the end
		end_position = body.find(boundary_marker);
		std::cout << "Final chunk detected." << std::endl;
		std::cout << "Looking for final boundary: '" << boundary_marker << "'" << std::endl;
		std::cout << "End position: " << end_position << std::endl;
		
		// If final boundary not found, try regular boundary
		if (end_position == std::string::npos)
		{
			boundary_marker = "--" + boundary;
			end_position = body.find(boundary_marker);
			std::cout << "Trying regular boundary: '" << boundary_marker << "'" << std::endl;
			std::cout << "End position with regular boundary: " << end_position << std::endl;
		}
		
		if (end_position == std::string::npos)
		{
			std::cout << "ERROR: Cannot find data end!" << std::endl;
			std::cout << "Body content (first 200 chars): " << body.substr(0, 200) << std::endl;
			std::cout << "Body size: " << body.size() << std::endl;
			status = save_request_body("debug_error.txt", body, loc);
			if (status != POSTED_SUCCESSFULLY)
				return status;
			return BAD_REQUEST;
		}
		// Cut CRLF before boundary if exists
		if (end_position >= 2 && body.substr(end_position - 2, 2) == "\r\n")
			end_position -= 2;
		else if (end_position >= 1 && body[end_position - 1] == '\n')
			end_position -= 1;
		std::string final_chunk = body.substr(0, end_position);
		status = save_request_body(file_name, final_chunk, loc);
		if (status != POSTED_SUCCESSFULLY)
			return status;
		return POSTED_SUCCESSFULLY;
	}
	// Handle middle chunks → write everything
	status = save_request_body(file_name, body, loc);
	if (status != POSTED_SUCCESSFULLY)
		return status;
	return POSTED_SUCCESSFULLY;
}

RequestStatus PostHandler::parse_type_body(const std::string &body,
	const std::map<std::string, std::string> &http_headers,
	const LocationContext *loc, size_t expected_body_size)
{
	RequestStatus status;
	if (http_headers.find("content-type") != http_headers.end())
	{
		std::string content_type = http_headers.at("content-type");
		if (content_type.find("multipart/form-data") != std::string::npos)
		{
			std::cout << "Parsing as multipart/form-data" << std::endl;
			status = parse_form_data(body, content_type, loc, expected_body_size);
		}
		else
		{
			status = save_request_body("post_body_default.txt", body, loc);
		}
	}
	else
	{
		std::cout << "No Content-Type header found- saving to file" << std::endl;
		status = save_request_body("post_body_default.txt", body, loc);
	}
	return status;
}

RequestStatus PostHandler::handle_post_request_with_chunked(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	const ServerContext *cfg, const LocationContext *loc)
{
	size_t	processed_pos;
	size_t	crlf_pos;
	RequestStatus status;
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
					remove_file_data(file_path);
					return (PAYLOAD_TOO_LARGE);
				}
				status = parse_type_body(chunk_body_parser, http_headers, loc);
				buffer_not_parser.clear();
				chunk_body_parser.clear();
				if (status != POSTED_SUCCESSFULLY)
					return status;
				return (POSTED_SUCCESSFULLY);
			}
		}
		// Check if we have enough chunk data + CRLF
		if (buffer_not_parser.size() - processed_pos < chunk_size + 2)
			break ; // not enough data yet
		// append chunk to file directly
		total_received_size += chunk_size;
		std::string chunk_data = buffer_not_parser.substr(processed_pos, chunk_size);
		if(total_received_size > parse_max_body_size(cfg->clientMaxBodySize))
		{
			std::cout << "ERROR: POST body size is too large!" << std::endl;
			remove_file_data(file_path);
			return (PAYLOAD_TOO_LARGE);
		}
		status = parse_type_body(chunk_data, http_headers, loc);
		if (status != POSTED_SUCCESSFULLY)
			return status;
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
	const LocationContext *loc, const std::string &requested_path)
{
	size_t	start;
	size_t	end;
	
	// Check if this is a CGI request first
	if (is_cgi_request(loc, requested_path))
	{
		std::cout << "=== CGI POST REQUEST DETECTED ===" << std::endl;
		std::cout << "CGI Extension: " << loc->cgiExtension << std::endl;
		std::cout << "CGI Path: " << loc->cgiPath << std::endl;
		
		// Handle CGI POST body data collection - save directly to file
		if (http_headers.find("transfer-encoding") != http_headers.end())
		{
			std::string transfer_encoding = http_headers.at("transfer-encoding");
			start = transfer_encoding.find_first_not_of(" \t\r\n");
			end = transfer_encoding.find_last_not_of(" \t\r\n");
			if (start != std::string::npos && end != std::string::npos)
			{
				transfer_encoding = transfer_encoding.substr(start, end - start + 1);
			}
			
			if (transfer_encoding == "chunked")
			{
				std::cout << "CGI POST: Using chunked transfer encoding" << std::endl;
				return handle_cgi_chunked_post(http_headers, incoming_data, cfg, loc);
			}
		}
		
		// Handle Content-Length based CGI POST - save directly to file
		if (expected_body_size > 0)
		{
			total_received_size += incoming_data.size();
			RequestStatus status = save_cgi_body_with_filename(incoming_data, http_headers);
			incoming_data.clear();
			
			if (status != POSTED_SUCCESSFULLY)
				return status;
				
			// Check body size limit
			if (total_received_size > parse_max_body_size(cfg->clientMaxBodySize))
			{
				std::cout << "ERROR: CGI POST body size too large!" << std::endl;
				// Remove the CGI file on error
				std::string filename = cgi_filename.empty() ? "cgi_post_data.txt" : cgi_filename;
				remove(("/tmp/" + filename).c_str());
				return PAYLOAD_TOO_LARGE;
			}
			
			if (total_received_size < expected_body_size)
			{
				std::cout << "CGI POST: Need more body data (" << total_received_size 
						  << "/" << expected_body_size << " bytes)" << std::endl;
				return BODY_BEING_READ;
			}
			
			std::cout << "CGI POST: Received complete body (" << total_received_size << " bytes)" << std::endl;
			return POSTED_SUCCESSFULLY;
		}
		
		// No body expected for CGI
		std::cout << "CGI POST: No body expected" << std::endl;
		return POSTED_SUCCESSFULLY;
	}
	
	// Regular POST handling (non-CGI)
	if(loc->uploadStore.empty())
	{
		std::cout << "No upload store configured, skipping file save." << std::endl;
		return (BAD_REQUEST);
	}
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
		RequestStatus status = parse_type_body(incoming_data, http_headers, loc, expected_body_size);
		incoming_data.clear();
		if(status != POSTED_SUCCESSFULLY)
			return status;
		if (total_received_size > parse_max_body_size(cfg->clientMaxBodySize))
		{
			std::cout << "ERROR: POST body size is too large!" << std::endl;
			remove_file_data(file_path);
			return (PAYLOAD_TOO_LARGE);
		}
		if (total_received_size < expected_body_size)
		{
			std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
			return (BODY_BEING_READ);
		}
	}
	std::cout << "Total received size: " << total_received_size << std::endl;
	std::cout << "Expected body size: " << expected_body_size << std::endl;
	return (POSTED_SUCCESSFULLY);
}


RequestStatus PostHandler::handle_cgi_chunked_post(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	const ServerContext *cfg, const LocationContext *loc)
{
	(void)http_headers; // Suppress unused parameter warning
	(void)loc; // Suppress unused parameter warning
	
	std::cout << "=== CGI CHUNKED POST HANDLER ===" << std::endl;
	std::cout << "Incoming data size: " << incoming_data.size() << std::endl;
	
	buffer_not_parser += incoming_data;
	incoming_data.clear();
	
	size_t processed_pos = 0;
	
	while (true)
	{
		if (chunk_size == 0)
		{
			// Look for chunk size line
			size_t crlf_pos = buffer_not_parser.find("\r\n", processed_pos);
			if (crlf_pos == std::string::npos)
				break; // need more data
				
			std::string size_str = buffer_not_parser.substr(processed_pos, crlf_pos - processed_pos);
			chunk_size = std::strtoul(size_str.c_str(), NULL, 16);
			processed_pos = crlf_pos + 2;
			
			if (chunk_size == 0)
			{
				// End of chunks
				std::cout << "CGI POST: Received complete chunked body (" << total_received_size << " bytes)" << std::endl;
				buffer_not_parser.clear();
				return POSTED_SUCCESSFULLY;
			}
		}
		
		// Check if we have enough chunk data + CRLF
		if (buffer_not_parser.size() - processed_pos < chunk_size + 2)
			break; // not enough data yet
			
		// Extract chunk data and save directly to file
		std::string chunk_data = buffer_not_parser.substr(processed_pos, chunk_size);
		total_received_size += chunk_size;
		
		RequestStatus status = save_cgi_body_with_filename(chunk_data, http_headers);
		if (status != POSTED_SUCCESSFULLY)
			return status;
		
		// Check body size limit
		if (total_received_size > parse_max_body_size(cfg->clientMaxBodySize))
		{
			std::cout << "ERROR: CGI POST chunked body size too large!" << std::endl;
			std::string filename = cgi_filename.empty() ? "cgi_post_data.txt" : cgi_filename;
			remove(("/tmp/" + filename).c_str());
			buffer_not_parser.clear();
			return PAYLOAD_TOO_LARGE;
		}
		
		processed_pos += chunk_size + 2;
		chunk_size = 0;
	}
	
	// Remove processed part from buffer
	if (processed_pos > 0)
		buffer_not_parser.erase(0, processed_pos);
		
	return BODY_BEING_READ;
}

