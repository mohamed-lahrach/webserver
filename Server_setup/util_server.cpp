#include "server.hpp"

Server::Server(): server_fd(-1), epoll_fd(-1)
{
    std::cout << "=== CREATING SERVER ===" << std::endl;    
    // Initialize the address structure
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    
    std::cout << "✓ Server object created" << std::endl;
}

// Destructor
Server::~Server()
{
    std::cout << "=== DESTROYING SERVER ===" << std::endl;
    
    // // Close epoll file descriptor if open
    // if (epoll_fd != -1) {
    //     close(epoll_fd);
    //     std::cout << "✓ Epoll closed" << std::endl;
    // }
    
    // // Close server socket if open
    // if (server_fd != -1) {
    //     close(server_fd);
    //     std::cout << "✓ Server socket closed" << std::endl;
    // }
    
    std::cout << "✓ Server object destroyed" << std::endl;
}
void Server::init_data(int PORT, std::string hostname)
{
    this->port = PORT;
    this->hostname = hostname;
}
