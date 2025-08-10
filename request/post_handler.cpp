#include "post_handler.hpp"
#include "request_status.hpp"
#include <iostream>

PostHandler::PostHandler()
{
}

PostHandler::~PostHandler()
{
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
	(void)requested_path;
	if (incoming_data.size() < expected_body_size)
	{
		std::cout << "⏳ WAITING FOR MORE POST BODY DATA..." << std::endl;
		std::cout << "Progress: " << incoming_data.size() << "/" << expected_body_size << " bytes" << std::endl;
		std::cout << "Missing: " << (expected_body_size
			- incoming_data.size()) << " bytes" << std::endl;
		return (BODY_BEING_READ);
	}
	// We have complete body - process it
	std::cout << "-----------------------" << std::endl;
    std::cout << "-----------------------" << std::endl;
	std::cout << "✅ COMPLETE POST BODY RECEIVED!" << std::endl;
	std::cout << "Final POST data: [" << incoming_data << "]" << std::endl;
	// Print all HTTP headers
	std::cout << "=== HTTP HEADERS ===" << std::endl;
	std::cout << "Total headers: " << http_headers.size() << std::endl;
	for (std::map<std::string,
		std::string>::const_iterator it = http_headers.begin(); it != http_headers.end(); ++it)
	{
		std::cout << "Header: " << it->first << "= [" << it->second << "]" << std::endl;
	}
	std::cout << "-----------------------" << std::endl;
    std::cout << "-----------------------" << std::endl;
	return (EVERYTHING_IS_OK); // Now we can process
}
