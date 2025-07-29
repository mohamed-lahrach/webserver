#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>
# include <map>

class Request
{
  private:
  std::string method;
  std::string path;
  std::string version;
  std::map<std::string, std::string> headers;
  public:
	// Constructor
	Request();

	// Destructor
	~Request();
	const std::string& get_method() const { return method; }
	const std::string& get_path() const { return path; }
	const std::string& get_version() const { return version; }
	const std::map<std::string, std::string>& get_headers() const { return headers; }

	bool handle_request(int client_fd, const char *request_data);
};

#endif