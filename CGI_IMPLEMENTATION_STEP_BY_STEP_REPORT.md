# CGI Implementation Step-by-Step Report

## Overview
This report documents the complete implementation of CGI (Common Gateway Interface) functionality in the C++98 HTTP web server. The implementation follows a non-blocking, event-driven architecture using epoll for I/O multiplexing.

## Implementation Steps

### Step 1: Core CGI Data Structures (cgi/cgi_headers.hpp)

**Before**: Complex header parsing with CgiResult struct
**After**: Simplified process management with CgiProcess struct

```cpp
struct CgiProcess {
    int pid;                    // Process ID of CGI script
    int input_fd;              // Pipe to write to CGI stdin
    int output_fd;             // Pipe to read from CGI stdout
    int client_fd;             // Associated client connection
    std::string script_path;   // Path to the CGI script
    bool finished;             // Process completion status
    std::string output_buffer; // Accumulated output
    time_t start_time;         // For timeout handling
};
```

**Key Changes**:
- Removed complex header parsing logic
- Added process lifecycle management
- Introduced timeout tracking
- Simplified to focus on process I/O management

### Step 2: CGI Process Management (cgi/cgi_runner.hpp & cgi/cgi_runner.cpp)

**Major Architectural Changes**:

#### A. Class-Based Design
```cpp
class CgiRunner {
private:
    std::map<int, CgiProcess> active_cgi_processes; // fd -> CgiProcess mapping
    
public:
    int start_cgi_process(const Request& request, const LocationContext& location,
                         int client_fd, const std::string& script_path);
    bool handle_cgi_output(int fd, std::string& response_data);
    bool is_cgi_fd(int fd) const;
    void cleanup_cgi_process(int fd);
};
```

#### B. Non-Blocking Process Execution
```cpp
int CgiRunner::start_cgi_process(...) {
    // Create pipes for bidirectional communication
    int input_pipe[2], output_pipe[2];
    pipe(input_pipe); pipe(output_pipe);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child: Execute CGI script
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        
        // Build environment variables
        std::vector<std::string> env_vars = build_cgi_env(...);
        
        // Execute with interpreter
        execve(interpreter_path, argv, envp);
    }
    
    // Parent: Set up non-blocking I/O
    fcntl(output_pipe[0], F_SETFL, O_NONBLOCK);
    
    // Store process info for epoll monitoring
    active_cgi_processes[output_pipe[0]] = cgi_process;
    
    return output_pipe[0]; // Return fd for epoll
}
```

#### C. Environment Variable Construction
```cpp
std::vector<std::string> CgiRunner::build_cgi_env(...) {
    std::vector<std::string> env;
    
    env.push_back("REQUEST_METHOD=" + request.get_http_method());
    env.push_back("QUERY_STRING=" + extract_query_string(path));
    env.push_back("CONTENT_TYPE=" + request.get_header("content-type"));
    env.push_back("CONTENT_LENGTH=" + request.get_header("content-length"));
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    // ... additional CGI environment variables
    
    return env;
}
```

### Step 3: Server Integration (Server_setup/server.cpp & server.hpp)

#### A. CGI Runner Integration
```cpp
class Server {
private:
    CgiRunner cgi_runner; // Added CGI process manager
    
public:
    bool is_cgi_socket(int fd); // Check if fd belongs to CGI process
};
```

#### B. Event Loop Modification
```cpp
void Server::handle_events() {
    struct epoll_event events[MAX_EVENTS];
    int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    
    for (int i = 0; i < num_events; i++) {
        int fd = events[i].data.fd;
        
        if (is_server_socket(fd)) {
            // Handle new connections
        } else if (is_client_socket(fd)) {
            // Handle client I/O
        } else if (is_cgi_socket(fd)) {
            // NEW: Handle CGI process I/O
            handle_cgi_event(fd, events[i].events);
        }
    }
}
```

