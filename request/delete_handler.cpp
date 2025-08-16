#include "delete_handler.hpp"
#include "request_status.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

DeleteHandler::DeleteHandler()
{
}

DeleteHandler::~DeleteHandler()
{
}

RequestStatus DeleteHandler::handle_delete_request(
    const std::string& requested_path)
{
    std::cout << "=== DELETE HANDLER ===" << std::endl;
    std::cout << "Deleting: " << requested_path << std::endl;

    
    std::string file_path = requested_path.substr(1);
    std::cout << "Resolved path: " << file_path << std::endl;
    
    if (file_path.empty())
        return FORBIDDEN;
    
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0)
        return NOT_FOUND;
    
    if (S_ISDIR(file_stat.st_mode))
        return FORBIDDEN; 
    
    if (unlink(file_path.c_str()) == 0)
    {
        std::cout << "File deleted successfully" << std::endl;
        return DELETED;
    }
    
    return FORBIDDEN;
}



