#include "server.hpp"

void Server::run()
{
	const int			MAX_EVENTS = 10;
	struct epoll_event	events[MAX_EVENTS];
	int					num_events;
	int					fd;

	std::cout << "=== RUNNING SERVER ===" << std::endl;
	std::cout << "Server starting on port " << port << std::endl;
	server_fd = setup_Socket(port);
	if (server_fd == -1)
	{
		throw std::runtime_error("Failed to setup server socket on port ");
	}
	epoll_fd = setup_epoll(server_fd);
	if (epoll_fd == -1)
	{
		close(server_fd);
		throw std::runtime_error("Failed to setup epoll");
	}
	std::cout << "✅ Server successfully started on port " << port << std::endl;
	// Real event loop
	while (true)
	{
		std::cout << "Waiting for events on port " << port << "..." << std::endl;
		// Wait for events (1000ms timeout)
		num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
		if (num_events == -1)
		{
			std::cout << "Error in epoll_wait: " << strerror(errno) << std::endl;
			break ;
		}
		if (num_events == 0)
		{
			std::cout << "No events (timeout)" << std::endl;
			continue ;
		}
		for (int i = 0; i < num_events; i++)
		{
			fd = events[i].data.fd;
			if (fd == server_fd)
			{
				Client::handle_new_connection(server_fd, epoll_fd,
					active_clients);
			}
			else
			{
				std::map<int, Client>::iterator it = active_clients.find(fd);
				if (it != active_clients.end())
				{
					if (events[i].events & EPOLLIN)
						it->second.handle_client_data_input(epoll_fd, active_clients);
				  	else if (events[i].events & EPOLLOUT)
						it->second.handle_client_data_output(fd,epoll_fd,active_clients);
				}
				else
				{
					std::cout << "Warning: Client " << fd << " not found in active_clients map" << std::endl;
				}
			}
		}
	}
}
