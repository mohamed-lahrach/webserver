#ifndef GET_HANDLER_HPP
#define GET_HANDLER_HPP

#include <string>
#include <map>
#include "request_status.hpp"

class GetHandler
{

public:

    GetHandler();
    ~GetHandler();
    
    RequestStatus handle_get_request(
        const std::string& requested_path,
        const std::map<std::string, std::string>& http_headers
    );

};

#endif 
