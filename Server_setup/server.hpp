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
    public:
        Server(int port);
        ~Server();
        void run();

private:
        int _port;
        int _server_fd;

        void createSocket();
        void bindSocket();
        void startListening();
};

#endif // SERVER_HPP
