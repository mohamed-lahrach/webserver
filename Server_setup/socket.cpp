#include "server.hpp"

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

	if (hostname == "web")
	{
		// Special handling for "web" hostname
		std::cout << "✓ Configuring " << hostname << " server for local access" << std::endl;
		serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
			// Bind to localhost for "web"
	}
	else
	{
		// Default behavior
		serverAddress.sin_addr.s_addr = INADDR_ANY;
	}

	// Binding socket
	if (bind(serverSocket, (struct sockaddr *)&serverAddress,
			sizeof(serverAddress)) == -1)
	{
		std::cout << "Failed to bind socket to port " << port << std::endl;
		close(serverSocket);
		return (-1);
	}

	if (!hostname.empty())
	{
		std::cout << "✓ " << hostname << " server bound to port " << port << std::endl;
	}
	else
	{
		std::cout << "✓ Socket bound to port " << port << std::endl;
	}

	// Listening to the assigned socket
	if (listen(serverSocket, 5) == -1)
	{
		std::cout << "Failed to listen" << std::endl;
		close(serverSocket);
		return (-1);
	}

	if (!hostname.empty())
	{
		std::cout << "✓ " << hostname << " server listening on port " << port << std::endl;
		std::cout << "✓ " << hostname << " server setup complete!" << std::endl;
		std::cout << "✓ Try accessing: http://" << hostname << ":" << port << std::endl;
		std::cout << "✓ Or fallback to: http://localhost:" << port << std::endl;
	}
	else
	{
		std::cout << "✓ Socket listening" << std::endl;
		std::cout << "✓ Server setup complete!" << std::endl;
	}

	return (serverSocket);
}