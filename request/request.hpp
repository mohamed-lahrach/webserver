#ifndef REQUEST_HPP
# define REQUEST_HPP

# include <string>

class Request
{
  private:
  std::string method;
  std::string path;
  std::string version;
  public:
	// Constructor
	Request();

	// Destructor
	~Request();
	const std::string& get_method() const { return method; }
	const std::string& get_path() const { return path; }
	const std::string& get_version() const { return version; }

	bool handle_request(int client_fd, const char *request_data);
};

#endif