#ifndef POST_HANDLER_HPP
# define POST_HANDLER_HPP

# include "request_status.hpp"
# include <map>
# include <string>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <sstream> 
#include <cstdlib>
#include "../config/parser.hpp"

class PostHandler
{
  private:
	bool is_chunked;
    std::string chunk_buffer;

  public:
	PostHandler();
	~PostHandler();

	RequestStatus handle_post_request(const std::string &requested_path,
		const std::map<std::string, std::string> &http_headers,
		std::string &incoming_data, size_t expected_body_size, const ServerContext *cfg, const LocationContext *loc);
	void parse_type_body(const std::string &body,
		const std::map<std::string, std::string> &http_headers);
	void save_request_body(const std::string &filename,
		const std::string &body);
	void parse_data_body(const std::string &body,
		const std::string &content_type);
	RequestStatus handle_post_request_with_chunked(const std::map<std::string, std::string> &http_headers,
		std::string &incoming_data, size_t expected_body_size, const ServerContext *cfg, const LocationContext *loc);
	std::string extract_boundary(const std::string &content_type);
	std::string extract_filename(const std::string &body);
	size_t parse_max_body_size(const std::string &size_str);
};

#endif
