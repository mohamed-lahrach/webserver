#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>

class Request
{
  private:
  public:
	// Constructor
	Request();

	// Destructor
	~Request();
	bool handle_request(int client_fd, const char *request_data);
};

#endif