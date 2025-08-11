#include "post_handler.hpp"
#include "request_status.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
PostHandler::PostHandler()
{
}

PostHandler::~PostHandler()
{
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
void PostHandler::parse_multipart_data(const std::string &body, const std::string &content_type)
{
	(void)content_type;
	std::cout << "Parsing multipart data..." << std::endl;

	std::string boundary = extract_boundary(content_type);
	std::cout << "Extracted boundary: " << boundary << std::endl;
	save_request_body("post_body_form_data.png", body);
}

void PostHandler::parse_body_for_each_type(const std::string &body, const std::map<std::string, std::string> &http_headers)
{
    if (http_headers.find("Content-Type") != http_headers.end())
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
	std::cout << "=== POST HANDLER ===" << std::endl;
	std::cout << "Processing POST to: " << requested_path << std::endl;
	std::cout << "Received body: " << incoming_data.size() << " bytes" << std::endl;
	std::cout << "Expected total: " << expected_body_size << " bytes" << std::endl;
	std::cout << "POST data content: [" << incoming_data << "]" << std::endl;
	if (incoming_data.size() < expected_body_size)
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
