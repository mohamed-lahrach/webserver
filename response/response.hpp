#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <cstring>
# include <iostream>
# include <string>
# include <sys/socket.h>
# include <map>
class Response
{
  private:
  int status_code;
  std::string content;
  std::map<std::string, std::string> headers;
  public:
	// Constructor
	Response();

	// Destructor
	~Response();
	
	// Setter methods
	void set_code(int code);
	void set_content(const std::string& body_content);
	void set_header(const std::string& key, const std::string& value);
	
	void handle_response(int client_fd);
};

#endif