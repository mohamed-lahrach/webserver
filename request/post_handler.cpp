#include "post_handler.hpp"

void PostHandler::parse_data_body(const std::string &body, const std::string &content_type)
{
    std::cout << "Parsing multipart data..." << std::endl;
    
    std::string boundary = extract_boundary(content_type);
    std::string file_name = extract_filename(body);
    std::cout << "Extracted boundary: " << boundary << std::endl;
    std::cout << "Extracted filename: " << file_name << std::endl;
    
    // Find where file data starts (after headers)
    size_t start_position = body.find("\r\n\r\n");
    if (start_position == std::string::npos)
    {
        start_position = body.find("\n\n");
        if (start_position == std::string::npos)
        {
            std::cout << "ERROR: Cannot find data start!" << std::endl;
            save_request_body("debug_error.txt", body);
            return;
        }
        start_position += 2; // Skip \n\n
    }
    else
    {
        start_position += 4; // Skip \r\n\r\n
    }
    
    // Find the boundary that marks the end of data
    std::string boundary_marker = "--" + boundary;
    size_t end_position = body.find(boundary_marker, start_position);
    
    if (end_position == std::string::npos)
    {
        std::cout << "ERROR: Cannot find data end!" << std::endl;
        save_request_body("debug_error.txt", body);
        return;
    }
    
    // Extract ONLY the data between start and boundary
    // Remove any trailing \r\n before the boundary
    size_t data_end = end_position;
    
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
    
    std::string clean_data = body.substr(start_position, data_end - start_position);
    
    std::cout << "Extracted clean data (" << clean_data.length() << " bytes): '" << clean_data << "'" << std::endl;
    save_request_body(file_name, clean_data);
}

void PostHandler::parse_type_body(const std::string &body,
	const std::map<std::string, std::string> &http_headers)
{
	if (http_headers.find("Content-Type") != http_headers.end())
	{
		std::string content_type = http_headers.at("Content-Type");
		if (content_type.find("multipart/form-data") != std::string::npos)
		{
            std::cout << "Parsing as multipart/form-data" << std::endl;
			parse_data_body(body, content_type);
		}
		else
		{
			save_request_body("post_body.txt", body);
		}
	}
	else
	{
		std::cout << "No Content-Type header found- saving to file" << std::endl;
		save_request_body("post_body.txt", body);
	}
}

RequestStatus PostHandler::handle_post_request_with_chunked(const std::map<std::string,
	std::string> &http_headers, std::string &incoming_data,
	size_t expected_body_size, const ServerContext *cfg,
	const LocationContext *loc)
{
	size_t	processed_pos;
	size_t	crlf_pos;

	(void)expected_body_size;
	(void)cfg;
	(void)loc;
	// Append new data to parse buffer
	buffer_not_parser += incoming_data;
	incoming_data.clear();
	processed_pos = 0;
	while (true)
	{
		if (chunk_size == 0)
		{
			// Read chunk size line
			crlf_pos = buffer_not_parser.find("\r\n", processed_pos);
			if (crlf_pos == std::string::npos)
				break ; // need more data
			std::string size_str = buffer_not_parser.substr(processed_pos,
					crlf_pos - processed_pos);
			chunk_size = std::strtoul(size_str.c_str(), NULL, 16);
			processed_pos = crlf_pos + 2;
			if (chunk_size == 0)
			{
				// End of chunks
				parse_type_body(chunk_body_parser, http_headers);
				buffer_not_parser.clear();
				chunk_body_parser.clear();
				return (EVERYTHING_IS_OK);
			}
		}
		// Check if we have enough chunk data + CRLF
		if (buffer_not_parser.size() - processed_pos < chunk_size + 2)
			break ; // not enough data yet
		// Extract chunk data
		chunk_body_parser.append(buffer_not_parser, processed_pos, chunk_size);
		processed_pos += chunk_size + 2; // skip CRLF
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
	std::cout << "Handling POST request for path: " << requested_path << std::endl;
	(void)loc;
	if (http_headers.find("Transfer-Encoding") != http_headers.end()
		&& http_headers.at("Transfer-Encoding") == "chunked\r")
	{
		return (handle_post_request_with_chunked(http_headers, incoming_data,
				expected_body_size, cfg, loc));
	}
	else if (incoming_data.size() < expected_body_size)
	{
		std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
		return (BODY_BEING_READ);
	}
	std::cout << "Received full POST body data!" << std::endl;
	if (cfg->clientMaxBodySize != "0"
		&& incoming_data.size() > parse_max_body_size(cfg->clientMaxBodySize))
	{
		std::cout << "ERROR: POST body size is too large!" << std::endl;
		// print the size of the body
		std::cout << "Received body size: " << incoming_data.size() << std::endl;
		std::cout << "Max allowed body size: " << parse_max_body_size(cfg->clientMaxBodySize) << std::endl;
		return (PAYLOAD_TOO_LARGE);
	}
	parse_type_body(incoming_data, http_headers);
	std::cout << "-----------------------" << std::endl;
	std::cout << "-----------------------" << std::endl;
	return (EVERYTHING_IS_OK); // Now we can process
}
