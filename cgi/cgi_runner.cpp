#include "cgi_runner.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

CgiRunner::CgiRunner() {
    std::cout << "CGI Runner initialized" << std::endl;
}

CgiRunner::~CgiRunner() {
    // Clean up any remaining processes
    for (std::map<int, CgiProcess>::iterator it = active_cgi_processes.begin(); 
         it != active_cgi_processes.end(); ++it) {
        if (it->second.pid > 0) {
            kill(it->second.pid, SIGTERM);
            waitpid(it->second.pid, NULL, 0);
        }
        if (it->second.input_fd >= 0) close(it->second.input_fd);
        if (it->second.output_fd >= 0) close(it->second.output_fd);
    }
}

static std::vector<char*> vector_to_char_array(const std::vector<std::string>& vec) {
    std::vector<char*> result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result.push_back(const_cast<char*>(vec[i].c_str()));
    }
    result.push_back(NULL);
    return result;
}

std::vector<std::string> CgiRunner::build_cgi_env(const Request& request, 
                                                 const std::string& server_name,
                                                 const std::string& server_port,
                                                 const std::string& script_name) {
    std::vector<std::string> env;
    
    env.push_back("REQUEST_METHOD=" + request.get_http_method());
    
    env.push_back("QUERY_STRING=" + request.get_query_string());
    
    // Add headers as environment variables
    const std::map<std::string, std::string>& headers = request.get_all_headers();
    std::map<std::string, std::string>::const_iterator it;
    
    it = headers.find("content-type");
    if (it != headers.end()) {
        env.push_back("CONTENT_TYPE=" + it->second);
    }
    
    it = headers.find("content-length");
    if (it != headers.end()) {
        std::string content_length = it->second;
        // Trim whitespace from content-length value
        while (!content_length.empty() && (content_length[0] == ' ' || content_length[0] == '\t' || content_length[0] == '\r' || content_length[0] == '\n')) {
            content_length.erase(0, 1);
        }
        while (!content_length.empty() && (content_length[content_length.size()-1] == ' ' || content_length[content_length.size()-1] == '\t' || content_length[content_length.size()-1] == '\r' || content_length[content_length.size()-1] == '\n')) {
            content_length.erase(content_length.size()-1, 1);
        }
        env.push_back("CONTENT_LENGTH=" + content_length);
        std::cout << "\033[36m📏 Setting CONTENT_LENGTH=" << content_length << "\033[0m" << std::endl;
    } else {
        std::cout << "\033[33m⚠️  No content-length header found in request\033[0m" << std::endl;
    }
    
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("SERVER_NAME=" + server_name);
    env.push_back("SERVER_PORT=" + server_port);
    env.push_back("SCRIPT_NAME=" + script_name);
    env.push_back("PATH_INFO=");
    
    // Add some common HTTP headers
    it = headers.find("host");
    if (it != headers.end()) {
        env.push_back("HTTP_HOST=" + it->second);
    }
    
    it = headers.find("user-agent");
    if (it != headers.end()) {
        env.push_back("HTTP_USER_AGENT=" + it->second);
    }
    
    // Add Cookie header for session support
    it = headers.find("cookie");
    if (it != headers.end()) {
        env.push_back("HTTP_COOKIE=" + it->second);
    }
    
    // Add custom file headers for CGI uploads
    it = headers.find("x-file-name");
    if (it != headers.end()) {
        env.push_back("HTTP_X_FILE_NAME=" + it->second);
    }
    
    it = headers.find("x-file-type");
    if (it != headers.end()) {
        env.push_back("HTTP_X_FILE_TYPE=" + it->second);
    }
    
    it = headers.find("x-file-size");
    if (it != headers.end()) {
        env.push_back("HTTP_X_FILE_SIZE=" + it->second);
    }
    
    return env;
}

