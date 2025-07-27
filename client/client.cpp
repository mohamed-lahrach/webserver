#include "client.hpp"

// Constructor
Client::Client() : connect_time(0), request_count(0)
{
    std::cout << "Client constructor called" << std::endl;
}

// Destructor
Client::~Client()
{
    std::cout << "Client destructor called" << std::endl;
}

time_t Client::get_connect_time() const
{
    return connect_time;
}

int Client::get_request_count() const
{
    return request_count;
}

// Setter methods

void Client::set_connect_time(time_t time)
{
    connect_time = time;
}

void Client::increment_request_count()
{
    request_count++;
}




//////

bool Client::handle_new_connection(int server_fd, int epoll_fd, std::map<int, Client>& active_clients)
{
    Client client;
    
    // Accept connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    client.client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client.client_fd == -1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "Error accepting connection: " << strerror(errno) << std::endl;
        }
        return false;
    }

    std::cout << "✓ New client connected: " << client.client_fd << std::endl;
    
    // Set connection time
    //client.set_connect_time(time(NULL));
    
    // Add client to epoll
    struct epoll_event client_event;
    client_event.events = EPOLLIN;
    client_event.data.fd = client.client_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.client_fd, &client_event) == -1) {
        std::cout << "Failed to add client to epoll" << std::endl;
        close(client.client_fd);
        return false;
    }
    
    // Add to active clients map
    active_clients[client.client_fd] = client;
    std::cout << "✓ Client " << client.client_fd << " added to map" << std::endl;
    std::cout << "✓ Total active clients: " << active_clients.size() << std::endl;
    
    return true;
}

bool Client::handle_client_data(int epoll_fd, std::map<int, Client>& active_clients)
{
    char buffer[1024] = {0};
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        // ========== REQUEST HANDLING ==========
        buffer[bytes_received] = '\0';
        std::cout << "=== CLIENT " << client_fd << ": PROCESSING REQUEST ===" << std::endl;
        std::cout << "Message from client " << client_fd << ": " << buffer << std::endl;
        
        // Increment request count
        increment_request_count();
        std::cout << "✓ Client " << client_fd << " request count: " << get_request_count() << std::endl;
        
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
        }
        
    } else if (bytes_received == 0) {
        std::cout << "Client " << client_fd << " disconnected" << std::endl;
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "Error receiving data: " << strerror(errno) << std::endl;
        }
    }
    
    // ========== CLEANUP ==========
    std::cout << "✓ Client " << client_fd << " was connected for " 
              << (time(NULL) - get_connect_time()) << " seconds" << std::endl;
    std::cout << "✓ Client " << client_fd << " made " << get_request_count() << " requests" << std::endl;
    
    // Remove client from epoll and close
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    
    // Remove from active clients map
    active_clients.erase(client_fd);
    std::cout << "✓ Client " << client_fd << " connection closed" << std::endl;
    std::cout << "✓ Remaining active clients: " << active_clients.size() << std::endl;
    
    return true;
}