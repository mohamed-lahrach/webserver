#include "client.hpp"

Client::Client() : client_fd(-1), request_status(NEED_MORE_DATA)
{
	std::cout << "Client constructor called" << std::endl;
}

Client::~Client()
{
}

int Client::handle_new_connection(int server_fd, int epoll_fd, std::map<int, Client> &active_clients)
{
	Client client;
	struct sockaddr_in client_addr;
	socklen_t client_len;
	struct epoll_event client_event;

	client_len = sizeof(client_addr);
	client.client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
							  &client_len);
	if (client.client_fd == -1)
	{
		std::cout << "ERROR: Failed to accept connection" << std::endl;
		return -1;
	}
	int flags = fcntl(client.client_fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cout << "ERROR: fcntl F_GETFL failed" << std::endl;
		close(client.client_fd);
		return -1;
	}

	if (fcntl(client.client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cout << "ERROR: fcntl F_SETFL failed" << std::endl;
		close(client.client_fd);
		return -1;
	}

	std::cout << "New client connected: " << client.client_fd << std::endl;
	client_event.events = EPOLLIN;
	client_event.data.fd = client.client_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.client_fd, &client_event) == -1)
	{
		std::cout << "ERROR: Failed to add client to epoll" << std::endl;
		close(client.client_fd);
		return -1;
	}
	active_clients[client.client_fd] = client;
	std::cout << "Client " << client.client_fd << " added to map" << std::endl;
	std::cout << "Total active clients: " << active_clients.size() << std::endl;
	return client.client_fd; 
}

void Client::handle_client_data_input(int epoll_fd, std::map<int, Client> &active_clients, ServerContext &server_config, CgiRunner &cgi_runner)
{
	char buffer[7000000] = {0};
	ssize_t bytes_received;
	struct epoll_event ev;

	bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_received > 0)
	{
		buffer[bytes_received] = '\0';
		std::cout << "=== CLIENT " << client_fd << ": PROCESSING REQUEST ===" << std::endl;

		RequestStatus result = current_request.add_new_data(buffer, bytes_received);

		switch (result)
		{
		case NEED_MORE_DATA:
			std::cout << "We need more data from the client" << std::endl;
			return;
		case BAD_REQUEST:
			request_status = result;
			break;
		case HEADERS_ARE_READY:
		{
			current_request.set_config(server_config);
			request_status = current_request.figure_out_http_method();

			if (request_status == BODY_BEING_READ)
			{
				std::cout << "Need more body data - waiting for more..." << std::endl;
				return;
			}

			std::cout << "Request fully processed and ready!" << std::endl;
			std::cout << "Final request - Method: " << current_request.get_http_method()
					  << " Path: " << current_request.get_requested_path() << std::endl;
			if (current_request.is_cgi_request())
			{
				std::cout << "Detected CGI request - starting CGI process" << std::endl;
				LocationContext *location = current_request.get_location();
				if (location)
				{
					std::cout << "CGI Extensions: ";
					for (size_t i = 0; i < location->cgiExtensions.size(); ++i)
					{
						std::cout << location->cgiExtensions[i];
						if (i < location->cgiExtensions.size() - 1)
							std::cout << " ";
					}
					std::cout << std::endl;
					std::cout << "CGI Paths: ";
					for (size_t i = 0; i < location->cgiPaths.size(); ++i)
					{
						std::cout << location->cgiPaths[i];
						if (i < location->cgiPaths.size() - 1)
							std::cout << " ";
					}
					std::cout << std::endl;
				}
				if (location)
				{
					std::string script_path = resolve_file_path(current_request.get_requested_path(), location);
					int cgi_output_fd = cgi_runner.start_cgi_process(current_request, *location, client_fd, script_path);
					if (cgi_output_fd >= 0)
					{
						struct epoll_event cgi_ev;
						cgi_ev.events = EPOLLIN;
						cgi_ev.data.fd = cgi_output_fd;
						if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_output_fd, &cgi_ev) == -1)
						{
							std::cerr << "Failed to add CGI output fd to epoll" << std::endl;
							cgi_runner.cleanup_cgi_process(cgi_output_fd);
						}
						else
						{
							std::cout << "CGI process started, monitoring output fd: " << cgi_output_fd << std::endl;
							return;
						}
					}
					else
					{
						if (cgi_output_fd == -2) 
						{
							request_status = NOT_FOUND;
							std::cerr << "CGI script resulted in 404 Not Found" << std::endl;
						}
						else if (cgi_output_fd == -3)
						{ 
							request_status = FORBIDDEN;
							std::cerr << "CGI script resulted in 403 Forbidden" << std::endl;
						}
						else
						{
							request_status = INTERNAL_ERROR;
							std::cerr << "Failed to start CGI process (internal error)" << std::endl;
						}
					}
				}
			}
			break;
		}
		default:
			std::cout << "UNKNOWN" << std::endl;
			break;
		}
		ev.events = EPOLLOUT;
		ev.data.fd = client_fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &ev) == -1)
		{
			cleanup_connection(epoll_fd, active_clients);
			std::cout << "Failed to modify client socket to EPOLLOUT mode" << std::endl;
			return;
		}
	}
	else if (bytes_received == 0)
	{
		std::cout << "Client " << client_fd << " closed connection gracefully" << std::endl;
		cleanup_connection(epoll_fd, active_clients);
	}
	else
	{
		std::cout << "Error receiving data from client " << client_fd << std::endl;
		cleanup_connection(epoll_fd, active_clients);
	}
}