// Return values:
//  >=0 : success, this is the output fd to monitor
//   -1 : generic internal error (pipe/fork failure, exec failure)
//   -2 : script not found (404)
//   -3 : script not readable (403)
int CgiRunner:: start_cgi_process(const Request& request, 
                                const LocationContext& location,
                                int client_fd,
                                const std::string& script_path) {
    
    std::cout << "Starting CGI process for script: " << script_path << std::endl;
    
    // Find the appropriate interpreter for this script
    std::string interpreter_path;
    std::string file_ext;
    
    // Extract file extension
    size_t dot_pos = script_path.rfind('.');
    if (dot_pos != std::string::npos) {
        file_ext = script_path.substr(dot_pos);
    }
    
    // Find matching extension and get corresponding interpreter
    for (size_t i = 0; i < location.cgiExtensions.size() && i < location.cgiPaths.size(); ++i) {
        if (file_ext == location.cgiExtensions[i]) {
            interpreter_path = location.cgiPaths[i];
            break;
        }
    }
    
    if (interpreter_path.empty()) {
        std::cout << "No interpreter found for extension: " << file_ext << std::endl;
        return -1;
    }
    
    std::cout << "\033[32mCGI interpreter: " << interpreter_path << "\033[0m" << std::endl;
    std::cout << "\033[32mRequest method: " << request.get_http_method() << "\033[0m" << std::endl;
    std::cout << "\033[32mRequest path: " << request.get_requested_path() << "\033[0m" << std::endl;
    
    // Check if script file exists
    if (access(script_path.c_str(), F_OK) != 0) {
        std::cerr << "CGI script not found: " << script_path << std::endl;
        return -2; // NOT_FOUND
    }
    
    if (access(script_path.c_str(), R_OK) != 0) {
        std::cerr << "CGI script not readable: " << script_path << std::endl;
        return -3; // FORBIDDEN
    }
    
    // Create pipes for communication
    int input_pipe[2], output_pipe[2];
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        std::cerr << "Failed to create pipes for CGI" << std::endl;
        return -1;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to fork CGI process" << std::endl;
        close(input_pipe[0]); close(input_pipe[1]);
        close(output_pipe[0]); close(output_pipe[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child process - execute CGI script
        close(input_pipe[1]);  // Close write end of input pipe (we read from it) we read from input_pipe[0]
        close(output_pipe[0]); // Close read end of output pipe (we write to it) we write to output_pipe[1]
        
        // Change to the directory containing the CGI script
        std::string script_dir = script_path; 
        std::string script_filename = script_path;
        size_t last_slash = script_dir.find_last_of('/');
        if (last_slash != std::string::npos) {
            script_dir = script_dir.substr(0, last_slash);
            script_filename = script_path.substr(last_slash + 1);
            if (chdir(script_dir.c_str()) != 0) {
                std::cerr << "Failed to change directory to: " << script_dir << std::endl;
                _exit(1);
            }
            std::cout << "Changed working directory to: " << script_dir << std::endl;
        }
        
        // Redirect stdin and stdout
        if (dup2(input_pipe[0], STDIN_FILENO) == -1 ||
            dup2(output_pipe[1], STDOUT_FILENO) == -1) {
            _exit(1);
        }
        
        // Close the original pipe file descriptors
        close(input_pipe[0]); 
        close(output_pipe[1]);
        
        // Build environment
        std::vector<std::string> env_vars = build_cgi_env(request, "localhost", "8080", script_path);
        std::vector<char*> envp = vector_to_char_array(env_vars);
        
        // Build arguments
        std::vector<std::string> args;
        args.push_back(interpreter_path);  // interpreter path
        args.push_back(script_filename);   // script filename (relative to working directory)
        std::vector<char*> argv = vector_to_char_array(args);
        
        // Execute the CGI script
        // execve will write in stdout which is  now redirected to output_pipe[1]
        execve(argv[0], &argv[0], &envp[0]);
        _exit(127); // exec failed
    }
    
    // Parent process
    close(input_pipe[0]);  // Close read end of input pipe
    close(output_pipe[1]); // Close write end of output pipe
    
    // Make output pipe non-blocking
    int flags = fcntl(output_pipe[0], F_GETFL, 0);
    fcntl(output_pipe[0], F_SETFL, flags | O_NONBLOCK);
    
    // Store CGI process info
    CgiProcess cgi_proc; // Initialize with default constructor
    cgi_proc.pid = pid; // Store the actual pid of the forked process which is the child
    cgi_proc.input_fd = input_pipe[1]; // Parent writes to this fd
    cgi_proc.output_fd = output_pipe[0]; // Parent reads from this fd
    cgi_proc.client_fd = client_fd; // Original client socket fd 
    cgi_proc.script_path = script_path; // Store the script path like /usr/lib/cgi-bin/script.py
    cgi_proc.finished = false; // Not finished yet which helps in handle_cgi_output to prevent infinite loop
    cgi_proc.start_time = time(NULL); // Record start time for timeout handling which helps in handle_cgi_output
    
    active_cgi_processes[output_pipe[0]] = cgi_proc; // Map output fd to CgiProcess struct
    
    // Write request body to CGI stdin if it's a POST request
    if (request.get_http_method() == "POST") {
        // Try to get body from CGI POST handler first, then fallback to regular body
        std::string body = request.get_cgi_post_body();
        if (body.empty()) {
            body = request.get_request_body();
        }
        
        if (!body.empty()) {
            std::cout << "\033[35m📊 POST body size: " << body.size() << " bytes\033[0m" << std::endl;
            write(input_pipe[1], body.c_str(), body.size());
            std::cout << "\033[32mWrote " << body.size() << " bytes of POST data to CGI stdin\033[0m" << std::endl;
            //std::cout << "\033[32mWrote " << body << "\033[0m" << std::endl;
        } else {
            std::cout << "\033[33m⚠️  POST body is empty!\033[0m" << std::endl;
        }
    }
    
    // Close input pipe to signal EOF to CGI
    close(input_pipe[1]);
    active_cgi_processes[output_pipe[0]].input_fd = -1;
    
    std::cout << "CGI process started with PID: " << pid << ", output fd: " << output_pipe[0] << std::endl;
    return output_pipe[0]; // Return output fd for epoll monitoring
}

bool CgiRunner::handle_cgi_output(int fd, std::string& response_data) {


    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);

    
    if (it == active_cgi_processes.end()) {
        std::cout << "CGI fd " << fd << " not found in active processes" << std::endl;
        return false;
    }
    
    // Check if process is already finished to prevent infinite loop
    if (it->second.finished) {
        std::cout << "CGI process already finished, returning cached response" << std::endl;
        response_data = format_cgi_response(it->second.output_buffer);
        return true;
    }
    
    // Check for timeout (10 seconds) - this is backup in case the main timeout checker misses it
    time_t current_time = time(NULL);
    if (current_time - it->second.start_time > 10) {
        std::cout << "\033[31m⏰ CGI process timed out in handle_cgi_output! PID: " << it->second.pid << "\033[0m" << std::endl;
        if (it->second.pid > 0) {
            kill(it->second.pid, SIGKILL);
            waitpid(it->second.pid, NULL, 0);
            std::cout << "\033[31m🔪 Killed timed out CGI process PID: " << it->second.pid << "\033[0m" << std::endl;
        }
        it->second.finished = true;
        it->second.output_buffer = "Status: 504 Gateway Timeout\r\n"
                                  "Content-Type: text/html\r\n"
                                  "\r\n"
                                  "<html><body><h1>504 Gateway Timeout</h1>"
                                  "<p>The CGI script took too long to respond (10+ seconds).</p></body></html>";
        response_data = format_cgi_response(it->second.output_buffer);
        return true;
    }
    
    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    
    //std::cout << "CGI read result: " << bytes_read << " bytes, errno: " << errno << std::endl;
    
    if (bytes_read > 0) {
        it->second.output_buffer.append(buffer, bytes_read);
        std::cout << "Read " << bytes_read << " bytes from CGI process, total: " << it->second.output_buffer.size() << std::endl;
        return false; // Continue reading, don't send response yet
    } else if (bytes_read == 0) {
        // EOF - CGI process finished
        std::cout << "CGI process finished (EOF), total output: " << it->second.output_buffer.size() << " bytes" << std::endl;
        it->second.finished = true;
        
        // Wait for the child process to avoid zombies
        if (it->second.pid > 0) {
            int status;
            pid_t result = waitpid(it->second.pid, &status, WNOHANG);
            if (result > 0) {
                std::cout << "CGI process " << it->second.pid << " exited with status " << status << std::endl;
            } else if (result == 0) {
                // Process still running, wait for it
                result = waitpid(it->second.pid, &status, 0);
                if (result > 0) {
                    std::cout << "CGI process " << it->second.pid << " exited with status " << status << std::endl;
                }
            }
        }
        
        // Format CGI output as HTTP response
        response_data = format_cgi_response(it->second.output_buffer);
        std::cout << "Formatted CGI response: " << response_data.size() << " bytes" << std::endl;
        return true; // Response is ready
    } else {
        // Error occurred (bytes_read < 0)
        std::cout << "CGI read error or would block, trying again later" << std::endl;
        return false; // Try again later - could be EAGAIN/EWOULDBLOCK
    }
}

