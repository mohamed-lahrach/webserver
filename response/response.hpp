#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <cstring>
# include <iostream>
# include <string>
# include <sys/socket.h>
class Response
{
  private:
  public:
	// Constructor
	Response();

	// Destructor
	~Response();
	void handle_response(int client_fd);
};

#endif