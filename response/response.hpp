#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <cstring>
#include <iostream>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <map>
#include "../config/parser.hpp"
#include "../request/request_status.hpp"
#include "../utils/mime_types.hpp"

class Client;
class Response
{
private:
	int status_code;
	std::string content;
	std::map<std::string, std::string> headers;
	MimeTypes mine_type;
	std::string current_file_path;
	std::ifstream *file_stream;
	bool is_streaming_file;
	char file_buffer[9000];

public:
	Response();

	~Response();

	void set_code(int code);
	void set_content(const std::string &body_content);
	void set_header(const std::string &key, const std::string &value);
	void set_error_response(RequestStatus status);
	void set_redirect_response(int status_code, const std::string &location);
	bool handle_return_directive(const std::string &return_directive);
	void handle_directory_listing(const std::string &file_path, const std::string &path, LocationContext *location_config);

	void analyze_request_and_set_response(const std::string &path,LocationContext *location_config);
	void check_file(const std::string &file_path);
	void start_file_streaming(int client_fd);
	void finish_file_streaming();
	void continue_file_streaming(int client_fd);
	bool is_still_streaming() const;
	std::string list_dir(const std::string &path, const std::string &request_path);

	std::string what_reason(int code);

	void handle_response(int client_fd);
};

#endif