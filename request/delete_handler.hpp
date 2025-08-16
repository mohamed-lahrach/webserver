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
    
    RequestStatus handle_delete_request(const std::string& file_path);
};

#endif 