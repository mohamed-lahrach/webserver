#include "delete_handler.hpp"
#include "request_status.hpp"
#include <iostream>

DeleteHandler::DeleteHandler()
{

}

DeleteHandler::~DeleteHandler()
{
}

RequestStatus DeleteHandler::handle_delete_request(
    const std::string& requested_path,
    const std::map<std::string, std::string>& http_headers)
{
    std::cout << "=== DELETE HANDLER ===" << std::endl;
    std::cout << "Client wants to delete: " << requested_path << std::endl;
(void)requested_path;
(void)http_headers;
    return HEADERS_ARE_READY;
}



