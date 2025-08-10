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
    // afiche rewquest path
    std::cout<<"path: "<<requested_path<<std::endl;
	save_request_body("post_body.txt", incoming_data);
	std::cout << "-----------------------" << std::endl;
	std::cout << "-----------------------" << std::endl;
    (void)http_headers; // Unused in this example, but can be used for further processing
	return (EVERYTHING_IS_OK); // Now we can process
}
