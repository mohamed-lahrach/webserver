# Project Structure

## Directory Organization

### Core Components
- **`main.cpp`** - Entry point, config parsing, and server initialization
- **`Server_setup/`** - Server initialization, socket setup, and epoll management
  - `server.hpp/cpp` - Main Server class with epoll event loop
  - `socket_setup.cpp` - Socket creation and binding logic
  - `epoll_setup.cpp` - Epoll initialization and event management
  - `util_server.cpp` - Server utility functions
- **`client/`** - Client connection management
  - `client.hpp/cpp` - Client class for connection state tracking
- **`request/`** - HTTP request parsing and handling
  - `request.hpp/cpp` - Main Request class and parsing logic
  - `get_handler.hpp/cpp` - GET method implementation
  - `post_handler.hpp/cpp` - POST method and file upload handling
  - `delete_handler.hpp/cpp` - DELETE method implementation
  - `post_handler_utils.cpp` - POST request utilities
  - `request_status.hpp` - Request state enumeration
- **`response/`** - HTTP response generation
  - `response.hpp/cpp` - Response formatting and status codes
- **`cgi/`** - CGI script execution
  - `cgi_runner.hpp/cpp` - CGI process management with fork/execve
  - `cgi_headers.hpp` - CGI-specific data structures
- **`config/`** - Configuration file parsing
  - `parser.hpp/cpp` - Main configuration parser
  - `Lexer.hpp/cpp` - Configuration tokenization
  - `helper_functions.hpp/cpp` - Parsing utilities
- **`utils/`** - Utility functions
  - `mime_types.hpp/cpp` - MIME type detection
  - `utils.hpp/cpp` - General utility functions

### Configuration & Content
- **`test_configs/`** - Server configuration files
  - `default.conf` - Main server configuration
  - `cgi_test.conf` - CGI-specific configuration
- **`www/`** - Web content root directory
  - `index.html` - Default homepage
  - `cgi-bin/` - CGI scripts directory
  - `dir/` - Test directory for directory listing
  - `restricted_dir/` - Access-controlled content
- **`upload/`** - File upload destination directory
- **`error_pages/`** - Custom error page templates

### Build & Documentation
- **`Makefile`** - Build configuration with standard targets
- **`infos/`** - Project documentation and evaluation guides
- **`mime.types`** - MIME type mappings file

## Architecture Patterns

### Class Hierarchy
```
Server (main event loop)
├── Client (connection management)
├── Request (HTTP parsing)
│   ├── GetHandler
│   ├── PostHandler
│   └── DeleteHandler
├── Response (HTTP formatting)
├── CgiRunner (process management)
└── Parser/Lexer (configuration)
```

### Key Design Principles
- **Single Responsibility** - Each class handles one aspect of HTTP processing
- **Event-Driven** - All I/O operations go through epoll event loop
- **Non-blocking** - No blocking operations in main thread
- **Resource Management** - RAII pattern for file descriptors and memory
- **Configuration-Driven** - Behavior controlled by config files

### File Naming Conventions
- **Headers**: `.hpp` extension
- **Implementation**: `.cpp` extension
- **Object files**: `.o` extension (generated during build)
- **Executable**: `webserv` (final binary)
- **Config files**: `.conf` extension

### Code Organization Rules
- Include guards in all header files (`#ifndef CLASSNAME_HPP`)
- Private members first, then public in class definitions
- Forward declarations to minimize header dependencies
- Consistent indentation and bracket placement
- Error handling without errno checking (evaluation constraint)