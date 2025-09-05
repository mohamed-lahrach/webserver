#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../request/request.hpp"
# include "../response/response.hpp"
# include "../cgi/cgi_runner.hpp"
# include <arpa/inet.h>
# include <cstring>
# include <ctime>
# include <exception>
# include <fcntl.h>
# include <iostream>
# include <map>
# include <netinet/in.h>
# include <stdexcept>
# include <string>
# include <sys/epoll.h>
# include <sys/socket.h>
# include <unistd.h>
#include "../config/parser.hpp"

class	Response;
class	Request;

class Client
{
  private:
	time_t connect_time; // Keep same order as in constructor
	int request_count;   // Move this after connect_time
	int client_fd;
	Request current_request;
	Response current_response;
	RequestStatus request_status;

  public:
	// Constructor
	Client();

	// Destructor
	~Client();

	// Getter methods
	time_t get_connect_time() const;
	int get_request_count() const;

	// Setter methods
	void set_connect_time(time_t time);
	void increment_request_count();
	/// 

	static int handle_new_connection(int server_fd, int epoll_fd, std::map<int,
		Client> &active_clients);
	void handle_client_data_input(int epoll_fd,std::map<int, Client> &active_clients,ServerContext& server_config, CgiRunner& cgi_runner);
	void handle_client_data_output(int client_fd, int epoll_fd, std::map<int,
		Client> &active_clients,ServerContext& server_config);
	void cleanup_connection(int epoll_fd, std::map<int, Client> &active_clients);
};

#endif // CLIENT_HPP