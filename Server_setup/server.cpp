#include "server.hpp"

void Server::run()
{
	const int			MAX_EVENTS = 10;
	struct epoll_event	events[MAX_EVENTS];
	int					num_events;
	int					fd;
	int					server_fd;
	int					port;
	ServerContext		*server_config;
	int					client_fd;

	std::cout << "=== RUNNING MULTIPLE SERVERS ===" << std::endl;
	std::cout << "Monitoring " << server_fds.size() << " server sockets" << std::endl;
	for (size_t i = 0; i < server_fds.size(); ++i)
	{
		server_fd = server_fds[i];
		port = fd_to_port[server_fd];
		std::cout << "✅ Server " << (i
			+ 1) << " listening on port " << port << " (fd: " << server_fd << ")" << std::endl;
	}
	while (true)
	{
		std::cout << "Waiting for events on all servers..." << std::endl;
		// Wait for events (1000ms timeout)
		num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
		if (num_events == -1)
		{
			throw std::runtime_error("Error in epoll_wait");
		}
		if (num_events == 0)
		{
			std::cout << "No events (timeout)" << std::endl;
			continue ;
		}
		for (int i = 0; i < num_events; i++)
		{
			fd = events[i].data.fd;
			// Check if it's a server socket (new connection)
			if (is_server_socket(fd))
			{
				// Get server info
				port = fd_to_port[fd];
				server_config = fd_to_config[fd];
				std::cout << "📥 New connection on server port " << port << " (server fd: " << fd << ")" << std::endl;
				// Handle new connection and get the client fd
				client_fd = Client::handle_new_connection(fd, epoll_fd,
						active_clients);
				// Link client to server - NOW WE KNOW THE EXACT CLIENT!
				client_to_server[client_fd] = fd;
				std::cout << "👤 Client " << client_fd << " connected to server " << port << std::endl;
			}
			// Handle existing client data
			else
			{
				std::map<int, Client>::iterator it = active_clients.find(fd);
				if (it != active_clients.end())
				{
					// Get the server config for this client
					server_config = get_client_config(fd);
					if (server_config == NULL)
					{
						std::cout << "❌ No server config found for client " << fd << std::endl;
						continue ;
					}
					server_fd = client_to_server[fd];
					port = fd_to_port[server_fd];
					if (events[i].events & EPOLLIN)
					{
						std::cout << "📨 Data input from client " << fd << " (server port " << port << ")" << std::endl;
						it->second.handle_client_data_input(epoll_fd,
							active_clients, *server_config);
					}
					else if (events[i].events & EPOLLOUT)
					{
						std::cout << "📤 Data output to client " << fd << " (server port " << port << ")" << std::endl;
						it->second.handle_client_data_output(fd, epoll_fd,
							active_clients, *server_config);
					}
				}
				else
				{
					std::cout << "⚠️ Warning: Client " << fd << " not found in active_clients map" << std::endl;
				}
			}
		}
	}
}
