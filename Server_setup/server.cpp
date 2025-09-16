#include "server.hpp"

void Server::run()
{
	const int			MAX_EVENTS = 10;
	const int			TIMEOUT = 30000; // 30 seconds
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
		std::cout << "Server " << (i
			+ 1) << " listening on port " << port << " (fd: " << server_fd << ")" << std::endl;
	}
	while (true)
	{
		// Check for CGI timeouts before waiting for new events
		std::vector<int> timed_out_fds = cgi_runner.get_timed_out_cgi_fds();
		if (!timed_out_fds.empty())
		{
			std::cout << "\033[34m[TIMEOUT SCAN] Found " << timed_out_fds.size() << " timed out CGI processes\033[0m" << std::endl;
		}
		
		for (size_t i = 0; i < timed_out_fds.size(); ++i)
		{
			int cgi_fd = timed_out_fds[i];
			std::string timeout_response;
			if (cgi_runner.check_cgi_timeout(cgi_fd, timeout_response))
			{
				std::cout << "CGI timeout detected on fd " << cgi_fd << std::endl;
				
				// Send timeout response to client
				int client_fd = cgi_runner.get_client_fd(cgi_fd);
				if (client_fd >= 0 && !timeout_response.empty())
				{
					std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
					if (client_it != active_clients.end())
					{
						ssize_t bytes_sent = send(client_fd, timeout_response.c_str(), timeout_response.size(), 0);
						if (bytes_sent == -1 || bytes_sent == 0)
						{
							std::cout << "Failed to send CGI timeout response to client " << client_fd << std::endl;
						}
						else
						{
							std::cout << "Sent " << bytes_sent << " bytes of CGI timeout response to client " << client_fd << std::endl;
						}
						client_it->second.cleanup_connection(epoll_fd, active_clients);
					}
				}
				
				// Clean up the timed out CGI process
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi_fd, NULL);
				cgi_runner.cleanup_cgi_process(cgi_fd);
			}
		}
		
		num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT);
        if (num_events == 0)
        {
            check_client_timeouts(active_clients);
            continue; // No events, continue waiting
        }
		
		for (int i = 0; i < num_events; i++)
		{
			fd = events[i].data.fd;
			if (is_server_socket(fd))
			{
				port = fd_to_port[fd];
				server_config = fd_to_config[fd];
				std::cout << "New connection on server port " << port << " (server fd: " << fd << ")" << std::endl;
				client_fd = Client::handle_new_connection(fd, epoll_fd,
						active_clients);
				if (client_fd == -1)
				{
					std::cout << "Failed to handle new connection on port " << port << std::endl;
					continue; // skip this connection and continue with next event
				}
				client_to_server[client_fd] = fd;
				std::cout << "Client " << client_fd << " connected to server " << port << std::endl;
			}
			else if (is_client_socket(fd))
			{
				std::map<int, Client>::iterator it = active_clients.find(fd);
				if (it != active_clients.end())
				{
					server_config = get_client_config(fd);
					if (server_config == NULL)
					{
						std::cout << "No server config found for client " << fd << std::endl;
						continue ;
					}
					server_fd = client_to_server[fd];
					port = fd_to_port[server_fd];
					if (events[i].events & EPOLLIN)
					{
						std::cout << "Data input from client " << fd << " (server port " << port << ")" << std::endl;
						it->second.handle_client_data_input(epoll_fd,
							active_clients, *server_config, cgi_runner);
					}
					else if (events[i].events & EPOLLOUT)
					{
						std::cout << "Data output to client " << fd << " (server port " << port << ")" << std::endl;
						it->second.handle_client_data_output(fd, epoll_fd,
							active_clients, *server_config);
					}
				}
			}
			else if (is_cgi_socket(fd))
			{
				std::cout << "Handling CGI process I/O on fd " << fd << std::endl;
				
				if (events[i].events & EPOLLIN)
				{
					std::string response_data;
					// Update activity timestamp when we receive data
					cgi_runner.update_cgi_activity(fd);
					if (cgi_runner.handle_cgi_output(fd, response_data))
					{
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
						int client_fd = cgi_runner.get_client_fd(fd);
						if (client_fd >= 0 && !response_data.empty())
						{
							std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
							if (client_it != active_clients.end())
							{
								ssize_t bytes_sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
								if (bytes_sent == -1 || bytes_sent == 0)
								{
									std::cout << "Failed to send CGI response to client " << client_fd << std::endl;
								}
								else
								{
									std::cout << "Sent " << bytes_sent << " bytes of CGI response to client " << client_fd << std::endl;
								}
								client_it->second.cleanup_connection(epoll_fd, active_clients);
							}
						}
						// Clean up CGI process
						cgi_runner.cleanup_cgi_process(fd);
					}
				}
				else if (events[i].events & (EPOLLHUP | EPOLLERR))
				{
					std::cout<<"----------------------+++++++++++++++++\n";
					std::cout << "CGI process fd " << fd << " closed or error occurred" << std::endl;
					std::string response_data;
					if (cgi_runner.handle_cgi_output(fd, response_data))
					{
						int client_fd = cgi_runner.get_client_fd(fd);
						if (client_fd >= 0 && !response_data.empty())
						{
							std::map<int, Client>::iterator client_it = active_clients.find(client_fd);
							if (client_it != active_clients.end())
							{
								ssize_t bytes_sent = send(client_fd, response_data.c_str(), response_data.size(), 0);
								if (bytes_sent == -1 || bytes_sent == 0)
								{
									std::cout << "Failed to send CGI response to client " << client_fd << std::endl;
								}
								else
								{
									std::cout << "Sent " << bytes_sent << " bytes of CGI response to client " << client_fd << std::endl;
								}
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
				std::cout << "Warning: Unknown fd " << fd << " - not a server or client socket or CGI" << std::endl;
			}
		}
	}
}