bool CgiRunner::handle_cgi_input(int fd, const std::string& data) {
    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
    if (it == active_cgi_processes.end() || it->second.input_fd < 0) {
        return false;
    }
    
    ssize_t bytes_written = write(it->second.input_fd, data.c_str(), data.size());
    if (bytes_written < 0) {
        std::cout << "Error writing to CGI process" << std::endl;
        return false;
    }
    
    std::cout << "Wrote " << bytes_written << " bytes to CGI process" << std::endl;
    return true;
}

bool CgiRunner::is_cgi_fd(int fd) const {
    return active_cgi_processes.find(fd) != active_cgi_processes.end();
}

int CgiRunner::get_client_fd(int fd) const {
    std::map<int, CgiProcess>::const_iterator it = active_cgi_processes.find(fd);
    if (it != active_cgi_processes.end()) {
        return it->second.client_fd;
    }
    return -1;
}

void CgiRunner::cleanup_cgi_process(int fd) {
    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
    if (it != active_cgi_processes.end()) {
        std::cout << "Cleaning up CGI process with PID: " << it->second.pid << std::endl;
        
        if (it->second.pid > 0) {
            int status;
            waitpid(it->second.pid, &status, WNOHANG);
        }
        
        if (it->second.input_fd >= 0) {
            close(it->second.input_fd);
        }
        if (it->second.output_fd >= 0) {
            close(it->second.output_fd);
        }
        
        active_cgi_processes.erase(it);
    }
}

