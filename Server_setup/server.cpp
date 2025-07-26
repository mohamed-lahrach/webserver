#include "server.hpp"
#include <cstdio>
#include <cstring>

void Server::run()
{
    std::cout << "=== RUNNING SERVER ===" << std::endl;
    std::cout << "Server starting on port " << port << std::endl; 
    int serverSocket = setup_Socket(port);
    if (serverSocket == -1)
    {
        throw std::runtime_error("Failed to setup server socket on port ");
    }
    int epoll_fd = setup_epoll(serverSocket);
    if (epoll_fd == -1) {
        close(serverSocket);
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
        for (int i = 0; i < num_events; i++) {
            int fd = events[i].data.fd;
            
            if (fd == serverSocket) {
                // New connection
                handle_new_connection(serverSocket, epoll_fd);
            } else {
                // Data from existing client
                handle_client_data(fd, epoll_fd);
            }
        }
    }
}

// Add these helper functions
void Server::handle_new_connection(int serverSocket, int epoll_fd) {
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "Error accepting connection: " << strerror(errno) << std::endl;
        }
        return;
    }
    
    std::cout << "✓ New client connected: " << clientSocket << std::endl;
    
    // Add client to epoll
    struct epoll_event client_event;
    client_event.events = EPOLLIN;
    client_event.data.fd = clientSocket;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientSocket, &client_event) == -1) {
        std::cout << "Failed to add client to epoll" << std::endl;
        close(clientSocket);
    }
}

void Server::handle_client_data(int client_fd, int epoll_fd) {
    char buffer[1024] = {0};
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
     if (bytes_received > 0) {
        // ========== REQUEST HANDLING ==========
        buffer[bytes_received] = '\0';
        std::cout << "=== PROCESSING REQUEST ===" << std::endl;
        std::cout << "Message from client " << client_fd << ": " << buffer << std::endl;
        
        // Parse and validate the request
        Response response;
        Request request;
        if (request.handle_request(client_fd, buffer))
        {
            std::cout << "✓ Request processed successfully" << std::endl;
            
            // ========== RESPONSE HANDLING ==========
            std::cout << "=== SENDING RESPONSE ===" << std::endl;
            response.handle_response(client_fd);
        }
        else
        {
            std::cout << "✗ Request processing failed" << std::endl;
            // handling error 
            //handle_error_response(client_fd);
        }
        
    } else if (bytes_received == 0) {
        std::cout << "Client " << client_fd << " disconnected" << std::endl;
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "Error receiving data: " << strerror(errno) << std::endl;
        }
    }
    
    // Remove client from epoll and close
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    std::cout << "✓ Client " << client_fd << " connection closed" << std::endl;
}

