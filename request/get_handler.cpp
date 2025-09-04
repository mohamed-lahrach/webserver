#include "get_handler.hpp"
#include "request_status.hpp"
#include "../cgi/cgi_headers.hpp"
#include <iostream>

GetHandler::GetHandler()
{
}

GetHandler::~GetHandler()
{
}
RequestStatus GetHandler::handle_get_request(const std::string &requested_path)
{
    std::cout << "=== GET HANDLER - COMPREHENSIVE VALIDATION ===" << std::endl;
    std::cout << "Requested path: '" << requested_path << "'" << std::endl;

    if (requested_path.empty())
    {
        std::cout << "ERROR: Empty path provided" << std::endl;
        return BAD_REQUEST;
    }
    if (requested_path.find("..") != std::string::npos)
    {
        std::cout << "ERROR: Path traversal attempt detected: " << requested_path << std::endl;
        return BAD_REQUEST;
    }

    if (requested_path.find("//") != std::string::npos)
    {
        return BAD_REQUEST;
    }

    if (requested_path.length() > 2048)
    {
        std::cout << " ERROR: Path too long (" << requested_path.length() << " chars) - max 2048" << std::endl;
        return BAD_REQUEST;
    }

    return EVERYTHING_IS_OK;
}
std::string GetHandler::handle_cgi_get_request(
    const std::string &requested_path,
    const std::string &query_string,
    const std::map<std::string, std::string> &headers,
    const LocationContext *location)
{
    if (!CgiHandler::is_cgi_request(requested_path, location))
        return "";
    
    CgiRequest cgi_req;
    cgi_req.method = "GET";
    cgi_req.path = requested_path;
    cgi_req.query_string = query_string;
    cgi_req.http_version = "HTTP/1.1";
    cgi_req.headers = headers;
    cgi_req.body = "";
    
    return CgiHandler::handle_cgi_request(cgi_req, location);
}