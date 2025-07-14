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

class Server
{
private:
    int server_fd;
    int port;
    int epoll_fd;
    struct sockaddr_in address;

public:
    Server(int port);
    ~Server();

    int setup_Socket(int port);
    int setup_epoll(int serverSocket);
    void run_server(int serverSocket, int epoll_fd); // Accept connections and handle events
};

#endif // SERVER_HPP
