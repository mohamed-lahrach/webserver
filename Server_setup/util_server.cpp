#include "server.hpp"

Server::Server(): server_fd(-1), epoll_fd(-1)
{
    std::cout << "=== CREATING SERVER ===" << std::endl;
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
void Server::init_data(ServerContext &server_config)
{
	this->port = atoi((server_config.port).c_str());
	this->hostname = server_config.host;
        // NOW initialize the address with the correct port
    ///aficher data 
    std::cout << "=== INITIALIZING SERVER DATA ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "✓ Server data initialized" << std::endl;
}
