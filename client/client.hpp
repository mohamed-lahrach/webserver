#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <map>
#include <exception>
#include "../request/request.hpp"
#include "../response/response.hpp"

class Response;
class Request;
class Client
{
private:
    time_t connect_time;         // Keep same order as in constructor
    int request_count;           // Move this after connect_time
    int client_fd;

public:
    // Constructor
    Client();
    
    // Destructor
    ~Client();
    
    // Getter methods
    time_t get_connect_time() const;
    int get_request_count() const;
    
    // Setter methods
    void set_connect_time(time_t time);
    void increment_request_count();
    static bool handle_new_connection(int server_fd, int epoll_fd, std::map<int, Client>& active_clients);
    bool handle_client_data(int epoll_fd, std::map<int, Client>& active_clients);
};

#endif // CLIENT_HPP