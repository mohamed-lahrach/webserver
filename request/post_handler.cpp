#include "post_handler.hpp"
#include "request_status.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
PostHandler::PostHandler()
{
	is_chunked = false; // Initialize chunked transfer encoding flag
	std::cout << "PostHandler initialized." << std::endl;
}

PostHandler::~PostHandler()
{
	std::cout << "PostHandler destroyed." << std::endl;
}
void PostHandler::save_request_body(const std::string &filename, const std::string &body)

{
	std::ofstream file(filename.c_str(), std::ios::binary); // binary mode for any data
	if (!file)
	{
		throw std::runtime_error("Could not open file for writing");
	}
	file.write(body.data(), body.size());
	file.close();
}
std::string PostHandler::extract_boundary(const std::string &content_type)
{
	size_t pos = content_type.find("boundary=");
	if (pos == std::string::npos)
	{
		throw std::runtime_error("Boundary not found in Content-Type header");
	}
	return content_type.substr(pos + 9); // 9 is the length of "boundary="
}
std::string PostHandler::extract_filename(const std::string &body)
{
	size_t pos = body.find("filename=\"");
	if (pos == std::string::npos)
	{
		throw std::runtime_error("Filename not found in body");
	}
	pos += 10;
	size_t end_pos = body.find("\"", pos);
	if (end_pos == std::string::npos)
	{
		throw std::runtime_error("End quote for filename not found");
	}
	return body.substr(pos, end_pos - pos);
}
void PostHandler::parse_multipart_data(const std::string &body, const std::string &content_type)
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

void PostHandler::parse_body_for_each_type(const std::string &body, const std::map<std::string, std::string> &http_headers)
{
	if(is_chunked)
	{
		std::cout << "Handling chunked transfer encoding..." << std::endl;
		//save_request_body("chunked_post_body.txt", body);
		std::cout << "Chunked data saved to chunked_post_body.txt" << std::endl;
		is_chunked = false; // Reset after handling
	}
    else if (http_headers.find("Content-Type") != http_headers.end())
    {
        std::string content_type = http_headers.at("Content-Type");
        if (content_type.find("multipart/form-data") != std::string::npos)
        {
            std::cout << "Parsing as multipart/form-data" << std::endl;
            parse_multipart_data(body, content_type);
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

RequestStatus PostHandler::handle_post_request(const std::string &requested_path,
	const std::map<std::string, std::string> &http_headers,
	std::string &incoming_data, size_t expected_body_size)
{
	// check if a have chunked transfer encoding
	(void)requested_path; // Unused in this context
	if (http_headers.find("Transfer-Encoding") != http_headers.end() &&
		http_headers.at("Transfer-Encoding") == "chunked")
	{
		is_chunked = true;
		std::cout << "Received chunked transfer encoding POST request." << std::endl;

		// Handle chunked transfer encoding here if needed
		// For now, we will just treat it as a regular POST request
	}		
	else if (incoming_data.size() < expected_body_size)
	{
		std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
		std::cout << "Progress: " << incoming_data.size() << "/" << expected_body_size << " bytes" << std::endl;
		std::cout << "Missing: " << (expected_body_size
			- incoming_data.size()) << " bytes" << std::endl;
		return (BODY_BEING_READ);
	}
	std::cout << "Received full POST body data!" << std::endl;
	parse_body_for_each_type(incoming_data, http_headers);
	std::cout << "Request body parsed successfully." << std::endl;
	std::cout << "-----------------------" << std::endl;
	std::cout << "-----------------------" << std::endl;
	return (EVERYTHING_IS_OK); // Now we can process
}
