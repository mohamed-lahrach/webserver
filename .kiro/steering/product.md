# Product Overview

This is **webserv**, a custom HTTP/1.0 web server implementation written in C++98. The project is part of the 42 School curriculum and focuses on understanding HTTP protocol fundamentals and non-blocking I/O operations.

## Key Features

- HTTP/1.0 compliant web server
- Support for GET, POST, and DELETE methods
- Non-blocking I/O using epoll/select/kqueue
- Configuration file support (nginx-style syntax)
- Static file serving with directory listing
- File upload capabilities
- Custom error pages
- Multiple server blocks and location routing
- CGI support for dynamic content

## Purpose

The server demonstrates core web server concepts including:
- Socket programming and network communication
- HTTP request/response parsing and handling
- Configuration file parsing
- Non-blocking event-driven architecture
- MIME type handling
- Error handling and status codes

This is an educational project designed to provide deep understanding of how web servers work at the protocol level.