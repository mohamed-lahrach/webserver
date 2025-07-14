#include "server.hpp"  // Add this for epoll

#include "server.hpp"

// Constructor
Server::Server(int port) : server_fd(-1),port(port), epoll_fd(-1)
{
    std::cout << "=== CREATING SERVER ===" << std::endl;
    std::cout << "Server constructor called with port: " << port << std::endl;
    
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
    
    // Close epoll file descriptor if open
    if (epoll_fd != -1) {
        close(epoll_fd);
        std::cout << "✓ Epoll closed" << std::endl;
    }
    
    // Close server socket if open
    if (server_fd != -1) {
        close(server_fd);
        std::cout << "✓ Server socket closed" << std::endl;
    }
    
    std::cout << "✓ Server object destroyed" << std::endl;
}

// Rest of your existing functions...
void make_socket_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cout << "Error getting socket flags" << std::endl;
        return;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cout << "Error setting non-blocking" << std::endl;
    } else {
        std::cout << "Socket made non-blocking successfully" << std::endl;
    }
}

// Function 1: Setup the server socket
int Server::setup_Socket(int port)
{
    std::cout << "=== SETTING UP SERVER ===" << std::endl;
    
    // Creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cout << "Failed to create socket" << std::endl;
        return -1;
    }
    std::cout << "✓ Socket created" << std::endl;

    // Set socket option to reuse address
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    std::cout << "✓ Socket options set" << std::endl;

    // Specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Binding socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cout << "Failed to bind socket to port " << port << std::endl;
        close(serverSocket);
        return -1;
    }
    std::cout << "✓ Socket bound to port " << port << std::endl;

    // Listening to the assigned socket
    if (listen(serverSocket, 5) == -1) {
        std::cout << "Failed to listen" << std::endl;
        close(serverSocket);
        return -1;
    }
    std::cout << "✓ Socket listening" << std::endl;

    // Make server socket non-blocking
    make_socket_non_blocking(serverSocket);
    
    std::cout << "✓ Server setup complete!" << std::endl;
    return serverSocket;
}

// Function 2: Setup epoll for event monitoring
int Server::setup_epoll(int serverSocket)
{
    std::cout << "=== SETTING UP EPOLL ===" << std::endl;
    
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cout << "Failed to create epoll" << std::endl;
        return -1;
    }
    std::cout << "✓ Epoll created" << std::endl;

    // Add server socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;  // Monitor for incoming connections
    event.data.fd = serverSocket;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverSocket, &event) == -1) {
        std::cout << "Failed to add server socket to epoll" << std::endl;
        close(epoll_fd);
        return -1;
    }
    std::cout << "✓ Server socket added to epoll" << std::endl;
    
    std::cout << "✓ Epoll setup complete!" << std::endl;
    return epoll_fd;
}

// Function 3: Run the server event loop
void Server::run_server(int serverSocket, int epoll_fd)
{
    std::cout << "=== RUNNING SERVER ===" << std::endl;
    std::cout << "Server listening on port 2440 (epoll mode)..." << std::endl;
    
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    
    while (true) {
        // Wait for events (with 1000ms timeout)
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
                // New connection on server socket
                std::cout << "New connection detected!" << std::endl;
                
                int clientSocket = accept(serverSocket, NULL, NULL);
                if (clientSocket == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        std::cout << "Error accepting connection: " << strerror(errno) << std::endl;
                    }
                    continue;
                }
                
                std::cout << "Client connected!" << std::endl;
                make_socket_non_blocking(clientSocket);
                
                // Add client socket to epoll for reading
                struct epoll_event client_event;
                client_event.events = EPOLLIN;
                client_event.data.fd = clientSocket;
                
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientSocket, &client_event) == -1) {
                    std::cout << "Failed to add client to epoll" << std::endl;
                    close(clientSocket);
                }
                
            } else {
                // Data available on client socket
                std::cout << "Data available on client socket " << fd << std::endl;
                
                char buffer[1024] = { 0 };
                ssize_t bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    std::cout << "Message from client: " << buffer << std::endl;
                    
                    // Send response
                    const char* httpResponse =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: 21\r\n"
                        "\r\n"
                        "Hello, world! oussama";
                    send(fd, httpResponse, strlen(httpResponse), 0);
                    
                } else if (bytes_received == 0) {
                    std::cout << "Client disconnected" << std::endl;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        std::cout << "Error receiving data: " << strerror(errno) << std::endl;
                    }
                }
                
                // Remove client from epoll and close
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                std::cout << "Connection closed" << std::endl;
                
                // For this simple example, break after one client
                // In a real server, you'd continue the loop
                return;
            }
        }
    }
}
