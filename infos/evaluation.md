# Evaluation Compliance Guide

## Critical Requirements for Passing

### Immediate Failure Conditions (Grade 0)
- **Multiple epoll/select calls** - Must use single epoll_wait() in main loop
- **Direct file descriptor access** - All I/O must go through epoll
- **errno checking after recv/send** - Forbidden, results in automatic failure
- **Segfaults or crashes** - Any unexpected termination fails evaluation
- **Memory leaks** - Must pass valgrind testing
- **Compilation issues** - Must compile without re-link problems

### Mandatory Features Checklist

#### Core HTTP Server
- [ ] GET, POST, DELETE request handling
- [ ] Unknown request methods (must not crash)
- [ ] Proper HTTP status codes (must match standards)
- [ ] File upload and retrieval functionality
- [ ] Static website serving capability

#### Configuration System
- [ ] Multiple servers with different ports
- [ ] Multiple servers with different hostnames
- [ ] Custom error pages (especially 404)
- [ ] Client body size limiting
- [ ] Route setup to different directories
- [ ] Default file configuration for directories
- [ ] Method restrictions per route

#### CGI Support (MANDATORY)
- [ ] CGI script execution (fork + execve)
- [ ] Proper working directory for CGI
- [ ] GET and POST method support with CGI
- [ ] Error handling for CGI failures
- [ ] No server crashes on CGI errors

#### Browser Compatibility
- [ ] Serve fully static websites
- [ ] Directory listing functionality
- [ ] URL redirection handling
- [ ] Proper request/response headers
- [ ] Developer tools compatibility

#### Stress Testing Requirements
- [ ] Siege stress test >99.5% availability
- [ ] No memory leaks during extended operation
- [ ] No hanging connections
- [ ] Indefinite operation without restart

### Implementation Patterns Required

#### Single epoll() Pattern
```cpp
// CORRECT: Single epoll_wait() checking both events
while (true) {
    int events = epoll_wait(epoll_fd, event_list, MAX_EVENTS, timeout);
    for (int i = 0; i < events; i++) {
        if (event_list[i].events & EPOLLIN) {
            // ONE read operation only
        }
        if (event_list[i].events & EPOLLOUT) {
            // ONE write operation only  
        }
    }
}
```

#### Error Handling Pattern
```cpp
// CORRECT: Check return values, no errno
ssize_t result = recv(fd, buffer, size, 0);
if (result <= 0) {
    cleanup_client(fd);  // Remove on error/disconnect
}

// FORBIDDEN: errno checking
if (result == -1 && errno == EAGAIN) { /* FAILS EVALUATION */ }
```

#### CGI Execution Pattern
```cpp
// REQUIRED: Fork + execve for CGI
pid_t pid = fork();
if (pid == 0) {
    // Child: setup environment, pipes, execve
    execve(cgi_path, args, env);
} else {
    // Parent: manage pipes through epoll
}
```

### Testing Commands for Evaluation

#### Basic Functionality
```bash
# Test basic methods
curl -X GET http://localhost:8080/
curl -X POST -d "data" http://localhost:8080/
curl -X DELETE http://localhost:8080/file.txt

# Test unknown methods (should not crash)
curl -X PATCH http://localhost:8080/

# Test file operations
curl -X POST -F "file=@test.txt" http://localhost:8080/upload
curl http://localhost:8080/uploaded_file.txt
```

#### Configuration Testing
```bash
# Multiple hostnames
curl --resolve example.com:80:127.0.0.1 http://example.com/

# Body size limits
curl -X POST -H "Content-Type: text/plain" --data "LARGE_BODY_HERE" http://localhost:8080/

# Error pages
curl http://localhost:8080/nonexistent_file
```

#### CGI Testing
```bash
# CGI with GET
curl http://localhost:8080/cgi-bin/script.py

# CGI with POST
curl -X POST -d "param=value" http://localhost:8080/cgi-bin/script.py
```

#### Stress Testing
```bash
# Install siege
brew install siege  # or apt-get install siege

# Stress test (must achieve >99.5% availability)
siege -b http://localhost:8080/

# Memory leak check
valgrind --leak-check=full ./webserv config.conf
```

### Common Failure Points

1. **Multiple epoll calls** - Using epoll_wait() in different functions
2. **Direct socket operations** - Bypassing epoll for any I/O
3. **errno usage** - Checking errno after system calls
4. **Missing CGI** - No CGI implementation at all
5. **Incorrect status codes** - Not matching HTTP standards
6. **Memory leaks** - Not cleaning up resources properly
7. **Crash on invalid input** - Not handling malformed requests
8. **Configuration parsing** - Not supporting all required directives

### Pre-Evaluation Checklist

Before evaluation, ensure:
- [ ] Code compiles without warnings/errors
- [ ] All required features are implemented
- [ ] No segfaults with any input
- [ ] Memory leak testing passes
- [ ] Stress testing achieves required availability
- [ ] CGI scripts work in correct directory
- [ ] Browser compatibility verified
- [ ] All configuration options functional