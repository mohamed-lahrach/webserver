#include "server.hpp"

int Server::setup_Socket_with_host(int port, const std::string& host)
{
    std::cout << "=== SETTING UP SERVER ON " << host << ":" << port << " ===" << std::endl;
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cout << "Failed to create socket for " << host << ":" << port << std::endl;
        return (-1);
    }
    std::cout << "✓ Socket created for " << host << ":" << port << std::endl;

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        std::cout << "Warning: Failed to set SO_REUSEADDR" << std::endl;
    }
    std::cout << "✓ Socket options set" << std::endl;

    // Specifying the address
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // Convert IP string to binary format
    if (host == "0.0.0.0" || host.empty())
    {
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        std::cout << "✓ Using all interfaces (0.0.0.0)" << std::endl;
    }
    else
    {
        if (inet_pton(AF_INET, host.c_str(), &serverAddress.sin_addr) <= 0)
        {
            std::cout << "Invalid IP address: " << host << std::endl;
            close(serverSocket);
            return (-1);
        }
        std::cout << "✓ Using specific address: " << host << std::endl;
    }

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        std::cout << "Failed to bind to " << host << ":" << port << std::endl;
        close(serverSocket);
        return (-1);
    }
    std::cout << "✓ Socket bound to " << host << ":" << port << std::endl;

    if (listen(serverSocket, 5) == -1)
    {
        std::cout << "Failed to listen on " << host << ":" << port << std::endl;
        close(serverSocket);
        return (-1);
    }

    std::cout << "✅ Server listening on " << host << ":" << port << "!" << std::endl;
    return (serverSocket);
}