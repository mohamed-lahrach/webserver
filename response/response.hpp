#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include <cstring>
# include <iostream>
# include <string>
# include <sys/socket.h>
# include <map>
# include "../request/request_status.hpp"
# include "../utils/mime_types.hpp"

class Client;
class Response
{
  private:
  int status_code;
  std::string content;
  std::map<std::string, std::string> headers;
  MimeTypes mime_detector;  
  public:

	Response();


	~Response();
	

	void set_code(int code);
	void set_content(const std::string& body_content);
	void set_header(const std::string& key, const std::string& value);
	void set_error_response(RequestStatus status);
	
	
	void analyze_request_and_set_response(const std::string& path);
	void check_upload_file(const std::string& file_path);
	std::string list_dir(const std::string& path, const std::string& request_path);
	

	std::string what_reason(int code);
	
	void handle_response(int client_fd);
};

#endif