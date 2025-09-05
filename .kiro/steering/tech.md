# Technology Stack

## Language & Standards
- **C++98** - Strict adherence to C++98 standard for compatibility
- **POSIX** - Uses POSIX system calls for networking and file operations

## Build System
- **Make** - Simple Makefile-based build system
- **Compiler**: c++ with flags: `-Wall -Wextra -Werror -std=c++98 -g3 -O0`

## Core Technologies
- **epoll** - Linux-specific I/O event notification for non-blocking operations
- **TCP sockets** - Standard BSD socket API for network communication
- **CGI** - Common Gateway Interface for dynamic content execution
- **HTTP/1.1** - Protocol implementation with proper header parsing

## Key Libraries & System Calls
- Standard C++ STL containers (vector, map, string)
- POSIX networking: `socket()`, `bind()`, `listen()`, `accept()`
- I/O multiplexing: `epoll_create()`, `epoll_ctl()`, `epoll_wait()`
- Process management: `fork()`, `execve()` for CGI execution
- File operations: `open()`, `read()`, `write()`, `close()`

## Common Commands

### Build & Run
```bash
make                    # Build the webserv binary
make clean             # Remove object files
make fclean            # Remove all build artifacts
make re                # Clean rebuild
make run               # Build, clean objects, and run with default config
make debug             # Build with DEBUG flag enabled
make leaks             # Run with valgrind for memory leak detection
```

### Execution
```bash
./webserv ./test_configs/default.conf    # Run with specific config
./webserv ./test_configs/cgi_test.conf   # Run with CGI test config
```

### Testing
```bash
valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./webserv config.conf
```

## Critical Implementation Constraints
- **Single epoll_wait()** - Must use only one epoll_wait() call in main loop
- **No errno checking** - Forbidden after recv/send operations (evaluation requirement)
- **Non-blocking I/O** - All socket operations must be non-blocking
- **Memory management** - Must be leak-free (valgrind compliant)
- **No external libraries** - Only standard C++98 STL and POSIX system calls