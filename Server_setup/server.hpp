#ifndef SERVER_HPP
#define SERVER_HPP

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
#include "../client/client.hpp"
#include "../request/request.hpp"
#include "../response/response.hpp"
class Client;
class Server
{
private:
    int server_fd;
    int port;
    int epoll_fd;
    std::string hostname;

    std::map<int, Client> active_clients;
    struct sockaddr_in address;

public:
    Server();
    ~Server();

    
    void init_data(int PORT, std::string hostname);
    void run();

    int setup_Socket(int port);
    int setup_epoll(int serverSocket);
    
    // Accept connections and handle events
};

#endif
