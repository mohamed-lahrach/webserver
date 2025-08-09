#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
#include "../config/parser.hpp"
# include <map>
# include "request_status.hpp"
# include "get_handler.hpp"
# include "post_handler.hpp"
# include "delete_handler.hpp"

class Request
{
  private:
  std::string http_method;       
  std::string requested_path;    
  std::string http_version;     
  std::string full_path;    
  std::map<std::string, std::string> http_headers;  
  

  std::string incoming_data;      
  bool got_all_headers;          
  size_t expected_body_size;
  size_t body_bytes_we_have;
  std::string request_body;       
  const ServerContext* cfg_;
  const LocationContext* loc_;
  
  
  GetHandler get_handler;
  PostHandler post_handler;
  DeleteHandler delete_handler;       
  
  // Helper to select the best-matching location by longest prefix
  const LocationContext* match_location(const std::string& path) const;

  public:

	Request();

	
	~Request();
	
	const std::string& get_http_method() const { return http_method; }
	const std::string& get_requested_path() const { return requested_path; }
	const std::string& get_http_version() const { return http_version; }
	const std::map<std::string, std::string>& get_all_headers() const { return http_headers; }
	const std::string& get_request_body() const { return request_body; }

    // Bind the parsed server config so Request can consult locations/methods
    void set_config(const ServerContext& cfg);

	RequestStatus add_new_data(const char *new_data, size_t data_size);
	RequestStatus figure_out_http_method();
	

	bool parse_http_headers(const std::string& header_text);
	
	bool handle_request(int client_fd, const char *request_data);
};

#endif