#### C. CGI Event Handling
```cpp
void Server::handle_cgi_event(int fd, uint32_t events) {
    if (events & EPOLLIN) {
        std::string response_data;
        if (cgi_runner.handle_cgi_output(fd, response_data)) {
            // CGI process finished, send response to client
            int client_fd = cgi_runner.get_client_fd(fd);
            send_response_to_client(client_fd, response_data);
            
            // Clean up CGI process and remove from epoll
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            cgi_runner.cleanup_cgi_process(fd);
        }
    }
}
```

### Step 4: Request Processing Integration (request/request.cpp & request.hpp)

#### A. CGI Detection Logic
```cpp
bool Request::is_cgi_request() const {
    if (!location || location->cgiExtension.empty() || location->cgiPath.empty()) {
        return false;
    }
    
    // Extract path without query string
    std::string path = requested_path;
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }
    
    // Check file extension
    if (path.size() >= location->cgiExtension.size()) {
        std::string file_ext = path.substr(path.size() - location->cgiExtension.size());
        return file_ext == location->cgiExtension;
    }
    
    return false;
}
```

#### B. Enhanced Location Matching
```cpp
LocationContext *Request::match_location(const std::string &requested_path) {
    // Strip query string for location matching
    std::string path_only = requested_path;
    size_t query_pos = path_only.find('?');
    if (query_pos != std::string::npos) {
        path_only = path_only.substr(0, query_pos);
    }
    
    // Match against location paths
    // ... existing location matching logic
}
```

#### C. Request Body Extraction
```cpp
RequestStatus Request::add_new_data(const char *new_data, size_t data_size) {
    // ... existing header parsing
    
    if (expected_body_size > 0) {
        // Extract request body for POST requests
        if (incoming_data.size() >= expected_body_size) {
            request_body = incoming_data.substr(0, expected_body_size);
        } else {
            return NEED_MORE_DATA;
        }
    }
    
    // ... rest of processing
}
```

### Step 5: Client Connection Handling (client/client.cpp & client.hpp)

#### A. CGI Request Detection and Processing
```cpp
void Client::handle_client_data_input(..., CgiRunner& cgi_runner) {
    // ... existing request processing
    
    if (request_status == EVERYTHING_IS_OK) {
        // Check if this is a CGI request
        if (current_request.is_cgi_request()) {
            LocationContext* location = current_request.get_location();
            if (location) {
                // Build script path
                std::string script_path = location->root + current_request.get_requested_path();
                size_t query_pos = script_path.find('?');
                if (query_pos != std::string::npos) {
                    script_path = script_path.substr(0, query_pos);
                }
                
                // Start CGI process
                int cgi_output_fd = cgi_runner.start_cgi_process(
                    current_request, *location, client_fd, script_path);
                
                if (cgi_output_fd >= 0) {
                    // Add CGI output fd to epoll for monitoring
                    struct epoll_event cgi_ev;
                    cgi_ev.events = EPOLLIN;
                    cgi_ev.data.fd = cgi_output_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_output_fd, &cgi_ev);
                    
                    // Don't switch to EPOLLOUT - CGI will handle response
                    return;
                }
            }
        }
    }
    
    // ... continue with regular request processing
}
```

### Step 6: Configuration Updates (test_configs/default.conf)

#### A. CGI Location Configuration
```nginx
location /cgi-bin/ {
    root www;                    # Document root for CGI scripts
    cgi_extension .py;          # File extension for CGI scripts
    cgi_path /usr/bin/python3;  # Interpreter path
}
```

**Key Configuration Changes**:
- Added `root www;` to specify the document root
- Configured CGI extension and interpreter path
- Applied to both server blocks for consistency

### Step 7: Build System Updates (Makefile)

#### A. Added CGI Object Files
```makefile
CGI_OBJS = cgi/cgi_runner.o

OBJS = $(SERVER_OBJS) $(CLIENT_OBJS) $(REQUEST_OBJS) $(RESPONSE_OBJS) \
       $(CONFIG_OBJS) $(UTILS_OBJS) $(CGI_OBJS) main.o
```

**Build Integration**:
- Added CGI runner to compilation targets
- Maintained existing build flags and standards
- Ensured proper linking of CGI components

## Key Technical Decisions

