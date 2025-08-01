#ifndef SERVER_HPP
# define SERVER_HPP

# include "../client/client.hpp"
# include "../request/request.hpp"
# include "../response/response.hpp"
# include <arpa/inet.h>
# include <cstring>
# include <exception>
# include <fcntl.h>
# include <iostream>
# include <map>
# include <netdb.h>
# include <netinet/in.h>
# include <stdexcept>
# include <sys/epoll.h>
# include <sys/socket.h>
# include <unistd.h>

class	Client;
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
};

#endif
