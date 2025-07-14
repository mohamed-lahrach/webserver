#include <iostream>
#include "server.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>     // ← ADD THIS for fcntl() and O_NONBLOCK
#include <errno.h>     // ← ADD THIS for errno
using namespace std;

void make_socket_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        cout << "Error getting socket flags" << endl;
        return;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        cout << "Error setting non-blocking" << endl;
    } else {
        cout << "Socket made non-blocking successfully" << endl;
    }
}

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cout << "Failed to create socket" << endl;
        return -1;
    }

    // Set socket option to reuse address
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(2440);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cout << "Failed to bind socket" << endl;
        close(serverSocket);
        return -1;
    }

    // listening to the assigned socket
    if (listen(serverSocket, 5) == -1) {
        cout << "Failed to listen" << endl;
        close(serverSocket);
        return -1;
    }

    // Make server socket non-blocking
    make_socket_non_blocking(serverSocket);
    
    cout << "Server listening on port 2440 (non-blocking mode)..." << endl;

    // Loop to handle connections
    while (true) {
        // Try to accept a connection (non-blocking)
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        
        if (clientSocket == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connection available right now, continue loop
                usleep(100000);  // Sleep 100ms to avoid busy waiting
                continue;
            } else {
                cout << "Error accepting connection: " << strerror(errno) << endl;
                break;
            }
        }

        cout << "Client connected!" << endl;
        
        // Make client socket non-blocking too
        make_socket_non_blocking(clientSocket);

        // Try to receive data (non-blocking)
        char buffer[1024] = { 0 };
        ssize_t bytes_received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            cout << "Message from client: " << buffer << endl;
            
            // Send response
            const char* httpResponse =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 21\r\n"
                "\r\n"
                "Hello, world! oussama";
            send(clientSocket, httpResponse, strlen(httpResponse), 0);
            
        } else if (bytes_received == 0) {
            cout << "Client disconnected" << endl;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cout << "No data available yet" << endl;
            } else {
                cout << "Error receiving data: " << strerror(errno) << endl;
            }
        }

        close(clientSocket);
        cout << "Connection closed" << endl;
        
        // For this simple example, break after one connection
        // In a real server, you'd continue the loop
        break;
    }

    close(serverSocket);
    return 0;
}