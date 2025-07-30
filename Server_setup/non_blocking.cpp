#include "server.hpp"

// Function 2: Setup epoll for event monitoring
int Server::setup_epoll(int serverSocket)
{
	std::cout << "=== SETTING UP EPOLL ===" << std::endl;

	// Create epoll instance
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		std::cout << "Failed to create epoll" << std::endl;
		return (-1);
	}
	std::cout << "✓ Epoll created" << std::endl;

	// Add server socket to epoll
	struct epoll_event event;
	event.events = EPOLLIN; // Monitor for incoming connections
	event.data.fd = serverSocket;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverSocket, &event) == -1)
	{
		std::cout << "Failed to add server socket to epoll" << std::endl;
		close(epoll_fd);
		return (-1);
	}
	std::cout << "✓ Server socket added to epoll" << std::endl;

	std::cout << "✓ Epoll setup complete!" << std::endl;
	return (epoll_fd);
}
