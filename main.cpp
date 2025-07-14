#include "Server_setup/server.hpp"

int main()
{
    const int PORT = 2440;
    
    // Step 1: Setup server
    Server server(PORT);
    int serverSocket = server.setup_Socket(PORT);
    if (serverSocket == -1) {
        return -1;
    }
    
    // Step 2: Setup epoll
    int epoll_fd = server.setup_epoll(serverSocket);
    if (epoll_fd == -1) {
        close(serverSocket);
        return -1;
    }
    
    // Step 3: Run server
    server.run_server(serverSocket, epoll_fd);
    
    // Cleanup
    close(epoll_fd);
    close(serverSocket);
    std::cout << "Server shutdown complete" << std::endl;
    
    return 0;
}
