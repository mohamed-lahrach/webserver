#include "server.hpp"

int Server::setup_Socket_with_host(int port, const std::string& host)
{
    std::cout << "=== SETTING UP SERVER ON " << host << ":" << port << " ===" << std::endl;

    int serverSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (serverSocket == -1)
    {
        std::cout << "Failed to create socket for " << host << ":" << port << std::endl;
        return (-1);
    }
    std::cout << "Socket created for " << host << ":" << port << std::endl;
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        std::cout << "Warning: Failed to set SO_REUSEADDR" << std::endl;
    }
    std::cout << "Socket options set" << std::endl;
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    if (host == "0.0.0.0" || host.empty())
    {
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        std::cout << "Using all interfaces (0.0.0.0)" << std::endl;
    }
    else
    {
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM; 
        
        int status = getaddrinfo(host.c_str(), NULL, &hints, &result);
        if (status != 0)
        {
            std::cout << "Invalid address/hostname: " << host << std::endl;
            close(serverSocket);
            return (-1);
        }
        
        struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
        serverAddress.sin_addr = addr_in->sin_addr;
        freeaddrinfo(result);
        std::cout << "Using specific address: " << host << std::endl;
    }

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cout << "Failed to bind to " << host << ":" << port << std::endl;
        close(serverSocket);
        return (-1);
    }
    std::cout << "Socket bound to " << host << ":" << port << std::endl;

    if (listen(serverSocket, SOMAXCONN) == -1)
    {
        std::cout << "Failed to listen on " << host << ":" << port << std::endl;
        close(serverSocket);
        return (-1);
    }

    std::cout << "Server listening on " << host << ":" << port << "!" << std::endl;
    return (serverSocket);
}