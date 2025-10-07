# webserv

A small HTTP web server implemented in C++ (C++98). This project implements a configurable HTTP server with support for static files, CGI execution, and basic request methods. It's structured for development and testing with the included configuration files and `www/` test content.

you can see the subject in folder /subject

<img src="/subject/1.png" with = 500>

## Quick summary

- Language: C++ (C++98)
- Build system: Makefile
- Main executable: `webserv`
- Example config: `test_configs/default.conf`
- Test website root: `www/`

## Features

- Serve static files from configured document roots
- CGI support (see `www/cgi-bin/`)
- Basic HTTP methods: GET, POST, DELETE
- Configurable servers and locations via a config file parser
- epoll-based I/O (edge-triggered/reactor style) for efficiency

<img src="/subject/2.png" with = 500>

<img src="/subject/3.png" with = 500>

<img src="/subject/1.png" with = 500>

## Repository layout (important files)

- `main.cpp` - program entry
- `Makefile` - build and run helpers
- `Server_setup/` - server, socket, and epoll setup code
- `request/` - request parsing and method handlers (GET, POST, DELETE)
- `response/` - response generation
- `config/` - configuration lexer and parser
- `cgi/` - CGI runner utilities
- `utils/` - helper utilities and MIME types
- `www/` - sample website, CGI scripts and test pages
- `test_configs/` - example server configs used for testing

## Requirements

- A C++ compiler that supports C++98 (g++)
- GNU Make
- POSIX-compatible OS (Linux recommended for testing)

## Build

Build the project using Make:

```zsh
make
```

For a debug build (adds `-DDEBUG`):

```zsh
make debug
```

Clean build artifacts:

```zsh
make clean
make fclean   # removes the `webserv` binary as well
```

## Run

There's a convenience make target that builds and runs the server using the provided default config:

```zsh
make run
```

Or run the compiled binary directly and pass a configuration file path:

```zsh
./webserv ./test_configs/default.conf
```

By default the example config (`test_configs/default.conf`) references the `www/` directory in this repo as the server document root.

## Testing the server (simple examples)

From another terminal, try a few requests with `curl` (adjust host/port to match your config):

```zsh
# List index page
curl -i http://127.0.0.1:3080/

# GET a static file
curl -i http://127.0.0.1:3080/index.html

# Execute a CGI script
curl -i "http://127.0.0.1:3080/cgi-bin/get.py"

# POST with form data to a CGI
curl -i -X POST -d "name=foo" http://127.0.0.1:3080/cgi-bin/post_test.py

# DELETE a file (if the config and permissions allow it)
curl -i -X DELETE http://127.0.0.1:3080/some/path
```

See `www/cgi-bin/` for example CGI scripts used during testing.

## Configuration

The server reads a configuration file on startup. Example config files live in `test_configs/`.
The parser and lexer are implemented under `config/`.

Key fields typically include:

- listen address/port
- server names
- root (document root)
- index file
- location blocks (for routing, CGI, uploads, redirects, etc.)

Refer to `test_configs/default.conf` and `test_configs/multi_cgi.conf` as working examples.

## Development notes

- The codebase targets C++98 and uses a Makefile-based workflow.
- To enable debug logging, use `make debug` or compile with `-DDEBUG`.
- The project uses epoll for multiplexed I/O. Be careful when testing on platforms that do not support epoll.
- Source files are organized by responsibility (server setup, request handling, config parsing, CGI runner, utilities).

Contract (quick):

- Input: a filesystem-backed configuration file path passed as argv[1].
- Output: an HTTP server that listens on the address/port from the config and serves requests according to config rules.
- Error modes: startup errors (invalid config, port in use), runtime errors (bad requests, IO errors), CGI failures. Check stderr/log output.

Common edge-cases to keep in mind:

- Missing or invalid configuration file
- Large request bodies (POST) and upload handling
- Concurrent connections and resource limits
- Permission issues when writing/deleting files
