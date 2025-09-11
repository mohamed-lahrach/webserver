#include "client.hpp"

// Constructor
Client::Client() : connect_time(0), request_count(0), request_status(NEED_MORE_DATA)
{
	std::cout << "Client constructor called" << std::endl;
}

// Destructor
Client::~Client()
{
	std::cout << "Client destructor called" << std::endl;
}

time_t Client::get_connect_time() const
{
	return (connect_time);
}

int Client::get_request_count() const
{
	return (request_count);
}

// Setter methods

void Client::set_connect_time(time_t time)
{
	connect_time = time;
}

void Client::increment_request_count()
{
	request_count++;
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
		throw std::runtime_error("Error accepting connection: ");
	}
	// handle non-blocking
	int flags = fcntl(client.client_fd, F_GETFL, 0);
	if (flags == -1) {
        perror("fcntl F_GETFL");
        close(client.client_fd);
    } else if (fcntl(client.client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        close(client.client_fd);
    }
	std::cout << "✓ New client connected: " << client.client_fd << std::endl;
	// Set connection time
	client.set_connect_time(time(NULL));
	// Add client to epoll
	client_event.events = EPOLLIN;
	client_event.data.fd = client.client_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.client_fd, &client_event) ==
		-1)
	{
		close(client.client_fd);
		throw std::runtime_error("Failed to add client to epoll");
	}
	// Add to active clients map
	active_clients[client.client_fd] = client;
	std::cout << "✓ Client " << client.client_fd << " added to map" << std::endl;
	std::cout << "✓ Total active clients: " << active_clients.size() << std::endl;
	return client.client_fd; // Return the new client fd
}

void Client::handle_client_data_input(int epoll_fd, std::map<int, Client> &active_clients, ServerContext &server_config, CgiRunner& cgi_runner)
{
	char buffer[7000000] = {0};
	ssize_t bytes_received;
	struct epoll_event ev;

	bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_received > 0)
	{
		buffer[bytes_received] = '\0';
		std::cout << "=== CLIENT " << client_fd << ": PROCESSING REQUEST ===" << std::endl;
		increment_request_count();

		RequestStatus result = current_request.add_new_data(buffer, bytes_received);

		switch (result)
		{
		case NEED_MORE_DATA:
			std::cout << "We need more data from the client" << std::endl;
			return;

		case HEADERS_ARE_READY:
		{
			std::cout << "Processing request data - checking handler..." << std::endl;
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
			
			// Check if this is a CGI request
			if (current_request.is_cgi_request()) {
				std::cout << "🔧 Detected CGI request - starting CGI process" << std::endl;
				LocationContext* location = current_request.get_location();
				if (location) {
					std::cout << "CGI Extension: " << location->cgiExtension << std::endl;
					std::cout << "CGI Path: " << location->cgiPath << std::endl;
				}
				if (location) {
					// Build script path
					std::string script_path = location->root + current_request.get_requested_path();
					
					// Start CGI process
					int cgi_output_fd = cgi_runner.start_cgi_process(current_request, *location, client_fd, script_path);
					if (cgi_output_fd >= 0) {
						// Add CGI output fd to epoll for monitoring
						struct epoll_event cgi_ev;
						cgi_ev.events = EPOLLIN;
						cgi_ev.data.fd = cgi_output_fd;
						if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_output_fd, &cgi_ev) == -1) {
							std::cerr << "Failed to add CGI output fd to epoll" << std::endl;
							cgi_runner.cleanup_cgi_process(cgi_output_fd);
						} else {
							std::cout << "✅ CGI process started, monitoring output fd: " << cgi_output_fd << std::endl;
							// Don't switch client to EPOLLOUT - CGI will handle the response
							return;
						}
					} else {
						std::cerr << "Failed to start CGI process" << std::endl;
						request_status = INTERNAL_ERROR;
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
			cleanup_connection(epoll_fd, active_clients); // ← Better cleanup
			throw std::runtime_error("Failed to modify client socket to EPOLLOUT mode");
		}
	}
	else 
	{
		std::cout << "Client " << client_fd << " disconnected" << std::endl;
		cleanup_connection(epoll_fd, active_clients);
	}


}

void Client::handle_client_data_output(int client_fd, int epoll_fd,
									   std::map<int, Client> &active_clients, ServerContext &server_config)
{
	(void)server_config;
	std::cout << "=== GENERATING RESPONSE FOR CLIENT ===========>" << client_fd << " ===" << std::endl;

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
	{
		std::cout << "File streaming in progress - keeping connection alive" << std::endl;
	}
}

void Client::cleanup_connection(int epoll_fd, std::map<int,
													   Client> &active_clients)
{
	std::cout << "=== CLEANING UP CLIENT " << client_fd << " ===" << std::endl;

	// Display connection statistics
	std::cout << "✓ Client " << client_fd << " was connected for " << (time(NULL) - get_connect_time()) << " seconds" << std::endl;
	std::cout << "✓ Client " << client_fd << " made " << get_request_count() << " requests" << std::endl;

	// Remove from epoll monitoring
	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
	{
		std::cout << "Warning: Failed to remove client " << client_fd << " from epoll" << std::endl;
	}
	else
	{
		std::cout << "✓ Client " << client_fd << " removed from epoll" << std::endl;
	}

	// Close the socket
	if (close(client_fd) == -1)
	{
		std::cout << "Warning: Failed to close client " << client_fd << " socket" << std::endl;
	}
	else
	{
		std::cout << "✓ Client " << client_fd << " socket closed" << std::endl;
	}

	// Remove from active clients map
	active_clients.erase(client_fd);
	std::cout << "✓ Client removed from active clients map" << std::endl;
}