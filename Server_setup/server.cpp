#include "server.hpp"
#include <cstdio>
#include <cstring>

void Server::run()
{
    std::cout << "=== RUNNING SERVER ===" << std::endl;
    std::cout << "Server starting on port " << port << std::endl; 
    server_fd = setup_Socket(port);
    if (server_fd == -1)
    {
        throw std::runtime_error("Failed to setup server socket on port ");
    }
    epoll_fd = setup_epoll(server_fd);
    if (epoll_fd == -1) {
        close(server_fd);
        throw std::runtime_error("Failed to setup epoll"); 
    }
    
    std::cout << "✅ Server successfully started on port " << port << std::endl;
    
    // Real event loop
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    
    while (true)
    {
        std::cout << "Waiting for events on port " << port << "..." << std::endl;
        
        // Wait for events (1000ms timeout)
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        
        if (num_events == -1) {
            std::cout << "Error in epoll_wait: " << strerror(errno) << std::endl;
            break;
        }
        
        if (num_events == 0) {
            std::cout << "No events (timeout)" << std::endl;
            continue;
        }
        
        // Process each event
    for (int i = 0; i < num_events; i++)
    {
        int fd = events[i].data.fd;
    
        if (fd == server_fd)
            {
            // New connection
                Client::handle_new_connection(server_fd, epoll_fd, active_clients);
            }
            else
            {
            // Data from existing client - find the client and let it handle its data
            std::map<int, Client>::iterator it = active_clients.find(fd);
            if (it != active_clients.end()) {
                it->second.handle_client_data(epoll_fd, active_clients);
            }
            else
            {
                std::cout << "Warning: Client " << fd << " not found in active_clients map" << std::endl;
            }
    }
}
    }
}


