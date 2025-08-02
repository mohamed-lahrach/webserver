#include "get_handler.hpp"
#include "request_status.hpp"
#include <iostream>

GetHandler::GetHandler()
{

}

GetHandler::~GetHandler()
{
}

RequestStatus GetHandler::handle_get_request(
    const std::string& requested_path,
    const std::map<std::string, std::string>& http_headers)
{
    std::cout << "=== GET HANDLER ===" << std::endl;
    std::cout << "Client wants to get: " << requested_path << std::endl;
    (void)requested_path;
    (void)http_headers;
    return HEADERS_ARE_READY;
}
