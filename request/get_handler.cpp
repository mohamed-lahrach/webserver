#include "get_handler.hpp"
#include "request_status.hpp"
#include <iostream>

GetHandler::GetHandler()
{

}

GetHandler::~GetHandler()
{
}
RequestStatus GetHandler::handle_get_request(const std::string& requested_path)
{
    std::cout << "=== GET HANDLER - COMPREHENSIVE VALIDATION ===" << std::endl;
    std::cout << "Requested path: '" << requested_path << "'" << std::endl;
    

    if (requested_path.empty()) {
        std::cout << "ERROR: Empty path provided" << std::endl;
        return BAD_REQUEST;
    }
    
    if (requested_path[0] != '/') {
        std::cout << "ERROR: Path must start with '/' - got: " << requested_path << std::endl;
        return BAD_REQUEST;
    }

    if (requested_path.length() > 2048) {
        std::cout << " ERROR: Path too long (" << requested_path.length() << " chars) - max 2048" << std::endl;
        return BAD_REQUEST;
    }
    
    return EVERYTHING_IS_READY;
}
