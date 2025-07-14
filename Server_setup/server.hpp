#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>

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

    void setupSocket();
    void setupEpoll();
    void run(); // Accept connections and handle events
};

#endif // SERVER_HPP
