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
			if (is_server_socket(fd))
			{
				// Get server info
				port = fd_to_port[fd];
				server_config = fd_to_config[fd];
				std::cout << "📥 New connection on server port " << port << " (server fd: " << fd << ")" << std::endl;
				client_fd = Client::handle_new_connection(fd, epoll_fd,
						active_clients);
				client_to_server[client_fd] = fd;
				std::cout << "👤 Client " << client_fd << " connected to server " << port << std::endl;
			}
			else if (is_client_socket(fd))
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
							active_clients, *server_config, cgi_runner);
					}
					else if (events[i].events & EPOLLOUT)
					{
						std::cout << "📤 Data output to client " << fd << " (server port " << port << ")" << std::endl;
						it->second.handle_client_data_output(fd, epoll_fd,
							active_clients, *server_config);
					}
				}
			}
			else if (is_cgi_socket(fd))
			{
				// Handle CGI process I/O here
				std::cout << "⚙️ Handling CGI process I/O on fd " << fd << std::endl;
				
				if (events[i].events & EPOLLIN)
				{
					std::string response_data;
					if (cgi_runner.handle_cgi_output(fd, response_data))
					{
						// CGI process finished, remove from epoll immediately to prevent infinite loop
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
						
						// Get the associated client and send response
						int client_fd = cgi_runner.get_client_fd(fd);
						if (client_fd >= 0 && !response_data.empty())
						{
							std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
							if (client_it != active_clients.end())
							{
								// Send CGI response to client
								ssize_t sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
								if (sent > 0)
								{
									std::cout << "Sent " << sent << " bytes of CGI response to client " << client_fd << std::endl;
								}
								
								// Close client connection after sending response
								client_it->second.cleanup_connection(epoll_fd, active_clients);
							}
						}
						
						// Clean up CGI process
						cgi_runner.cleanup_cgi_process(fd);
					}
				}
				else if (events[i].events & (EPOLLHUP | EPOLLERR))
				{
					// CGI process closed or error occurred
					std::cout << "CGI process fd " << fd << " closed or error occurred" << std::endl;
					
					// Try to read any remaining data before closing
					std::string response_data;
					if (cgi_runner.handle_cgi_output(fd, response_data))
					{
						// Get the associated client and send response
						int client_fd = cgi_runner.get_client_fd(fd);
						if (client_fd >= 0 && !response_data.empty())
						{
							std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
							if (client_it != active_clients.end())
							{
								// Send CGI response to client
								ssize_t sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
								if (sent > 0)
								{
									std::cout << "Sent " << sent << " bytes of CGI response to client " << client_fd << std::endl;
								}
								
								// Close client connection after sending response
								client_it->second.cleanup_connection(epoll_fd, active_clients);
							}
						}
					}
					
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
					cgi_runner.cleanup_cgi_process(fd);
				}
			}
			else
			{
				std::cout << "⚠️ Warning: Unknown fd " << fd << " - not a server or client socket or CGI" << std::endl;
				// Could be a CGI process or other type - handle as needed
			}
		}
	}
}
