#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <iostream>
#include <sys/socket.h>
#include <cstring>
class Response
{
private:
    
public:
    // Constructor
    Response();
    
    // Destructor
    ~Response();
    void handle_response(int client_fd);
};

#endif