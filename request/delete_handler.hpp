#ifndef DELETE_HANDLER_HPP
#define DELETE_HANDLER_HPP

#include <string>
#include <map>
#include "request_status.hpp"

class DeleteHandler
{

    
public:

    DeleteHandler();
    ~DeleteHandler();
    
    RequestStatus handle_delete_request(
        const std::string& requested_path,
        const std::map<std::string, std::string>& http_headers
    );
    

};

#endif 
