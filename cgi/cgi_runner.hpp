#ifndef CGI_RUNNER_HPP
#define CGI_RUNNER_HPP

#include "cgi_headers.hpp"
#include "../request/request.hpp"
#include "../config/parser.hpp"
#include <map>

class CgiRunner {
private:
    std::map<int, CgiProcess> active_cgi_processes; // fd -> CgiProcess
    
    std::vector<std::string> build_cgi_env(const Request& request, 
                                          const std::string& server_name,
                                          const std::string& server_port,
                                          const std::string& script_name);
    
public:
    CgiRunner();
    ~CgiRunner();
    
    // Start a CGI process and return the output fd for epoll monitoring
    int start_cgi_process(const Request& request, 
                         const LocationContext& location,
                         int client_fd,
                         const std::string& script_path);
    
    // Handle I/O on CGI file descriptors
    bool handle_cgi_output(int fd, std::string& response_data);
    bool handle_cgi_input(int fd, const std::string& data);
    
    // Check if fd belongs to a CGI process
    bool is_cgi_fd(int fd) const;
    
    // Get client fd associated with CGI process
    int get_client_fd(int fd) const;
    
    // Clean up finished CGI process
    void cleanup_cgi_process(int fd);
    
    // Check for finished processes
    void check_finished_processes();
    
    // Timeout management
    bool check_cgi_timeout(int fd, std::string& response_data);
    void update_cgi_activity(int fd);
    std::vector<int> get_timed_out_cgi_fds() const;
    
private:
    // Format CGI output into HTTP response
    std::string format_cgi_response(const std::string& cgi_output);
};

#endif