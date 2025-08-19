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
#include <cstdlib>  // For atoi()

#include "../config/parser.hpp"
class	Client;
class Server
{
  private:
    int server_fd;
    int port;
    int epoll_fd;
    std::string hostname;

    std::vector<int> server_fds;
    std::map<int, ServerContext*> fd_to_config;
    std::map<int, int> fd_to_port;              // Add this
    std::map<int, int> client_to_server; 
    std::map<int, Client> active_clients;
    struct sockaddr_in address;

  public:
    Server();
    ~Server();

    void init_data(const std::vector<ServerContext>& configs);
    void run();

    int setup_Socket_with_host(int port, const std::string& host);
    int setup_epoll();

    bool is_server_socket(int fd);
    ServerContext* get_server_config(int fd);
    ServerContext* get_client_config(int client_fd);
};

#endif
