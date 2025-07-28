#include "server.hpp"
#include <arpa/inet.h>  // For in_addr and other network types
#include <netdb.h>      // For gethostbyname() and hostent
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h> // For socket functions

int Server::setup_Socket(int port)
{
	std::cout << "=== SETTING UP SERVER ===" << std::endl;

	// Creating socket
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

	// if(getaddrinfo())

	// Try to parse hostname as IP address first
	if (!hostname.empty())
	{
		if (inet_pton(AF_INET, hostname.c_str(), &serverAddress.sin_addr) <= 0)
		{
			std::cerr << "Invalid IP address: " << hostname << std::endl;
			close(serverSocket);
			return (-1);
		}
		std::cout << "✓ Using IP address: " << hostname << std::endl;
	}
	else
	{
		// Use INADDR_ANY for localhost or empty hostname
		serverAddress.sin_addr.s_addr = INADDR_ANY;
		std::cout << "✓ Using all interfaces (0.0.0.0)" << std::endl;
	}

	// Binding socket
	if (bind(serverSocket, (struct sockaddr *)&serverAddress,
			sizeof(serverAddress)) == -1)
	{
		std::cout << "Failed to bind socket to port " << port << std::endl;
		close(serverSocket);
		return (-1);
	}

	// Listening to the assigned socket
	if (listen(serverSocket, 5) == -1)
	{
		std::cout << "Failed to listen" << std::endl;
		close(serverSocket);
		return (-1);
	}
	std::cout << "✓ " << hostname << " server setup complete!" << std::endl;
	return (serverSocket);
}