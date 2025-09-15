#include "server.hpp"

Server::Server() : server_fd(-1), epoll_fd(-1)
{
	std::cout << "=== CREATING SERVER ===" << std::endl;
	std::cout << "Server object created" << std::endl;
}

Server::~Server()
{
	std::cout << "=== DESTROYING SERVER ===" << std::endl;
	if (epoll_fd != -1)
	{
		close(epoll_fd);
		std::cout << "Epoll closed" << std::endl;
	}
	for (size_t i = 0; i < server_fds.size(); i++)
	{
		if (server_fds[i] != -1)
		{
			close(server_fds[i]);
			std::cout << "Server socket " << server_fds[i] << " closed" << std::endl;
		}
	}
	if (!active_clients.empty())
	{
		std::cout << "Cleaning up " << active_clients.size() << " remaining clients" << std::endl;
		active_clients.clear(); 
	}
	client_to_server.clear();
	fd_to_port.clear();
	fd_to_config.clear();
	std::cout << "Server object destroyed" << std::endl;
}

bool Server::is_server_socket(int fd)
{
	for (size_t i = 0; i < server_fds.size(); i++)
	{
		if (server_fds[i] == fd)
		{
			return (true);
		}
	}
	return (false);
}

bool Server::is_client_socket(int fd)
{
	return (active_clients.find(fd) != active_clients.end());
}

bool Server::is_cgi_socket(int fd)
{
	return cgi_runner.is_cgi_fd(fd);
}
ServerContext *Server::get_server_config(int server_fd)
{
	std::map<int, ServerContext *>::iterator it = fd_to_config.find(server_fd);
	if (it != fd_to_config.end())
	{
		return (it->second);
	}
	return (NULL);
}

ServerContext *Server::get_client_config(int client_fd)
{
	int	server_fd;

	std::map<int, int>::iterator server_it = client_to_server.find(client_fd);
	if (server_it != client_to_server.end())
	{
		server_fd = server_it->second;
		return (get_server_config(server_fd));
	}
	return (NULL);
}
void Server::init_data(const std::vector<ServerContext> &configs)
{
	int					port;
	int					server_fd;
	struct epoll_event	event;

	epoll_fd = setup_epoll();
	if (epoll_fd == -1)
	{
		throw std::runtime_error("Failed to create epoll instance");
	}
	for (size_t i = 0; i < configs.size(); i++)
	{
		const ServerContext &config = configs[i];
		port = atoi(config.port.c_str());
		std::cout << "=== SERVER " << (i + 1) << " SETUP ===" << std::endl;
		std::cout << "Config host: '" << config.host << "'" << std::endl;
		std::cout << "Config port: '" << config.port << "'" << std::endl;
		server_fd = setup_Socket_with_host(port, config.host);
		if (server_fd == -1)
		{
			throw std::runtime_error("Failed to setup socket for port "
				+ config.port);
		}
		event.events = EPOLLIN;
		event.data.fd = server_fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
		{
			close(server_fd);
			throw std::runtime_error("Failed to add server socket to epoll");
		}
		std::cout << "Server socket " << server_fd << " added to epoll for port " << port << std::endl;
		// store in mapping tables
		server_fds.push_back(server_fd);
		fd_to_port[server_fd] = port;
		fd_to_config[server_fd] = const_cast<ServerContext *>(&configs[i]);
		std::cout << "Server socket created on port " << port << " (fd: " << server_fd << ")" << std::endl;
	}
}
