#include "server.hpp"

//Setup epol for event monitoring
int Server::setup_epoll()
{
	std::cout << "=== SETTING UP EPOLL ===" << std::endl;

	// Create epoll instance
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		std::cout << "Failed to create epoll" << std::endl;
		return (-1);
	}
	std::cout << "Epoll created" << std::endl;
	std::cout << "Epoll setup complete!" << std::endl;
	return (epoll_fd);
}
