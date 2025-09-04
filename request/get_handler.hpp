#ifndef GET_HANDLER_HPP
#define GET_HANDLER_HPP

#include <string>
#include <map>
#include "request_status.hpp"
#include "../config/parser.hpp"
#include "../cgi/cgi_headers.hpp"

class GetHandler
{

public:
    GetHandler();
    ~GetHandler();

    RequestStatus handle_get_request(
        const std::string &requested_path);
    
    std::string handle_cgi_get_request(
        const std::string &requested_path,
        const std::string &query_string,
        const std::map<std::string, std::string> &headers,
        const LocationContext *location);
};

#endif