void CgiRunner::check_finished_processes() {
    std::vector<int> finished_fds;
    
    for (std::map<int, CgiProcess>::iterator it = active_cgi_processes.begin();
         it != active_cgi_processes.end(); ++it) {
        if (it->second.finished) {
            finished_fds.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < finished_fds.size(); ++i) {
        cleanup_cgi_process(finished_fds[i]);
    }
}

void CgiRunner::check_cgi_timeouts() {
    std::vector<int> timed_out_fds;
    time_t current_time = time(NULL);
    
    for (std::map<int, CgiProcess>::iterator it = active_cgi_processes.begin();
         it != active_cgi_processes.end(); ++it) {
        
        if (!it->second.finished && (current_time - it->second.start_time > 10)) {
            std::cout << "\033[31m⏰ CGI process timed out! PID: " << it->second.pid 
                      << " (running for " << (current_time - it->second.start_time) << " seconds)\033[0m" << std::endl;
            
            // Kill the process
            if (it->second.pid > 0) {
                kill(it->second.pid, SIGKILL);
                waitpid(it->second.pid, NULL, 0);
                std::cout << "\033[31m🔪 Killed CGI process PID: " << it->second.pid << "\033[0m" << std::endl;
            }
            
            // Mark as finished with timeout response
            it->second.finished = true;
            it->second.output_buffer = "Status: 504 Gateway Timeout\r\n"
                                      "Content-Type: text/html\r\n"
                                      "\r\n"
                                      "<html><body><h1>504 Gateway Timeout</h1>"
                                      "<p>The CGI script took too long to respond (10+ seconds).</p></body></html>";
            
            timed_out_fds.push_back(it->first);
        }
    }
    
    // Send timeout responses to clients
    for (size_t i = 0; i < timed_out_fds.size(); ++i) {
        int fd = timed_out_fds[i];
        std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
        if (it != active_cgi_processes.end()) {
            int client_fd = it->second.client_fd;
            if (client_fd >= 0) {
                std::string formatted_response = format_cgi_response(it->second.output_buffer);
                ssize_t bytes_sent = send(client_fd, formatted_response.c_str(), 
                                        formatted_response.size(), 0);
                if (bytes_sent > 0) {
                    std::cout << "\033[33m📤 Sent timeout response to client " << client_fd << "\033[0m" << std::endl;
                }
            }
        }
    }
}

std::string CgiRunner::format_cgi_response(const std::string& cgi_output) {
    // Split CGI output into headers and body
    std::string headers, body;
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = cgi_output.find("\n\n");
        if (header_end != std::string::npos) {
            headers = cgi_output.substr(0, header_end);
            body = cgi_output.substr(header_end + 2);
        } else {
            // No headers found, treat entire output as body
            headers = "";
            body = cgi_output;
        }
    } else {
        headers = cgi_output.substr(0, header_end);
        body = cgi_output.substr(header_end + 4);
    }
    
    // Parse ALL CGI headers and collect them
    std::string content_type = "text/html; charset=utf-8";  // Default content type
    std::vector<std::string> all_headers;
    int status_code = 200;  // Default status
    
    if (!headers.empty()) {
        std::istringstream header_stream(headers);
        std::string line;
        while (std::getline(header_stream, line)) {
            // Remove trailing \r if present
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line.erase(line.size() - 1);
            }
            
            if (line.empty()) continue;  // Skip empty lines
            
            // Check for Status header
            if (line.find("Status:") == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string status_str = line.substr(colon_pos + 1);
                    // Trim whitespace
                    while (!status_str.empty() && (status_str[0] == ' ' || status_str[0] == '\t')) {
                        status_str.erase(0, 1);
                    }
                    // Extract status code (first number)
                    std::istringstream status_stream(status_str);
                    status_stream >> status_code;
                    if (status_code < 100 || status_code > 599) {
                        status_code = 200;  // Fallback to 200 if invalid
                    }
                }
                continue;  // Don't include Status header in HTTP response
            }
            
            // Check for Content-Type header
            if (line.find("Content-Type:") == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    content_type = line.substr(colon_pos + 1);
                    // Trim whitespace
                    while (!content_type.empty() && (content_type[0] == ' ' || content_type[0] == '\t')) {
                        content_type.erase(0, 1);
                    }
                }
                continue;  // We'll add Content-Type separately
            }
            
            // For all other headers (including Set-Cookie), add them to the response
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                all_headers.push_back(line);
            }
        }
    }
    
    // Build HTTP response with proper status line
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code;
    switch (status_code) {
        case 200: response << " OK"; break;
        case 302: response << " Found"; break;
        case 404: response << " Not Found"; break;
        case 500: response << " Internal Server Error"; break;
        default: response << " Status"; break;
    }
    response << "\r\n";
    
    // Add Content-Type
    response << "Content-Type: " << content_type << "\r\n";
    
    // Add all other CGI headers (including Set-Cookie)
    for (size_t i = 0; i < all_headers.size(); ++i) {
        response << all_headers[i] << "\r\n";
    }
    
    // Add Content-Length and Connection headers
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}


