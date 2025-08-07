#include "server.hpp"

int Server::setup_Socket(int port)
{
	std::cout << "=== SETTING UP SERVER ===" << std::endl;
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1)
	{
		std::cout << "Failed to create socket" << std::endl;
		return (-1);
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

	// use getaddrinfo for address resolution
	if (!hostname.empty())
	{
		struct addrinfo hints, *result;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;       // IPv4 only
		hints.ai_socktype = SOCK_STREAM; // TCP socket
		hints.ai_flags = AI_PASSIVE;     // For server socket
	
		int status = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
		if (status != 0)
		{
			std::cerr << "Failed to resolve hostname/IP: " << hostname << "- Error: " << gai_strerror(status) << std::endl;
			close(serverSocket);
			return (-1);
		}

		// Extract the IP address from result
		struct sockaddr_in *addr_in = (struct sockaddr_in *)result->ai_addr;
		serverAddress.sin_addr = addr_in->sin_addr;
		freeaddrinfo(result);
	}
	else
	{
		// Empty hostname - bind to all interfaces for localhost 
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		std::cout << "✓ Using all interfaces (0.0.0.0)" << std::endl;
	}
	if (bind(serverSocket, (struct sockaddr *)&serverAddress,
			sizeof(serverAddress)) == -1)
	{
		std::cout << "Failed to bind socket to port " << port << std::endl;
		close(serverSocket);
		return (-1);
	}
	if (listen(serverSocket, 5) == -1)
	{
		std::cout << "Failed to listen" << std::endl;
		close(serverSocket);
		return (-1);
	}
	std::cout << "✓ " << hostname << " server setup complete!" << std::endl;
	return (serverSocket);
}