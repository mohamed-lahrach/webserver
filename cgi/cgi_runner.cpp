#include "cgi_runner.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

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
    
    // No query string support - set empty query string
    env.push_back("QUERY_STRING=");
    
    // Add headers as environment variables
    const std::map<std::string, std::string>& headers = request.get_all_headers();
    std::map<std::string, std::string>::const_iterator it;
    
    it = headers.find("content-type");
    if (it != headers.end()) {
        env.push_back("CONTENT_TYPE=" + it->second);
    }
    
    it = headers.find("content-length");
    if (it != headers.end()) {
        env.push_back("CONTENT_LENGTH=" + it->second);
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
    
    return env;
}

int CgiRunner:: start_cgi_process(const Request& request, 
                                const LocationContext& location,
                                int client_fd,
                                const std::string& script_path) {
    
    std::cout << "Starting CGI process for script: " << script_path << std::endl;
    std::cout << "CGI interpreter: " << location.cgiPath << std::endl;
    std::cout << "Request method: " << request.get_http_method() << std::endl;
    std::cout << "Request path: " << request.get_requested_path() << std::endl;
    
    // Check if script file exists
    if (access(script_path.c_str(), F_OK) != 0) {
        std::cerr << "CGI script not found: " << script_path << std::endl;
        return -1;
    }
    
    if (access(script_path.c_str(), R_OK) != 0) {
        std::cerr << "CGI script not readable: " << script_path << std::endl;
        return -1;
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
        args.push_back(location.cgiPath);  // interpreter path
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
    if (request.get_http_method() == "POST" && !request.get_request_body().empty()) {
        const std::string& body = request.get_request_body();
        write(input_pipe[1], body.c_str(), body.size());
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
    
    // Check for timeout (30 seconds)
    time_t current_time = time(NULL);
    if (current_time - it->second.start_time > 30) {
        std::cout << "CGI process timed out, killing process" << std::endl;
        if (it->second.pid > 0) {
            kill(it->second.pid, SIGKILL);
            waitpid(it->second.pid, NULL, 0);
        }
        it->second.finished = true;
        response_data = "HTTP/1.1 504 Gateway Timeout\r\nContent-Type: text/plain\r\nContent-Length: 19\r\nConnection: close\r\n\r\nCGI process timeout";
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
        // Error or would block
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::cout << "CGI read would block, trying again later" << std::endl;
            return false; // Would block, try again later
        } else {
            std::cerr << "Error reading from CGI process: " << strerror(errno) << std::endl;
            it->second.finished = true;
            // Create error response
            response_data = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: 22\r\nConnection: close\r\n\r\nCGI process failed.\n";
            return true;
        }
    }
}

bool CgiRunner::handle_cgi_input(int fd, const std::string& data) {
    std::map<int, CgiProcess>::iterator it = active_cgi_processes.find(fd);
    if (it == active_cgi_processes.end() || it->second.input_fd < 0) {
        return false;
    }
    
    ssize_t bytes_written = write(it->second.input_fd, data.c_str(), data.size());
    if (bytes_written < 0) {
        std::cerr << "Error writing to CGI process: " << strerror(errno) << std::endl;
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
    
    // Extract Content-Type from CGI headers
    std::string content_type = "text/plain; charset=utf-8";
    if (!headers.empty()) {
        std::istringstream header_stream(headers);
        std::string line;
        while (std::getline(header_stream, line)) {
            if (line.find("Content-Type:") == 0) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    content_type = line.substr(colon_pos + 1);
                    // Trim whitespace
                    while (!content_type.empty() && (content_type[0] == ' ' || content_type[0] == '\t')) {
                        content_type.erase(0, 1);
                    }
                    if (!content_type.empty() && content_type[content_type.size() - 1] == '\r') {
                        content_type.erase(content_type.size() - 1);
                    }
                }
                break;
            }
        }
    }
    
    // Build HTTP response
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    
    return response.str();
}


