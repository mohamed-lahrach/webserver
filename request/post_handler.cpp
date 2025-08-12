#include "post_handler.hpp"

void PostHandler::parse_data_body(const std::string &body, const std::string &content_type)
{
	(void)content_type;
	std::cout << "Parsing multipart data..." << std::endl;

	
	std::string boundary = extract_boundary(content_type);
	std::string file_name = extract_filename(body);
	std::cout << "Extracted boundary: " << boundary << std::endl;
	std::cout << "Extracted filename: " << file_name << std::endl;
	std::cout<<">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;

	// Find where file data starts (after headers)
    size_t start_position = body.find("\r\n\r\n");
    if (start_position == std::string::npos)
    {
        // Try \n\n if \r\n\r\n not found
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
	size_t end_position = body.find("--" + boundary);
	if (end_position == std::string::npos)
	{
		std::cout << "ERROR: Cannot find data end!" << std::endl;
		save_request_body("debug_error.txt", body);
		return;
	}
	std::string file_data = body.substr(start_position, end_position - start_position);
	save_request_body(file_name, file_data);
}

void parse_data_body_with_chuked()
{

}

void PostHandler::parse_type_body(const std::string &body, const std::map<std::string, std::string> &http_headers)
{
    if (http_headers.find("Content-Type") != http_headers.end())
    {
        std::string content_type = http_headers.at("Content-Type");
        if (content_type.find("multipart/form-data") != std::string::npos)
        {
			if(is_chunked)
			{
				std::cout << "Parsing as multipart/form-data + chunked" << std::endl;
				parse_data_body(chunk_buffer + body, content_type);
				chunk_buffer.clear(); // Clear the buffer after processing
				is_chunked = false; // Reset chunked state
			}
			else
			{
				std::cout << "Parsing as multipart/form-data" << std::endl;
				parse_data_body(body, content_type);
			}
        }
        else 
        {
            save_request_body("post_body.txt", body);
        }
    }
    else
    {
        std::cout << "No Content-Type header found - saving to file" << std::endl;
        save_request_body("post_body.txt", body);
    }
}
RequestStatus PostHandler::handle_post_request_with_chunked(const std::map<std::string, std::string> &http_headers,
	std::string &incoming_data, size_t expected_body_size , const ServerContext *cfg, const LocationContext *loc)
{
		(void)http_headers;
		(void)incoming_data;
		(void)expected_body_size;
		(void)cfg;
		(void)loc;
		char *buffer;
		size_t pos = incoming_data.find("\r\n");
		std::string size_str = incoming_data.substr(0, pos);
		size_t chunk_size = std::strtoul(size_str.c_str(), NULL, 16);
		//save_request_body("chunked_data.txt", incoming_data);
		return(EVERYTHING_IS_OK);
}
RequestStatus PostHandler::handle_post_request(const std::string &requested_path,
	const std::map<std::string, std::string> &http_headers,
	std::string &incoming_data, size_t expected_body_size , const ServerContext *cfg, const LocationContext *loc)
{
	(void)requested_path;
	std::cout << "Handling POST request for path: " << requested_path << std::endl;
	exit(0);
	(void)loc;

	if (http_headers.find("Transfer-Encoding") != http_headers.end() &&
	http_headers.at("Transfer-Encoding") == "chunked\r")
	{
		return handle_post_request_with_chunked(http_headers, incoming_data, expected_body_size, cfg, loc);
	}
	else if (incoming_data.size() < expected_body_size)
	{
		std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
		return (BODY_BEING_READ);
	}
	std::cout << "Received full POST body data!" << std::endl;
	if (cfg->clientMaxBodySize != "0" && incoming_data.size() > parse_max_body_size(cfg->clientMaxBodySize))
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