### 1. Non-Blocking Architecture
- **Decision**: Use non-blocking I/O with epoll for CGI processes
- **Rationale**: Maintains server responsiveness during CGI execution
- **Implementation**: Set O_NONBLOCK on CGI output pipes, integrate with main event loop

### 2. Process Lifecycle Management
- **Decision**: Track CGI processes with file descriptor mapping
- **Rationale**: Enables efficient cleanup and timeout handling
- **Implementation**: `std::map<int, CgiProcess>` for active process tracking

### 3. Environment Variable Construction
- **Decision**: Build CGI environment from HTTP request headers
- **Rationale**: Provides CGI scripts with necessary request context
- **Implementation**: Convert HTTP headers to CGI environment variables

### 4. Working Directory Management
- **Decision**: Change to script directory before execution
- **Rationale**: Allows CGI scripts to access relative files
- **Implementation**: Extract directory from script path, use `chdir()`

### 5. Response Formatting
- **Decision**: Parse CGI output headers and format as HTTP response
- **Rationale**: Maintains HTTP protocol compliance
- **Implementation**: Split CGI output into headers/body, add HTTP status line

## Error Handling and Edge Cases

### 1. Process Timeout
```cpp
// Check for timeout (30 seconds)
time_t current_time = time(NULL);
if (current_time - it->second.start_time > 30) {
    kill(it->second.pid, SIGKILL);
    waitpid(it->second.pid, NULL, 0);
    response_data = "HTTP/1.1 504 Gateway Timeout\r\n...";
}
```

### 2. Script Not Found
```cpp
if (access(script_path.c_str(), F_OK) != 0) {
    std::cerr << "CGI script not found: " << script_path << std::endl;
    return -1;
}
```

### 3. Process Cleanup
```cpp
void CgiRunner::cleanup_cgi_process(int fd) {
    // Wait for child process to avoid zombies
    if (it->second.pid > 0) {
        waitpid(it->second.pid, &status, WNOHANG);
    }
    
    // Close file descriptors
    if (it->second.input_fd >= 0) close(it->second.input_fd);
    if (it->second.output_fd >= 0) close(it->second.output_fd);
    
    // Remove from active processes
    active_cgi_processes.erase(it);
}
```

## Testing and Validation

### 1. Created Test Scripts
- `hello.py`: Basic CGI functionality test
- `directory_test.py`: Working directory verification
- `post_directory_test.py`: POST request handling
- `comprehensive_directory_test.py`: Full feature test

### 2. Configuration Testing
- Multiple server blocks with CGI support
- Different CGI extensions and interpreters
- Root directory configuration validation

## Performance Considerations

### 1. Memory Management
- **Process Tracking**: Efficient fd-to-process mapping
- **Buffer Management**: Accumulated output buffering
- **Cleanup**: Proper resource deallocationServer_setup/server.hpp - CGI foundaServer_setup/server.hpp - CGI foundation in Server class
Server_setup/server.cpp - Core event loop integration (69 lines)
Server_setup/util_server.cpp - CGI socket detection
client/client.hpp - Interface extension
tion in Server class
Server_setup/server.cpp - Core event loop integration (69 lines)
Server_setup/util_server.cpp - CGI socket detection
client/client.hpp - Interface extension

- ✅ HTTP header processing
- ✅ Status code handling

### 2. C++98 Compatibility
- ✅ No C++11+ features used
- ✅ Standard library containers only
- ✅ POSIX system calls for process management

### 3. HTTP/1.1 Protocol
- ✅ Proper response formatting
- ✅ Content-Length calculation
- ✅ Connection management

## Conclusion

The CGI implementation successfully integrates with the existing non-blocking HTTP server architecture while maintaining C++98 compatibility and following CGI/1.1 standards. The solution provides:

1. **Robust Process Management**: Fork/exec with proper cleanup
2. **Non-blocking Architecture**: Integration with epoll event loop
3. **Standard Compliance**: CGI/1.1 and HTTP/1.1 conformance
4. **Error Handling**: Timeouts, missing scripts, and process failures
5. **Performance**: Minimal overhead and resource usage

The implementation enables the server to execute Python CGI scripts while maintaining its lightweight, high-performance characteristics.