void Client::handle_client_data_output(int client_fd, int epoll_fd,
									   std::map<int, Client> &active_clients, ServerContext &server_config)
{
	std::cout << "=== GENERATING RESPONSE FOR CLIENT ===========>" << client_fd << " ===" << std::endl;

	current_response.set_server_config(&server_config);

	if (current_response.is_still_streaming())
	{
		std::cout << "Continuing file streaming..." << std::endl;
		current_response.handle_response(client_fd);
	}
	else
	{

		if (request_status == DELETED_SUCCESSFULLY)
		{
			current_response.set_code(200);
			current_response.set_content("<html><body><h1>200 OK</h1><p>File deleted successfully.</p></body></html>");
			current_response.set_header("Content-Type", "text/html");
			current_response.set_header("Connection", "close");
		}
		else if (request_status == POSTED_SUCCESSFULLY)
		{
			current_response.set_code(201);
			current_response.set_content("<html><body><h1>201 Created</h1><p>File created successfully.</p></body></html>");
			current_response.set_header("Content-Type", "text/html");
			current_response.set_header("Connection", "close");
		}
		else if (request_status != EVERYTHING_IS_OK)
		{
			std::cout << "Setting error response for status: " << request_status << std::endl;
			current_response.set_error_response(request_status);
		}
		else
		{
			std::string request_path = current_request.get_requested_path();
			std::cout << "=== ANALYZING REQUEST PATH: " << request_path << " ===" << std::endl;

			LocationContext *location = current_request.get_location();
			std::cout << "Creating normal response for path: " << request_path << std::endl;
			current_response.analyze_request_and_set_response(request_path, location);
		}

		current_response.handle_response(client_fd);
	}

	if (!current_response.is_still_streaming())
	{
		std::cout << "Response complete - cleaning up connection" << std::endl;
		cleanup_connection(epoll_fd, active_clients);
	}
	else
		std::cout << "File streaming in progress - keeping connection alive" << std::endl;
}

void Client::cleanup_connection(int epoll_fd, std::map<int,
													   Client> &active_clients)
{
	std::cout << "=== CLEANING UP CLIENT " << client_fd << " ===" << std::endl;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
	{
		std::cout << "Warning: Failed to remove client " << client_fd << " from epoll" << std::endl;
	}
	else
	{
		std::cout << "Client " << client_fd << " removed from epoll" << std::endl;
	}
	if (close(client_fd) == -1)
	{
		std::cout << "Warning: Failed to close client " << client_fd << " socket" << std::endl;
	}
	else
	{
		std::cout << "Client " << client_fd << " socket closed" << std::endl;
	}
	active_clients.erase(client_fd);
	std::cout << "Client removed from active clients map" << std::endl;
}